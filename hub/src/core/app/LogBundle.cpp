// SPDX-License-Identifier: GPL-3.0-only
#include "LogBundle.h"
#include "ProductIdentity.h"
#include "core/install/Support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSysInfo>
#include <algorithm>
#include <array>
namespace ac::report {
namespace {
const QRegularExpression::PatternOption kNoCase = QRegularExpression::CaseInsensitiveOption;
// A folder, in both spellings, wherever a path continues from it: never a longer name that only starts the same.
void replaceFolder(QString &text, const QString &folder, const QString &name) {
  if (folder.isEmpty())
    return;
  const auto clean = QDir::cleanPath(folder);
  // A folder at a drive's root or nowhere names no one.
  if (clean.size() < 4 || clean == QDir::rootPath())
    return;
  for (const auto &spelling : {clean, QDir::toNativeSeparators(clean)})
    text.replace(QRegularExpression(QRegularExpression::escape(spelling) + R"re((?=$|[\\/\s"'):;,]))re", kNoCase), name);
}
// Words the Hub writes itself or that every PC has: an account called one of these is not worth hiding, and
// replacing it would damage the text.
bool commonWord(const QString &name) {
  static const QStringList words{"user",   "users", "home",  "hub",    "path",  "name",  "email",     "token",
                                 "redacted", "admin", "administrator", "public", "default", "guest", "owner",
                                 "root",   "system", "test", "steam",  "game",  "games", "log",       "logs",
                                 "install", "data", "temp",  "tmp",    "local", "localhost"};
  return words.contains(name.toLower());
}
// A path that was not the person's own folder: the folders are not wanted, the file's own name often is.
QString shortenPaths(const QString &text, const QRegularExpression &paths) {
  QString out;
  qsizetype last = 0;
  auto found = paths.globalMatch(text);
  while (found.hasNext()) {
    const auto match = found.next();
    out += text.mid(last, match.capturedStart() - last);
    const auto path = match.captured();
    const auto cut = std::max(path.lastIndexOf('\\'), path.lastIndexOf('/'));
    out += QStringLiteral("<path>");
    if (cut >= 0 && cut < path.size() - 1)
      out += path.mid(cut);
    last = match.capturedEnd();
  }
  return out + text.mid(last);
}
} // namespace
Redaction Redaction::thisPc(const QString &root) {
  Redaction who;
  who.root = root;
  who.home = QDir::homePath();
  QStringList raw{QDir(who.home).dirName(), qEnvironmentVariable("USERNAME"), qEnvironmentVariable("USER"),
                  qEnvironmentVariable("LOGNAME"), QSysInfo::machineHostName()};
  for (const auto &name : raw)
    if (!name.trimmed().isEmpty() && !who.names.contains(name.trimmed(), Qt::CaseInsensitive))
      who.names << name.trimmed();
  return who;
}
QString redact(const QString &text, const Redaction &who) {
  auto out = text;
  // Folders the Hub knows first, then any user folder (another account's, or a path from another PC).
  replaceFolder(out, who.root, QStringLiteral("<hub>"));
  replaceFolder(out, who.home, QStringLiteral("<home>"));
  static const QRegularExpression profile(R"re((?<![A-Za-z0-9>])(?:[A-Za-z]:)?[\\/]+(?:Users|home)[\\/]+[^\\/\s"'<>|:;,)]+)re", kNoCase);
  out.replace(profile, QStringLiteral("<home>"));
  static const QRegularExpression network(R"re((?<![A-Za-z0-9])\\\\[^\s\\/"'<>|]+[\\/][^\s"'<>|]*)re");
  out.replace(network, QStringLiteral("<network path>"));
  static const QRegularExpression drivePath(R"re((?<![A-Za-z0-9])[A-Za-z]:[\\/][^\s"'<>|*?]*)re");
  out = shortenPaths(out, drivePath);
  // Web logins, keys and tokens.
  static const QRegularExpression login(R"re(\b([a-z][a-z0-9+.\-]*://)[^/\s:@]+(?::[^/\s@]*)?@)re", kNoCase);
  out.replace(login, QStringLiteral("\\1<redacted>@"));
  static const QRegularExpression query(R"re(([?&](?:access_token|token|api[_-]?key|apikey|key|sig|signature|auth|password|secret)=)[^&\s"']+)re", kNoCase);
  out.replace(query, QStringLiteral("\\1<redacted>"));
  static const QRegularExpression secret(
      R"re((\b(?:[a-z0-9_\-]*(?:token|secret|password|passwd|api[_-]?key|apikey|authorization|credential)[a-z0-9_\-]*)["']?\s*[:=]\s*["']?)(?:(?:Bearer|Basic|Token)\s+)?[^\s"',;&]+)re",
      kNoCase);
  out.replace(secret, QStringLiteral("\\1<redacted>"));
  static const QRegularExpression keyed(R"re(\b([a-z0-9_]*key["']?\s*[:=]\s*["']?)[A-Za-z0-9_\-+/=]{16,})re", kNoCase);
  out.replace(keyed, QStringLiteral("\\1<redacted>"));
  static const QRegularExpression shaped(
      R"re(\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9_\-]{20,}|xox[baprs]-[A-Za-z0-9\-]{10,}|AKIA[0-9A-Z]{16}|eyJ[A-Za-z0-9_\-]{8,}\.[A-Za-z0-9_\-]{8,}\.[A-Za-z0-9_\-]{4,})(?![A-Za-z0-9_\-]))re");
  out.replace(shaped, QStringLiteral("<token>"));
  static const QRegularExpression steamAccount(R"re(\b7656119\d{10}\b)re");
  out.replace(steamAccount, QStringLiteral("<steam id>"));
  static const QRegularExpression email(R"re(\b[a-z0-9._%+\-]+@[a-z0-9\-]+(?:\.[a-z0-9\-]+)+)re", kNoCase);
  out.replace(email, QStringLiteral("<email>"));
  // The account and computer names, last, wherever they stand alone as a word.
  for (const auto &name : who.names) {
    const auto word = name.trimmed();
    if (word.size() < 3 || commonWord(word))
      continue;
    out.replace(QRegularExpression(QStringLiteral("(?<![A-Za-z0-9])") + QRegularExpression::escape(word) + QStringLiteral("(?![A-Za-z0-9])"), kNoCase),
                QStringLiteral("<name>"));
  }
  return out;
}
namespace {
quint32 crc32Of(const QByteArray &bytes) {
  static const auto table = [] {
    std::array<quint32, 256> made{};
    for (quint32 i = 0; i < 256; ++i) {
      quint32 c = i;
      for (int bit = 0; bit < 8; ++bit)
        c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
      made[i] = c;
    }
    return made;
  }();
  quint32 crc = 0xFFFFFFFFU;
  for (const char byte : bytes)
    crc = table[(crc ^ static_cast<quint32>(static_cast<quint8>(byte))) & 0xFFU] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFU;
}
void put16(QByteArray &out, quint32 value) {
  out.append(static_cast<char>(value & 0xFFU));
  out.append(static_cast<char>((value >> 8) & 0xFFU));
}
void put32(QByteArray &out, quint32 value) {
  put16(out, value & 0xFFFFU);
  put16(out, (value >> 16) & 0xFFFFU);
}
} // namespace
QByteArray zipOf(const QList<QPair<QString, QByteArray>> &entries, const QDateTime &when) {
  const auto utc = when.toUTC();
  const quint32 year = static_cast<quint32>(std::max(1980, utc.date().year()));
  const quint32 dosDate = ((year - 1980U) << 9) | (static_cast<quint32>(utc.date().month()) << 5) | static_cast<quint32>(utc.date().day());
  const quint32 dosTime = (static_cast<quint32>(utc.time().hour()) << 11) | (static_cast<quint32>(utc.time().minute()) << 5) |
                          (static_cast<quint32>(utc.time().second()) / 2U);
  QByteArray out, directory;
  for (const auto &entry : entries) {
    const auto name = entry.first.toUtf8();
    const auto crc = crc32Of(entry.second);
    const auto size = static_cast<quint32>(entry.second.size());
    const auto offset = static_cast<quint32>(out.size());
    // Local header, then the bytes as they are (method 0: stored). Bit 11 says the name is UTF-8.
    put32(out, 0x04034B50U);
    put16(out, 20);
    put16(out, 0x0800);
    put16(out, 0);
    put16(out, dosTime);
    put16(out, dosDate);
    put32(out, crc);
    put32(out, size);
    put32(out, size);
    put16(out, static_cast<quint32>(name.size()));
    put16(out, 0);
    out.append(name);
    out.append(entry.second);
    put32(directory, 0x02014B50U);
    put16(directory, 20);
    put16(directory, 20);
    put16(directory, 0x0800);
    put16(directory, 0);
    put16(directory, dosTime);
    put16(directory, dosDate);
    put32(directory, crc);
    put32(directory, size);
    put32(directory, size);
    put16(directory, static_cast<quint32>(name.size()));
    put16(directory, 0);
    put16(directory, 0);
    put16(directory, 0);
    put16(directory, 0);
    put32(directory, 0);
    put32(directory, offset);
    directory.append(name);
  }
  const auto directoryAt = static_cast<quint32>(out.size());
  out.append(directory);
  put32(out, 0x06054B50U);
  put16(out, 0);
  put16(out, 0);
  put16(out, static_cast<quint32>(entries.size()));
  put16(out, static_cast<quint32>(entries.size()));
  put32(out, static_cast<quint32>(directory.size()));
  put32(out, directoryAt);
  put16(out, 0);
  return out;
}
namespace {
struct Candidate {
  QString name, path;
  QDateTime time;
  int rank = 9;
};
// Plain file names only: nothing odd goes into a ZIP's names, and nothing outside the logs folder is reached.
bool plainName(const QString &name) {
  static const QRegularExpression plain(R"re(^[A-Za-z0-9][A-Za-z0-9._ \-]*$)re");
  return plain.match(name).hasMatch() && !name.contains("..");
}
bool readable(const QFileInfo &info) {
  const auto suffix = info.suffix().toLower();
  return info.isFile() && !info.isSymLink() && !info.isJunction() && plainName(info.fileName()) &&
         (suffix == "log" || suffix == "txt" || suffix == "json");
}
QList<Candidate> gather(const QString &logsFolder) {
  QList<Candidate> found;
  const QDir logs(logsFolder);
  for (const auto &info : QDir(logs.filePath("install")).entryInfoList(QDir::Files | QDir::NoSymLinks))
    if (readable(info))
      found.append({"hub-logs/install/" + info.fileName(), info.absoluteFilePath(), info.lastModified(), 3});
  for (const auto &info : logs.entryInfoList(QDir::Files | QDir::NoSymLinks))
    if (readable(info))
      found.append({"hub-logs/" + info.fileName(), info.absoluteFilePath(), info.lastModified(),
                    info.fileName() == "crashes.json" ? 1 : info.fileName().startsWith("launch-") ? 2 : 4});
  // The last launch: the newest .log inside any game's own folder of launch logs.
  Candidate last;
  for (const auto &folder : logs.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
    if (folder.isJunction() || !plainName(folder.fileName()) || folder.fileName() == "install")
      continue;
    for (const auto &info : QDir(folder.absoluteFilePath()).entryInfoList(QDir::Files | QDir::NoSymLinks))
      if (readable(info) && info.suffix().toLower() == "log" && (last.path.isEmpty() || info.lastModified() > last.time))
        last = {"last-launch/" + folder.fileName() + "/" + info.fileName(), info.absoluteFilePath(), info.lastModified(), 0};
  }
  if (!last.path.isEmpty())
    found.append(last);
  std::stable_sort(found.begin(), found.end(), [](const Candidate &a, const Candidate &b) {
    return a.rank != b.rank ? a.rank < b.rank : a.time > b.time;
  });
  return found;
}
// The end of a file, from a whole line; empty when it cannot be read.
QByteArray endOf(const QString &path, qint64 limit, bool *cut) {
  *cut = false;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  if (file.size() > limit) {
    *cut = file.seek(file.size() - limit);
  }
  auto bytes = file.read(limit);
  if (*cut) {
    const auto lineEnd = bytes.indexOf('\n');
    if (lineEnd >= 0)
      bytes.remove(0, lineEnd + 1);
  }
  return bytes;
}
} // namespace
Bundle writeBundle(const QString &root, const QString &folder, const Redaction &who, const QString &appName,
                   const QString &appVersion, const BundleLimits &limits, const QDateTime &when) {
  Bundle bundle;
  QList<QPair<QString, QByteArray>> entries;
  // The logs folder itself must be a plain folder inside the Hub's own.
  QString logs;
  try {
    logs = install::scopedPath("user/logs", root);
  } catch (const std::exception &) {
  }
  const auto found = logs.isEmpty() ? QList<Candidate>() : gather(logs);
  for (const auto &item : found) {
    if (entries.size() >= limits.files) {
      bundle.leftOut << item.name + ": too many files for one report";
      continue;
    }
    bool cut = false;
    const auto raw = endOf(item.path, limits.perFile, &cut);
    if (raw.isEmpty()) {
      bundle.leftOut << item.name + ": empty or could not be read";
      continue;
    }
    auto text = redact(QString::fromUtf8(raw), who);
    if (cut)
      text = "[The start of this log was left out to keep the report small.]\n" + text;
    const auto bytes = text.toUtf8();
    if (bundle.bytes + bytes.size() > limits.total) {
      bundle.leftOut << item.name + ": would make the report too big";
      continue;
    }
    bundle.bytes += bytes.size();
    bundle.included << item.name;
    entries.append(qMakePair(item.name, bytes));
  }
  QString info = appName + " problem report\nVersion: " + appVersion + "\nMade: " + when.toUTC().toString(Qt::ISODate) +
                 "\nSystem: " + QSysInfo::prettyProductName() + ", " + QSysInfo::currentCpuArchitecture() + "\n\n";
  info += bundle.included.isEmpty() ? QString("No logs were found yet.\n") : "Logs in this file:\n- " + bundle.included.join("\n- ") + "\n";
  if (!bundle.leftOut.isEmpty())
    info += "\nLeft out:\n- " + bundle.leftOut.join("\n- ") + "\n";
  info += "\nFolder names, your account and computer names, emails, web logins, keys and tokens were taken out of the "
          "logs. No game files, BIOS files or saves are in this file. You can open it and look before you send it.\n";
  entries.prepend(qMakePair(QString("about.txt"), redact(info, who).toUtf8()));
  const auto stem = QString::fromUtf8(ac::product::short_id) + "-report";
  bundle.path = QDir::cleanPath(folder) + "/" + stem + "-" + when.toUTC().toString("yyyyMMdd-HHmmss") + ".zip";
  try {
    if (!QDir().mkpath(folder))
      throw install::Error("E_WRITE_DENIED", "The folder for the report could not be made");
    install::atomicWrite(bundle.path, zipOf(entries, when));
  } catch (const std::exception &error) {
    bundle.error = QString::fromUtf8(error.what());
    bundle.path.clear();
    return bundle;
  }
  // Only the newest few reports are kept.
  auto old = QDir(folder).entryInfoList({stem + "-*.zip"}, QDir::Files, QDir::Name);
  while (old.size() > 5)
    QFile::remove(old.takeFirst().absoluteFilePath());
  bundle.ok = true;
  return bundle;
}
} // namespace ac::report
