// SPDX-License-Identifier: GPL-3.0-only
#include "Support.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <archive.h>
#include <archive_entry.h>
namespace ac::install {
namespace {
struct Reader {
  archive *a = archive_read_new();
  Reader(const QString &path) {
    archive_read_support_filter_gzip(a);
    archive_read_support_filter_none(a);
    archive_read_support_format_zip(a);
    archive_read_support_format_7zip(a);
    archive_read_support_format_tar(a);
    if (archive_read_open_filename(a, QFile::encodeName(path).constData(),
                                   128 * 1024) != ARCHIVE_OK) {
      const auto msg = QString::fromUtf8(archive_error_string(a));
      archive_read_free(a);
      a = nullptr;
      throw Error("E_ARCHIVE_CORRUPT", msg);
    }
  }
  ~Reader() {
    if (a)
      archive_read_free(a);
  }
};
bool matches(const QString &path, const QStringList &patterns) {
  for (const auto &p : patterns)
    if (QDir::match(p, path))
      return true;
  return false;
}
QList<ArchiveEntry> walk(const QString &path, ArchiveLimits limits,
                         const Json &guard, const QString &stage = {},
                         int strip = 0, const QStringList &include = {},
                         const QStringList &exclude = {}) {
  Reader r(path);
  QList<ArchiveEntry> result;
  QSet<QString> names;
  archive_entry *entry = nullptr;
  const qint64 compressed = QFileInfo(path).size();
  const qint64 ceiling = 20LL * 1024 * 1024 * 1024;
  const qint64 max = limits.maxExpanded ? std::min(limits.maxExpanded, ceiling)
                                        : std::min(compressed * 4, ceiling);
  qint64 total = 0;
  int status = 0;
  while ((status = archive_read_next_header(r.a, &entry)) == ARCHIVE_OK) {
    if (result.size() >= limits.maxEntries)
      throw Error("E_ARCHIVE_LIMITS", "Too many archive entries");
    const char *utf8 = archive_entry_pathname_utf8(entry);
    QString name =
        QString::fromUtf8(utf8 ? utf8 : archive_entry_pathname(entry));
    name.replace('\\', '/');
    while (name.endsWith('/'))
      name.chop(1);
    validateRelative(name);
    contentGuard(name, guard);
    if (names.contains(name.toCaseFolded()))
      throw Error("E_ARCHIVE_UNSAFE", "Archive case collision");
    names.insert(name.toCaseFolded());
    const auto type = archive_entry_filetype(entry);
    if (archive_entry_symlink(entry) || archive_entry_hardlink(entry) ||
        (type != AE_IFREG && type != AE_IFDIR))
      throw Error("E_ARCHIVE_UNSAFE",
                  "Archive links and special files refused");
    if (archive_entry_is_encrypted(entry) > 0)
      throw Error("E_ARCHIVE_ENCRYPTED", "Encrypted archive refused");
    const auto size = static_cast<qint64>(archive_entry_size(entry));
    if (size < 0 || size > max - total)
      throw Error("E_ARCHIVE_LIMITS", "Expanded archive exceeds cap");
    total += size;
    result.push_back({name, size, type == AE_IFDIR});
    auto parts = name.split('/');
    for (int i = 0; i < strip && !parts.isEmpty(); ++i)
      parts.removeFirst();
    const auto output = parts.join('/');
    const bool selected = !stage.isEmpty() && !output.isEmpty() &&
                          type == AE_IFREG &&
                          (include.isEmpty() || matches(output, include)) &&
                          !matches(output, exclude);
    QFile file;
    if (selected) {
      const auto dest = scopedPath(output, stage);
      QDir().mkpath(QFileInfo(dest).absolutePath());
      file.setFileName(dest);
      if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        throw Error("E_WRITE_DENIED", "Cannot extract staging file");
    }
    const auto start = archive_filter_bytes(r.a, -1);
    qint64 actual = 0;
    QCryptographicHash contentHash(QCryptographicHash::Sha256);
    char buffer[128 * 1024];
    la_ssize_t count = 0;
    while ((count = archive_read_data(r.a, buffer, sizeof buffer)) > 0) {
      contentHash.addData(QByteArrayView(buffer, count));
      actual += count;
      if (actual > size || actual > max)
        throw Error("E_ARCHIVE_LIMITS",
                    "Archive stream exceeds advertised size");
      if (selected && file.write(buffer, count) != count)
        throw Error("E_WRITE_DENIED", "Cannot write staging file");
    }
    if (count < 0 || actual != size)
      throw Error("E_ARCHIVE_CORRUPT",
                  "Archive CRC, size or truncation failure");
    const auto entryHash = QString::fromLatin1(contentHash.result().toHex());
    for (const auto &blocked :
         (guard.is_object() ? guard.value("sha256", Json::array())
                            : Json::array()))
      if (blocked.is_string() &&
          QString::fromStdString(blocked.get<std::string>())
                  .compare(entryHash, Qt::CaseInsensitive) == 0)
        throw Error("E_CONTENT_GUARD", "Known game content entry hash refused");
    const auto consumed =
        std::max<qint64>(1, archive_filter_bytes(r.a, -1) - start);
    if (size > 1024 * 1024 && size / consumed > 1000)
      throw Error("E_ARCHIVE_LIMITS", "Archive compression ratio exceeds cap");
  }
  if (status != ARCHIVE_EOF)
    throw Error("E_ARCHIVE_CORRUPT", "Cannot finish archive listing");
  if (archive_read_has_encrypted_entries(r.a) > 0)
    throw Error("E_ARCHIVE_ENCRYPTED", "Encrypted archive refused");
  return result;
}
} // namespace
void Archive::makeUserMediaAlias(const Json &requirements,
                                 const QString &destination) {
  // Only confirmed local user media reaches this API; never called by artifact
  // downloads.
  const auto temp = destination + ".staging";
  QDir().mkpath(QFileInfo(temp).absolutePath());
  archive *writer = archive_write_new();
  archive_write_set_format_zip(writer);
  archive_write_set_options(writer, "zip:compression=store");
  if (archive_write_open_filename(
          writer, QFile::encodeName(temp).constData()) != ARCHIVE_OK) {
    archive_write_free(writer);
    throw Error("E_WRITE_DENIED", "Cannot create local media alias");
  }
  QSet<QString> targets;
  try {
    for (const auto &requirement : requirements) {
      Reader reader(string(requirement, "sourcePath"));
      auto expected = requirement.value("entries", Json::array());
      QSet<QString> found;
      archive_entry *entry = nullptr;
      int status = 0;
      while ((status = archive_read_next_header(reader.a, &entry)) ==
             ARCHIVE_OK) {
        const auto name = QString::fromUtf8(archive_entry_pathname(entry));
        validateRelative(name);
        if (archive_entry_symlink(entry) || archive_entry_hardlink(entry))
          throw Error("E_ARCHIVE_UNSAFE", "Media archive link refused");
        const Json *match = nullptr;
        for (const auto &candidate : expected)
          if (string(candidate, "sourceName") == name) {
            match = &candidate;
            break;
          }
        if (!match) {
          archive_read_data_skip(reader.a);
          continue;
        }
        const auto target = string(*match, "targetName");
        validateRelative(target);
        if (targets.contains(target.toCaseFolded()))
          throw Error("E_ARCHIVE_UNSAFE", "Alias target collision");
        targets.insert(target.toCaseFolded());
        const auto expectedSize = match->value("size", int64_t(0));
        if (expectedSize < 0 || expectedSize > 512LL * 1024 * 1024 ||
            archive_entry_size(entry) != expectedSize)
          throw Error("E_MEDIA_MISMATCH", "Alias media size differs");
        QByteArray data;
        char buffer[128 * 1024];
        la_ssize_t n = 0;
        quint32 crc = 0xffffffff;
        while ((n = archive_read_data(reader.a, buffer, sizeof buffer)) > 0) {
          if (data.size() + n > expectedSize)
            throw Error("E_MEDIA_MISMATCH",
                        "Alias media exceeds expected size");
          for (la_ssize_t i = 0; i < n; ++i) {
            crc ^= static_cast<uchar>(buffer[i]);
            for (int bit = 0; bit < 8; ++bit)
              crc = (crc >> 1) ^
                    (0xedb88320U &
                     static_cast<quint32>(-static_cast<qint32>(crc & 1)));
          }
          data.append(buffer, static_cast<qsizetype>(n));
        }
        if (n < 0 || data.size() != expectedSize ||
            (crc ^ 0xffffffffU) != match->value("crc32", quint32(0)))
          throw Error("E_MEDIA_MISMATCH",
                      "Alias media CRC differs from confirmed identity");
        auto *out = archive_entry_new();
        archive_entry_set_pathname(out, target.toUtf8().constData());
        archive_entry_set_size(out, data.size());
        archive_entry_set_filetype(out, AE_IFREG);
        archive_entry_set_perm(out, 0644);
        if (archive_write_header(writer, out) != ARCHIVE_OK ||
            archive_write_data(writer, data.constData(),
                               static_cast<size_t>(data.size())) !=
                data.size()) {
          archive_entry_free(out);
          throw Error("E_WRITE_DENIED", "Cannot write local media alias");
        }
        archive_entry_free(out);
        found.insert(name);
      }
      if (status != ARCHIVE_EOF ||
          found.size() != static_cast<qsizetype>(expected.size()))
        throw Error("E_MEDIA_MISMATCH", "Required alias entries missing");
    }
    if (archive_write_close(writer) != ARCHIVE_OK)
      throw Error("E_ARCHIVE_CORRUPT", "Cannot finish local media alias");
    archive_write_free(writer);
    writer = nullptr;
    if (QFileInfo::exists(destination))
      throw Error("E_WRITE_CONFLICT", "Alias target already exists");
    if (!QFile::rename(temp, destination))
      throw Error("E_WRITE_DENIED", "Cannot publish local media alias");
  } catch (...) {
    if (writer)
      archive_write_free(writer);
    QFile::remove(temp);
    throw;
  }
}
QList<ArchiveEntry> Archive::inspect(const QString &path, ArchiveLimits limits,
                                     const Json &guard) {
  return walk(path, limits, guard);
}
QStringList Archive::extract(const QString &path, const QString &staging,
                             ArchiveLimits limits, const Json &guard, int strip,
                             const QStringList &include,
                             const QStringList &exclude) {
  inspect(path, limits,
          guard); // No staging payload until every entry has passed.
  QDir().mkpath(staging);
  walk(path, limits, guard, staging, strip, include, exclude);
  QStringList files;
  for (const auto &e : inspect(path, limits, guard)) {
    if (e.directory)
      continue;
    auto parts = e.path.split('/');
    for (int i = 0; i < strip && !parts.isEmpty(); ++i)
      parts.removeFirst();
    const auto name = parts.join('/');
    if (!name.isEmpty() && (include.isEmpty() || matches(name, include)) &&
        !matches(name, exclude))
      files << name;
  }
  return files;
}
} // namespace ac::install
