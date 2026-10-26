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
#include <QSet>
#include <algorithm>
#include <iterator>
namespace ac::library {
namespace {
int regionRank(const QString &region) {
  static const QStringList order{"USA", "World", "Europe", "Japan"};
  const auto i = order.indexOf(region);
  return i < 0 ? order.size() : i;
}
int formatRank(const QString &system, const QString &file) {
  const auto ext = QFileInfo(file).suffix().toLower();
  static const QHash<QString, QStringList> order{{"ps2", {"iso", "cue", "bin", "chd", "cso", "gz", "m3u"}},
                                                 {"ps1", {"cue", "iso", "bin", "chd", "pbp", "m3u"}},
                                                 {"gamecube", {"iso", "gcm", "gcz", "rvz", "wia", "nkit", "m3u"}},
                                                 {"wii", {"iso", "wbfs", "rvz", "wia", "gcz", "wad", "nkit", "m3u"}},
                                                 {"dreamcast", {"gdi", "chd", "cdi", "cue", "m3u"}}};
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
  // A later disc of a set is never the one to start; it ranks after every first disc.
  return std::make_tuple(copy.value("disc", 1).toInt() > 1 ? 1 : 0, copy.value("verified").toBool() ? 0 : 1, copy.value("profile").toBool() ? 0 : 1,
                         hinted(system, file) ? 0 : 1, regionRank(copy.value("region").toString()), formatRank(system, file),
                         file.size(), file);
}
// Why a copy was not the one kept, in the order the rule looks: the first thing in which it falls behind.
QString whyNot(const QString &system, const QVariantMap &kept, const QVariantMap &other) {
  const auto keptFile = kept.value("file").toString(), otherFile = other.value("file").toString();
  if (kept.value("verified").toBool() && !other.value("verified").toBool())
    return "It is known only by its set name; the kept copy was read from the disc itself.";
  if (kept.value("profile").toBool() && !other.value("profile").toBool())
    return "It has no VR profile; the kept copy has one.";
  if (hinted(system, keptFile) && !hinted(system, otherFile))
    return "Its folder does not name the system; the kept copy's does.";
  const auto keptRegion = kept.value("region").toString(), otherRegion = other.value("region").toString();
  if (regionRank(keptRegion) != regionRank(otherRegion))
    return QString("It is the %1 release; the kept copy is the %2 one.").arg(otherRegion.isEmpty() ? QString("unknown-region") : otherRegion,
                                                                              keptRegion.isEmpty() ? QString("unknown-region") : keptRegion);
  if (formatRank(system, keptFile) != formatRank(system, otherFile))
    return QString("It is a .%1 file; the kept copy is the plainer .%2.").arg(QFileInfo(otherFile).suffix().toLower(), QFileInfo(keptFile).suffix().toLower());
  if (otherFile.size() != keptFile.size())
    return "The same copy in a longer path.";
  return "It is the same in every way; the first by path was kept.";
}
// A playlist that sits beside another copy of the same game only points at it, so it is not a second copy.
bool playlistOfAnother(const QVariantMap &copy, const QVariantList &group) {
  const auto file = copy.value("file").toString();
  if (QFileInfo(file).suffix().compare("m3u", Qt::CaseInsensitive) != 0)
    return false;
  const auto folder = QFileInfo(file).absolutePath();
  for (const auto &value : group) {
    const auto other = value.toMap().value("file").toString();
    if (other != file && QFileInfo(other).absolutePath() == folder && QFileInfo(other).suffix().compare("m3u", Qt::CaseInsensitive) != 0)
      return true;
  }
  return false;
}
// The games found more than once: same game and disc number, found in more than one file (the files ranked by the rule,
// the first kept). Known by the disc's own identity, never the name; different releases of a game count as copies of it.
QVariantList duplicatesOf(const QHash<QString, QHash<QString, QVariantList>> &found, const QHash<QString, qint64> &sizes,
                          const QHash<QString, QString> &titles) {
  QVariantList groups;
  for (auto system = found.cbegin(); system != found.cend(); ++system)
    for (auto game = system.value().cbegin(); game != system.value().cend(); ++game) {
      QMap<int, QVariantList> discs;
      for (const auto &value : game.value())
        discs[value.toMap().value("disc", 1).toInt()] << value;
      for (auto disc = discs.cbegin(); disc != discs.cend(); ++disc) {
        auto list = disc.value();
        const auto key = system.key();
        std::sort(list.begin(), list.end(), [&](const QVariant &a, const QVariant &b) { return rank(key, a.toMap()) < rank(key, b.toMap()); });
        QVariantList real;
        for (const auto &value : list)
          if (!playlistOfAnother(value.toMap(), list))
            real << value;
        if (real.size() < 2)
          continue;
        const auto kept = real.first().toMap();
        QVariantList rows;
        bool same = true;
        for (const auto &value : real) {
          const auto copy = value.toMap();
          const auto file = copy.value("file").toString();
          const bool first = file == kept.value("file").toString();
          same = same && copy.value("identity") == kept.value("identity");
          rows << QVariantMap{{"file", file}, {"name", QFileInfo(file).fileName()}, {"folder", QFileInfo(file).absolutePath()}, {"size", sizes.value(file)},
                              {"region", copy.value("region")}, {"identity", copy.value("identity")}, {"kept", first},
                              {"why", first ? QString("The copy kept: %1").arg(rule()) : whyNot(key, kept, copy)}};
        }
        const auto title = kept.value("title").toString();
        groups << QVariantMap{{"system", key}, {"game", game.key()}, {"title", !title.isEmpty() ? title : titles.value(key + "/" + game.key(), game.key())},
                              {"disc", disc.key()}, {"count", int(rows.size())}, {"sameDisc", same}, {"kept", kept.value("file")}, {"copies", rows}};
      }
    }
  std::sort(groups.begin(), groups.end(), [](const QVariant &a, const QVariant &b) {
    const auto x = a.toMap(), y = b.toMap();
    return std::make_tuple(x.value("system").toString(), x.value("game").toString(), x.value("disc").toInt()) <
           std::make_tuple(y.value("system").toString(), y.value("game").toString(), y.value("disc").toInt());
  });
  return groups;
}
// A system as it is said in a reason.
QString systemName(const QString &system) {
  static const QHash<QString, QString> names{{"ps1", "PlayStation"}, {"ps2", "PlayStation 2"}, {"wii", "Wii"}, {"gamecube", "GameCube"}, {"dreamcast", "Dreamcast"}, {"saturn", "Saturn"}};
  return names.value(system, system);
}
// One file the Hub looked at and matched to nothing, with why, from what the reader said (so nothing is read again).
QVariantMap unrecognisedEntry(const QString &path, const QString &ext, qint64 size, const QVariantMap &outcome) {
  QVariantMap entry{{"file", path}, {"name", QFileInfo(path).fileName()}, {"folder", QDir::fromNativeSeparators(QFileInfo(path).absolutePath())}, {"size", size}};
  if (outcome.value("unknown") == "archive") {
    entry.insert("kind", "archive");
    entry.insert("code", "unknownArchive");
    entry.insert("reason", "A zip that is not one of the arcade games the Hub plays. Arcade games are known by the name of their set.");
    return entry;
  }
  if (outcome.value("unknown") == "disc") {
    const auto why = scan::unreadReason(ext, outcome.value("error").toString());
    entry.insert("kind", "disc");
    entry.insert("code", why.code);
    entry.insert("reason", why.reason);
    if (!why.detail.isEmpty())
      entry.insert("detail", why.detail);
    return entry;
  }
  const auto system = outcome.value("other").toString();
  entry.insert("kind", "other");
  entry.insert("code", "otherSystem");
  entry.insert("system", system);
  entry.insert("reason", QString("A %1 disc, but not one of the games the Hub plays.").arg(systemName(system)));
  return entry;
}
// The most files named in the list of those not recognised; the counts are always whole.
constexpr int kMaxUnrecognisedListed = 500;
// Whether a path is the folder or below it (the folder is clean, with forward slashes).
bool within(const QString &path, const QString &folder) {
#ifdef Q_OS_WIN
  const auto sense = Qt::CaseInsensitive;
#else
  const auto sense = Qt::CaseSensitive;
#endif
  const auto clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
  return clean.compare(folder, sense) == 0 || clean.startsWith(folder.endsWith('/') ? folder : folder + "/", sense);
}
} // namespace

QString rule() {
  return "Read from the disc itself first, then the release with a VR profile, then USA, World, Europe, "
         "Japan, then the plainest format, then the shortest path.";
}
QString duplicateRule() {
  return "The same game found in more than one file is grouped by what is inside the files, never by their names, with a "
         "set's later discs counted apart. One copy is kept: " + rule();
}
namespace {
// Raised whenever what a disc reader makes of a file changes, so every saved reading is read again. It is the only
// thing the saved index is keyed on besides the file itself: the games and disc data are looked up on every scan.
constexpr int kIndexVersion = 4;
QString indexStamp() { return QString("reader-%1").arg(kIndexVersion); }
// What one file is, before any game is looked up: a pinball table, an archive's copies (known by its name, so it
// is worked out on every scan and costs no read), or a disc: {reading: {system, identity}} as the disc itself says,
// or {unknown: "disc", error, retry} when it could not be read. Kept in the index to be used again.
QVariantMap classify(const QString &path, const QString &ext, const QSet<QString> &model3Sets, const QSet<QString> &model2Sets,
                     std::atomic_bool &cancel) {
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
  if(!read.error.isEmpty()||read.identity.isEmpty())return {{"unknown","disc"},{"error",read.error},{"retry",read.error.contains("budget")||read.error.contains("time")}};
  return {{"reading",QVariantMap{{"system",read.system},{"identity",read.identity}}}};
}
// What a saved or fresh reading gives against the disc data of this build: the copies it gives (system and copy),
// the system it is another disc of, or a kind the Hub reads but does not know ("disc" or "archive").
QVariantMap outcomeOf(const QString &path, const QVariantMap &saved) {
  const auto reading=saved.value("reading").toMap();
  if(reading.isEmpty())return saved;
  const auto readSystem=reading.value("system").toString(),identity=reading.value("identity").toString();
  const auto row=scan::DiscIndex::match(readSystem,identity);
  const auto system=readSystem=="gc"?QString("gamecube"):readSystem;
  if(row.isEmpty())return system.isEmpty()?QVariantMap{{"unknown","disc"}}:QVariantMap{{"other",system}};
  QVariantMap found{{"file",path},{"identity",identity},{"game",row.value("game")},{"title",row.value("title")},
                    {"region",row.value("region")},{"verified",true},{"profile",row.value("profile",false)}};
  if(row.contains("disc"))found["disc"]=row.value("disc");
  if(readSystem=="ps1"||readSystem=="ps2")found["serial"]=identity;
  else if(readSystem=="dreamcast")found["product"]=identity;
  else if(readSystem=="wii"||readSystem=="gc")found["id"]=identity;
  return QVariantMap{{"copies",QVariantList{QVariantMap{{"system",system},{"copy",found}}}}};
}
// The index as it would be saved now: every reading made, and the last reading of each file not yet reached.
QVariantMap indexOf(const QVariantMap &made, const QVariantMap &before) {
  auto files = made;
  for (auto it = before.cbegin(); it != before.cend(); ++it)
    if (!files.contains(it.key()))
      files.insert(it.key(), it.value());
  return QVariantMap{{"stamp", indexStamp()}, {"files", files}};
}
} // namespace

QVariantMap scan(const QStringList &roots, const Progress &progress, std::atomic_bool &cancel, int depth, QVariantMap *index, const Checkpoint &checkpoint) {
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
  QHash<QString, qint64> sizes;
  // The first files by path that matched nothing, kept to a fixed number however many there are.
  QMap<QString, QVariantMap> unread;
  int unreadTotal = 0, unknownDiscs = 0, unknownArchives = 0;
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
  // The last reading of each file, used again while its size and time are unchanged. It is what the disc said, not
  // what it matched, so a change to the disc data or the lists of games does not read a disc again.
  const auto stamp = indexStamp();
  const auto before = index && index->value("stamp").toString() == stamp ? index->value("files").toMap() : QVariantMap{};
  QVariantMap after;
  int done = 0, reused = 0, unsaved = 0;
  QElapsedTimer sinceSave;
  sinceSave.start();
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
    const auto unchanged = !saved.isEmpty() && saved.value("size").toLongLong() == size && saved.value("modified").toLongLong() == modified;
    QVariantMap reading;
    if (ext == "zip" || ext == "vpx") {
      // Known by name alone: worked out again, as the lists of games may have changed.
      reading = classify(path, ext, model3Sets, model2Sets, cancel);
      if (unchanged)
        ++reused;
    } else if (!QStringList{"cue","gdi","m3u"}.contains(ext) && unchanged && !saved.value("outcome").toMap().value("retry").toBool()) {
      reading = saved.value("outcome").toMap();
      ++reused;
    } else {
      reading = classify(path, ext, model3Sets, model2Sets, cancel);
      if (cancel)
        break; // A reading cut short is neither counted nor kept.
      ++unsaved;
    }
    after.insert(path, QVariantMap{{"size", size}, {"modified", modified}, {"outcome", reading}});
    const auto outcome = outcomeOf(path, reading);
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
    if (outcome.contains("unknown") || outcome.contains("other")) {
      ++unreadTotal;
      if (unread.size() < kMaxUnrecognisedListed || path < std::prev(unread.cend()).key()) {
        unread.insert(path, unrecognisedEntry(path, ext, size, outcome));
        if (unread.size() > kMaxUnrecognisedListed)
          unread.erase(std::prev(unread.end()));
      }
    }
    sizes.insert(path, size);
    // The index is handed over every ten seconds while new readings come in, so a crash or a shut-down part-way
    // keeps what was read.
    if (checkpoint && index && unsaved > 0 && sinceSave.elapsed() >= 10000) {
      checkpoint(indexOf(after, before));
      unsaved = 0;
      sinceSave.restart();
    }
  }
  if (index) {
    // Stopped part-way: the files not reached keep their last reading for next time.
    *index = cancel ? indexOf(after, before) : QVariantMap{{"stamp", stamp}, {"files", after}};
  }
  QHash<QString, QString> titles;
  for (const auto &game : model3::games())
    titles.insert("model3/" + game.toMap().value("id").toString(), game.toMap().value("title").toString());
  for (const auto &game : model2::games())
    titles.insert("model2/" + game.toMap().value("id").toString(), game.toMap().value("title").toString());
  const auto duplicates = duplicatesOf(copies, sizes, titles);
  QHash<QString, int> duplicateGames;
  for (const auto &group : duplicates)
    ++duplicateGames[group.toMap().value("system").toString()];
  QVariantList listed;
  for (auto it = unread.cbegin(); it != unread.cend(); ++it)
    listed << it.value();
  QVariantMap systems;
  for (auto it = copies.cbegin(); it != copies.cend(); ++it) {
    QVariantMap byGame;
    for (auto game = it.value().cbegin(); game != it.value().cend(); ++game) {
      auto list = game.value();
      std::sort(list.begin(), list.end(), [&](const QVariant &a, const QVariant &b) { return rank(it.key(), a.toMap()) < rank(it.key(), b.toMap()); });
      byGame.insert(game.key(), list);
    }
    systems.insert(it.key(), QVariantMap{{"copies", byGame}, {"games", byGame.size()}, {"other", other.value(it.key())}, {"duplicateGames", duplicateGames.value(it.key())}});
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
          {"unrecognised", QVariantMap{{"discs", unknownDiscs}, {"archives", unknownArchives}, {"files", listed}, {"total", unreadTotal}, {"more", unreadTotal - int(listed.size())}}},
          {"duplicates", duplicates}};
}
QVariantList unrecognisedIn(const QVariantMap &library, const QString &folder) {
  QVariantList found;
  const auto where = QDir::cleanPath(QDir::fromNativeSeparators(folder));
  for (const auto &value : library.value("unrecognised").toMap().value("files").toList())
    if (folder.isEmpty() || within(value.toMap().value("file").toString(), where))
      found << value;
  return found;
}
bool covers(const QVariantMap &library, const QString &folder) {
  if (folder.isEmpty() || !library.contains("files"))
    return false;
  for (const auto &root : library.value("roots").toStringList())
    if (within(folder, QDir::cleanPath(QDir::fromNativeSeparators(root))))
      return true;
  return false;
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
