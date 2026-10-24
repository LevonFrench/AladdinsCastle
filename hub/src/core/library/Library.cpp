// SPDX-License-Identifier: GPL-3.0-only
#include "Library.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/model2/Model2.h"
#include "core/model3/Model3.h"
#include "core/ps2/Ps2.h"
#include "core/scan/Scan.h"
#include "core/scan/DiscIndex.h"
#include "core/wii/Wii.h"
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QSet>
#include <algorithm>
namespace ac::library {
namespace {
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
namespace {
// Raised whenever what a file is read as changes, so every saved reading is read again.
constexpr int kIndexVersion = 2;
// What one file is: the copies it gives (system and copy), the system it is another disc of, a kind the Hub
// reads but does not know ("disc" or "archive"), or a pinball table. Kept in the index to be used again.
QVariantMap classify(const QString &path, const QString &ext, const QSet<QString> &model3Sets, const QSet<QString> &model2Sets,
                     std::atomic_bool &cancel) {
  const auto copy = [](const QString &system, const QVariantMap &found) {
    return QVariantMap{{"copies", QVariantList{QVariantMap{{"system", system}, {"copy", found}}}}};
  };
  if (ext == "vpx")
    return {{"table", true}};
  if (ext == "zip") {
    // Arcade sets are known by their set name; Set-up checks what is inside by CRC.
    const auto set = QFileInfo(path).completeBaseName().toLower();
    QVariantList found;
    if (model3Sets.contains(set))
      found << QVariantMap{{"system", "model3"}, {"copy", QVariantMap{{"file", path}, {"game", set}, {"identity", set}, {"verified", false}}}};
    if (model2Sets.contains(set))
      found << QVariantMap{{"system", "model2"}, {"copy", QVariantMap{{"file", path}, {"game", set}, {"identity", set}, {"verified", false}}}};
    else for(const auto &value:model2::games()){
      const auto game=value.toMap();if(game.value("file").toString()==set)
        found<<QVariantMap{{"system","model2"},{"copy",QVariantMap{{"file",path},{"game",game.value("id")},{"identity",set},{"verified",false}}}};
    }
    return found.isEmpty() ? QVariantMap{{"unknown", "archive"}} : QVariantMap{{"copies", found}};
  }
  const auto read=scan::Scanner::inspect(path,cancel);
  if(!read.error.isEmpty()||read.identity.isEmpty())return {{"unknown","disc"},{"retry",read.error.contains("budget")||read.error.contains("time")}};
  const auto row=scan::DiscIndex::match(read.system,read.identity);
  if(row.isEmpty())return read.system.isEmpty()?QVariantMap{{"unknown","disc"}}:QVariantMap{{"other",read.system}};
  QVariantMap found{{"file",path},{"identity",read.identity},{"game",row.value("game")},{"title",row.value("title")},
                    {"region",row.value("region")},{"verified",true},{"profile",row.value("profile",false)}};
  if(read.system=="ps1"||read.system=="ps2")found["serial"]=read.identity;
  else if(read.system=="dreamcast")found["product"]=read.identity;
  else if(read.system=="wii"||read.system=="gc")found["id"]=read.identity;
  return copy(read.system,found);
}
// The games the Hub knows, as a short stamp: a saved reading made against other lists is read again.
QString listsStamp() {
  QByteArray all = QByteArray::number(kIndexVersion);
  for (const auto &list : {model3::games(), model2::games(), ps2::games(), wii::games(), dreamcast::games()})
    for (const auto &game : list)
      all += '|' + game.toMap().value("id").toString().toUtf8();
  all+=scan::DiscIndex::fingerprint().toUtf8();
  return QString::fromLatin1(QCryptographicHash::hash(all, QCryptographicHash::Sha1).toHex().left(16));
}
} // namespace

QVariantMap scan(const QStringList &roots, const Progress &progress, std::atomic_bool &cancel, int depth, QVariantMap *index) {
  QElapsedTimer clock;
  clock.start();
  QStringList cleanRoots;
  for (const auto &root : roots)
    if (!root.trimmed().isEmpty() && QFileInfo(root).isDir())
      cleanRoots << QDir::cleanPath(QDir::fromNativeSeparators(root));
  if (progress)
    progress("Looking through your folders");
  const auto files = scan::walk(cleanRoots, {"zip", "iso", "wbfs", "rvz", "wia", "bin", "chd", "cso", "gz", "gdi", "cdi", "cue", "m3u", "gcz", "gcm", "wad", "nkit", "pbp", "img", "vpx"}, depth, cancel);
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
  // The last reading of each file, used again while its size and time are unchanged; made against the same
  // lists of games, or not used at all.
  const auto stamp = listsStamp();
  const auto before = index && index->value("stamp").toString() == stamp ? index->value("files").toMap() : QVariantMap{};
  QVariantMap after;
  int done = 0, reused = 0;
  for (const auto &file : files) {
    if (cancel)
      break;
    if (progress && ++done % 200 == 0)
      progress(QString("Read %1 of %2 files").arg(done).arg(files.size()));
    const QFileInfo info(file);
    const auto ext = info.suffix().toLower();
    const auto path = QDir::fromNativeSeparators(info.absoluteFilePath());
    if (ext == "bin" && tracks.contains(path.toLower()))
      continue;
    const auto size = info.size();
    const auto modified = info.lastModified().toMSecsSinceEpoch();
    const auto saved = before.value(path).toMap();
    QVariantMap outcome;
    if (!QStringList{"cue","gdi","m3u"}.contains(ext) && !saved.value("outcome").toMap().value("retry").toBool() && !saved.isEmpty() && saved.value("size").toLongLong() == size && saved.value("modified").toLongLong() == modified) {
      outcome = saved.value("outcome").toMap();
      ++reused;
    } else {
      outcome = classify(path, ext, model3Sets, model2Sets, cancel);
      if (cancel)
        break; // A reading cut short is neither counted nor kept.
    }
    after.insert(path, QVariantMap{{"size", size}, {"modified", modified}, {"outcome", outcome}});
    if (outcome.value("table").toBool())
      ++tables[QDir::fromNativeSeparators(info.absolutePath())];
    for (const auto &value : outcome.value("copies").toList()) {
      const auto found = value.toMap();
      const auto copy = found.value("copy").toMap();
      copies[found.value("system").toString()][copy.value("game").toString()] << copy;
    }
    if (const auto system = outcome.value("other").toString(); !system.isEmpty())
      ++other[system];
    if (outcome.value("unknown") == "disc")
      ++unknownDiscs;
    else if (outcome.value("unknown") == "archive")
      ++unknownArchives;
  }
  if (index) {
    // Stopped part-way: the files not reached keep their last reading for next time.
    if (cancel)
      for (auto it = before.cbegin(); it != before.cend(); ++it)
        if (!after.contains(it.key()))
          after.insert(it.key(), it.value());
    *index = QVariantMap{{"stamp", stamp}, {"files", after}};
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
          {"files", files.size()}, {"reused", reused}, {"depth", depth}, {"cancelled", bool(cancel)}, {"systems", systems}, {"pinballFolders", folders},
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
