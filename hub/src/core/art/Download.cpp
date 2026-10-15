// SPDX-License-Identifier: GPL-3.0-only
#include "Download.h"
#include "ProductIdentity.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <algorithm>
namespace ac::art {
namespace {
constexpr char libretroHost[] = "https://thumbnails.libretro.com";
constexpr char steamGridApi[] = "https://www.steamgriddb.com/api/v2";
// libretro's three picture folders, and the name each is kept under here.
const char *const libretroFolders[] = {"Named_Snaps", "Named_Titles", "Named_Boxarts"};
const char *const libretroKinds[] = {"snap", "title", "boxart"};
constexpr qint64 maxBytes = 20 * 1024 * 1024;
bool safeId(const QString &id) {
  static const QRegularExpression ok("^[A-Za-z0-9._-]{1,128}$");
  return ok.match(id).hasMatch() && !id.startsWith('.');
}
QString picture(const QString &base) {
  for (const auto &e : {"png", "jpg", "jpeg", "webp", "bmp"}) {
    const QFileInfo f(base + "." + e);
    if (f.isFile() && !f.isSymLink())
      return f.absoluteFilePath();
  }
  return {};
}
int regionRank(const QString &name) {
  const auto tags = name.section('(', 1);
  if (tags.contains("USA"))
    return 0;
  if (tags.contains("World"))
    return 1;
  if (tags.contains("Europe"))
    return 2;
  if (tags.contains("Japan"))
    return 3;
  return tags.isEmpty() ? 0 : 4;
}
bool lesserCopy(const QString &name) {
  static const QRegularExpression tag(
      "[(\\[][^)\\]]*\\b(beta|proto|prototype|demo|sample|bootleg|hack|kiosk|"
      "taikenban|trial|program)\\b",
      QRegularExpression::CaseInsensitiveOption);
  return tag.match(name).hasMatch();
}
} // namespace

QString libretroName(QString name) {
  for (const QChar c : QStringLiteral("&*/:`<>?\\|\""))
    name.replace(c, '_');
  return name;
}
QString matchKey(QString name) {
  name = name.toLower();
  name.remove(QRegularExpression("\\([^)]*\\)|\\[[^\\]]*\\]"));
  name.replace('&', " and ").replace('_', ' ');
  // "House of the Dead 2, The" and "The House of the Dead 2" are one title.
  name.remove(QRegularExpression(",\\s*(the|a|an)(?=\\s*($|-|:))"));
  name = name.trimmed();
  name.remove(QRegularExpression("^(the|a|an)\\s+"));
  name.remove(QRegularExpression("[^a-z0-9]"));
  return name;
}
QString bestLibretroMatch(const QStringList &wanted, const QStringList &listing) {
  for (const auto &w : wanted)
    if (!w.trimmed().isEmpty() && listing.contains(libretroName(w.trimmed())))
      return libretroName(w.trimmed());
  for (const auto &w : wanted) {
    const auto key = matchKey(w);
    if (key.isEmpty())
      continue;
    QStringList same;
    for (const auto &entry : listing)
      if (matchKey(entry) == key)
        same << entry;
    if (same.isEmpty())
      continue;
    std::stable_sort(same.begin(), same.end(), [](const QString &a, const QString &b) {
      const auto rank = [](const QString &n) {
        return std::make_tuple(lesserCopy(n) ? 1 : 0, regionRank(n), n.count('('),
                               n.size());
      };
      return rank(a) < rank(b);
    });
    return same.first();
  }
  return {};
}
bool isArcade(const QString &system) {
  return system == "model2" || system == "model3" || system == "system22" || system == "naomi";
}
QString pictureKind(const QString &relativePath) {
  const auto parts = relativePath.toLower().split('/', Qt::SkipEmptyParts);
  // The nearest folder that says what it holds wins: "pack/flyers/vcop2.png" is a flyer.
  for (auto it = parts.crbegin() + (parts.isEmpty() ? 0 : 1); it != parts.crend(); ++it) {
    const auto &p = *it;
    if (p.contains("flyer"))
      return "flyer";
    if (p.contains("marquee"))
      return "marquee";
    if (p.contains("box") || p.contains("cover"))
      return "boxart";
    if (p.contains("title"))
      return "title";
    if (p.contains("snap") || p.contains("screenshot"))
      return "snap";
    if (p.contains("fanart") || p.contains("hero") || p.contains("background"))
      return "hero";
    if (p.contains("wheel") || p.contains("logo"))
      return "logo";
  }
  return "other";
}
LocalIndex LocalIndex::scan(const QStringList &folders, const std::atomic_bool *cancel) {
  LocalIndex index;
  for (const auto &folder : folders) {
    const QDir root(folder);
    if (folder.isEmpty() || !root.exists())
      continue;
    QDirIterator it(root.absolutePath(), {"*.png", "*.jpg", "*.jpeg", "*.webp", "*.bmp"},
                    QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext() && index.files_ < 300000) {
      if (cancel && cancel->load())
        return index;
      const QFileInfo info(it.next());
      if (info.isSymLink())
        continue;
      // The pack's own folder name counts too: a folder chosen as ".../Flyers" holds flyers.
      const auto kind = pictureKind(root.dirName() + "/" + root.relativeFilePath(info.absoluteFilePath()));
      if (kind == "logo")
        continue;
      const auto base = info.completeBaseName();
      index.byKey_[base.toLower()] << qMakePair(kind, info.absoluteFilePath());
      if (const auto loose = matchKey(base); !loose.isEmpty())
        index.byKey_["~" + loose] << qMakePair(kind, info.absoluteFilePath());
      ++index.files_;
    }
  }
  return index;
}
QString LocalIndex::find(const QStringList &names, const QStringList &kinds) const {
  for (const auto &kind : kinds)
    for (const bool loose : {false, true})
      for (const auto &name : names) {
        if (name.trimmed().isEmpty())
          continue;
        const auto key = loose ? "~" + matchKey(name) : libretroName(name.trimmed()).toLower();
        for (const auto &entry : byKey_.value(key))
          if (entry.first == kind)
            return entry.second;
        if (!loose)
          for (const auto &entry : byKey_.value(name.trimmed().toLower()))
            if (entry.first == kind)
              return entry.second;
      }
  return {};
}
Picks choosePictures(const QString &userRoot, const QString &id, const QString &system,
                     const QString &title, const QStringList &names, const LocalIndex *local) {
  Picks picks;
  if (!safeId(id))
    return picks;
  const bool arcade = isArcade(system);
  const auto own = [&](const QString &role) {
    return userRoot.isEmpty() ? QString() : picture(userRoot + "/art/" + id + "/" + role);
  };
  const auto fetched = [&](const QString &kind) {
    if (userRoot.isEmpty())
      return QString();
    const QFileInfo f(Downloader::downloadFolder(userRoot) + "/" + id + "/" + kind + ".png");
    return f.isFile() && !f.isSymLink() ? f.absoluteFilePath() : QString();
  };
  const auto keys = QStringList(names) << title;
  const auto first = [](std::initializer_list<std::function<QString()>> ways) {
    for (const auto &way : ways)
      if (auto path = way(); !path.isEmpty())
        return path;
    return QString();
  };
  const QStringList cardKinds = arcade ? QStringList{"flyer", "boxart", "title", "marquee", "snap"}
                                       : QStringList{"boxart", "title", "snap"};
  picks.card = first({
      [&] {
        for (const auto &role : QStringList{"banner"} + cardKinds)
          if (auto p = own(role); !p.isEmpty())
            return p;
        return QString();
      },
      [&] { return local ? local->find(keys, QStringList(cardKinds) << "other") : QString(); },
      [&] {
        // libretro's arcade box art is mostly the flyer.
        for (const auto &kind : arcade ? QStringList{"boxart", "title", "snap", "grid"}
                                       : QStringList{"boxart", "title", "grid", "snap"})
          if (auto p = fetched(kind); !p.isEmpty())
            return p;
        return QString();
      }});
  picks.hero = first({[&] { return own("hero").isEmpty() ? own("fanart") : own("hero"); },
                      [&] { return local ? local->find(keys, {"hero"}) : QString(); },
                      [&] { return fetched("hero"); }, [&] { return own("snap"); },
                      [&] { return local ? local->find(keys, {"snap"}) : QString(); },
                      [&] { return fetched("snap"); }});
  if (picks.hero.isEmpty() && !picks.card.isEmpty()) {
    picks.hero = picks.card;
    picks.heroBlurred = true;
  }
  return picks;
}
QString libretroSystem(const QString &system) {
  if (system == "naomi")
    return "Sega - Naomi";
  if (system == "dreamcast")
    return "Sega - Dreamcast";
  if (system == "ps2")
    return "Sony - PlayStation 2";
  // The arcade boards are under MAME, named by each game's full description.
  if (system == "model2" || system == "model3" || system == "system22")
    return "MAME";
  return {};
}
QStringList parseListing(const QByteArray &html) {
  QStringList names;
  static const QRegularExpression link("href=\"([^\"/?]+)\\.png\"",
                                       QRegularExpression::CaseInsensitiveOption);
  for (auto it = link.globalMatch(QString::fromUtf8(html)); it.hasNext();) {
    auto name = QUrl::fromPercentEncoding(it.next().captured(1).toUtf8());
    name.replace("&amp;", "&").replace("&#39;", "'").replace("&quot;", "\"");
    if (!name.isEmpty())
      names << name;
  }
  names.removeDuplicates();
  return names;
}

NetworkFetcher::NetworkFetcher() : manager_(std::make_unique<QNetworkAccessManager>()) {}
NetworkFetcher::~NetworkFetcher() = default;
void NetworkFetcher::get(const QNetworkRequest &request,
                         std::function<void(int, QByteArray)> done) {
  auto *reply = manager_->get(request);
  QObject::connect(reply, &QNetworkReply::downloadProgress, reply,
                   [reply](qint64 received, qint64) {
                     if (received > maxBytes)
                       reply->abort();
                   });
  QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done] {
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto body = reply->error() == QNetworkReply::NoError ||
                              reply->error() >= QNetworkReply::ContentAccessDenied
                          ? reply->readAll()
                          : QByteArray();
    reply->deleteLater();
    done(status, body);
  });
}

