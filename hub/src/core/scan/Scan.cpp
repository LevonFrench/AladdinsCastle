// SPDX-License-Identifier: GPL-3.0-only
#include "Scan.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QQueue>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamReader>
#include <QtEndian>
#include <algorithm>
#include <sstream>
#include <toml++/toml.hpp>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <winver.h>
#endif
namespace ac::scan {
namespace {
QString s(const Json &j, const char *key, const QString &fallback = {}) {
  return j.contains(key) && j[key].is_string()
             ? QString::fromStdString(j[key].get<std::string>())
             : fallback;
}
QStringList strings(const Json &j, const char *key) {
  QStringList r;
  if (j.contains(key) && j[key].is_array())
    for (const auto &v : j[key])
      if (v.is_string())
        r << QString::fromStdString(v.get<std::string>());
  return r;
}
Json list(const QStringList &l) {
  Json a = Json::array();
  for (const auto &s : l)
    a.push_back(s.toStdString());
  return a;
}
bool blocked(const QString &name) {
  const auto n = name.toLower();
  const auto stem = QFileInfo(n).completeBaseName();
  if (QStringList{"credential", "credentials", "license", "licence",
                  "activation", "authentication", "auth", "token", "tokens",
                  "entitlement", "entitlements", "keychain", "keychains"}
          .contains(stem))
    return true;
  return QStringList{".git",        ".ssh",         ".aws",
                     ".azure",      ".gnupg",       ".codex",
                     ".claude",     "appdata",      "windows",
                     "programdata", "$recycle.bin", "system volume information",
                     "credentials", "keychains",    "licenses",
                     "licences",    "entitlements", "auth",
                     "tokens"}
      .contains(n);
}
QStringList enumerate(const QStringList &roots, const QStringList &extensions,
                      int depth, std::atomic_bool &cancel) {
  QStringList files;
  QSet<QString> visited;
  QQueue<QPair<QString, int>> queue;
  for (const auto &r : roots) {
    QFileInfo f(r);
    if (f.isSymLink() || f.isJunction() || blocked(f.fileName()))
      continue;
    if (f.isFile()) {
      if (extensions.contains(f.suffix().toLower()))
        files << f.absoluteFilePath();
    } else if (f.isDir() && !blocked(f.fileName()))
      queue.enqueue({f.absoluteFilePath(), 0});
  }
  while (!queue.isEmpty() && !cancel) {
    auto [path, d] = queue.dequeue();
    QDir dir(path);
    const auto canonical = dir.canonicalPath();
    if (visited.contains(canonical))
      continue;
    visited.insert(canonical);
    for (const auto &f : dir.entryInfoList(
             QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
             QDir::Name)) {
      if (cancel)
        break;
      if (f.isSymLink() || f.isJunction() || blocked(f.fileName()))
        continue;
      if (f.isDir()) {
        if (d < depth)
          queue.enqueue({f.absoluteFilePath(), d + 1});
      } else if (extensions.contains(f.suffix().toLower()))
        files << f.absoluteFilePath();
    }
  }
  return files;
}
QString version(const QString &path) {
#ifdef Q_OS_WIN
  DWORD unused = 0;
  const auto n =
      GetFileVersionInfoSizeW(reinterpret_cast<LPCWSTR>(path.utf16()), &unused);
  if (n && n < 4 * 1024 * 1024) {
    QByteArray b(static_cast<qsizetype>(n), 0);
    if (GetFileVersionInfoW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, n,
                            b.data())) {
      VS_FIXEDFILEINFO *v = nullptr;
      UINT length = 0;
      if (VerQueryValueW(b.data(), L"\\", reinterpret_cast<void **>(&v),
                         &length) &&
          length >= sizeof(VS_FIXEDFILEINFO))
        return QString("%1.%2.%3.%4")
            .arg(HIWORD(v->dwFileVersionMS))
            .arg(LOWORD(v->dwFileVersionMS))
            .arg(HIWORD(v->dwFileVersionLS))
            .arg(LOWORD(v->dwFileVersionLS));
    }
  }
#else
  Q_UNUSED(path)
#endif
  return {};
}
bool executable(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  auto b = f.read(64);
  if (b.startsWith("\x7f"
                   "ELF"))
    return true;
  if (!b.startsWith("MZ") || b.size() < 64)
    return false;
  auto p = qFromLittleEndian<quint32>(b.constData() + 60);
  if (p > 16 * 1024 * 1024 || !f.seek(p))
    return false;
  return f.read(4) == QByteArray("PE\0\0", 4);
}
Json identityJson(const FileIdentity &f) {
  Json entries = Json::array();
  for (const auto &e : f.entries)
    entries.push_back({{"name", e.name.toStdString()},
                       {"crc", e.crc},
                       {"size", e.size},
                       {"hasCrc", e.hasCrc}});
  return {{"path", f.path.toStdString()},
          {"size", f.size},
          {"mtime", f.mtime},
          {"kind", f.kind.toStdString()},
          {"identity", f.identity.toStdString()},
          {"chdHeaderSha1", f.chdHeaderSha1.toStdString()},
          {"error", f.error.toStdString()},
          {"entries", entries}};
}
FileIdentity fromIdentity(const Json &j) {
  FileIdentity f;
  f.path = s(j, "path");
  f.kind = s(j, "kind");
  f.identity = s(j, "identity");
  f.chdHeaderSha1 = s(j, "chdHeaderSha1");
  f.error = s(j, "error");
  f.size = j.value("size", qint64(0));
  f.mtime = j.value("mtime", qint64(0));
  if (j.contains("entries"))
    for (const auto &e : j["entries"])
      f.entries.push_back({s(e, "name"), e.value("crc", quint32(0)),
                           e.value("size", quint64(0)),
                           e.value("hasCrc", false)});
  return f;
}
struct Rom {
  QString name, merge;
  quint32 crc = 0;
  quint64 size = 0;
  bool hasSize = false;
  QString bios;
};
struct Disk {
  QString name, sha1, bios, merge;
};
struct Set {
  QString name, parent;
  QMap<QString, Rom> roms;
  QStringList devices, biosOrder;
  QVector<Disk> disks;
};
using Sets = QMap<QString, Set>;
Sets readSets(const QString &path, bool supermodel, QStringList &errors,
              std::atomic_bool &cancel) {
  Sets result;
  QFile f(path);
  if (path.isEmpty())
    return result;
  if (!f.open(QIODevice::ReadOnly)) {
    errors << "ROM metadata unavailable";
    return result;
  }
  QXmlStreamReader xml(&f);
  Set current;
  QString region;
  while (!xml.atEnd() && !cancel) {
    xml.readNext();
    const auto tag = xml.name();
    if (xml.isStartElement()) {
      auto a = xml.attributes();
      if (tag == "machine" || (supermodel && tag == "game")) {
        current = {};
        current.name = a.value("name").toString();
        current.parent = a.value(supermodel ? "parent" : "cloneof").toString();
      } else if (supermodel && tag == "region")
        region = a.value("name").toString();
      else if (!supermodel && tag == "biosset") {
        const auto bios = a.value("name").toString();
        if (a.value("default") == "yes") current.biosOrder.prepend(bios);
        else current.biosOrder << bios;
      } else if (!supermodel && tag == "disk") {
        if (a.value("status") != "nodump" && a.value("optional") != "yes")
          current.disks << Disk{a.value("name").toString(), a.value("sha1").toString().toLower(),
                                a.value("bios").toString(), a.value("merge").toString()};
      } else if (!supermodel && tag == "device_ref")
        current.devices << a.value("name").toString();
      else if ((!supermodel && tag == "rom") || (supermodel && tag == "file")) {
        if (a.value("status") == "nodump" || a.value("optional") == "yes")
          continue;
        const auto crcText = a.value(supermodel ? "crc32" : "crc").toString();
        if (crcText.isEmpty())
          continue;
        bool ok = false;
        const auto crc = crcText.startsWith("0x")
                             ? crcText.mid(2).toUInt(&ok, 16)
                             : crcText.toUInt(&ok, 16);
        if (!ok)
          continue;
        Rom rom{a.value("name").toString(), a.value("merge").toString(), crc,
                a.value("size").toULongLong(), a.hasAttribute("size"),
                a.value("bios").toString()};
        const QString k =
            supermodel ? region + "/" + a.value("offset").toString() : rom.name + "/" + rom.bios;
        current.roms[k] = rom;
      }
    } else if (xml.isEndElement() &&
               (tag == "machine" || (supermodel && tag == "game"))) {
      if (!current.name.isEmpty())
        result[current.name] = current;
    }
  }
  if (xml.hasError()) {
    errors << "Invalid ROM metadata XML";
    result.clear();
  }
  return result;
}
Set inherited(const QString &name, const Sets &sets, QSet<QString> seen = {}) {
  auto c = sets.value(name);
  if (seen.contains(name))
    return c;
  seen.insert(name);
  if (!c.parent.isEmpty() && sets.contains(c.parent)) {
    auto p = inherited(c.parent, sets, seen);
    for (auto it = c.roms.cbegin(); it != c.roms.cend(); ++it)
      p.roms[it.key()] = it.value();
    p.name = c.name;
    p.parent = c.parent;
    p.devices << c.devices;
    return p;
  }
  return c;
}
QString family(QString name, const Sets &sets) {
  QSet<QString> seen;
  while (sets.contains(name) && !sets[name].parent.isEmpty() &&
         !seen.contains(name)) {
    seen.insert(name);
    name = sets[name].parent;
  }
  return name;
}
struct ArchiveIndex {
  QHash<quint32, QVector<const FileIdentity *>> providers;
  QHash<const FileIdentity *, QHash<quint32, QSet<quint64>>> signatures;
  QHash<QString, QVector<const FileIdentity *>> chds;
  explicit ArchiveIndex(const QVector<FileIdentity> &files) {
    for (const auto &file : files) {
      // The v5 header identity survives an unsupported/corrupt sparse map.
      // It is header evidence only; never claim a payload audit.
      if (file.kind.startsWith("chd-") && !file.chdHeaderSha1.isEmpty())
        chds[QFileInfo(file.path).absolutePath().toCaseFolded()].push_back(&file);
      if (file.kind == "archive-crc" && file.error.isEmpty()) {
        QSet<quint32> seen;
        for (const auto &entry : file.entries)
          if (entry.hasCrc) {
            signatures[&file][entry.crc].insert(entry.size);
            if (!seen.contains(entry.crc)) {
              providers[entry.crc].push_back(&file);
              seen.insert(entry.crc);
            }
          }
      }
    }
  }
  bool contains(const FileIdentity *file, const Rom &rom) const {
    const auto f = signatures.constFind(file);
    if (f == signatures.cend())
      return false;
    const auto r = f->constFind(rom.crc);
    return r != f->cend() && (!rom.hasSize || r->contains(rom.size));
  }
};
using Families = QHash<QString, QStringList>;
Families familyIndex(const Sets &sets) {
  Families index;
  for (auto it = sets.cbegin(); it != sets.cend(); ++it)
    index[family(it.key(), sets)] << it.key();
  return index;
}
void supportRequirement(Binding &binding, const QString &set,
                        const FileIdentity &source, const Rom &rom) {
  for (const auto &entry : source.entries)
    if (entry.hasCrc && entry.crc == rom.crc &&
        (!rom.hasSize || entry.size == rom.size)) {
      for (auto &r : binding.supportRequirements)
        if (r.set == set && r.sourcePath == source.path) {
          r.entries.push_back({entry.name, rom.name, entry.crc, entry.size});
          return;
        }
      binding.supportRequirements.push_back(
          {set, source.path, {{entry.name, rom.name, entry.crc, entry.size}}});
      return;
    }
}
QStringList biosChoices(const Set &set) {
  QStringList choices;
  for (const auto &rom : set.roms)
    if (!rom.bios.isEmpty() && !choices.contains(rom.bios)) choices << rom.bios;
  for (const auto &disk : set.disks)
    if (!disk.bios.isEmpty() && !choices.contains(disk.bios)) choices << disk.bios;
  for (qsizetype i = set.biosOrder.size(); i > 0; --i)
    if (choices.removeAll(set.biosOrder[i - 1])) choices.prepend(set.biosOrder[i - 1]);
  if (choices.isEmpty()) choices << QString();
  return choices;
}
QVector<Rom> biosRoms(const Set &set, const QString &bios) {
  QVector<Rom> out;
  for (const auto &rom : set.roms)
    if (rom.bios.isEmpty() || rom.bios == bios) out << rom;
  return out;
}
const FileIdentity *matchingDisk(const Disk &disk, const Set &set,
                                const FileIdentity &archive, const ArchiveIndex &index) {
  if (!QRegularExpression("^[a-f0-9]{40}$").match(disk.sha1).hasMatch()) return nullptr;
  auto root = QFileInfo(archive.path).absolutePath();
  if (archive.kind.startsWith("chd-")) root = QFileInfo(root).absolutePath();
  QStringList names{set.name};
  if (!disk.merge.isEmpty() && !set.parent.isEmpty()) names << set.parent;
  for (const auto &name : names)
    for (const auto *file : index.chds.value(QDir(root).filePath(name).toCaseFolded()))
      if (file->chdHeaderSha1.compare(disk.sha1, Qt::CaseInsensitive) == 0) return file;
  return nullptr;
}
Binding deviceRoms(const Set &set, const ArchiveIndex &index, const FileIdentity &primary) {
  Binding best;
  bool first = true;
  // Device BIOS choices have no independently addressable launch switch.
  // Require their metadata default instead of silently accepting an alternative.
  for (const auto &bios : QStringList{biosChoices(set).first()}) {
    Binding candidate;
    for (const auto &rom : biosRoms(set, bios)) {
      bool found = false;
      for (const auto *support : index.providers.value(rom.crc))
        if (index.contains(support, rom)) {
          if (support->path != primary.path) candidate.supportPaths << support->path;
          supportRequirement(candidate, set.name, *support, rom);
          found = true;
          break;
        }
      if (!found) candidate.missing << "device:" + set.name + ":" + rom.name;
    }
    for (const auto &disk : set.disks) {
      if (!disk.bios.isEmpty() && disk.bios != bios) continue;
      if (const auto *chd = matchingDisk(disk, set, primary, index))
        candidate.supportPaths << chd->path;
      else candidate.missing << "device-disk:" + set.name + ":" + disk.name;
    }
    if (first || candidate.missing.size() < best.missing.size()) {
      best = candidate; first = false;
    }
  }
  return best;
}
Binding matchSet(const QString &name, const Sets &sets,
                 const ArchiveIndex &index, const Families &families,
                 bool supermodel, std::atomic_bool &cancel) {
  Binding best;
  int bestScore = 0;
  const auto targetFamily = family(name, sets);
  for (const auto &candidate : families.value(targetFamily)) {
    if (cancel) break;
    const auto expected = supermodel ? inherited(candidate, sets) : sets[candidate];
    if (expected.roms.isEmpty() && expected.disks.isEmpty()) continue;
    QSet<const FileIdentity *> relevant;
    for (const auto &rom : expected.roms)
      for (const auto *f : index.providers.value(rom.crc))
        if (index.contains(f, rom)) relevant.insert(f);
    // Disk-only machines anchor to their CHD, never an unrelated archive.
    if (expected.roms.isEmpty() && !expected.disks.isEmpty())
      for (const auto &files : index.chds)
        for (const auto *file : files)
          for (const auto &disk : expected.disks)
            if (matchingDisk(disk, expected, *file, index) == file) relevant.insert(file);
    auto candidates = relevant.values();
    std::sort(candidates.begin(), candidates.end(),
              [](const FileIdentity *a, const FileIdentity *b) { return a->path < b->path; });
    for (const auto *candidateFile : candidates) {
      if (cancel) break;
      const auto &file = *candidateFile;
      for (const auto &bios : biosChoices(expected)) {
        const auto roms = biosRoms(expected, bios);
        int score = 0;
        for (const auto &rom : roms)
          if (rom.merge.isEmpty() && index.contains(&file, rom)) ++score;
        Binding b;
        b.path = file.path; b.identity = expected.name;
        b.bios = bios;
        b.setCandidates = families.value(targetFamily);
        b.proof = supermodel ? "supermodel-header-crc" : "mame-header-crc";
        for (const auto &disk : expected.disks) {
          if (!disk.bios.isEmpty() && disk.bios != bios) continue;
          if (const auto *chd = matchingDisk(disk, expected, file, index)) {
            b.supportPaths << chd->path;
            ++score;
          } else b.missing << "disk:" + disk.name + ":" + disk.sha1;
        }
        // BIOS/parent archives alone cannot anchor the game match.
        if (!score) continue;
        for (const auto &rom : roms) {
          if (index.contains(&file, rom)) {
            supportRequirement(b, expected.name, file, rom); continue;
          }
          bool found = false;
          if (!rom.merge.isEmpty() || (supermodel && !expected.parent.isEmpty()))
            for (const auto *support : index.providers.value(rom.crc))
              if (index.contains(support, rom)) {
                b.supportPaths << support->path;
                supportRequirement(b, expected.name, *support, rom);
                found = true; break;
              }
          if (!found) b.missing << QString("rom:%1:%2").arg(
              rom.name, QString::number(rom.crc, 16).rightJustified(8, '0'));
        }
        QSet<QString> visited;
        QQueue<QString> devices;
        for (const auto &d : expected.devices) devices.enqueue(d);
        while (!devices.isEmpty()) {
          const auto d = devices.dequeue();
          if (visited.contains(d)) continue;
          visited.insert(d);
          if (!sets.contains(d)) { b.missing << "device-metadata:" + d; continue; }
          for (const auto &next : sets[d].devices) devices.enqueue(next);
          auto requirements = deviceRoms(sets[d], index, file);
          b.missing << requirements.missing;
          b.supportPaths << requirements.supportPaths;
          b.supportRequirements << requirements.supportRequirements;
        }
        b.supportPaths.removeDuplicates();
        b.verified = b.missing.isEmpty();
        if (best.path.isEmpty() || (b.verified && !best.verified) ||
            (b.verified == best.verified &&
             (score > bestScore || (score == bestScore &&
              ((b.identity == name && best.identity != name) ||
               (b.identity == best.identity && bios == biosChoices(expected).first() && best.bios != bios)))))) {
          best = b; bestScore = score;
        }
      }
    }
  }
  return best;
}
QString mameMetadata(const QVector<ToolBinding> &tools, const ScanOptions &o,
                     std::atomic_bool &cancel, QStringList &errors) {
  if (!o.mameXml.isEmpty())
    return o.mameXml;
  for (const auto &t : tools)
    if (t.id == "mame" && t.verified) {
      if (o.userRoot.isEmpty())
        return {};
      QDir().mkpath(o.userRoot + "/cache");
      QFileInfo info(t.path);
      const auto stamp =
          QString::number(info.size()) + ":" +
          QString::number(info.lastModified().toMSecsSinceEpoch());
      const auto k = QString::fromLatin1(
          QCryptographicHash::hash((t.path + stamp).toUtf8(),
                                   QCryptographicHash::Sha256)
              .toHex());
      const auto path = o.userRoot + "/cache/mame-" + k + ".xml";
      if (QFileInfo::exists(path))
        return path;
      const auto temp = path + ".partial";
      QProcess p;
      p.setProgram(t.path);
      p.setArguments({"-listxml"});
      p.setWorkingDirectory(info.absolutePath());
      p.setStandardOutputFile(temp);
      p.start();
      if (!p.waitForStarted(3000)) {
        errors << "Located MAME could not produce listxml";
        return {};
      }
      QElapsedTimer timer;
      timer.start();
      while (!p.waitForFinished(100)) {
        if (cancel || timer.elapsed() > 120000) {
          p.kill();
          p.waitForFinished();
          QFile::remove(temp);
          errors << "MAME listxml cancelled or timed out";
          return {};
        }
      }
      if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        QFile::remove(temp);
        errors << "MAME listxml failed";
        return {};
      }
      if (QFile::rename(temp, path))
        return path;
      QFile::remove(temp);
    }
  return {};
}
} // namespace
QString Scanner::requirementId(const GameRecord &game, const Json &media,
                               int index) {
  return ac::mediaRequirementId(media, game.id, index);
}
ScanOptions Scanner::optionsFromJson(const Json &j) {
  ScanOptions o;
  o.mediaRoots = strings(j, "mediaRoots");
  o.toolRoots = strings(j, "toolRoots");
  o.artRoots = strings(j, "artRoots");
  o.userRoot = s(j, "userRoot");
  o.mameXml = s(j, "mameXml");
  o.supermodelXml = s(j, "supermodelXml");
  o.serialIndex = s(j, "serialIndex");
  o.maxDepth = qBound(0, j.value("maxDepth", 6), 16);
  o.locateOnly = j.value("locateOnly", false);
  return o;
}
Json ScanResult::toJson() const {
  Json b = Json::array(), t = Json::array(), f = Json::array(),
       st = Json::array();
  for (const auto &v : bindings) {
    Json requirements = Json::array();
    for (const auto &r : v.supportRequirements) {
      Json entries = Json::array();
      for (const auto &e : r.entries)
        entries.push_back({{"sourceName", e.sourceName.toStdString()},
                           {"targetName", e.targetName.toStdString()},
                           {"crc32", e.crc32},
                           {"size", e.size}});
      requirements.push_back({{"set", r.set.toStdString()},
                              {"sourcePath", r.sourcePath.toStdString()},
                              {"entries", entries}});
    }
    b.push_back({{"gameId", v.gameId.toStdString()},
                 {"requirementId", v.requirementId.toStdString()},
                 {"path", v.path.toStdString()},
                 {"proof", v.proof.toStdString()},
                 {"identity", v.identity.toStdString()},
                 {"bios", v.bios.toStdString()},
                 {"setCandidates", list(v.setCandidates)},
                 {"verified", v.verified},
                 {"supportPaths", list(v.supportPaths)},
                 {"supportRequirements", requirements},
                 {"missing", list(v.missing)}});
  }
  for (const auto &v : tools)
    t.push_back({{"id", v.id.toStdString()},
                 {"path", v.path.toStdString()},
                 {"version", v.version.toStdString()},
                 {"size", v.size}, {"mtime", v.mtime},
                 {"verified", v.verified}});
  for (const auto &v : files) {
    auto j = identityJson(v);
    j["cacheHit"] = v.cacheHit;
    f.push_back(j);
  }
  for (const auto &v : states) {
    Json variants = Json::array();
    for (const auto &variant : v.variants)
      variants.push_back(
          {{"id", variant.id.toStdString()},
           {"verified", variant.verified},
           {"installedWhenExists", variant.installedWhenExists},
           {"manifestExists", variant.manifestExists},
           {"updateAvailable", variant.updateAvailable},
           {"updateVersion", variant.updateVersion.toStdString()}});
    st.push_back({{"gameId", v.gameId.toStdString()},
                  {"mediaFound", list(v.mediaFound)},
                  {"toolsOk", list(v.toolsOk)},
                  {"toolsOlder", list(v.toolsOlder)},
                  {"variants", variants},
                  {"jobStatus", static_cast<int>(v.jobStatus)},
                  {"jobProgress", v.jobProgress.toStdString()},
                  {"selectedVariantId", v.selectedVariantId.toStdString()},
                  {"lastPlayed", v.lastPlayed},
                  {"firstSeen", v.firstSeen},
                  {"recentIndex", v.recentIndex},
                  {"artSource", static_cast<int>(v.artSource)}});
  }
  qint64 entries = 0, cacheHits = 0;
  for (const auto &file : files) {
    entries += file.entries.size();
    if (file.cacheHit)
      ++cacheHits;
  }
  return {{"cancelled", cancelled},
          {"elapsedMs", elapsedMs},
          {"fileCount", files.size()},
          {"archiveEntryCount", entries},
          {"cacheHits", cacheHits},
          {"bindings", b},
          {"tools", t},
          {"files", f},
          {"states", st},
          {"diagnostics", list(diagnostics)}};
}
ScanResult Scanner::run(const CatalogData &catalog, const ScanOptions &o,
                        std::atomic_bool &cancel, const Progress &progress) {
  ScanResult r;
  QElapsedTimer elapsed;
  elapsed.start();
  if (progress)
    progress({{"phase", "locating"}, {"completed", 0}, {"total", 0}});
  const auto executables =
      enumerate(o.toolRoots, {"exe", "appimage", ""}, o.maxDepth, cancel);
  for (const auto &path : executables) {
    if (cancel)
      break;
    const auto base = QFileInfo(path).completeBaseName().toLower();
    QString id;
    if (base == "mame" || base == "mame64")
      id = "mame";
    else if (base.startsWith("pcsx2"))
      id = "pcsx2";
    else if (base == "supermodel")
      id = "supermodel";
    if (!id.isEmpty() && catalog.emulators.contains(id.toStdString())) {
      const auto &manifest = catalog.emulators[id.toStdString()];
      if (manifest.contains("locate")) {
        const auto patterns = strings(manifest["locate"], "exe");
        bool matched = patterns.isEmpty();
        for (const auto &pattern : patterns)
          if (QRegularExpression(
                  QRegularExpression::wildcardToRegularExpression(pattern),
                  QRegularExpression::CaseInsensitiveOption)
                  .match(QFileInfo(path).fileName())
                  .hasMatch()) {
            matched = true;
            break;
          }
        if (!matched)
          continue;
      }
    }
    if (!id.isEmpty() && executable(path))
      r.tools.push_back({id, path, version(path), true, QFileInfo(path).size(), QFileInfo(path).lastModified().toMSecsSinceEpoch()});
  }
  if (o.locateOnly) {
    r.cancelled = cancel;
    r.elapsedMs = elapsed.elapsed();
    return r;
  }
  QMap<QString, FileIdentity> cache;
  const auto cachePath = o.userRoot + "/cache/scan.toml";
  if (!o.userRoot.isEmpty())
    try {
      auto table = toml::parse_file(cachePath.toStdString());
      if (table["version"].value_or(0) == 4)
        if (auto a = table["files"].as_array())
          for (const auto &n : *a)
            if (auto p = n.value<std::string>()) {
              auto f = fromIdentity(Json::parse(*p));
              cache[f.path] = f;
            }
    } catch (const std::exception &) {
    }
  const auto paths = enumerate(o.mediaRoots,
                               {"zip", "7z", "iso", "chd", "bin", "img", "rom",
                                "rom0", "rom1", "exe", "gcm"},
                               o.maxDepth, cancel);
  int completed = 0;
  for (const auto &path : paths) {
    if (cancel)
      break;
    QFileInfo info(path);
    FileIdentity f;
    if (cache.contains(path) && cache[path].size == info.size() &&
        cache[path].mtime == info.lastModified().toMSecsSinceEpoch()) {
      f = cache[path];
      f.cacheHit = true;
    } else
      f = inspect(path, cancel);
    if (cancel)
      break;
    r.files << f;
    if (progress)
      progress({{"phase", "headers"},
                {"completed", ++completed},
                {"total", paths.size()}});
  }
  if (cancel) {
    r.cancelled = true;
    r.elapsedMs = elapsed.elapsed();
    return r;
  }
  auto mame = readSets(mameMetadata(r.tools, o, cancel, r.diagnostics), false,
                       r.diagnostics, cancel);
  auto model3Path = o.supermodelXml;
  if (model3Path.isEmpty())
    for (const auto &tool : r.tools)
      if (tool.id == "supermodel") {
        const auto candidate =
            QFileInfo(tool.path).absolutePath() + "/Config/Games.xml";
        if (QFileInfo::exists(candidate)) {
          model3Path = candidate;
          break;
        }
      }
  auto model3 = readSets(model3Path, true, r.diagnostics, cancel);
  const ArchiveIndex archiveIndex(r.files);
  const auto mameFamilies = familyIndex(mame),
             model3Families = familyIndex(model3);
  Json serials = Json::object();
  QFile index(o.serialIndex.isEmpty() ? ":/scan/serial-index.json"
                                      : o.serialIndex);
  if (index.open(QIODevice::ReadOnly))
    try {
      serials = Json::parse(index.readAll().toStdString());
    } catch (const std::exception &) {
      r.diagnostics << "Invalid serial index";
    }
  int matchedGames = 0;
  for (const auto &g : catalog.games) {
    if (cancel)
      break;
    auto state = g.runtime;
    state.gameId = g.id;
    state.mediaFound.clear();
    state.toolsOk.clear();
    state.toolsOlder.clear();
    for (const auto &tool : r.tools)
      if (tool.verified)
        state.toolsOk << tool.id;
    state.toolsOk.removeDuplicates();
    if (g.raw.contains("media") && g.raw["media"].is_array()) {
      int i = 0;
      for (const auto &m : g.raw["media"]) {
        Binding binding;
        binding.gameId = g.id;
        binding.requirementId = requirementId(g, m, i++);
        const auto kind = s(m, "kind");
        if (kind == "mame-romset" || (kind == "bios" && !s(m, "set").isEmpty())) {
          const auto name = s(m, "set");
          auto candidate =
              matchSet(name, mame, archiveIndex, mameFamilies, false, cancel);
          if (g.raw.value("hardware", std::string()) == "sega-model-3") {
            auto model = matchSet(name, model3, archiveIndex, model3Families,
                                  true, cancel);
            if (model.verified || candidate.path.isEmpty())
              candidate = model;
          }
          candidate.gameId = binding.gameId;
          candidate.requirementId = binding.requirementId;
          binding = candidate;
          if (binding.path.isEmpty())
            binding.missing << "No CRC-verified set candidate";
        } else if (kind == "disc" || kind == "bios") {
          for (const auto &f : r.files) {
            if (!f.error.isEmpty() || f.identity.isEmpty())
              continue;
            bool found = false;
            if (kind == "bios")
              found = f.kind == "ps2-bios-romdir" &&
                      s(g.raw, "hardware") == "sony-ps2";
            else if (!s(m, "chd_sha1").isEmpty() && !f.chdHeaderSha1.isEmpty())
              found = s(m, "chd_sha1").compare(f.chdHeaderSha1, Qt::CaseInsensitive) == 0;
            else if (!s(m, "serial").isEmpty())
              found = s(m, "serial") == f.identity;
            else if (serials.contains(f.identity.toStdString())) {
              const auto &entry = serials[f.identity.toStdString()];
              found = s(entry, "gameId") == g.id;
            }
            if (found) {
              binding.path = f.path;
              binding.identity = f.identity;
              binding.proof = f.kind;
              binding.verified = true;
              if (kind == "bios")
                for (const auto &component : r.files)
                  if (component.kind == "ps2-bios-component" &&
                      QFileInfo(component.path).absolutePath() ==
                          QFileInfo(f.path).absolutePath() &&
                      QFileInfo(component.path).completeBaseName() ==
                          QFileInfo(f.path).completeBaseName())
                    binding.supportPaths << component.path;
              break;
            }
          }
          if (!binding.verified)
            binding.missing << "No matching verified header identity";
        } else if (kind == "pc-game" || kind == "pc") {
          auto names = strings(m, "find");
          if (!s(m, "find").isEmpty())
            names << s(m, "find");
          if (!s(m, "exe").isEmpty())
            names << s(m, "exe");
          for (const auto &path : paths)
            if (names.contains(QFileInfo(path).fileName(),
                               Qt::CaseInsensitive) &&
                executable(path)) {
              binding.path = path;
              binding.identity = version(path);
              binding.proof = "executable-name-only";
              binding.verified = true;
              break;
            }
        } else
          binding.missing << "Unsupported media requirement";
        if (binding.verified)
          state.mediaFound << binding.requirementId;
        r.bindings << binding;
      }
    }
    r.states << state;
    if (progress)
      progress({{"phase", "matching"},
                {"completed", ++matchedGames},
                {"total", catalog.games.size()}});
  }
  r.cancelled = cancel;
  r.elapsedMs = elapsed.elapsed();
  if (!r.cancelled && !o.userRoot.isEmpty()) {
    QDir().mkpath(o.userRoot + "/cache");
    toml::table table;
    table.insert("version", 4);
    toml::array a;
    for (const auto &f : r.files)
      a.push_back(identityJson(f).dump());
    table.insert("files", std::move(a));
    std::ostringstream text;
    text << table;
    QSaveFile output(cachePath);
    if (output.open(QIODevice::WriteOnly)) {
      output.write(QByteArray::fromStdString(text.str()));
      output.commit();
    }
    QSaveFile bindings(o.userRoot + "/cache/scan-bindings.json");
    if (bindings.open(QIODevice::WriteOnly)) {
      bindings.write(QByteArray::fromStdString(r.toJson().dump(2)));
      bindings.commit();
    }
    QSaveFile folders(o.userRoot + "/scan-folders.json");
    if (folders.open(QIODevice::WriteOnly)) {
      Json config{{"mediaRoots", list(o.mediaRoots)},
                  {"toolRoots", list(o.toolRoots)},
                  {"artRoots", list(o.artRoots)}};
      folders.write(QByteArray::fromStdString(config.dump(2)));
      folders.commit();
    }
  }
  if (progress)
    progress({{"phase", r.cancelled ? "cancelled" : "complete"},
              {"completed", completed},
              {"total", paths.size()}});
  r.elapsedMs = elapsed.elapsed();
  return r;
}
} // namespace ac::scan
