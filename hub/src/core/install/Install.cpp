// SPDX-License-Identifier: GPL-3.0-only
#include "Install.h"
#include "ArtifactStore.h"
#include "Support.h"
#include "core/catalog/CatalogLoader.h"
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QThread>
#include <QUuid>
#include <QtConcurrent>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace ac::install {
namespace {
struct Abrupt {
}; // Only fault injection throws this; bypasses automatic rollback.
QString fileIdentity(const QString &path) {
#ifdef Q_OS_WIN
  HANDLE h =
      CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_READ_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    throw Error("E_READ", "Cannot inspect media identity");
  BY_HANDLE_FILE_INFORMATION info{};
  const bool ok = GetFileInformationByHandle(h, &info) != 0;
  CloseHandle(h);
  if (!ok)
    throw Error("E_READ", "Cannot inspect media identity");
  return QString::number(info.dwVolumeSerialNumber) + ":" +
         QString::number(info.nFileIndexHigh) + ":" +
         QString::number(info.nFileIndexLow);
#else
  struct stat st{};
  if (::stat(QFile::encodeName(path).constData(), &st) != 0)
    throw Error("E_READ", "Cannot inspect media identity");
  return QString::number(st.st_dev) + ":" + QString::number(st.st_ino);
#endif
}
bool hardlink(const QString &source, const QString &destination) {
#ifdef Q_OS_WIN
  return CreateHardLinkW(reinterpret_cast<LPCWSTR>(destination.utf16()),
                         reinterpret_cast<LPCWSTR>(source.utf16()),
                         nullptr) != 0;
#else
  return ::link(QFile::encodeName(source).constData(),
                QFile::encodeName(destination).constData()) == 0;
#endif
}
void cleanStaging(const Request &r, const QString &run) {
  if (!QRegularExpression("^[0-9a-f-]{36}$").match(run).hasMatch())
    return;
  const auto staging = scopedPath("user/state/staging/" + run, r.root);
  // This run-specific internal staging directory is never a user-supplied path.
  if (QFileInfo(staging).exists() && !QFileInfo(staging).isSymLink())
    QDir(staging).removeRecursively();
}
QStringList strings(const Json &v) {
  QStringList out;
  if (v.is_array())
    for (const auto &x : v)
      if (x.is_string())
        out << QString::fromStdString(x.get<std::string>());
  return out;
}
QMap<QString, QString> variables(const Request &r, const QString &dir) {
  QMap<QString, QString> vars{
      {"install_root", QDir(r.root).absolutePath()},
      {"install_dir", dir},
      {"download_dir", r.root + "/user/cache/artifacts"},
      {"emulators_dir", r.root + "/emulators"},
      {"game.id", r.gameId},
      {"game.title", r.gameId},
      {"platform", "windows-x64"}};
  for (const auto *space : {"media", "tools"}) {
    const auto group = r.bindings.value(space, Json::object());
    for (auto it = group.begin(); it != group.end(); ++it) {
      const auto base = QString(space) + "." + QString::fromStdString(it.key());
      const auto path = string(it.value(), "path");
      vars[base + ".path"] = path;
      vars[base + ".dir"] =
          QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath();
      vars[base + ".name"] = QFileInfo(path).fileName();
      vars[base + ".sha256"] = string(it.value(), "sha256");
      vars[base + ".verified"] =
          it.value().value("verified", false) ? "true" : "false";
      vars[base + ".version"] = string(it.value(), "version");
    }
  }
  for (const auto &[name, j] :
       {std::pair<QString, Json>{"settings", r.settings},
        {"profile", r.profile}})
    for (auto it = j.begin(); it != j.end(); ++it)
      vars[name + "." + QString::fromStdString(it.key())] =
          it.value().is_string()
              ? QString::fromStdString(it.value().get<std::string>())
              : QString::fromStdString(it.value().dump());
  return vars;
}
bool condition(const Json &when, const Request &r) {
  if (when.empty())
    return true;
  if (!when.is_object())
    throw Error("E_PLAN_INVALID", "Condition must be a table");
  if (when.contains("all") || when.contains("any")) {
    const bool all = when.contains("all");
    if (when.size() != 1)
      throw Error("E_PLAN_INVALID", "Unknown condition key");
    bool result = all;
    for (const auto &w : when[all ? "all" : "any"]) {
      const auto pass = condition(w, r);
      result = all ? (result && pass) : (result || pass);
    }
    return result;
  }
  if (when.contains("not")) {
    if (when.size() != 1)
      throw Error("E_PLAN_INVALID", "Unknown condition key");
    return !condition(when["not"], r);
  }
  if (when.contains("setting")) {
    if (when.size() != 2 || !when.contains("equals"))
      throw Error("E_PLAN_INVALID", "Unknown setting condition");
    return dotted(Json{{"settings", r.settings}, {"profile", r.profile}},
                  string(when, "setting")) == when["equals"];
  }
  if (when.contains("media") || when.contains("tool")) {
    const auto media = when.contains("media");
    if (when.size() != 2 || !when.contains("state"))
      throw Error("E_PLAN_INVALID", "Unknown binding condition");
    const auto key = string(when, media ? "media" : "tool").toStdString();
    const auto group =
        r.bindings.value(media ? "media" : "tools", Json::object());
    const auto item = group.value(key, Json::object());
    const auto state = string(when, "state");
    if (state != "verified" && state != "present")
      throw Error("E_PLAN_INVALID", "Unknown binding state");
    return state == "verified" ? item.value("verified", false)
                               : !string(item, "path").isEmpty();
  }
  throw Error("E_PLAN_INVALID", "Unknown condition");
}
void event(const Options &o, const QString &run, const QString &kind,
           const QString &text, const QString &code = {},
           const QString &step = {}, int index = 0, int total = 0) {
  QVariantMap value{
      {"ts", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
      {"run", run},
      {"kind", kind},
      {"text", text},
      {"code", code},
      {"step", step},
      {"index", index},
      {"total", total}};
  if (o.event)
    o.event(value);
}
struct Lock {
  QLockFile global, variant;
  explicit Lock(const Request &r)
      : global(scopedPath("user/state/locks/global.lock", r.root)),
        variant(scopedPath("user/state/locks/" + r.gameId + "--" + r.variantId +
                               ".lock",
                           r.root)) {
    stateBase(r);
    QDir().mkpath(r.root + "/user/state/locks");
    global.setStaleLockTime(0);
    variant.setStaleLockTime(0);
    if (!global.tryLock(0) || !variant.tryLock(0))
      throw Error("E_LOCKED", "Another install is active; queued service can "
                              "retry after it completes");
  }
};
void fault(const Options &o, const QString &point) {
  if (o.fault)
    try {
      o.fault(point);
    } catch (...) {
      throw Abrupt();
    }
}
Json emptyManifest(const Request &r, const Plan &p, const QString &run) {
  return Json{
      {"game", r.gameId.toStdString()},
      {"variant", r.variantId.toStdString()},
      {"install_dir",
       QDir(r.root).relativeFilePath(p.installDir).toStdString()},
      {"run", run.toStdString()},
      {"installed_when", string(p.variant, "installed_when").toStdString()},
      {"recipe_sha256",
       sha256(QByteArray::fromStdString(r.recipe.dump())).toStdString()},
      {"file", Json::array()},
      {"directory", Json::array()},
      {"config", Json::array()},
      {"shortcut", Json::array()}};
}
class Transaction {
public:
  Request r;
  Options options;
  QString base, run, dir, activeStep;
  Json manifest, baseline, stateBefore, manifestBefore, guard = Json::object();
  bool warning = false;
  Transaction(Request req, Options opts, Plan p)
      : r(std::move(req)), options(std::move(opts)), base(stateBase(r)),
        run(QUuid::createUuid().toString(QUuid::WithoutBraces)),
        dir(p.installDir) {
    stateBefore = readEnvelope(base + ".toml");
    manifestBefore = readEnvelope(base + ".manifest.toml");
    baseline = manifestBefore;
    manifest = emptyManifest(r, p, run);
    if (baseline.contains("directory"))
      manifest["directory"] = baseline["directory"];
    const auto caller = options.event;
    const auto log =
        r.root + "/user/logs/install/" +
        QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss") + "-" + run;
    QDir().mkpath(QFileInfo(log).absolutePath());
    options.event = [caller, log, this](const QVariantMap &value) {
      if (value.value("kind").toString() == "warn") warning = true;
      if (caller)
        caller(value);
      const auto json =
          QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);
      durableAppend(log + ".jsonl", Json::parse(json.toStdString()));
      QFile text(log + ".log");
      if (text.open(QIODevice::WriteOnly | QIODevice::Append)) {
        text.write((value.value("kind").toString() + "  " +
                    value.value("text").toString() + "\n")
                       .toUtf8());
        text.flush();
      }
    };
    if (!stateBefore.empty() &&
        QSet<QString>{"installing", "updating", "repairing", "uninstalling",
                      "rollback-incomplete"}
            .contains(string(stateBefore, "state")))
      throw Error("E_INTERRUPTED", "Recovery required before another run");
    QDir().mkpath(QFileInfo(base).absolutePath());
    if (QFileInfo::exists(base + ".journal.jsonl"))
      atomicWrite(base + ".journal.jsonl.previous",
                  readBytes(base + ".journal.jsonl"));
    atomicWrite(base + ".journal.jsonl", {});
    append(Json{{"kind", "begin"},
                {"run", run.toStdString()},
                {"state_before", stateBefore},
                {"manifest_before", manifestBefore}});
    Json state{{"game", r.gameId.toStdString()},
               {"variant", r.variantId.toStdString()},
               {"state", r.operation == "update"      ? "updating"
                         : r.operation == "repair"    ? "repairing"
                         : r.operation == "uninstall" ? "uninstalling"
                                                      : "installing"},
               {"run", run.toStdString()},
               {"previous_state", string(stateBefore, "state", "not-installed").toStdString()}};
    writeEnvelope(base + ".toml", state);
    fault(options, "start-state");
  }
  void append(const Json &j) { durableAppend(base + ".journal.jsonl", j); }
  QString backupFile(const QString &path) {
    const auto hash = hashFile(path), destination = r.root + "/user/state/backups/" + hash;
    if (!QFileInfo::exists(destination)) atomicCopy(path, destination);
    return hash;
  }
  void rawCopy(const QString &source, const QString &path) {
    const auto prior = QFileInfo::exists(path) ? backupFile(path) : QString();
    mkdir(QFileInfo(path).absolutePath());
    Json intent{{"kind", "intent"}, {"op", "file"}, {"path", path.toStdString()},
                {"prior", prior.toStdString()}, {"after", hashFile(source).toStdString()}, {"remove", false}};
    append(intent); fault(options, "intent");
    atomicCopy(source, path);
    if (hashFile(path) != string(intent, "after")) throw Error("E_FILE_CHANGED", "Copy source changed during streaming");
    fault(options, "write");
    intent["kind"] = "done"; append(intent); fault(options, "done");
  }
  void unlinkMedia(const QString &path, const Json &row) {
    const auto source = string(row, "source");
    scopedPath(source, QFileInfo(source).absolutePath());
    // Keep only link identity in the journal; the original media is not backed up.
    if (!QFileInfo(source).isFile() || fileIdentity(source) != string(row, "file_id"))
      throw Error("E_MEDIA_MISMATCH", "Original media link changed; keep the installed link");
    Json intent{{"kind", "intent"}, {"op", "media-unlink"}, {"path", path.toStdString()},
                {"source", source.toStdString()}, {"file_id", string(row, "file_id").toStdString()}};
    append(intent); fault(options, "intent");
    if (!QFile::remove(path)) throw Error("E_FILE_IN_USE", "Cannot remove media link");
    fault(options, "write"); intent["kind"] = "done"; append(intent); fault(options, "done");
  }
  void checkpoint() {
    fault(options, "before-manifest");
    writeEnvelope(base + ".manifest.toml", manifest);
    fault(options, "after-manifest");
  }
  void mkdir(const QString &path) {
    if (QFileInfo::exists(path)) {
      if (!QFileInfo(path).isDir())
        throw Error("E_WRITE_CONFLICT", "Directory path is a file");
      return;
    }
    const auto parent = QFileInfo(path).absolutePath();
    if (parent != path && !QFileInfo::exists(parent))
      mkdir(parent);
    append(Json{
        {"kind", "intent"}, {"op", "mkdir"}, {"path", path.toStdString()}});
    fault(options, "intent");
    if (!QDir().mkdir(path))
      throw Error("E_WRITE_DENIED", "Cannot create " + path);
    fault(options, "write");
    append(
        Json{{"kind", "done"}, {"op", "mkdir"}, {"path", path.toStdString()}});
    fault(options, "done");
    manifest["directory"].push_back(path.toStdString());
  }
  void rawWrite(const QString &path, const QByteArray &bytes,
                bool remove = false) {
    const auto existed = QFileInfo::exists(path);
    const auto prior = existed ? backupFile(path) : QString();
    mkdir(QFileInfo(path).absolutePath());
    Json intent{{"kind", "intent"},
                {"op", "file"},
                {"path", path.toStdString()},
                {"prior", prior.toStdString()},
                {"after", remove ? "" : sha256(bytes).toStdString()},
                {"remove", remove}};
    append(intent);
    fault(options, "intent");
    if (remove) {
      if (existed && !QFile::remove(path))
        throw Error("E_FILE_IN_USE", "Cannot remove file");
    } else
      atomicWrite(path, bytes);
    fault(options, "write");
    intent["kind"] = "done";
    append(intent);
    fault(options, "done");
  }
  void rename(const QString &from, const QString &to) {
    Json intent{{"kind", "intent"},
                {"op", "rename"},
                {"path", from.toStdString()},
                {"to", to.toStdString()}};
    append(intent);
    fault(options, "intent");
    if (QFileInfo::exists(to) || !QDir().rename(from, to))
      throw Error("E_FILE_IN_USE", "Cannot swap install directory");
    fault(options, "write");
    intent["kind"] = "done";
    append(intent);
    fault(options, "done");
  }
  void removeEmptyDirectory(const QString &path) {
    if (!QFileInfo(path).isDir() ||
        !QDir(path)
             .entryList(QDir::AllEntries | QDir::Hidden | QDir::System |
                        QDir::NoDotAndDotDot)
             .isEmpty())
      return;
    Json intent{
        {"kind", "intent"}, {"op", "rmdir"}, {"path", path.toStdString()}};
    append(intent);
    fault(options, "intent");
    if (!QDir().rmdir(path))
      throw Error("E_FILE_IN_USE", "Cannot remove empty owned directory");
    fault(options, "write");
    intent["kind"] = "done";
    append(intent);
    fault(options, "done");
  }
  bool linkMedia(const QString &source, const QString &destination,
                 const QString &step) {
    const auto path = scopedPath(destination, dir);
    const auto hash = hashFile(source);
    if (QFileInfo::exists(path)) {
      if (fileIdentity(source) != fileIdentity(path))
        throw Error("E_WRITE_CONFLICT", "Media link destination exists");
      return true;
    }
    mkdir(QFileInfo(path).absolutePath());
    Json intent{{"kind", "intent"},
                {"op", "file"},
                {"path", path.toStdString()},
                {"prior", ""},
                {"after", hash.toStdString()},
                {"remove", false}};
    append(intent);
    fault(options, "intent");
    if (!hardlink(source, path)) {
      append(Json{{"kind", "noop"}, {"path", path.toStdString()}});
      return false;
    }
    fault(options, "write");
    intent["kind"] = "done";
    append(intent);
    fault(options, "done");
    manifest["file"].push_back(
        Json{{"path", QDir(dir).relativeFilePath(path).toStdString()},
             {"kind", "data"},
             {"origin", "user-media-link"},
             {"step", step.toStdString()},
             {"sha256", hash.toStdString()},
             {"bytes", QFileInfo(source).size()},
             {"prior", "absent"},
             {"backup", ""},
             {"source", source.toStdString()},
             {"file_id", fileIdentity(source).toStdString()},
             {"owners", Json::array({r.variantId.toStdString()})}});
    checkpoint();
    return true;
  }
  Json oldRow(const QString &relative) const {
    for (const auto &row : manifest.value("file", Json::array()))
      if (string(row, "path") == relative)
        return row;
    for (const auto &row : baseline.value("file", Json::array()))
      if (string(row, "path") == relative)
        return row;
    return Json::object();
  }
  void write(const QString &path, const QByteArray &bytes,
             const QString &origin, const QString &step,
             const Json &keys = Json::array(), const QString &format = {},
             const QString &copySource = {}) {
    const auto safe = scopedPath(path, dir);
    const auto relative = QDir(dir).relativeFilePath(safe);
    const auto old = oldRow(relative);
    const auto ext = QFileInfo(path).suffix().toLower();
    const bool config =
        QSet<QString>{"ini", "cfg", "json", "toml", "yaml", "xml", "txt", "csv"}
            .contains(ext);
    const bool exists = QFileInfo::exists(path);
    const auto hash = copySource.isEmpty() ? sha256(bytes) : hashFile(copySource);
    if (origin != "user-media-copy" && origin != "user-media-link")
      for (const auto &blocked : guard.value("sha256", Json::array()))
        if (blocked.is_string() &&
            QString::fromStdString(blocked.get<std::string>())
                    .compare(hash, Qt::CaseInsensitive) == 0)
          throw Error("E_CONTENT_GUARD",
                      "Known content hash refused before file placement");
    if (keys.empty() && !old.empty() && exists && hashFile(path) != string(old, "sha256")) {
      if (!config)
        throw Error("E_FILE_CHANGED", "Owned binary changed: " + relative);
      auto kept = old;
      kept["user_edited"] = true;
      for (auto current = manifest["file"].begin();
           current != manifest["file"].end(); ++current)
        if (string(*current, "path") == relative) {
          manifest["file"].erase(current);
          break;
        }
      manifest["file"].push_back(kept);
      warning = true;
      event(options, run, "warn", "Kept your edited " + relative);
      return;
    }
    if ((origin == "user-media-copy" || origin == "user-media-link") &&
        exists && hashFile(path) != hash)
      throw Error("E_WRITE_CONFLICT", "Media destination differs");
    Json row = old.empty()
                   ? Json{{"path", relative.toStdString()},
                          {"prior", exists ? "backup" : "absent"},
                          {"backup",
                           exists ? backupFile(path).toStdString() : ""}}
                   : old;
    row["kind"] = config ? "config" : "binary";
    row["origin"] = origin.toStdString();
    row["step"] = step.toStdString();
    row["sha256"] = hash.toStdString();
    row["bytes"] = copySource.isEmpty() ? bytes.size() : QFileInfo(copySource).size();
    row["user_edited"] = false;
    if (!keys.empty() && !old.empty() && exists &&
        hashFile(path) != string(old, "sha256")) {
      row["user_edited"] = true;
      warning = true;
    }
    row["owners"] = Json::array({r.variantId.toStdString()});
    if (!keys.empty()) {
      row["format"] = format.toStdString();
      auto managed = old.value("keys", Json::array());
      row["keys"] = Json::array();
      for (auto key : keys) {
        for (const auto &prior : managed)
          if (string(prior, "key") == string(key, "key") &&
              string(prior, "section") == string(key, "section")) {
            key["prior"] = prior["prior"];
            key["prior_value"] = prior["prior_value"];
            break;
          }
        row["keys"].push_back(key);
      }
      for (const auto &prior : managed) {
        bool replaced = false;
        for (const auto &current : row["keys"])
          if (string(prior, "key") == string(current, "key") &&
              string(prior, "section") == string(current, "section"))
            replaced = true;
        if (!replaced)
          row["keys"].push_back(prior);
      }
    }
    if (!exists || hashFile(path) != hash) {
      if (copySource.isEmpty()) rawWrite(path, bytes);
      else rawCopy(copySource, path);
    }
    for (size_t i = 0; i < manifest["file"].size(); ++i)
      if (string(manifest["file"][i], "path") == relative) {
        manifest["file"].erase(manifest["file"].begin() +
                               static_cast<Json::difference_type>(i));
        break;
      }
    manifest["file"].push_back(row);
    checkpoint();
  }
  void copy(const QString &source, const QString &destination, const QString &origin, const QString &step) {
    write(destination, {}, origin, step, Json::array(), {}, source);
  }
  void commit(const QString &state, const QString &version) {
    checkpoint();
    Json final{{"game", r.gameId.toStdString()},
               {"variant", r.variantId.toStdString()},
               {"state", state.toStdString()},
               {"run", run.toStdString()},
               {"installed_version", version.toStdString()},
               {"recipe_sha256", manifest["recipe_sha256"]},
               {"install_dir", manifest["install_dir"]},
               {"bindings", r.bindings}};
    append(Json{{"kind", "commit"}, {"state", final}, {"manifest", manifest}});
    fault(options, "commit");
    fault(options, "before-state");
    writeEnvelope(base + ".toml", final);
    fault(options, "after-state");
    event(options, run, "done",
          state == "not-installed" ? "Removed owned files"
                                   : "Installed and verified");
  }
};
Json loadJournal(const QString &path) {
  Json records = Json::array();
  if (!QFileInfo::exists(path))
    return records;
  for (const auto &line : readBytes(path).split('\n')) {
    if (line.trimmed().isEmpty())
      continue;
    try {
      records.push_back(Json::parse(line.toStdString()));
    } catch (const std::exception &) {
      break;
    }
  }
  return records;
}
bool rollback(const Request &r, const Json &records, const Options &options,
              const QString &errorCode = "E_INTERRUPTED") {
  bool complete = true;
  if (!records.empty())
    cleanStaging(r, string(records[0], "run"));
  for (auto it = records.rbegin(); it != records.rend(); ++it) {
    const auto &j = *it;
    if (string(j, "kind") != "intent")
      continue;
    const auto op = string(j, "op");
    try {
      const auto path = scopedPath(string(j, "path"), r.root);
      if (op == "rename") {
        const auto to = string(j, "to");
        if (QFileInfo::exists(to) && !QFileInfo::exists(path)) {
          if (!QDir().rename(to, path))
            throw Error("E_ROLLBACK_INCOMPLETE", "Cannot reverse swap");
        }
      } else if (op == "rmdir") {
        if (!QFileInfo::exists(path) && !QDir().mkpath(path))
          throw Error("E_ROLLBACK_INCOMPLETE",
                      "Cannot restore removed directory");
      } else if (op == "mkdir") {
        if (QFileInfo::exists(path) && !QDir().rmdir(path))
          complete = false;
      } else if (op == "media-unlink") {
        const auto source = string(j, "source"), identity = string(j, "file_id");
        scopedPath(source, QFileInfo(source).absolutePath());
        if (!QFileInfo::exists(path)) {
          if (!QFileInfo(source).isFile() || fileIdentity(source) != identity || !hardlink(source, path))
            throw Error("E_ROLLBACK_INCOMPLETE", "Cannot restore media link from original");
        } else if (fileIdentity(path) != identity)
          throw Error("E_ROLLBACK_INCOMPLETE", "Media link changed during rollback");
      } else if (op == "file") {
        const auto prior = string(j, "prior"), after = string(j, "after");
        const bool exists = QFileInfo::exists(path);
        if (exists && hashFile(path) != after && hashFile(path) != prior) {
          complete = false;
          continue;
        }
        if (!prior.isEmpty()) {
          const auto backupPath = r.root + "/user/state/backups/" + prior;
          if (hashFile(backupPath) != prior)
            throw Error("E_ROLLBACK_INCOMPLETE", "Backup checksum failed");
          atomicCopy(backupPath, path);
        } else if (exists && !QFile::remove(path))
          complete = false;
      }
    } catch (const Error &) {
      complete = false;
    }
  }
  if (!records.empty() && string(records[0], "kind") == "begin") {
    const auto base = stateBase(r);
    const auto &start = records[0];
    writeEnvelope(base + ".manifest.toml", start["manifest_before"]);
    auto state = start["state_before"];
    if (state.empty())
      state = Json{{"game", r.gameId.toStdString()},
                   {"variant", r.variantId.toStdString()},
                   {"state", "failed"}};
    state["previous_state"] = string(start["state_before"], "state", "not-installed").toStdString();
    state["last_error"] = errorCode.toStdString();
    state["state"] = complete ? "failed" : "rollback-incomplete";
    writeEnvelope(base + ".toml", state);
    if (complete)
      durableAppend(base + ".journal.jsonl",
                    Json{{"kind", "rollback-complete"}});
  }
  event(options, {}, complete ? "ok" : "fail",
        complete ? "Changes rolled back" : "Rollback incomplete",
        complete ? QString() : "E_ROLLBACK_INCOMPLETE");
  return complete;
}
void verify(const Transaction &tx, const Plan &plan) {
  auto predicate = string(plan.variant, "installed_when");
  if (predicate.startsWith("file:"))
    predicate = predicate.mid(5);
  predicate = expand(predicate, variables(tx.r, tx.dir), true);
  if (predicate.isEmpty() || !QFileInfo(predicate).isFile())
    throw Error("E_VERIFY_FAILED", "installed_when failed");
  for (const auto &row : tx.manifest["file"]) {
    const auto path = scopedPath(string(row, "path"), tx.dir);
    if (!QFileInfo::exists(path))
      throw Error("E_FILE_REMOVED", "Owned file vanished");
    if (!row.value("user_edited", false) &&
        hashFile(path) != string(row, "sha256"))
      throw Error("E_VERIFY_FAILED", "Owned file hash differs");
  }
  if (plan.variant.value("verify_pe64", false) && !verifyPe64(predicate))
    throw Error("E_VERIFY_FAILED", "Windows x64 PE verification failed");
}
Json guardFor(const Request &r) {
  const auto catalog = r.catalogRoot.isEmpty() ? r.root : r.catalogRoot;
  Json guard = loadContentGuard(catalog);
  Json names = guard.value("names", Json::array());
  QDirIterator it(catalog + "/games", {"game.toml"}, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    try {
      const auto game = CatalogLoader::parseToml(it.next());
      for (const auto &m : game.value("media", Json::array()))
        if (m.contains("set"))
          names.push_back(m["set"].get<std::string>() + ".zip");
    } catch (const std::exception &) {
      throw Error("E_CONTENT_GUARD", "Catalog media names are unavailable; install is blocked");
    }
  }
  guard["names"] = names;
  return guard;
}
} // namespace
Engine::Engine(Options options) : options_(std::move(options)) {}
Plan Engine::plan(const Request &r) const {
  stateBase(r);
  if (r.recipe.value("format", 0) != 1)
    throw Error("E_PLAN_INVALID", "Recipe format 1 required");
  const auto variants = r.recipe.value("variant", Json::object());
  if (!variants.contains(r.variantId.toStdString()))
    throw Error("E_PLAN_INVALID", "Unknown variant");
  Plan p;
  p.gameId = r.gameId;
  p.variantId = r.variantId;
  p.variant = variants[r.variantId.toStdString()];
  if (string(p.variant, "status") == "planned")
    throw Error("E_PLAN_INVALID", "Variant is coming soon");
  if (p.variant.contains("uninstall") &&
      p.variant["uninstall"].contains("remove"))
    throw Error("E_PLAN_INVALID", "Recipe remove lists forbidden");
  if (string(p.variant, "placement", "install-dir") != "install-dir")
    throw Error("E_STEP_DISABLED", "In-place VR variants are outside M1");
  p.installDir = scopedPath(string(p.variant, "install_dir",
                                   "installed/" + r.gameId + "/" + r.variantId),
                            r.root);
  const auto relative = QDir(r.root).relativeFilePath(p.installDir);
  const auto expected = r.gameId.startsWith("tool-")
                            ? "emulators/" + r.gameId.mid(5)
                            : "installed/" + r.gameId + "/" + r.variantId;
  if (relative != expected)
    throw Error(
        "E_PATH_OUTSIDE_ROOT",
        "Install directory must be the variant's unique managed folder");
  p.version = string(p.variant, "version", "1");
  if (p.version == "latest" || p.version == "cached")
    throw Error("E_PLAN_INVALID", "Concrete version required");
  auto vars = variables(r, p.installDir);
  const auto guard = guardFor(r);
  QSet<QString> ids, mediaOutputs;
  auto steps = p.variant.value("step", Json::array());
  QSet<QString> reserved;
  for (const auto &step : steps) if (!string(step, "id").isEmpty()) reserved.insert(string(step, "id"));
  int generated = 0;
  for (auto &step : steps) if (!step.contains("id")) {
    QString id;
    do { id = "auto-step-" + QString::number(++generated); } while (reserved.contains(id));
    reserved.insert(id); step["id"] = id.toStdString();
  }
  const QMap<QString, QSet<QString>> specific{
      {"github-release",
       {"repo", "tag", "asset", "sha256", "record_sha256", "version", "archive",
        "bytes", "to", "hosts", "max_bytes", "kind"}},
      {"download",
       {"url", "sha256", "record_sha256", "version", "name", "archive", "bytes",
        "to", "hosts", "max_bytes", "kind"}},
      {"locate-package", {"pattern", "kind", "sha256", "search", "path"}},
      {"require-media", {"media", "verify", "set", "sha256"}},
      {"copy-media", {"media", "to", "mode"}},
      {"extract",
       {"from", "to", "strip", "include", "exclude", "max_entries",
        "max_expanded", "overwrite"}},
      {"copy", {"from", "to", "include", "exclude"}},
      {"write-config", {"file", "format", "create", "set"}},
      {"shortcut", {"targets", "name", "exe", "args", "cwd", "icon"}}};
  const QSet<QString> common{"id", "do", "what", "optional", "when", "note"};
  for (const auto &step : steps) {
    const auto kind = string(step, "do"), id = string(step, "id");
    if (QSet<QString>{"adb-install", "registry", "run", "patch-text"}.contains(
            kind))
      throw Error("E_STEP_DISABLED", "Step outside M1: " + kind);
    if (!specific.contains(kind))
      throw Error("E_UNKNOWN_STEP", "Unknown step: " + kind);
    if (id.isEmpty() || ids.contains(id))
      throw Error("E_PLAN_INVALID", "Step IDs must be nonempty and unique");
    if (!QRegularExpression("^[a-z0-9_-]+$").match(id).hasMatch())
      throw Error("E_PLAN_INVALID",
                  "Step ID must be one literal safe component");
    validateRelative(id);
    ids.insert(id);
    for (auto it = step.begin(); it != step.end(); ++it)
      if (!common.contains(QString::fromStdString(it.key())) &&
          !specific[kind].contains(QString::fromStdString(it.key())))
        throw Error("E_PLAN_INVALID", "Unknown security step field: " +
                                          QString::fromStdString(it.key()));
    auto expanded = expandJson(step, vars);
    if (kind == "download" || kind == "github-release") {
      const auto tag =
          string(step, kind == "github-release" ? "tag" : "version");
      if (tag.isEmpty() || tag == "latest")
        throw Error("E_PLAN_INVALID", "Pinned version/tag required");
      const auto pin = string(step, "sha256");
      if (!QRegularExpression("^[a-fA-F0-9]{64}$").match(pin).hasMatch() &&
          !step.value("record_sha256", false))
        throw Error("E_PLAN_INVALID", "SHA-256 pin required");
      if (step.value("record_sha256", false) && !r.gameId.startsWith("tool-"))
        throw Error("E_PLAN_INVALID",
                    "Record-first-download allowed only for tool manifest");
      const auto hosts = strings(step.value("hosts", Json::array()));
      for (const auto &host : hosts)
        if (!ArtifactStore::globalHosts().contains(host))
          throw Error("E_HOST_NOT_ALLOWED", "Unlisted manifest host");
      const auto url = kind == "download"
                           ? string(step, "url")
                           : "https://github.com/" + string(step, "repo") +
                                 "/releases/download/" + tag + "/" +
                                 string(step, "asset");
      ArtifactStore::validateUrl(url, hosts);
      if (string(step, "asset").contains('*'))
        throw Error("E_ASSET_AMBIGUOUS", "Exact asset name required");
      if (step.contains("media"))
        throw Error("E_CONTENT_GUARD", "Artifact cannot request media");
    }
    for (const auto *field : {"to", "file"})
      if (expanded.contains(field)) {
        const auto target = string(expanded, field);
        if ((kind == "download" || kind == "github-release") &&
            field == QString("to"))
          scopedPath(target, r.root + "/user/cache/artifacts");
        else if (QDir::cleanPath(target) != QDir::cleanPath(p.installDir))
          scopedPath(target, p.installDir);
      }
    if (kind == "extract" || kind == "copy") {
      const auto rawFrom = string(step, "from"), from = string(expanded, "from");
      const auto output = QRegularExpression(
          "^\\$\\{steps\\.([a-z0-9_-]+)\\.(?:path|dir)\\}(/.*)?$").match(rawFrom);
      const bool shorthand = kind == "extract" && ids.contains(rawFrom) && rawFrom != id;
      const auto sourceId = output.hasMatch() ? output.captured(1) : rawFrom;
      if (rawFrom.contains("${media.") ||
          ((output.hasMatch() || shorthand) && mediaOutputs.contains(sourceId)))
        throw Error("E_SOURCE_OUT_OF_SCOPE", "Media cannot be an extract/copy source");
      for (const auto &media : r.bindings.value("media", Json::object())) {
        const auto bound = string(media, "path");
        if (bound.isEmpty())
          continue;
        // Compare lexical and canonical paths without reading or hashing media.
        auto normalize = [](const QString &path) {
          const QFileInfo info(path);
          const auto canonical = info.canonicalFilePath();
          auto normalized = QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
#ifdef Q_OS_WIN
          normalized = normalized.toLower();
#endif
          return normalized;
        };
        const auto normalizedSource = normalize(from), normalizedMedia = normalize(bound);
        if (normalizedSource == normalizedMedia ||
            (QFileInfo(from).isDir() && normalizedMedia.startsWith(normalizedSource + '/')))
          throw Error("E_SOURCE_OUT_OF_SCOPE", "Bound media cannot be an extract/copy source");
        if (QFileInfo(bound).isFile()) {
          const auto identity = fileIdentity(bound);
          auto rejectAlias = [&](const QString &path) {
            if (QFileInfo(path).isFile() && fileIdentity(path) == identity)
              throw Error("E_SOURCE_OUT_OF_SCOPE", "A media hard-link alias cannot be an extract/copy source");
          };
          if (QFileInfo(from).isFile()) rejectAlias(from);
          else if (QFileInfo(from).isDir()) {
            const auto sourceScope = output.hasMatch() || shorthand ? r.root :
                kind == "copy" ? (r.catalogRoot.isEmpty() ? r.root : r.catalogRoot) + "/games/" + r.gameId + "/setup" : QFileInfo(from).absolutePath();
            const auto sourceRoot = scopedPath(from, sourceScope);
            QStringList directories{sourceRoot};
            while (!directories.isEmpty()) {
              const auto directory = directories.takeLast();
              for (const auto &entry : QDir(directory).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
                if (entry.isSymLink() || entry.isJunction())
                  throw Error("E_SOURCE_OUT_OF_SCOPE", "Media-source inspection refuses linked directory entries");
                const auto path = scopedPath(entry.absoluteFilePath(), sourceRoot);
                if (entry.isDir()) directories.append(path);
                else rejectAlias(path);
              }
            }
          }
        }
      }
      if (output.hasMatch() || shorthand) {
        if (!ids.contains(sourceId) || sourceId == id)
          throw Error("E_SOURCE_OUT_OF_SCOPE", "Source must use one prior output");
        const auto suffix = output.captured(2);
        if (!suffix.isEmpty()) {
          validateRelative(suffix.mid(1));
          contentGuard(suffix, guard);
        }
      } else {
        if (rawFrom.contains("${steps."))
          throw Error("E_SOURCE_OUT_OF_SCOPE", "Source must use one prior output");
        if (kind == "copy")
          scopedPath(from, (r.catalogRoot.isEmpty() ? r.root : r.catalogRoot) +
                               "/games/" + r.gameId + "/setup");
        else
          scopedPath(from, QFileInfo(from).absolutePath());
        contentGuard(from, guard);
      }
    }
    if (kind == "require-media") {
      const auto verify = string(step, "verify", "hash");
      if (!QSet<QString>{"none", "name", "hash"}.contains(verify))
        throw Error("E_PLAN_INVALID", "Unknown media verification mode");
      if (verify == "name" && string(step, "set").isEmpty())
        throw Error("E_PLAN_INVALID", "Name verification requires a set name");
    }
    if (kind == "require-media" || kind == "copy-media")
      mediaOutputs.insert(id);
    if (kind == "write-config") {
      if (!QSet<QString>{"ini", "cfg", "toml", "json"}.contains(
              string(step, "format")))
        throw Error("E_PLAN_INVALID", "Unknown config format");
      for (const auto &entry : step.value("set", Json::array())) {
        if (entry.contains("from"))
          dotted(Json{{"settings", r.settings}, {"profile", r.profile}},
                 string(entry, "from"));
        if (entry.contains("bool") &&
            (entry["bool"].size() != 2 || !entry["bool"].contains("on") ||
             !entry["bool"].contains("off")))
          throw Error("E_PLAN_INVALID",
                      "Boolean map requires exactly on/off fields");
        for (auto it = entry.begin(); it != entry.end(); ++it)
          if (!QSet<QString>{"key", "section", "value", "from", "type", "map",
                             "default", "bool", "style", "decimals"}
                   .contains(QString::fromStdString(it.key())))
            throw Error("E_PLAN_INVALID", "Unknown managed key field");
      }
    }
    // Security validation precedes when filtering: hidden invalid steps still
    // fail.
    if (r.skipSteps.contains(id) && !step.value("optional", false))
      throw Error("E_PLAN_INVALID", "Only optional steps may be skipped");
    if (condition(step.value("when", Json::object()), r) &&
        !r.skipSteps.contains(id))
      p.steps.push_back(step);
    const auto base = "steps." + id;
    for (const auto *output : {"path", "dir", "sha256", "bytes", "tag"})
      vars[base + "." + output] = output == QString("dir") ? p.installDir
                                  : output == QString("path")
                                      ? r.root + "/user/cache/artifacts/" + id
                                      : QString("<" + id + "> ");
  }
  // Predicate/launch expansions are validated before any run writes.
  expandJson(p.variant.value("launch", Json::object()), vars);
  auto predicate = expand(string(p.variant, "installed_when"), vars, true);
  if (!predicate.startsWith("file:"))
    throw Error("E_PLAN_INVALID",
                "M1 requires a file installed_when predicate");
  scopedPath(predicate.mid(5), p.installDir);
  p.text = "PLAN  " + r.gameId + " / " + r.variantId + "  format 1\n";
  int n = 0;
  for (const auto &step : p.steps)
    p.text +=
        QString("  %1/%2  %3  %4\n")
            .arg(++n)
            .arg(p.steps.size())
            .arg(string(step, "what", string(step, "id")), string(step, "do"));
  p.text += "  verify  " + string(p.variant, "installed_when") + "\n";
  return p;
}
Result Engine::install(const Request &r) {
  std::unique_ptr<Transaction> tx;
  try {
    auto p = plan(r);
    Lock lock(r);
    const auto guard = guardFor(r);
    tx = std::make_unique<Transaction>(r, options_, p);
    tx->guard = guard;
    // Same-version repairs operate through the WAL; updates stage a complete
    // generation.
    const bool update =
        !tx->baseline.empty() &&
        string(tx->stateBefore, "installed_version") != p.version;
    const auto live = p.installDir;
    const auto stage = live + ".staging-" + tx->run;
    const auto extractRoot = r.root + "/user/state/staging/" + tx->run;
    tx->mkdir(extractRoot);
    tx->mkdir(live);
    if (update) {
      tx->mkdir(stage);
      QDirIterator links(live,
                         QDir::AllEntries | QDir::Hidden | QDir::System |
                             QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
      while (links.hasNext()) {
        const auto entry = links.next();
        if (QFileInfo(entry).isSymLink() || QFileInfo(entry).isJunction())
          throw Error("E_PATH_OUTSIDE_ROOT",
                      "Update refuses linked live entries");
      }
      QDirIterator files(live,
                         QDir::AllEntries | QDir::Hidden | QDir::NoSymLinks |
                             QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
      while (files.hasNext()) {
        const auto src = files.next();
        const auto dest = scopedPath(QDir(live).relativeFilePath(src), stage);
        if (QFileInfo(src).isDir()) {
          tx->mkdir(dest);
          bool owned = false;
          for (const auto &directory :
               tx->baseline.value("directory", Json::array()))
            if (QString::fromStdString(directory.get<std::string>()) == src)
              owned = true;
          if (!owned)
            for (auto directory = tx->manifest["directory"].begin();
                 directory != tx->manifest["directory"].end(); ++directory)
              if (QString::fromStdString(directory->get<std::string>()) ==
                  dest) {
                tx->manifest["directory"].erase(directory);
                break;
              }
        } else {
          bool mediaLink = false;
          for (const auto &row : tx->baseline.value("file", Json::array()))
            if (string(row, "path") == QDir(live).relativeFilePath(src) && string(row, "origin") == "user-media-link") mediaLink = true;
          if (!mediaLink) tx->rawCopy(src, dest);
        }
      }
      tx->dir = stage;
    }
    auto vars = variables(r, tx->dir);
    vars["staging_dir"] = extractRoot;
    ArtifactStore artifacts(r.root, tx->options);
    int index = 0;
    for (const auto &raw : p.steps) {
      if (options_.cancel && options_.cancel->load())
        throw Error("E_CANCELLED", "Cancelled between steps");
      auto step = expandJson(raw, vars);
      const auto kind = string(step, "do"), id = string(step, "id");
      tx->activeStep = id;
      event(tx->options, tx->run, "step", string(step, "what", id), {}, id,
            ++index, static_cast<int>(p.steps.size()));
      Json output = Json::object();
      if (kind == "github-release" || kind == "download") {
        const auto path = artifacts.acquire(step, guard);
        output =
            Json{{"path", path.toStdString()},
                 {"sha256", hashFile(path).toStdString()},
                 {"bytes", QFileInfo(path).size()},
                 {"tag",
                  string(step, "tag", string(step, "version")).toStdString()}};
      } else if (kind == "locate-package") {
        const auto path =
            r.handover.isEmpty() ? string(step, "path") : r.handover;
        const auto info = QFileInfo(path);
        if (!info.exists() ||
            QSet<QString>{"part", "tmp", "crdownload", "opdownload"}.contains(
                info.suffix().toLower()))
          throw Error("E_MEDIA_MISSING", "Choose and confirm a package path");
        if (!QDir::match(string(step, "pattern", "*"), info.fileName()))
          throw Error("E_MEDIA_MISMATCH", "Package filename does not match");
        const auto hash = hashFile(path), pin = string(step, "sha256");
        if (!pin.isEmpty() && pin != hash)
          throw Error("E_HASH_MISMATCH", "Located package differs from pin");
        contentGuard(path, guard);
        if (string(step, "kind") == "archive")
          Archive::inspect(path, {}, guard);
        output = Json{{"path", info.canonicalFilePath().toStdString()},
                      {"sha256", hash.toStdString()},
                      {"bytes", info.size()},
                      {"verified", !pin.isEmpty()}};
      } else if (kind == "require-media" || kind == "copy-media") {
        const auto key = string(step, "media").toStdString();
        const auto media = r.bindings.value("media", Json::object())
                               .value(key, Json::object());
        const auto path = string(media, "path");
        if (!QFileInfo(path).isFile())
          throw Error("E_MEDIA_MISSING", "Required media missing");
        const auto verification = kind == "copy-media" ? QString("hash") : string(step, "verify", "hash");
        if (verification == "hash") {
          const auto expected = string(step, "sha256", string(media, "sha256"));
          if (expected.isEmpty() || hashFile(path) != expected || !media.value("verified", false))
            throw Error("E_MEDIA_MISMATCH", "Media must be verified before install");
        } else if (verification == "name") {
          const auto expected = string(step, "set");
          if (QFileInfo(path).completeBaseName().compare(expected, Qt::CaseInsensitive) != 0)
            throw Error("E_MEDIA_MISMATCH", "Media filename differs from required set");
        }
        if (kind == "copy-media") {
          auto dest = string(step, "to");
          if (dest.endsWith('/') || QFileInfo(dest).isDir())
            dest += QFileInfo(path).fileName();
          dest = scopedPath(dest, tx->dir);
          const auto mode = string(step, "mode", "auto");
          if (!QSet<QString>{"auto", "link", "copy"}.contains(mode))
            throw Error("E_PLAN_INVALID", "Unknown media mode");
          const bool linked = mode != "copy" && tx->linkMedia(path, dest, id);
          if (!linked) {
            if (mode == "link" || !r.allowMediaCopy)
              throw Error(
                  "E_LINK_UNSUPPORTED",
                  "Hard link unavailable; confirm the extra media copy first");
            tx->copy(path, dest, "user-media-copy", id);
          }
          output["path"] = dest.toStdString();
          output["mode_used"] = linked ? "link" : "copy";
        } else {
          output = media;
          output["match"] = verification == "hash" ? "exact" : verification.toStdString();
          output["verified"] = verification == "hash";
        }
      } else if (kind == "extract") {
        auto from = string(step, "from");
        if (!from.contains('/') && vars.contains("steps." + from + ".path"))
          from = vars["steps." + from + ".path"];
        const auto destination =
            scopedPath(string(step, "to", tx->dir), tx->dir);
        const ArchiveLimits limits{step.value("max_expanded", int64_t(0)),
                                   step.value("max_entries", 50000)};
        int strip = step.contains("strip") && step["strip"].is_number_integer()
                        ? step["strip"].get<int>()
                        : 0;
        if (string(step, "strip") == "auto") {
          const auto list = Archive::inspect(from, limits, guard);
          QString wrapper;
          bool common = true;
          for (const auto &entry : list) {
            const auto first = entry.path.section('/', 0, 0);
            if (wrapper.isEmpty())
              wrapper = first;
            else if (wrapper != first)
              common = false;
            if (!entry.directory && !entry.path.contains('/'))
              common = false;
          }
          strip = common ? 1 : 0;
        }
        const auto staging = scopedPath(id, extractRoot);
        const auto files =
            Archive::extract(from, staging, limits, guard, strip,
                             strings(step.value("include", Json::array())),
                             strings(step.value("exclude", Json::array())));
        for (const auto &name : files)
          tx->copy(staging + "/" + name, scopedPath(name, destination), "extracted", id);
        output["dir"] = destination.toStdString();
        output["files"] = files.size();
      } else if (kind == "copy") {
        const auto from = string(step, "from"),
                   dest = scopedPath(string(step, "to"), tx->dir);
        scopedPath(from, QFileInfo(from).absolutePath());
        if (QFileInfo(from).isFile()) {
          contentGuard(from, guard);
          tx->copy(from, dest, "copied", id);
        } else {
          QDirIterator files(from, QDir::Files | QDir::NoSymLinks,
                             QDirIterator::Subdirectories);
          while (files.hasNext()) {
            const auto source = files.next(),
                       name = QDir(from).relativeFilePath(source);
            scopedPath(source, from);
            contentGuard(name, guard);
            if (!step.contains("include") ||
                QDir::match(strings(step["include"]), name)) {
              if (!step.contains("exclude") ||
                  !QDir::match(strings(step["exclude"]), name))
                tx->copy(source, scopedPath(name, dest), "copied", id);
            }
          }
        }
        output["dir"] = dest.toStdString();
      } else if (kind == "write-config") {
        const auto path = scopedPath(string(step, "file"), tx->dir);
        if (!QFileInfo::exists(path) && !step.value("create", false))
          throw Error("E_CONFIG_PARSE",
                      "Config missing; create must be explicit");
        const auto edit =
            editConfig(QFileInfo::exists(path) ? readBytes(path) : QByteArray(),
                       string(step, "format"), step.value("set", Json::array()),
                       r.settings, r.profile);
        tx->write(path, edit.bytes, "written", id, edit.keys,
                  string(step, "format"));
        if (edit.reserialized) {
          tx->warning = true;
          event(tx->options, tx->run, "warn",
                "Config was reserialized; comments were not preserved");
        }
      } else if (kind == "shortcut") {
        if (options_.shortcut) {
          const auto receipt = options_.shortcut(step);
          tx->manifest["shortcut"].push_back(receipt);
        } else {
          tx->warning = true;
          event(tx->options, tx->run, "warn",
                "Shortcut adapter is unavailable until lane G is integrated",
                "E_SHORTCUT_UNAVAILABLE", id);
        }
      }
      for (auto it = output.begin(); it != output.end(); ++it)
        vars["steps." + id + "." + QString::fromStdString(it.key())] =
            it.value().is_string()
                ? QString::fromStdString(it.value().get<std::string>())
                : QString::fromStdString(it.value().dump());
      tx->checkpoint();
      event(tx->options, tx->run, "ok", "Step complete", {}, id, index,
            static_cast<int>(p.steps.size()));
      tx->activeStep.clear();
    }
    for (const auto &row : tx->baseline.value("file", Json::array())) {
      bool present = false;
      for (const auto &current : tx->manifest["file"])
        if (string(current, "path") == string(row, "path"))
          present = true;
      if (present)
        continue;
      const auto path = scopedPath(string(row, "path"), tx->dir);
      if (update && QFileInfo::exists(path) &&
          hashFile(path) == string(row, "sha256"))
        tx->rawWrite(path, {}, true);
      else {
        tx->manifest["file"].push_back(row);
        if (update) {
          tx->warning = true;
          tx->manifest["file"].back()["user_edited"] = true;
        }
      }
    }
    tx->checkpoint();
    event(tx->options, tx->run, "verify", "Checking installed files");
    verify(*tx, p);
    QThread::msleep(
        static_cast<unsigned long>(std::max(0, options_.survivalMs)));
    verify(*tx, p);
    if (update) {
      const auto previous = live + ".previous-" + tx->run;
      tx->rename(live, previous);
      tx->rename(stage, live);
      tx->dir = live;
      for (auto &directory : tx->manifest["directory"]) {
        auto value = QString::fromStdString(directory.get<std::string>());
        if (value == stage || value.startsWith(stage + "/"))
          directory = (live + value.mid(stage.size())).toStdString();
      }
      verify(*tx, p);
    }
    const auto state = !r.skipSteps.isEmpty()
                           ? QString("installed-with-skipped")
                       : tx->warning ? QString("installed-with-warnings")
                                     : QString("installed");
    tx->commit(state, p.version);
    cleanStaging(r, tx->run);
    return {true, 0, state, {}, "Installed and verified", tx->manifest};
  } catch (const Error &e) {
    bool complete = true;
    if (tx)
      complete =
          rollback(r, loadJournal(tx->base + ".journal.jsonl"), options_,
                   tx->activeStep.isEmpty() ? e.code : tx->activeStep + ": " + e.code);
    event(options_, {}, "fail", QString::fromUtf8(e.what()), e.code);
    return {false,
            complete ? (e.code == "E_CANCELLED" ? 3 : 1) : 2,
            complete ? "failed" : "rollback-incomplete",
            e.code,
            QString::fromUtf8(e.what()),
            Json::object()};
  } catch (const std::exception &e) {
    const bool complete =
        !tx || rollback(r, loadJournal(tx->base + ".journal.jsonl"), options_, "E_PLAN_INVALID");
    event(options_, {}, "fail", QString::fromUtf8(e.what()), "E_PLAN_INVALID");
    return {false,
            complete ? 1 : 2,
            complete ? "failed" : "rollback-incomplete",
            "E_PLAN_INVALID",
            QString::fromUtf8(e.what()),
            Json::object()};
  }
}
Json Engine::uninstallPreview(const Request &r) const {
  const auto manifest = readEnvelope(stateBase(r) + ".manifest.toml");
  const auto dir = QDir(r.root).filePath(string(manifest, "install_dir"));
  Json rows = Json::array();
  for (const auto &row : manifest.value("file", Json::array())) {
    const auto path = scopedPath(string(row, "path"), dir);
    auto item = row;
    item["action"] =
        QFileInfo::exists(path) && hashFile(path) != string(row, "sha256")
            ? "keep-edited"
            : "remove-or-restore";
    rows.push_back(item);
  }
  return Json{{"file", rows},
              {"never_touched",
               Json::array({"user media", "unowned files", "user profiles"})}};
}
Result Engine::uninstall(const Request &r) {
  std::unique_ptr<Transaction> tx;
  try {
    Lock lock(r);
    const auto base = stateBase(r);
    const auto old = readEnvelope(base + ".manifest.toml");
    if (old.empty())
      return {true, 4, "not-installed", {}, "Nothing to uninstall", old};
    Plan p;
    p.installDir = scopedPath(string(old, "install_dir"), r.root);
    p.version = string(readEnvelope(base + ".toml"), "installed_version");
    auto req = r;
    req.operation = "uninstall";
    tx = std::make_unique<Transaction>(req, options_, p);
    tx->manifest = old;
    Json remaining = Json::array();
    for (auto it = old["file"].rbegin(); it != old["file"].rend(); ++it) {
      const auto &row = *it;
      const auto path = scopedPath(string(row, "path"), tx->dir);
      if (!QFileInfo::exists(path))
        continue;
      if (string(row, "origin") == "user-media-link" &&
          fileIdentity(path) != string(row, "file_id")) {
        remaining.push_back(row);
        tx->warning = true;
        continue;
      }
      if (row.value("user_edited", false) ||
          hashFile(path) != string(row, "sha256")) {
        if (row.contains("keys")) {
          bool kept = false;
          const auto next = undoConfig(readBytes(path), string(row, "format"),
                                       row["keys"], kept);
          if (next != readBytes(path))
            tx->rawWrite(path, next);
        }
        remaining.push_back(row);
        tx->warning = true;
        event(tx->options, tx->run, "warn",
              "Kept edited file: " + string(row, "path"));
        continue;
      }
      if (string(row, "prior") == "backup") {
        const auto backup = string(row, "backup");
        const auto backupPath = r.root + "/user/state/backups/" + backup;
        if (hashFile(backupPath) != backup)
          throw Error("E_ROLLBACK_INCOMPLETE", "Backup checksum failed");
        tx->rawCopy(backupPath, path);
      } else if (string(row, "origin") == "user-media-link")
        tx->unlinkMedia(path, row);
      else
        tx->rawWrite(path, {}, true);
      for (auto current = tx->manifest["file"].begin();
           current != tx->manifest["file"].end(); ++current)
        if (string(*current, "path") == string(row, "path")) {
          tx->manifest["file"].erase(current);
          break;
        }
      tx->checkpoint();
    }
    tx->manifest["file"] = remaining;
    auto dirs = strings(old.value("directory", Json::array()));
    std::sort(dirs.begin(), dirs.end(), [](const QString &a, const QString &b) {
      return a.size() > b.size();
    });
    for (const auto &dir : dirs) {
      const auto relative = QDir(r.root).relativeFilePath(dir);
      if (relative == "installed" || relative.startsWith("installed/") ||
          relative == "emulators" || relative.startsWith("emulators/"))
        tx->removeEmptyDirectory(scopedPath(dir, r.root));
    }
    const auto state = remaining.empty() ? QString("not-installed")
                                         : QString("uninstall-incomplete");
    tx->commit(state, p.version);
    return {remaining.empty(),
            remaining.empty() ? 0 : 2,
            state,
            {},
            remaining.empty() ? "Removed owned files"
                              : "Kept user-edited files",
            tx->manifest};
  } catch (const Error &e) {
    if (tx)
      rollback(r, loadJournal(tx->base + ".journal.jsonl"), options_);
    return {false,
            2,
            "uninstall-incomplete",
            e.code,
            QString::fromUtf8(e.what()),
            Json::object()};
  }
}
Result Engine::recover(const Request &r) {
  try {
    Lock lock(r);
    const auto base = stateBase(r);
    readEnvelope(base + ".toml");
    readEnvelope(base + ".manifest.toml");
    const auto records = loadJournal(base + ".journal.jsonl");
    if (records.empty())
      return {true, 4, {}, {}, "No interrupted run", Json::object()};
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
      if (string(*it, "kind") == "rollback-complete")
        return {true, 4, {}, {}, "Already rolled back", Json::object()};
      if (string(*it, "kind") == "commit") {
        writeEnvelope(base + ".manifest.toml", (*it)["manifest"]);
        writeEnvelope(base + ".toml", (*it)["state"]);
        return {true,
                0,
                string((*it)["state"], "state"),
                {},
                "Committed state recovered",
                (*it)["manifest"]};
      }
    }
    const auto ok = rollback(r, records, options_);
    return {ok,
            ok ? 0 : 2,
            ok ? "failed" : "rollback-incomplete",
            {},
            ok ? "Interrupted run rolled back" : "Rollback incomplete",
            Json::object()};
  } catch (const Error &e) {
    return {false,
            2,
            "rollback-incomplete",
            e.code,
            QString::fromUtf8(e.what()),
            Json::object()};
  }
}
RuntimeState Engine::runtimeState(const Request &r) const {
  auto runtime = r.runtime;
  runtime.gameId = r.gameId;
  try {
    const auto base = stateBase(r);
    const auto state = readEnvelope(base + ".toml");
    const auto manifest = readEnvelope(base + ".manifest.toml");
    VariantRuntimeState variant;
    variant.id = r.variantId;
    variant.manifestExists = !manifest.empty();
    variant.verified = string(state, "state").startsWith("installed");
    variant.installedWhenExists = variant.verified && variant.manifestExists;
    const auto dir = QDir(r.root).filePath(string(manifest, "install_dir"));
    const auto predicate = string(manifest, "installed_when");
    if (!predicate.startsWith("file:"))
      variant.installedWhenExists = false;
    else if (!QFileInfo(
                  scopedPath(expand(predicate.mid(5), variables(r, dir), true),
                             dir))
                  .isFile())
      variant.installedWhenExists = false;
    for (const auto &row : manifest.value("file", Json::array()))
      if (!QFileInfo(scopedPath(string(row, "path"), dir)).isFile())
        variant.installedWhenExists = false;
    bool replaced = false;
    for (auto &existing : runtime.variants)
      if (existing.id == variant.id) {
        existing = variant;
        replaced = true;
      }
    if (!replaced)
      runtime.variants.push_back(variant);
  } catch (const Error &) {
    runtime.jobStatus = JobStatus::Failed;
  }
  return runtime;
}
Request Engine::emulatorRequest(const QString &root, const Json &manifest,
                                bool consent) {
  Request r;
  r.root = root;
  const auto id = string(manifest, "id");
  r.gameId = "tool-" + id;
  r.variantId = "windows-x64";
  r.consentAccepted = consent;
  const auto gate = string(manifest, "gate");
  if (string(manifest, "redistribution") != "download-from-upstream-only" ||
      (manifest.contains("gate") && gate != "consent-install"))
    throw Error("E_STEP_DISABLED", gate.isEmpty()
                    ? "This tool must be located or searched for"
                    : "Automatic install blocked: " + gate);
  if (gate == "consent-install" && !consent)
    throw Error("E_CONSENT_REQUIRED",
                string(manifest, "consent", "Confirm the tool consent text first"));
  if (!strings(manifest.value("platforms", Json::array()))
           .contains("windows-x64"))
    throw Error("E_PLATFORM_UNVERIFIED",
                "Windows x64 artifact architecture has not been verified");
  auto download = manifest.at("install").at("windows-x64");
  download["id"] = "artifact";
  download["what"] = "Download official " + id.toStdString();
  const auto executables = strings(
      manifest.value("locate", Json::object()).value("exe", Json::array()));
  if (executables.isEmpty())
    throw Error("E_PLAN_INVALID", "Manifest executable missing");
  const auto exe =
      manifest.value("install_exe", executables.first().toStdString());
  Json variant{
      {"title", string(manifest, "name").toStdString()},
      {"status", "stable"},
      {"version",
       string(download, "version", string(download, "tag")).toStdString()},
      {"install_dir", ("emulators/" + id).toStdString()},
      {"installed_when", "file:${install_dir}/" + exe},
      {"verify_pe64", true},
      {"step", Json::array()}};
  // Display-only research notes are not security fields of the engine step.
  download.erase("checksum_asset");
  variant["step"].push_back(download);
  variant["step"].push_back(Json{{"id", "extract"},
                                 {"do", "extract"},
                                 {"from", "${steps.artifact.path}"},
                                 {"to", "${install_dir}"},
                                 {"strip", "auto"}});
  r.recipe =
      Json{{"format", 1}, {"variant", {{r.variantId.toStdString(), variant}}}};
  return r;
}
Json Engine::locateTool(const QString &root, const Json &manifest,
                        const QString &input) {
  scopedPath("user/state", root);
  scopedPath(input, QFileInfo(input).absolutePath());
  const auto id = string(manifest, "id");
  validateRelative(id);
  auto path = QFileInfo(input).canonicalFilePath();
  const auto patterns = strings(
      manifest.value("locate", Json::object()).value("exe", Json::array()));
  if (QFileInfo(path).isDir()) {
    const auto candidates = QDir(path).entryList(patterns, QDir::Files);
    if (candidates.size() != 1)
      throw Error("E_TOOL_MISSING", "Choose one matching emulator executable");
    path = QDir(path).filePath(candidates.first());
  }
  if (!QFileInfo(path).isFile() ||
      !QDir::match(patterns, QFileInfo(path).fileName()))
    throw Error("E_TOOL_MISSING", "Executable name does not match manifest");
  if (path.endsWith(".exe", Qt::CaseInsensitive) && !verifyPe64(path))
    throw Error("E_PLATFORM_UNVERIFIED",
                "Located executable is not a verified Windows x64 PE");
  Json binding{{"path", path.toStdString()},
               {"sha256", hashFile(path).toStdString()},
               {"bytes", QFileInfo(path).size()},
               {"mtime", QFileInfo(path).lastModified().toMSecsSinceEpoch()},
               {"verified", true},
               {"version", "unknown"}};
  const auto state = scopedPath("user/state/located.toml", root);
  auto located = readEnvelope(state);
  located["tools"][id.toStdString()] = binding;
  writeEnvelope(state, located);
  return binding;
}
InstallService::InstallService(QObject *parent) : QObject(parent) {
  qRegisterMetaType<ac::RuntimeState>();
  connect(&watcher_, &QFutureWatcher<Result>::finished, this, [this] {
    try {
      const auto result = watcher_.result();
      auto refreshed = request_;
      if (runtimeSource_)
        refreshed.runtime = runtimeSource_(request_.gameId);
      emit runtimeStateReady(Engine().runtimeState(refreshed));
      emit finished(result.success, result.message);
    } catch (const std::exception &e) {
      emit event({{"kind", "fail"},
                  {"code", "E_SERVICE"},
                  {"text", QString::fromUtf8(e.what())}});
      emit finished(false, QString::fromUtf8(e.what()));
    } catch (...) {
      emit event({{"kind", "fail"},
                  {"code", "E_SERVICE"},
                  {"text", "Worker stopped unexpectedly; recover this run"}});
      emit finished(false, "Worker stopped unexpectedly; recover this run");
    }
    if (!pending_.isEmpty()) {
      const auto next = pending_.takeFirst();
      QMetaObject::invokeMethod(
          this, [this, next] { start(next); }, Qt::QueuedConnection);
    }
  });
}
InstallService::~InstallService() {
  cancelled_ = true;
  watcher_.waitForFinished();
}
bool InstallService::busy() const { return watcher_.isRunning(); }
void InstallService::setRuntimeStateSource(
    std::function<RuntimeState(const QString &)> source) {
  runtimeSource_ = std::move(source);
}
void InstallService::cancel() { cancelled_ = true; }
void InstallService::start(const Request &request) {
  if (busy()) {
    pending_.push_back(request);
    emit event({{"kind", "warn"},
                {"text", "Install queued behind the active run"},
                {"code", "E_LOCKED"}});
    return;
  }
  request_ = request;
  cancelled_ = false;
  watcher_.setFuture(QtConcurrent::run([this, request] {
    Options options;
    options.cancel = &cancelled_;
    options.event = [this](const QVariantMap &value) {
      QMetaObject::invokeMethod(
          this, [this, value] { emit event(value); }, Qt::QueuedConnection);
    };
    Engine engine(options);
    if (request.operation == "uninstall")
      return engine.uninstall(request);
    if (request.operation == "recover")
      return engine.recover(request);
    return engine.install(request);
  }));
}
} // namespace ac::install
