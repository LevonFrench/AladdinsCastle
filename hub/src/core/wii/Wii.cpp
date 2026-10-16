// SPDX-License-Identifier: GPL-3.0-only
#include "Wii.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
namespace ac::wii {
namespace {
struct Game {
  const char *id, *catalogId, *title;
  int year;
  QStringList ids;
  const char *hold, *route;
};
const QList<Game> &table() {
  static const QList<Game> games{
      {"umbrella", "wii-resident-evil-umbrella-chronicles", "Resident Evil: The Umbrella Chronicles", 2007, {"RBUE08", "RBUP08", "RBUJ08", "RBUK08"}, "split", "working"},
      {"darkside", "wii-resident-evil-darkside-chronicles", "Resident Evil: The Darkside Chronicles", 2009, {"SBDE08", "SBDP08", "SBDJ08", "SBDK08"}, "split", "imperfect"},
      {"overkill", "wii-house-of-the-dead-overkill", "The House of the Dead: Overkill", 2009, {"RHOE8P", "RHOP8P", "RHOJ8P"}, "split", "unknown"},
      {"gunblade", "wii-gunblade-ny-la-machineguns", "Gunblade NY & L.A. Machineguns: Arcade Hits Pack", 2010, {"SQDE8P", "SQDP8P"}, "docked", "unknown"},
      {"sinpunish", "wii-sin-and-punishment-star-successor", "Sin & Punishment: Star Successor", 2009, {"R2VE01", "R2VP01", "R2VJ01"}, "split", "unknown"},
      {"deadspace", "wii-dead-space-extraction", "Dead Space: Extraction", 2009, {"RZJE69", "RZJP69"}, "split", "unknown"},
      {"crossbow", "wii-links-crossbow-training", "Link's Crossbow Training", 2007, {"RZPE01", "RZPP01", "RZPJ01", "RZPK01"}, "docked", "unknown"},
  };
  return games;
}
constexpr quint32 wiiMagic = 0x5D1C9EA3;
QByteArray readAt(QFile &file, qint64 offset, qint64 size) {
  if (!file.seek(offset))
    return {};
  return file.read(size);
}
bool printable(const QByteArray &id) {
  if (id.size() != 6)
    return false;
  for (const char c : id)
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
      return false;
  return true;
}
// The disc header (game id, then the Wii magic at 0x18) found at an offset of the file.
QString headerAt(QFile &file, qint64 offset) {
  const auto header = readAt(file, offset, 0x20);
  if (header.size() < 0x20 || qFromBigEndian<quint32>(header.constData() + 0x18) != wiiMagic)
    return {};
  const auto id = header.left(6);
  return printable(id) ? QString::fromLatin1(id) : QString();
}
} // namespace
QVariantList games() {
  QVariantList result;
  for (const auto &game : table())
    result << QVariantMap{{"id", QString::fromLatin1(game.id)}, {"catalogId", QString::fromLatin1(game.catalogId)},
                          {"title", QString::fromUtf8(game.title)}, {"year", game.year}, {"hold", QString::fromLatin1(game.hold)},
                          {"route", QString::fromLatin1(game.route)}, {"ids", game.ids}};
  return result;
}
QString discId(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  const auto start = readAt(file, 0, 16);
  if (start.size() < 16)
    return {};
  // WBFS: the disc header is copied one host sector in (2^byte 8 bytes).
  if (start.startsWith("WBFS")) {
    const int shift = quint8(start[8]);
    return shift >= 9 && shift <= 16 ? headerAt(file, qint64(1) << shift) : QString();
  }
  // RVZ and WIA: a 0x48-byte file header, then the disc part with its copy of the header 0x10 in.
  if (start.startsWith(QByteArray("RVZ\x01", 4)) || start.startsWith(QByteArray("WIA\x01", 4)))
    return headerAt(file, 0x58);
  // A plain image: the header is at the start.
  return headerAt(file, 0);
}
QString gameFor(const QString &id) {
  if (id.size() != 6)
    return {};
  for (const auto &game : table())
    for (const auto &known : game.ids)
      if (id.left(3) == known.left(3) && id.mid(4) == known.mid(4))
        return QString::fromLatin1(game.id);
  return {};
}
QVariantMap identify(const QString &path) {
  const QFileInfo info(path);
  const auto id = discId(info.absoluteFilePath());
  QVariantMap disc{{"file", QDir::fromNativeSeparators(info.absoluteFilePath())}, {"id", id}, {"game", QString()}, {"title", QString()}, {"verified", !id.isEmpty()}};
  const auto game = gameFor(id);
  for (const auto &row : table())
    if (game == QLatin1String(row.id)) {
      disc["game"] = game;
      disc["title"] = QString::fromUtf8(row.title);
    }
  return disc;
}
QVariantList discs(const QString &folder, std::atomic_bool &cancel) {
  QVariantList found;
  if (folder.isEmpty())
    return found;
  for (const auto &entry : QDir(folder).entryInfoList({"*.iso", "*.wbfs", "*.rvz", "*.wia"}, QDir::Files, QDir::Name)) {
    if (cancel)
      break;
    if (entry.isSymLink() || entry.isJunction())
      continue;
    found << identify(entry.absoluteFilePath());
  }
  return found;
}
QStringList missing(const QString &game, const QString &id, const QString &executable) {
  QStringList result;
  const Game *row = nullptr;
  for (const auto &candidate : table())
    if (game == QLatin1String(candidate.id))
      row = &candidate;
  if (!row)
    return result;
  const QDir user(QFileInfo(executable).absolutePath() + "/User");
  if (!id.isEmpty() && !QFileInfo(user.filePath("GameSettingsVR/" + id + ".ini")).isFile())
    result << QString("VR settings of its own (GameSettingsVR/%1.ini), so it starts with DolphinXR's default VR screen").arg(id);
  if (QLatin1String(row->hold) == QLatin1String("docked"))
    result << "docked controls: it is meant to be held with both hands on one gun, and the build is set for a gun in one hand";
  return result;
}
QVariantMap launchPlan(const QString &file, const QString &executable) {
  if (executable.isEmpty() || !QFileInfo(executable).isFile())
    throw install::Error("E_WII_SETUP", "The DolphinXR build is not set up. Choose its Dolphin.exe in Set-up.");
  if (file.isEmpty() || !QFileInfo(file).isFile())
    throw install::Error("E_WII_DISC", "The disc image is no longer where it was. Use Rescan in Set-up.");
  const QFileInfo program(executable);
  const auto native = [](const QString &path) { return QDir::toNativeSeparators(path); };
  return {{"executable", native(program.absoluteFilePath())},
          {"cwd", native(program.absolutePath())},
          {"args", QStringList{"--user", native(program.absolutePath() + "/User"), "-C", "Graphics.VR.EnableOpenXR=True", "-C",
                               "Graphics.VR.ResolutionScale=1.0", "-C", "Graphics.VR.EagerHeartbeat=False", "-C",
                               "Graphics.Stereoscopy.StereoMode=0", "-C", "Dolphin.Interface.ConfirmStop=False", "--exec",
                               native(QFileInfo(file).absoluteFilePath())}}};
}
} // namespace ac::wii