Downloader::Downloader(QString userRoot, std::function<bool()> enabled,
                       std::function<QString()> steamGridKey, QObject *parent)
    : QObject(parent), root_(std::move(userRoot)), enabled_(std::move(enabled)),
      key_(std::move(steamGridKey)) {}
Downloader::~Downloader() { *alive_ = false; }
QString Downloader::downloadFolder(const QString &userRoot) {
  return userRoot + "/art-downloaded";
}
QString Downloader::userAgent() {
  auto version = QCoreApplication::applicationVersion();
  if (version.isEmpty())
    version = "0.1";
  return QString("%1/%2 (+https://github.com/LevonFrench/AladdinsCastle; artwork on request)")
      .arg(QString::fromLatin1(product::program_name), version);
}
bool Downloader::start(const QVector<Target> &targets) {
  // The switch is checked first: off means no request of any kind.
  if (busy_ || !enabled_ || !enabled_() || targets.isEmpty() || root_.isEmpty())
    return false;
  targets_.clear();
  for (const auto &t : targets)
    if (safeId(t.id))
      targets_ << t;
  if (targets_.isEmpty())
    return false;
  busy_ = true;
  cancelled_ = false;
  got_ = failed_ = 0;
  changed_.clear();
  steamGridKey_ = key_ ? key_().trimmed() : QString();
  if (!fetcher_)
    fetcher_ = std::make_shared<NetworkFetcher>();
  index_ = {};
  QFile file(downloadFolder(root_) + "/index.json");
  if (file.open(QIODevice::ReadOnly))
    index_ = QJsonDocument::fromJson(file.readAll()).object();
  const auto alive = alive_;
  QTimer::singleShot(0, this, [this, alive] {
    if (*alive)
      runTarget(0);
  });
  return true;
}
void Downloader::runTarget(int index) {
  // Turning the setting off part-way stops the run before its next request.
  if (!enabled_())
    cancelled_ = true;
  if (cancelled_ || index >= targets_.size())
    return finish();
  const auto target = targets_[index];
  emit progress(QString("Artwork: %1 (%2 of %3)")
                    .arg(target.title)
                    .arg(index + 1)
                    .arg(targets_.size()));
  const auto alive = alive_;
  libretroStep(target, 0, [this, alive, target, index] {
    steamGridStep(target, [this, alive, index] {
      QTimer::singleShot(0, this, [this, alive, index] {
        if (*alive)
          runTarget(index + 1);
      });
    });
  });
}
void Downloader::libretroStep(const Target &target, int kind, std::function<void()> next) {
  const auto system = libretroSystem(target.system);
  if (system.isEmpty() || kind >= 3 || cancelled_)
    return next();
  const auto cont = [this, target, kind, next] { libretroStep(target, kind + 1, next); };
  const QString folder = libretroFolders[kind];
  const auto file = downloadFolder(root_) + "/" + target.id + "/" + libretroKinds[kind] + ".png";
  const auto miss = target.id + "|libretro|" + libretroKinds[kind];
  // Never asked again: a picture already here, or a recent "not there".
  if (QFileInfo::exists(file) || missFresh(miss))
    return cont();
  withListing(system, folder, [this, target, system, folder, file, miss, cont, kind](bool ok, QStringList names) {
    if (!ok) {
      ++failed_;
      return cont();
    }
    const auto name = bestLibretroMatch(QStringList(target.names) << target.title, names);
    if (name.isEmpty()) {
      recordMiss(miss);
      return cont();
    }
    QUrl url(libretroHost);
    url.setPath("/" + system + "/" + folder + "/" + name + ".png", QUrl::DecodedMode);
    fetch(url, false, [this, target, file, miss, cont, url, kind](int status, QByteArray body) {
      if (status == 200 && saveImage(body, file)) {
        recordFile(target.id, libretroKinds[kind], "libretro", url);
        ++got_;
        if (!changed_.contains(target.id))
          changed_ << target.id;
      } else if (status == 404)
        recordMiss(miss);
      else
        ++failed_;
      cont();
    });
  });
}
void Downloader::withListing(const QString &system, const QString &folder,
                             std::function<void(bool, QStringList)> done) {
  const auto key = system + "/" + folder;
  if (listings_.contains(key))
    return done(true, listings_.value(key));
  auto safe = key;
  safe.replace(QRegularExpression("[^A-Za-z0-9]+"), "-");
  const auto file = downloadFolder(root_) + "/listings/" + safe + ".txt";
  const QFileInfo info(file);
  if (info.isFile() && info.lastModified().daysTo(QDateTime::currentDateTime()) < listingDays) {
    QFile in(file);
    if (in.open(QIODevice::ReadOnly)) {
      const auto names = QString::fromUtf8(in.readAll()).split('\n', Qt::SkipEmptyParts);
      if (!names.isEmpty()) {
        listings_.insert(key, names);
        return done(true, names);
      }
    }
  }
  QUrl url(libretroHost);
  url.setPath("/" + system + "/" + folder + "/", QUrl::DecodedMode);
  fetch(url, false, [this, key, file, done](int status, QByteArray body) {
    const auto names = status == 200 ? parseListing(body) : QStringList();
    if (names.isEmpty())
      return done(false, {});
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile out(file);
    if (out.open(QIODevice::WriteOnly)) {
      out.write(names.join('\n').toUtf8());
      out.commit();
    }
    listings_.insert(key, names);
    done(true, names);
  });
}
bool Downloader::hasLibretroPicture(const QString &id) const {
  for (const auto *kind : libretroKinds)
    if (QFileInfo::exists(downloadFolder(root_) + "/" + id + "/" + kind + ".png"))
      return true;
  return false;
}
void Downloader::steamGridStep(const Target &target, std::function<void()> next) {
  if (steamGridKey_.isEmpty() || cancelled_)
    return next();
  // With a key, SteamGridDB gives the banner's hero for every game, and a card picture only
  // for games libretro had nothing for.
  const auto folder = downloadFolder(root_) + "/" + target.id + "/";
  const bool needGrid = !hasLibretroPicture(target.id) && !QFileInfo::exists(folder + "grid.png") &&
                        !missFresh(target.id + "|steamgriddb|grid");
  const bool needHero = !QFileInfo::exists(folder + "hero.png") && !missFresh(target.id + "|steamgriddb|hero");
  if (!needGrid && !needHero)
    return next();
  const auto miss = target.id + "|steamgriddb";
  if (missFresh(miss))
    return next();
  const int known = index_.value("steamgrid").toObject().value(target.id).toInt();
  if (known > 0)
    return steamGridAsset(target, known, 0, next);
  QUrl url(steamGridApi);
  url.setPath(url.path() + "/search/autocomplete/" + target.title, QUrl::DecodedMode);
  fetch(url, true, [this, target, miss, next](int status, QByteArray body) {
    if (status == 401 || status == 403) {
      // A refused key is not tried again in this run.
      ++failed_;
      steamGridKey_.clear();
      index_["refusedKey"] = true;
      return next();
    }
    if (status != 200) {
      if (status == 404)
        recordMiss(miss);
      else
        ++failed_;
      return next();
    }
    const auto title = matchKey(target.title);
    int found = 0;
    for (const auto &value : QJsonDocument::fromJson(body).object().value("data").toArray())
      if (matchKey(value.toObject().value("name").toString()) == title) {
        found = value.toObject().value("id").toInt();
        break;
      }
    if (found <= 0) {
      recordMiss(miss);
      return next();
    }
    auto ids = index_.value("steamgrid").toObject();
    ids[target.id] = found;
    index_["steamgrid"] = ids;
    saveIndex();
    steamGridAsset(target, found, 0, next);
  });
}
void Downloader::steamGridAsset(const Target &target, int gameId, int kind,
                                std::function<void()> next) {
  if (kind >= 2 || steamGridKey_.isEmpty() || cancelled_)
    return next();
  const auto cont = [this, target, gameId, kind, next] { steamGridAsset(target, gameId, kind + 1, next); };
  const QString name = kind == 0 ? "grid" : "hero";
  const auto file = downloadFolder(root_) + "/" + target.id + "/" + name + ".png";
  const auto miss = target.id + "|steamgriddb|" + name;
  if (QFileInfo::exists(file) || missFresh(miss) || (kind == 0 && hasLibretroPicture(target.id)))
    return cont();
  // Still pictures only, and nothing marked adult, joke or flashing.
  QUrl url(QString(steamGridApi) + (kind == 0 ? "/grids/game/" : "/heroes/game/") +
           QString::number(gameId));
  url.setQuery(QString(kind == 0 ? "dimensions=920x430,460x215&" : "") +
               "types=static&nsfw=false&humor=false&epilepsy=false&limit=1");
  fetch(url, true, [this, target, file, miss, cont, name](int status, QByteArray body) {
    if (status != 200) {
      if (status == 404)
        recordMiss(miss);
      else
        ++failed_;
      return cont();
    }
    const auto data = QJsonDocument::fromJson(body).object().value("data").toArray();
    if (data.isEmpty()) {
      recordMiss(miss);
      return cont();
    }
    const QUrl asset(data.first().toObject().value("url").toString());
    // Pictures come from SteamGridDB's own servers, and never with the key.
    if (asset.scheme() != "https" ||
        !(asset.host() == "steamgriddb.com" || asset.host().endsWith(".steamgriddb.com"))) {
      ++failed_;
      return cont();
    }
    fetch(asset, false, [this, target, file, cont, name, asset](int status, QByteArray body) {
      if (status == 200 && saveImage(body, file)) {
        recordFile(target.id, name, "steamgriddb", asset);
        ++got_;
        if (!changed_.contains(target.id))
          changed_ << target.id;
      } else
        ++failed_;
      cont();
    });
  });
}
void Downloader::fetch(const QUrl &url, bool steamGridApiCall,
                       std::function<void(int, QByteArray)> done) {
  const auto alive = alive_;
  const auto wait = qMax<qint64>(0, last_ + interval_ - QDateTime::currentMSecsSinceEpoch());
  QTimer::singleShot(int(wait), this, [this, alive, url, steamGridApiCall, done] {
    if (!*alive)
      return;
    // The switch is read again before every request, so turning it off stops the very next one.
    if (!enabled_())
      cancelled_ = true;
    if (cancelled_)
      return done(-1, {});
    last_ = QDateTime::currentMSecsSinceEpoch();
    ++requests_;
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", userAgent().toUtf8());
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    if (steamGridApiCall && url.host() == "www.steamgriddb.com" && !steamGridKey_.isEmpty())
      request.setRawHeader("Authorization", "Bearer " + steamGridKey_.toUtf8());
    fetcher_->get(request, [alive, done](int status, QByteArray body) {
      if (*alive)
        done(status, body);
    });
  });
}
bool Downloader::saveImage(const QByteArray &bytes, const QString &file) {
  if (bytes.isEmpty() || bytes.size() > maxBytes)
    return false;
  QImage image;
  if (!image.loadFromData(bytes) || image.isNull() || image.width() > 16384 ||
      image.height() > 16384)
    return false;
  // Kept as a plain PNG of at most 1920 wide, which is all a card or banner needs.
  if (image.width() > 1920)
    image = image.scaledToWidth(1920, Qt::SmoothTransformation);
  if (!QDir().mkpath(QFileInfo(file).absolutePath()))
    return false;
  QSaveFile out(file);
  return out.open(QIODevice::WriteOnly) && image.save(&out, "PNG") && out.commit();
}
bool Downloader::missFresh(const QString &key) const {
  const auto when = qint64(index_.value("misses").toObject().value(key).toDouble());
  return when > 0 && QDateTime::currentSecsSinceEpoch() - when < qint64(missDays) * 86400;
}
void Downloader::recordMiss(const QString &key) {
  auto misses = index_.value("misses").toObject();
  misses[key] = double(QDateTime::currentSecsSinceEpoch());
  index_["misses"] = misses;
}
void Downloader::recordFile(const QString &id, const QString &kind, const QString &source,
                            const QUrl &url) {
  // Where each picture came from and when, like the provenance sidecars of the art pipeline.
  auto files = index_.value("files").toObject();
  files[id + "/" + kind + ".png"] = QJsonObject{
      {"source", source}, {"url", url.toString()},
      {"fetched", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
  index_["files"] = files;
}
void Downloader::saveIndex() {
  QDir().mkpath(downloadFolder(root_));
  QSaveFile out(downloadFolder(root_) + "/index.json");
  if (out.open(QIODevice::WriteOnly)) {
    out.write(QJsonDocument(index_).toJson());
    out.commit();
  }
}
void Downloader::finish() {
  if (!busy_)
    return;
  busy_ = false;
  if (requests_ > 0 || !index_.isEmpty())
    saveIndex();
  QString summary;
  if (cancelled_ && !enabled_())
    summary = "Artwork download stopped: the setting was turned off.";
  else if (got_ > 0)
    summary = QString("Artwork: %1 new picture%2 for %3 game%4.")
                  .arg(got_)
                  .arg(got_ == 1 ? "" : "s")
                  .arg(changed_.size())
                  .arg(changed_.size() == 1 ? "" : "s");
  else
    summary = "Artwork: no new pictures found.";
  if (failed_ > 0)
    summary += QString(" %1 request%2 failed; try again later.").arg(failed_).arg(failed_ == 1 ? "" : "s");
  if (index_.value("refusedKey").toBool()) {
    summary += " SteamGridDB refused the API key.";
    index_.remove("refusedKey");
    saveIndex();
  }
  emit finished(summary, changed_);
}
} // namespace ac::art
