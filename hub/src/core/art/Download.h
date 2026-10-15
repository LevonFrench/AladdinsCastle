// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QHash>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QObject>
#include <QStringList>
#include <QVector>
#include <functional>
#include <memory>
class QNetworkAccessManager;
namespace ac::art {
// Optional artwork from the internet (decision D66). Off by default: with the setting off the
// downloader makes no request at all. Nothing runs by itself; the user asks per game or per system.
// Pictures are kept in the Hub's user folder, and the user's own pictures always come first.

// libretro thumbnail file names: these characters of a display name become "_".
QString libretroName(QString name);
// A loose key for comparing names: case, punctuation, articles and bracketed tags removed.
QString matchKey(QString name);
// The best file in a libretro folder for a game: an exact name first, then the same title with
// the most usual region (USA, World, Europe, Japan) and the fewest extra tags. Empty when none.
QString bestLibretroMatch(const QStringList &wanted, const QStringList &listing);
// The libretro thumbnail folder for a Hub system, or empty when libretro has none.
QString libretroSystem(const QString &system);
// File names (without ".png") from a thumbnail server folder listing.
QStringList parseListing(const QByteArray &html);

// What to look for: the game's id and system, its title, and its own names on this PC (a disc's
// file name, its folder). Those names are used for matching on this PC only and never stored.
struct Target {
  QString id, system, title;
  QStringList names;
};

// The network layer, replaced by a stand-in in the tests.
class Fetcher {
public:
  virtual ~Fetcher() = default;
  virtual void get(const QNetworkRequest &request,
                   std::function<void(int status, QByteArray body)> done) = 0;
};
class NetworkFetcher final : public Fetcher {
public:
  NetworkFetcher();
  ~NetworkFetcher() override;
  void get(const QNetworkRequest &request,
           std::function<void(int, QByteArray)> done) override;

private:
  std::unique_ptr<QNetworkAccessManager> manager_;
};

class Downloader : public QObject {
  Q_OBJECT
public:
  // userRoot is the Hub's user folder. enabled and steamGridKey are read at each start.
  Downloader(QString userRoot, std::function<bool()> enabled,
             std::function<QString()> steamGridKey, QObject *parent = nullptr);
  ~Downloader() override;
  void setFetcher(std::shared_ptr<Fetcher> fetcher) { fetcher_ = std::move(fetcher); }
  // Time between two requests; one second unless a test shortens it.
  void setInterval(int ms) { interval_ = ms; }
  // False, with no request made, when the setting is off, nothing is asked or a run is going.
  bool start(const QVector<Target> &targets);
  void cancel() { cancelled_ = true; }
  bool busy() const { return busy_; }
  int requestsMade() const { return requests_; }
  // The picture to show for a game: the user's own art for it first, then a downloaded one.
  // use is "tile" (a wide card) or "hero" (the wide banner). Empty when there is none.
  static QString pictureFor(const QString &userRoot, const QString &id, const QString &use);
  static QString downloadFolder(const QString &userRoot);
  static QString userAgent();
  static constexpr int missDays = 30, listingDays = 30;
signals:
  void progress(const QString &message);
  void finished(const QString &summary, const QStringList &changedIds);

private:
  void runTarget(int index);
  void libretroStep(const Target &target, int kind, std::function<void()> next);
  void steamGridStep(const Target &target, std::function<void()> next);
  void steamGridAsset(const Target &target, int gameId, int kind, std::function<void()> next);
  void withListing(const QString &system, const QString &folder,
                   std::function<void(bool ok, QStringList names)> done);
  void fetch(const QUrl &url, bool steamGridApi, std::function<void(int, QByteArray)> done);
  bool saveImage(const QByteArray &bytes, const QString &file);
  bool missFresh(const QString &key) const;
  void recordMiss(const QString &key);
  void recordFile(const QString &id, const QString &kind, const QString &source, const QUrl &url);
  void saveIndex();
  void finish();
  QString root_;
  std::function<bool()> enabled_;
  std::function<QString()> key_;
  std::shared_ptr<Fetcher> fetcher_;
  QVector<Target> targets_;
  QHash<QString, QStringList> listings_;
  QJsonObject index_;
  QString steamGridKey_;
  QStringList changed_;
  int interval_ = 1000, requests_ = 0, got_ = 0, failed_ = 0;
  qint64 last_ = 0;
  bool busy_ = false, cancelled_ = false;
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};
} // namespace ac::art
