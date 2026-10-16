// SPDX-License-Identifier: GPL-3.0-only
#include "Library.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/model2/Model2.h"
#include "core/model3/Model3.h"
#include "core/ps2/Ps2.h"
#include "core/scan/Scan.h"
#include "core/wii/Wii.h"
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
namespace ac::library {
namespace {
QString regionOfName(const QString &name) {
  if (name.contains("(USA") || name.contains("NTSC-U") || name.contains("(US)"))
    return "USA";
  if (name.contains("(World"))
    return "World";
  if (name.contains("(Europe") || name.contains("PAL") || name.contains("(EU"))
    return "Europe";
  if (name.contains("(Japan") || name.contains("NTSC-J"))
    return "Japan";
  return {};
}
QString wiiRegion(const QString &id) {
  const auto c = id.size() > 3 ? id[3] : QChar();
  return c == 'E' ? "USA" : c == 'P' ? "Europe" : c == 'J' ? "Japan" : c == 'K' ? "Korea" : QString();
}
QString ps2Region(const QString &serial) {
  const auto s = serial.toUpper();
  if (s.startsWith("SLUS") || s.startsWith("SCUS"))
    return "USA";
  if (s.startsWith("SLES") || s.startsWith("SCES") || s.startsWith("SCED"))
    return "Europe";
  if (s.startsWith("SLPS") || s.startsWith("SLPM") || s.startsWith("SCPS") || s.startsWith("SCAJ"))
    return "Japan";
  return {};
}
int regionRank(const QString &region) {
  static const QStringList order{"USA", "World", "Europe", "Japan"};
  const auto i = order.indexOf(region);
  return i < 0 ? order.size() : i;
}
int formatRank(const QString &system, const QString &file) {
  const auto ext = QFileInfo(file).suffix().toLower();
  static const QHash<QString, QStringList> order{{"ps2", {"iso", "bin", "chd", "cso", "gz"}},
                                                 {"wii", {"iso", "wbfs", "rvz", "wia"}},
                                                 {"dreamcast", {"gdi", "chd", "cdi", "cue"}}};
  const auto i = order.value(system).indexOf(ext);
  return i < 0 ? 9 : i;
}
// A folder whose name says the system, for two zips of the same set name in a mixed collection.
bool hinted(const QString &system, const QString &file) {
  const auto path = QDir::fromNativeSeparators(file).toLower();
  if (system == "model3")
    return path.contains("model3") || path.contains("model 3") || path.contains("supermodel");
  if (system == "model2")
    return path.contains("model2") || path.contains("model 2");
  return false;
}
auto rank(const QString &system, const QVariantMap &copy) {
  const auto file = copy.value("file").toString();
  return std::make_tuple(copy.value("verified").toBool() ? 0 : 1, copy.value("profile").toBool() ? 0 : 1,
                         hinted(system, file) ? 0 : 1, regionRank(copy.value("region").toString()), formatRank(system, file),
                         file.size(), file);
}
} // namespace

QString rule() {
  return "Read from the disc itself first, then the release with a VR profile, then USA, World, Europe, "
         "Japan, then the plainest format, then the shortest path.";
}
QVariantMap scan(const QStringList &roots, const Progress &progress, std::atomic_bool &cancel, int depth) {
  QElapsedTimer clock;
  clock.start();
  QStringList cleanRoots;
  for (const auto &root : roots)
    if (!root.trimmed().isEmpty() && QFileInfo(root).isDir())
      cleanRoots << QDir::cleanPath(QDir::fromNativeSeparators(root));
  if (progress)
    progress("Looking through your folders");
  const auto files = scan::walk(cleanRoots, {"zip", "iso", "wbfs", "rvz", "wia", "bin", "chd", "cso", "gz", "gdi", "cdi", "cue", "vpx"}, depth, cancel);
  QSet<QString> model3Sets, model2Sets;
  for (const auto &game : model3::games())
    model3Sets << game.toMap().value("id").toString();
  for (const auto &game : model2::games())
    model2Sets << game.toMap().value("id").toString();
  QHash<QString, QHash<QString, QVariantList>> copies; // system -> game -> copies
  QHash<QString, int> other;
  QHash<QString, int> tables;
  int unknownDiscs = 0, unknownArchives = 0;
  // A .bin that is a track of a .cue or .gdi is read through that sheet, not on its own.
  QSet<QString> tracks;
  for (const auto &file : files) {
    const auto ext = QFileInfo(file).suffix().toLower();
    if (ext != "cue" && ext != "gdi")
      continue;
    QFile sheet(file);
    if (sheet.size() > 64 * 1024 || !sheet.open(QIODevice::ReadOnly))
      continue;
    for (const auto &line : QString::fromUtf8(sheet.readAll()).split('\n')) {
      const auto quoted = line.section('"', 1, 1);
      const auto words = line.simplified().split(' ');
      const auto name = !quoted.isEmpty() ? quoted : (ext == "gdi" && words.size() >= 5 ? words[4] : QString());
      if (!name.isEmpty())
        tracks.insert(QDir::cleanPath(QFileInfo(file).absoluteDir().filePath(name)).toLower());
    }
  }
  const auto add = [&](const QString &system, QVariantMap copy) {
    copies[system][copy.value("game").toString()] << copy;
  };
  int done = 0;
  for (const auto &file : files) {
    if (cancel)
      break;
    if (progress && ++done % 200 == 0)
      progress(QString("Read %1 of %2 files").arg(done).arg(files.size()));
    const QFileInfo info(file);
    const auto ext = info.suffix().toLower();
    const auto path = QDir::fromNativeSeparators(info.absoluteFilePath());
    if (ext == "vpx") {
      ++tables[QDir::fromNativeSeparators(info.absolutePath())];
      continue;
    }
    if (ext == "zip") {
      // Arcade sets are known by their set name; Set-up checks what is inside by CRC.
      const auto set = info.completeBaseName().toLower();
      bool known = false;
      if (model3Sets.contains(set)) {
        add("model3", {{"file", path}, {"game", set}, {"identity", set}, {"verified", false}});
        known = true;
      }
      if (model2Sets.contains(set)) {
        add("model2", {{"file", path}, {"game", set}, {"identity", set}, {"verified", false}});
        known = true;
      }
      if (!known)
        ++unknownArchives;
      continue;
    }
    if (ext == "bin" && tracks.contains(path.toLower()))
      continue;
    // Disc images: the Wii's header first (cheapest), then a Dreamcast disc's own number, then a PS2 serial.
    if (ext == "iso" || ext == "wbfs" || ext == "rvz" || ext == "wia") {
      const auto disc = wii::identify(path);
      const auto id = disc.value("id").toString();
      if (!id.isEmpty()) {
        if (disc.value("game").toString().isEmpty())
          ++other["wii"];
        else {
          auto copy = disc;
          copy["identity"] = id;
          copy["region"] = wiiRegion(id);
          add("wii", copy);
        }
        continue;
      }
      if (ext != "iso") {
        ++unknownDiscs;
        continue;
      }
    }
    if (ext == "gdi" || ext == "cdi" || ext == "cue" || ext == "chd") {
      const auto disc = dreamcast::identify(path, cancel);
      const auto product = disc.value("product").toString();
      if (!disc.value("game").toString().isEmpty() && (!product.isEmpty() || ext != "chd")) {
        auto copy = disc;
        copy["identity"] = product;
        if (copy.value("region").toString() == "US")
          copy["region"] = "USA";
        else if (copy.value("region").toString() == "PAL")
          copy["region"] = "Europe";
        if (copy.value("region").toString().isEmpty())
          copy["region"] = regionOfName(info.fileName());
        add("dreamcast", copy);
        continue;
      }
      if (!product.isEmpty()) {
        ++other["dreamcast"];
        continue;
      }
      if (ext != "chd") {
        ++unknownDiscs;
        continue;
      }
    }
    if (ext == "iso" || ext == "bin" || ext == "chd" || ext == "cso" || ext == "gz") {
      const auto disc = ps2::identify(path, cancel);
      const auto serial = disc.value("serial").toString();
      if (!disc.value("game").toString().isEmpty() && (disc.value("verified").toBool() || ext == "cso" || ext == "gz")) {
        auto copy = disc;
        copy["identity"] = serial;
        copy["region"] = serial.isEmpty() ? regionOfName(info.fileName()) : ps2Region(serial);
        add("ps2", copy);
        continue;
      }
      if (!serial.isEmpty()) {
        ++other["ps2"];
        continue;
      }
    }
    ++unknownDiscs;
  }
  QVariantMap systems;
  for (auto it = copies.cbegin(); it != copies.cend(); ++it) {
    QVariantMap byGame;
    for (auto game = it.value().cbegin(); game != it.value().cend(); ++game) {
      auto list = game.value();
      std::sort(list.begin(), list.end(), [&](const QVariant &a, const QVariant &b) { return rank(it.key(), a.toMap()) < rank(it.key(), b.toMap()); });
      byGame.insert(game.key(), list);
    }
    systems.insert(it.key(), QVariantMap{{"copies", byGame}, {"games", byGame.size()}, {"other", other.value(it.key())}});
  }
  for (auto it = other.cbegin(); it != other.cend(); ++it)
    if (!systems.contains(it.key()))
      systems.insert(it.key(), QVariantMap{{"copies", QVariantMap{}}, {"games", 0}, {"other", it.value()}});
  QVariantList folders;
  for (auto it = tables.cbegin(); it != tables.cend(); ++it)
    folders << QVariantMap{{"folder", it.key()}, {"tables", it.value()}};
  std::sort(folders.begin(), folders.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("tables").toInt() > b.toMap().value("tables").toInt(); });
  return {{"roots", cleanRoots}, {"scanned", QDateTime::currentDateTime().toString("d MMMM yyyy, HH:mm")}, {"seconds", clock.elapsed() / 1000.0},
          {"files", files.size()}, {"depth", depth}, {"cancelled", bool(cancel)}, {"systems", systems}, {"pinballFolders", folders},
          {"unrecognised", QVariantMap{{"discs", unknownDiscs}, {"archives", unknownArchives}}}};
}
QVariantList copies(const QVariantMap &library, const QString &system, const QString &game) {
  return library.value("systems").toMap().value(system).toMap().value("copies").toMap().value(game).toList();
}
QVariantMap chosen(const QVariantMap &library, const QString &system, const QString &game, const QVariantMap &choices) {
  const auto list = copies(library, system, game);
  const auto wanted = choices.value(system + "/" + game).toString();
  if (!wanted.isEmpty())
    for (const auto &value : list)
      if (value.toMap().value("file").toString() == wanted && QFileInfo(wanted).isFile())
        return value.toMap();
  for (const auto &value : list)
    if (QFileInfo(value.toMap().value("file").toString()).isFile())
      return value.toMap();
  return {};
}
} // namespace ac::library
