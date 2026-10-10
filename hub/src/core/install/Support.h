// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Install.h"
#include <QMap>
#include <QSet>
namespace ac::install {
QString string(const Json &j, const char *key, const QString &fallback = {});
QByteArray readBytes(const QString &path);
void atomicCopy(const QString &source, const QString &destination);
void atomicWrite(const QString &path, const QByteArray &bytes);
void durableAppend(const QString &path, const Json &record);
void validateRelative(const QString &path);
QString scopedPath(const QString &path, const QString &root);
QString stateBase(const Request &r);
Json dotted(const Json &value, const QString &path);
QString expand(const QString &text, const QMap<QString, QString> &vars,
               bool path = false);
Json expandJson(const Json &j, const QMap<QString, QString> &vars,
                const QString &key = {});
struct ConfigEdit {
  QByteArray bytes;
  Json keys = Json::array();
  bool reserialized = false;
};
ConfigEdit editConfig(const QByteArray &before, const QString &format,
                      const Json &entries, const Json &settings,
                      const Json &profile);
QByteArray undoConfig(const QByteArray &current, const QString &format,
                      const Json &keys, bool &kept);
struct ArchiveLimits {
  qint64 maxExpanded = 0;
  int maxEntries = 50000;
};
struct ArchiveEntry {
  QString path;
  qint64 bytes = 0;
  bool directory = false;
};
class Archive {
public:
  static void makeUserMediaAlias(const Json &requirements, const QString &destination);
  static QList<ArchiveEntry> inspect(const QString &path,
                                     ArchiveLimits limits = {},
                                     const Json &guard = Json::object());
  static QStringList extract(const QString &path, const QString &staging,
                             ArchiveLimits limits = {},
                             const Json &guard = Json::object(), int strip = 0,
                             const QStringList &include = {},
                             const QStringList &exclude = {});
};
Json loadContentGuard(const QString &catalogRoot);
void contentGuard(const QString &name, const Json &guard);
bool verifyPe64(const QString &path);
} // namespace ac::install
