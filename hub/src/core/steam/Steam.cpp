// SPDX-License-Identifier: GPL-3.0-only
#include "Steam.h"
#include "core/install/Support.h"
#include "core/launch/Launch.h"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QPainter>
#include <QRegularExpression>
#include <QUuid>
#include <QtEndian>
namespace ac::steam {
namespace {
using install::Error;
Node text(const QByteArray &key, const QString &value) {
  Node n;
  n.type = 1;
  n.key = key;
  n.payload = value.toUtf8();
  return n;
}
Node integer(const QByteArray &key, quint32 value) {
  Node n;
  n.type = 2;
  n.key = key;
  n.payload.resize(4);
  qToLittleEndian(value, n.payload.data());
  return n;
}
const Node *field(const Node &n, const QByteArray &name) {
  const Node *result = nullptr;
  for (const auto &c : n.children)
    if (c.key == name) {
      if (result)
        throw Error("E_VDF", "Duplicate shortcut identity field");
      result = &c;
    }
  return result;
}
quint32 idOf(const Node &n) {
  const auto *id = field(n, "appid");
  return id && id->type == 2
             ? qFromLittleEndian<quint32>(id->payload.constData())
             : 0;
}
bool tagged(const Node &n, const QByteArray &tag) {
  const auto *tags = field(n, "tags");
  if (!tags || tags->type != 0)
    return false;
  for (const auto &c : tags->children)
    if (c.type == 1 && c.payload == tag)
      return true;
  return false;
}
void set(Node &n, Node replacement) {
  field(n,
        replacement.key); // Reject duplicate controlled fields before mutation.
  n.raw.clear();
  for (auto &c : n.children)
    if (c.key == replacement.key) {
      c = std::move(replacement);
      return;
    }
  n.children << std::move(replacement);
}
QString quote(const QString &path) {
  if (path.contains('"') || path.contains('\0'))
    throw Error("E_SHORTCUT", "Invalid shortcut path");
  return '"' + QDir::toNativeSeparators(path) + '"';
}
void backup(const QString &source, const QString &folder,
            const QString &label) {
  if (!QFileInfo::exists(source))
    return;
  QDir().mkpath(folder);
  const auto filename =
      label + "-" +
      QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") + "-" +
      QUuid::createUuid().toString(QUuid::Id128) + ".bak";
  install::atomicWrite(install::scopedPath(filename, folder),
                       install::readBytes(source));
  QDir dir(folder);
  const auto files = dir.entryList({label + "-*.bak"}, QDir::Files, QDir::Name);
  for (qsizetype i = 0; i < files.size() - 10; ++i)
    QFile::remove(install::scopedPath(files[i], folder));
}
} // namespace
QStringList accounts(const QString &root) {
  QStringList out;
  QDir userdata(root + "/userdata");
  for (const auto &name : userdata.entryList(QDir::Dirs | QDir::NoDotAndDotDot |
                                             QDir::NoSymLinks)) {
    if (name == "0" || !QRegularExpression("^[0-9]+$").match(name).hasMatch())
      continue;
    if (QFileInfo(userdata.filePath(name + "/config")).isDir())
      out << name;
  }
  return out;
}
Edit edit(const QByteArray &before, const Shortcut &s, bool remove) {
  if (!QRegularExpression("^[a-z0-9]+(?:-[a-z0-9]+)*$")
           .match(s.gameId)
           .hasMatch())
    throw Error("E_SHORTCUT", "Invalid game ID");
  auto d = parse(before);
  Node *root = nullptr;
  for (auto &n : d.roots)
    if (n.key == "shortcuts") {
      if (root || n.type != 0)
        throw Error("E_VDF", "Invalid shortcuts root");
      root = &n;
    }
  if (!root) {
    if (!d.roots.isEmpty())
      throw Error("E_VDF", "Missing shortcuts root");
    Node n;
    n.key = "shortcuts";
    d.roots << n;
    root = &d.roots.last();
  }
  const auto marker = QByteArray("AladdinsCastle:") + s.gameId.toUtf8();
  Node *owned = nullptr;
  for (auto &n : root->children)
    if (n.type == 0 && tagged(n, "AladdinsCastle") && tagged(n, marker)) {
      if (owned)
        throw Error("E_VDF", "Duplicate owned game entries");
      owned = &n;
    }
  Edit r;
  r.ownedFound = owned != nullptr;
  r.id = owned ? idOf(*owned) : s.storedAppId;
  if (owned && !r.id)
    throw Error("E_VDF", "Owned shortcut has an invalid AppId");
  if (!r.id)
    r.id = appId(quote(s.executable), s.title);
  if (remove && !owned) {
    r.bytes = before;
    return r;
  }
  for (const auto &n : root->children)
    if (&n != owned && n.type == 0 && idOf(n) == r.id)
      throw Error("E_APPID_COLLISION",
                  "Shortcut AppId is already used; no changes made");
  if (remove) {
    if (!owned) {
      r.bytes = before;
      return r;
    }
    for (qsizetype i = 0; i < root->children.size(); ++i)
      if (&root->children[i] == owned) {
        root->children.removeAt(i);
        break;
      }
  } else {
    if (!owned) {
      quint32 next = 0;
      for (const auto &n : root->children) {
        bool ok = false;
        const auto num = n.key.toUInt(&ok);
        if (ok) {
          if (num == 0xffffffffu)
            throw Error("E_VDF", "Shortcut index overflow");
          next = qMax(next, num + 1);
        }
      }
      Node n;
      n.key = QByteArray::number(next);
      root->children << n;
      owned = &root->children.last();
    }
    set(*owned, integer("appid", r.id));
    set(*owned, text("AppName", s.title));
    set(*owned, text("Exe", quote(s.executable)));
    set(*owned, text("StartDir", quote(s.startDir)));
    set(*owned, text("LaunchOptions", "--launch " + s.gameId));
    for (const auto *key : {"icon", "ShortcutPath", "DevkitGameID"})
      set(*owned, text(key, {}));
    for (const auto *key : {"IsHidden", "Devkit", "DevkitOverrideAppID"})
      set(*owned, integer(key, 0));
    for (const auto *key : {"AllowDesktopConfig", "AllowOverlay"})
      set(*owned, integer(key, 1));
    set(*owned, integer("OpenVR", s.vr ? 1u : 0u));
    set(*owned, integer("LastPlayTime",
                        static_cast<quint32>(qBound(qint64(0), s.lastPlayed,
                                                    qint64(0xffffffffu)))));
    Node tags;
    if (const auto *old = field(*owned, "tags"))
      tags = *old;
    tags.type = 0;
    tags.key = "tags";
    tags.raw.clear();
    for (const auto &tag : {QByteArray("AladdinsCastle"), marker}) {
      bool present = false;
      for (const auto &c : tags.children)
        if (c.type == 1 && c.payload == tag)
          present = true;
      if (!present) {
        quint32 next = 0;
        for (const auto &c : tags.children) {
          bool ok = false;
          auto n = c.key.toUInt(&ok);
          if (ok) {
            if (n == 0xffffffffu)
              throw Error("E_VDF", "Tag index overflow");
            next = qMax(next, n + 1);
          }
        }
        tags.children << text(QByteArray::number(next), QString::fromUtf8(tag));
      }
    }
    set(*owned, tags);
  }
  root->raw.clear();
  r.bytes = serialize(d);
  r.changed = r.bytes != before;
  return r;
}
Preview preview(const WriteRequest &r) {
  if (!QDir::isAbsolutePath(r.steamRoot) || !QDir::isAbsolutePath(r.userRoot))
    throw Error("E_PATH",
                "Steam and Hub user roots must be explicit absolute paths");
  if (r.accountId == "0" ||
      !QRegularExpression("^[0-9]+$").match(r.accountId).hasMatch())
    throw Error("E_STEAM_ACCOUNT", "Choose an explicit Steam userdata account");
  Preview p;
  p.target = install::scopedPath(
      "userdata/" + r.accountId + "/config/shortcuts.vdf", r.steamRoot);
  if (!QFileInfo(QFileInfo(p.target).absolutePath()).isDir())
    throw Error("E_STEAM_ACCOUNT",
                "Selected userdata config folder does not exist");
  p.backupFolder =
      install::scopedPath("backups/steam/" + r.accountId, r.userRoot);
  const auto before =
      QFileInfo::exists(p.target) ? install::readBytes(p.target) : QByteArray();
  p.edit = edit(before, r.shortcut, r.remove);
  for (const auto &name : artNames(p.edit.id))
    p.artPaths << install::scopedPath("grid/" + name,
                                      QFileInfo(p.target).absolutePath());
  p.json = {{"target", p.target.toStdString()},
            {"account", r.accountId.toStdString()},
            {"operation", r.remove ? "remove" : "upsert"},
            {"appid", p.edit.id},
            {"title", r.shortcut.title.toStdString()},
            {"exe", quote(r.shortcut.executable).toStdString()},
            {"launchOptions", ("--launch " + r.shortcut.gameId).toStdString()},
            {"OpenVR", r.shortcut.vr ? 1 : 0},
            {"backupFolder", p.backupFolder.toStdString()},
            {"beforeSha256", install::sha256(before).toStdString()},
            {"afterSha256", install::sha256(p.edit.bytes).toStdString()},
            {"art", Json::array()}};
  const QStringList roles{"header", "capsule", "hero", "logo"};
  p.json["StartDir"] = quote(r.shortcut.startDir).toStdString();
  p.json["launchId"] = QString::number(launchId(p.edit.id)).toStdString();
  const auto receiptPath = install::scopedPath(
      "state/steam/" + r.accountId + "/" + QString::number(p.edit.id) + ".json",
      r.userRoot);
  const auto receiptBytes = QFileInfo::exists(receiptPath)
                                ? install::readBytes(receiptPath)
                                : QByteArray();
  const auto receipt = receiptBytes.isEmpty()
                           ? Json::object()
                           : Json::parse(receiptBytes.toStdString());
  p.json["artReceiptSha256"] = install::sha256(receiptBytes).toStdString();
  p.json["overwriteCustomArt"] = r.overwriteCustomArt;
  for (qsizetype i = 0; i < p.artPaths.size(); ++i) {
    const auto existing = QFileInfo::exists(p.artPaths[i])
                              ? install::readBytes(p.artPaths[i])
                              : QByteArray();
    const auto digest = install::sha256(existing).toStdString();
    const auto name = QFileInfo(p.artPaths[i]).fileName().toStdString();
    const auto owned = receipt.contains(name) && receipt[name] == digest;
    const bool exists = QFileInfo::exists(p.artPaths[i]);
    const auto image = r.art.value(roles[i]);
    const QString action =
        r.remove ? (p.edit.ownedFound && exists && owned ? "remove" : "keep")
                 : (image.isNull() ? (exists ? "keep" : "none")
                                   : (exists && !owned && !r.overwriteCustomArt
                                          ? "keep-custom"
                                          : "write"));
    QByteArray pixels;
    if (!image.isNull())
      pixels = QByteArray(reinterpret_cast<const char *>(image.constBits()),
                          image.sizeInBytes());
    p.json["art"].push_back(
        {{"path", p.artPaths[i].toStdString()},
         {"beforeSha256", digest},
         {"action", action.toStdString()},
         {"pixelSha256", install::sha256(pixels).toStdString()},
         {"width", image.width()},
         {"height", image.height()}});
  }
  return p;
}
WriteResult apply(const WriteRequest &r, const Preview &approved,
                  bool permission, const std::function<bool()> &probe) {
  if (!permission)
    throw Error("E_APPROVAL_REQUIRED",
                "Owner approval is required before writing Steam files");
  const auto running = probe ? probe : [] {
    return launch::processRunning(
        {"steam.exe", "steamwebhelper.exe", "steam", "steamwebhelper"});
  };
  if (running())
    throw Error("E_STEAM_RUNNING",
                "Close Steam to update its library. Nothing has changed");
  const auto locks = install::scopedPath("locks", r.userRoot);
  QDir().mkpath(locks);
  QLockFile lock(install::scopedPath("steam-" + r.accountId + ".lock", locks));
  if (!lock.tryLock(0))
    throw Error("E_BUSY", "Another Hub Steam update is running");
  const auto p = preview(r);
  if (p.json != approved.json || p.edit.bytes != approved.edit.bytes)
    throw Error("E_PREVIEW_CHANGED",
                "Steam library or request changed; review a fresh preview");
  WriteResult result{p.edit.id, p.edit.changed, {}};
  if (r.remove && !p.edit.ownedFound)
    return result;
  const auto receiptPath = install::scopedPath(
      "state/steam/" + r.accountId + "/" + QString::number(p.edit.id) + ".json",
      r.userRoot);
  Json receipt = Json::object();
  if (QFileInfo::exists(receiptPath))
    receipt = Json::parse(install::readBytes(receiptPath).toStdString());
  backup(p.target, p.backupFolder, "shortcuts");
  if (running())
    throw Error("E_STEAM_RUNNING", "Steam started; no library change made");
  if (p.edit.changed)
    install::atomicWrite(p.target, p.edit.bytes);
  const QStringList roles{"header", "capsule", "hero", "logo"};
  const QList<QSize> sizes{{920, 430}, {600, 900}, {1920, 620}, {1280, 720}};
  for (qsizetype i = 0; i < p.artPaths.size(); ++i)
    try {
      if (running())
        throw Error("E_STEAM_RUNNING",
                    "Steam started; remaining art unchanged");
      const auto &path = p.artPaths[i];
      if (r.remove) {
        if (!QFileInfo::exists(path))
          continue;
        const auto name = QFileInfo(path).fileName().toStdString();
        if (!receipt.contains(name) ||
            receipt[name] != install::hashFile(path).toStdString()) {
          result.warnings << "Kept grid art that is unowned or changed: " +
                                 QFileInfo(path).fileName();
          continue;
        }
        backup(path, p.backupFolder,
               "art-" + QFileInfo(path).completeBaseName());
        if (!QFile::remove(path))
          throw Error("E_ART", "Could not remove owned grid art");
        receipt.erase(name);
      } else if (r.art.contains(roles[i]) && !r.art[roles[i]].isNull()) {
        if (p.json["art"][static_cast<size_t>(i)]["action"] == "keep-custom") {
          result.warnings << "Kept existing custom grid art: " +
                                 QFileInfo(path).fileName();
          continue;
        }
        backup(path, p.backupFolder,
               "art-" + QFileInfo(path).completeBaseName());
        auto image = r.art[roles[i]].scaled(sizes[i], Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
        QImage canvas(sizes[i], QImage::Format_ARGB32_Premultiplied);
        canvas.fill(i == 3 ? Qt::transparent : QColor("#101014"));
        QPainter painter(&canvas);
        painter.drawImage((canvas.width() - image.width()) / 2,
                          (canvas.height() - image.height()) / 2, image);
        painter.end();
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!canvas.save(&buffer, "PNG"))
          throw Error("E_ART", "Could not encode grid art");
        QDir().mkpath(QFileInfo(path).absolutePath());
        install::atomicWrite(path, bytes);
        receipt[QFileInfo(path).fileName().toStdString()] =
            install::sha256(bytes).toStdString();
        QDir().mkpath(QFileInfo(receiptPath).absolutePath());
        install::atomicWrite(receiptPath,
                             QByteArray::fromStdString(receipt.dump(2)));
      }
    } catch (const std::exception &e) {
      result.warnings << QString::fromUtf8(e.what());
    }
  if (r.remove && QFileInfo::exists(receiptPath))
    install::atomicWrite(receiptPath,
                         QByteArray::fromStdString(receipt.dump(2)));
  return result;
}
QMap<QString, QImage> fallbackArt(const QString &title,
                                  const QString &manufacturer) {
  QMap<QString, QImage> result;
  const QMap<QString, QSize> sizes{{"header", {920, 430}},
                                   {"capsule", {600, 900}},
                                   {"hero", {1920, 620}},
                                   {"logo", {1280, 720}}};
  for (auto it = sizes.begin(); it != sizes.end(); ++it) {
    QImage image(it.value(), QImage::Format_ARGB32_Premultiplied);
    image.fill(it.key() == "logo" ? Qt::transparent : QColor("#101014"));
    QPainter p(&image);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont font;
    font.setPixelSize(it.key() == "capsule" ? 42 : 64);
    font.setBold(true);
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(image.rect().adjusted(40, 40, -40, -100),
               Qt::AlignCenter | Qt::TextWordWrap, title);
    if (it.key() != "logo") {
      font.setPixelSize(28);
      font.setBold(false);
      p.setFont(font);
      p.setPen(QColor("#dd6600"));
      p.drawText(image.rect().adjusted(40, image.height() - 90, -40, -20),
                 Qt::AlignCenter, manufacturer);
    }
    p.end();
    result[it.key()] = image;
  }
  return result;
}
} // namespace ac::steam
