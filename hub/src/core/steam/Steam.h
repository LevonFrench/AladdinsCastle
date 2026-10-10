// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/catalog/GameRecord.h"
#include <QByteArray>
#include <QImage>
#include <QMap>
#include <functional>
namespace ac::steam {
struct Node {
  quint8 type = 0;
  QByteArray key, payload, raw;
  QVector<Node> children;
  quint8 terminator = 8;
};
struct Document {
  QVector<Node> roots;
  quint8 terminator = 8;
  bool originalEmpty = false;
};
Document parse(const QByteArray &bytes);
QByteArray serialize(const Document &document);
quint32 appId(const QString &storedQuotedExe, const QString &title);
quint64 launchId(quint32 id);
QStringList artNames(quint32 id);
QStringList accounts(const QString &steamRoot);
struct Shortcut {
  QString gameId, title, executable, startDir;
  bool vr = false;
  quint32 storedAppId = 0;
  qint64 lastPlayed = 0;
  QString variantId; // Required for upserts and variant-specific removal.
};
struct Edit {
  QByteArray bytes;
  quint32 id = 0;
  bool changed = false, ownedFound = false;
};
Edit edit(const QByteArray &before, const Shortcut &shortcut,
          bool remove = false);
struct WriteRequest {
  QString steamRoot, accountId, userRoot;
  Shortcut shortcut;
  bool remove = false;
  bool overwriteCustomArt = false;
  QMap<QString, QImage>
      art; // header, capsule, hero, logo; F supplies local art.
};
struct Preview {
  QString target, backupFolder;
  QStringList artPaths;
  Edit edit;
  Json json;
};
Preview preview(const WriteRequest &request);
struct WriteResult {
  quint32 id = 0;
  bool changed = false;
  QStringList warnings;
};
// Present preview and obtain owner approval; no default permission.
WriteResult apply(const WriteRequest &request, const Preview &approvedPreview,
                  bool ownerApproved,
                  const std::function<bool()> &steamRunning = {});
QMap<QString, QImage> fallbackArt(const QString &title,
                                  const QString &manufacturer);
} // namespace ac::steam
