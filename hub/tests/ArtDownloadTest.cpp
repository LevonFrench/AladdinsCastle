// SPDX-License-Identifier: GPL-3.0-only
// The optional artwork download (D66), with a stand-in network: no test here reaches the internet.
#include "core/art/Download.h"
#include <QBuffer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
namespace {
QByteArray png(QColor colour) {
  QImage image(64, 36, QImage::Format_ARGB32);
  image.fill(colour);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}
QByteArray listing(const QStringList &names) {
  QByteArray html = "<html><body><table>";
  for (const auto &n : names)
    html += "<tr><td><a href=\"" + QUrl::toPercentEncoding(n, "()',") + ".png\">" + n.toUtf8() + ".png</a></td></tr>";
  return html + "</table></body></html>";
}
struct Stub final : ac::art::Fetcher {
  QList<QNetworkRequest> seen;
  std::function<QPair<int, QByteArray>(const QUrl &)> answer = [](const QUrl &) {
    return qMakePair(404, QByteArray());
  };
  void get(const QNetworkRequest &request, std::function<void(int, QByteArray)> done) override {
    seen << request;
    const auto reply = answer(request.url());
    QTimer::singleShot(0, [reply, done] { done(reply.first, reply.second); });
  }
};
} // namespace
class ArtDownloadTest : public QObject {
  Q_OBJECT
private slots:
  void namesFollowLibretroRules() {
    // Every character libretro swaps for "_" in a thumbnail file name.
    QCOMPARE(ac::art::libretroName("a&b*c/d:e`f<g>h?i\\j|k\"l"), QString("a_b_c_d_e_f_g_h_i_j_k_l"));
    QCOMPARE(ac::art::libretroName("Foo: The Bar?"), QString("Foo_ The Bar_"));
    QCOMPARE(ac::art::matchKey("House of the Dead 2, The (USA)"), ac::art::matchKey("The House of the Dead 2"));
    QCOMPARE(ac::art::matchKey("Ghosts & Goblins"), ac::art::matchKey("Ghosts and Goblins (World)"));
    const QStringList folder{"House of the Dead 2, The (Europe)", "House of the Dead 2, The (USA) (Beta)",
                             "House of the Dead 2, The (USA)", "Confidential Mission (Japan) (En,Ja)",
                             "Confidential Mission (USA)", "Time Crisis (Japan)", "Time Crisis (World, TS2 Ver.B)",
                             "Foo_ Bar (USA)"};
    // A title alone: the usual region, never a beta.
    QCOMPARE(ac::art::bestLibretroMatch({"The House of the Dead 2"}, folder), QString("House of the Dead 2, The (USA)"));
    QCOMPARE(ac::art::bestLibretroMatch({"Time Crisis"}, folder), QString("Time Crisis (World, TS2 Ver.B)"));
    // A disc's own name wins when the folder has it exactly, after the substitutions.
    QCOMPARE(ac::art::bestLibretroMatch({"Confidential Mission (Japan) (En,Ja)", "Confidential Mission"}, folder),
             QString("Confidential Mission (Japan) (En,Ja)"));
    QCOMPARE(ac::art::bestLibretroMatch({"Foo: Bar (USA)"}, folder), QString("Foo_ Bar (USA)"));
    QVERIFY(ac::art::bestLibretroMatch({"Nothing Like It"}, folder).isEmpty());
    QCOMPARE(ac::art::parseListing(listing({"'99_ The Last War", "Rock & Roll (USA)"})),
             QStringList({"'99_ The Last War", "Rock & Roll (USA)"}));
    QCOMPARE(ac::art::libretroSystem("dreamcast"), QString("Sega - Dreamcast"));
    QCOMPARE(ac::art::libretroSystem("ps2"), QString("Sony - PlayStation 2"));
    QCOMPARE(ac::art::libretroSystem("model2"), QString("MAME"));
    QVERIFY(ac::art::libretroSystem("pinball").isEmpty());
  }
  void offMakesNoRequest() {
    QTemporaryDir t;
    auto stub = std::make_shared<Stub>();
    bool on = false;
    ac::art::Downloader downloader(t.path(), [&] { return on; }, [] { return QString("key"); });
    downloader.setFetcher(stub);
    downloader.setInterval(0);
    QVERIFY(!downloader.start({{"model2-vcop2", "model2", "Virtua Cop 2", {}}}));
    QTest::qWait(200);
    QVERIFY(stub->seen.isEmpty());
    QCOMPARE(downloader.requestsMade(), 0);
    QVERIFY(!QFileInfo::exists(ac::art::Downloader::downloadFolder(t.path())));
    // Turned off part-way: the run stops before its next request.
    on = true;
    stub->answer = [&](const QUrl &) { on = false; return qMakePair(500, QByteArray()); };
    QSignalSpy done(&downloader, &ac::art::Downloader::finished);
    QVERIFY(downloader.start({{"model2-vcop2", "model2", "Virtua Cop 2", {}}, {"ps2-tc2", "ps2", "Time Crisis II", {}}}));
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(stub->seen.size(), 1);
    QVERIFY(done.first()[0].toString().contains("turned off"));
  }
  void cachedPicturesAreNeverFetchedAgain() {
    QTemporaryDir t;
    auto stub = std::make_shared<Stub>();
    stub->answer = [](const QUrl &url) -> QPair<int, QByteArray> {
      const auto u = url.toString(QUrl::FullyDecoded);
      if (u == "https://thumbnails.libretro.com/MAME/Named_Snaps/")
        return {200, listing({"Time Crisis (World, TS2 Ver.B)", "Time Crisis (Japan)"})};
      if (u.startsWith("https://thumbnails.libretro.com/MAME/") && u.endsWith("/"))
        return {200, listing({"Another Game"})};
      if (u == "https://thumbnails.libretro.com/MAME/Named_Snaps/Time Crisis (World, TS2 Ver.B).png")
        return {200, png(Qt::red)};
      if (url.host() == "www.steamgriddb.com" && url.path() == "/api/v2/search/autocomplete/Virtua Cop 2")
        return {200, "{\"success\":true,\"data\":[{\"id\":7,\"name\":\"Virtua Cop\"},{\"id\":42,\"name\":\"Virtua Cop 2\"}]}"};
      if (url.path() == "/api/v2/grids/game/42")
        return {200, "{\"success\":true,\"data\":[{\"url\":\"https://cdn2.steamgriddb.com/grid/a.png\"}]}"};
      if (url.path() == "/api/v2/heroes/game/42")
        return {200, "{\"success\":true,\"data\":[{\"url\":\"https://cdn2.steamgriddb.com/hero/b.png\"}]}"};
      if (url.host() == "cdn2.steamgriddb.com")
        return {200, png(Qt::blue)};
      return {404, {}};
    };
    const QVector<ac::art::Target> targets{{"system22-timecris", "system22", "Time Crisis", {}},
                                           {"model2-vcop2", "model2", "Virtua Cop 2", {}}};
    {
      ac::art::Downloader downloader(t.path(), [] { return true; }, [] { return QString(" test-key "); });
      downloader.setFetcher(stub);
      downloader.setInterval(0);
      QSignalSpy done(&downloader, &ac::art::Downloader::finished);
      QVERIFY(downloader.start(targets));
      QVERIFY(!downloader.start(targets)); // One run at a time.
      QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 5000);
      QCOMPARE(done.first()[1].toStringList(), QStringList({"system22-timecris", "model2-vcop2"}));
      // Three folder lists and one snap; then SteamGridDB, which has a key: a search for Time Crisis's hero
      // (not there), and for Virtua Cop 2 a search, a grid and a hero with their pictures.
      QCOMPARE(stub->seen.size(), 10);
    }
    const auto folder = ac::art::Downloader::downloadFolder(t.path());
    QVERIFY(QFileInfo::exists(folder + "/system22-timecris/snap.png"));
    // libretro had Time Crisis, so SteamGridDB was not asked for it.
    QVERIFY(!QFileInfo::exists(folder + "/system22-timecris/grid.png"));
    QVERIFY(QFileInfo::exists(folder + "/model2-vcop2/grid.png") && QFileInfo::exists(folder + "/model2-vcop2/hero.png"));
    {
      QFile index(folder + "/index.json");
      QVERIFY(index.open(QIODevice::ReadOnly));
      const auto files = QJsonDocument::fromJson(index.readAll()).object().value("files").toObject();
      QCOMPARE(files.value("system22-timecris/snap.png").toObject().value("source").toString(), QString("libretro"));
      QCOMPARE(files.value("model2-vcop2/hero.png").toObject().value("url").toString(), QString("https://cdn2.steamgriddb.com/hero/b.png"));
    }
    for (const auto &request : stub->seen) {
      // The Hub says who it is, with where its source lives.
      QCOMPARE(request.rawHeader("User-Agent"), ac::art::Downloader::userAgent().toUtf8());
      QVERIFY(request.rawHeader("User-Agent").contains("github.com/LevonFrench/AladdinsCastle"));
      // The key goes to SteamGridDB's API only, never to the picture servers or libretro.
      QCOMPARE(request.hasRawHeader("Authorization"), request.url().host() == "www.steamgriddb.com");
      if (request.hasRawHeader("Authorization"))
        QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer test-key"));
    }
    // A new run, as after a restart: pictures, folder lists and "not there" answers all come from the cache.
    stub->seen.clear();
    ac::art::Downloader again(t.path(), [] { return true; }, [] { return QString("test-key"); });
    again.setFetcher(stub);
    again.setInterval(0);
    QSignalSpy done(&again, &ac::art::Downloader::finished);
    QVERIFY(again.start(targets));
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(stub->seen.isEmpty());
    QVERIFY(done.first()[0].toString().contains("no new pictures"));
    // What a card shows: the downloaded picture, until the user's own is there.
    const auto pick = [&](const QString &id, const QString &system, const QString &title) {
      return ac::art::choosePictures(t.path(), id, system, title, {}, nullptr);
    };
    QCOMPARE(pick("model2-vcop2", "model2", "Virtua Cop 2").card, QFileInfo(folder + "/model2-vcop2/grid.png").absoluteFilePath());
    QCOMPARE(pick("model2-vcop2", "model2", "Virtua Cop 2").hero, QFileInfo(folder + "/model2-vcop2/hero.png").absoluteFilePath());
    // Time Crisis has only a gameplay shot: it is the card, and the banner too.
    QCOMPARE(pick("system22-timecris", "system22", "Time Crisis").card, QFileInfo(folder + "/system22-timecris/snap.png").absoluteFilePath());
    QVERIFY(!pick("system22-timecris", "system22", "Time Crisis").heroBlurred);
    QVERIFY(QDir().mkpath(t.filePath("art/model2-vcop2")));
    QImage(32, 18, QImage::Format_RGB32).save(t.filePath("art/model2-vcop2/banner.jpg"));
    QCOMPARE(pick("model2-vcop2", "model2", "Virtua Cop 2").card, QFileInfo(t.filePath("art/model2-vcop2/banner.jpg")).absoluteFilePath());
    QVERIFY(pick("../escape", "model2", "x").card.isEmpty());
  }
  // Card and banner order per system, and folders of artwork on this PC before any download.
  void picturesInOrder() {
    QTemporaryDir t;
    const auto save = [&](const QString &relative, int w, int h) -> QString {
      QDir().mkpath(QFileInfo(t.filePath(relative)).absolutePath());
      QImage image(w, h, QImage::Format_RGB32);
      image.fill(Qt::gray);
      return image.save(t.filePath(relative)) ? QFileInfo(t.filePath(relative)).absoluteFilePath() : QString("not saved");
    };
    const auto got = "user/art-downloaded/";
    // Arcade: the flyer (libretro's arcade box art), then the title screen, then a gameplay shot.
    const auto flyer = save(got + QString("model3-scud/boxart.png"), 30, 40), title = save(got + QString("model3-scud/title.png"), 40, 30),
               snap = save(got + QString("model3-scud/snap.png"), 40, 30);
    auto picks = ac::art::choosePictures(t.filePath("user"), "model3-scud", "model3", "Scud Race", {"scud"}, nullptr);
    QCOMPARE(picks.card, flyer);
    QCOMPARE(picks.hero, snap);
    QFile::remove(flyer);
    QCOMPARE(ac::art::choosePictures(t.filePath("user"), "model3-scud", "model3", "Scud Race", {"scud"}, nullptr).card, title);
    // Console: the box, then the title screen; the banner falls back to the card, blurred.
    const auto box = save(got + QString("ps2-tc3/boxart.png"), 30, 42);
    picks = ac::art::choosePictures(t.filePath("user"), "ps2-tc3", "ps2", "Time Crisis 3", {"tc3"}, nullptr);
    QCOMPARE(picks.card, box);
    QCOMPARE(picks.hero, box);
    QVERIFY(picks.heroBlurred);
    // A SteamGridDB hero, when there is one, is the banner.
    const auto hero = save(got + QString("ps2-tc3/hero.png"), 96, 31);
    QCOMPARE(ac::art::choosePictures(t.filePath("user"), "ps2-tc3", "ps2", "Time Crisis 3", {"tc3"}, nullptr).hero, hero);
    // Folders of artwork already on this PC: by set name, by disc name, or loosely by title; they win over downloads.
    const auto packFlyer = save("packs/MAME Flyers/scud.png", 30, 40);
    const auto packMarquee = save("packs/arcade/marquees/scud.jpg", 70, 20);
    const auto discCover = save("packs/consoles/Covers/Time Crisis 3 (USA).png", 30, 42);
    save("packs/arcade/wheel/scud.png", 50, 20); // A logo is never a card.
    const auto pack = ac::art::LocalIndex::scan({t.filePath("packs/MAME Flyers"), t.filePath("packs/arcade"), t.filePath("packs/consoles")});
    QCOMPARE(pack.files(), 3);
    picks = ac::art::choosePictures(t.filePath("user"), "model3-scud", "model3", "Scud Race", {"scud"}, &pack);
    QCOMPARE(picks.card, packFlyer);
    QCOMPARE(pack.find({"scud"}, {"marquee"}), packMarquee);
    QCOMPARE(ac::art::choosePictures(t.filePath("user"), "ps2-tc3", "ps2", "Time Crisis 3", {"tc3"}, &pack).card, discCover);
    QCOMPARE(ac::art::pictureKind("Named_Boxarts/x.png"), QString("boxart"));
    QCOMPARE(ac::art::pictureKind("snaps/x.png"), QString("snap"));
    QCOMPARE(ac::art::pictureKind("misc/x.png"), QString("other"));
    // The user's own art for a game still comes first of all.
    const auto own = save("user/art/model3-scud/flyer.png", 30, 40);
    QCOMPARE(ac::art::choosePictures(t.filePath("user"), "model3-scud", "model3", "Scud Race", {"scud"}, &pack).card, own);
    Q_UNUSED(title);
  }
  // "Get artwork" for one game straight after another, as the Hub's buttons may: the second runs too.
  void aRunCanStartWhenTheLastOneEnds() {
    QTemporaryDir t;
    auto stub = std::make_shared<Stub>();
    stub->answer = [](const QUrl &url) -> QPair<int, QByteArray> {
      if (url.toString().endsWith("/"))
        return {200, listing({"Game One (USA)", "Game Two (USA)"})};
      return {200, png(Qt::green)};
    };
    ac::art::Downloader downloader(t.path(), [] { return true; }, [] { return QString(); });
    downloader.setFetcher(stub);
    downloader.setInterval(0);
    QSignalSpy done(&downloader, &ac::art::Downloader::finished);
    bool second = false;
    connect(&downloader, &ac::art::Downloader::finished, this, [&] {
      if (!second) {
        second = true;
        QVERIFY(downloader.start({{"wii-two", "wii", "Game Two", {}}}));
      }
    });
    QVERIFY(downloader.start({{"wii-one", "wii", "Game One", {}}}));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 5000);
    QCOMPARE(done.last()[1].toStringList(), QStringList({"wii-two"}));
    QVERIFY(QFileInfo::exists(ac::art::Downloader::downloadFolder(t.path()) + "/wii-two/boxart.png"));
  }
  void onlyRealPicturesAreKept() {
    QTemporaryDir t;
    auto stub = std::make_shared<Stub>();
    stub->answer = [](const QUrl &url) -> QPair<int, QByteArray> {
      if (url.toString().endsWith("/"))
        return {200, listing({"Time Crisis II (USA)"})};
      return {200, "<html>not a picture</html>"};
    };
    ac::art::Downloader downloader(t.path(), [] { return true; }, [] { return QString(); });
    downloader.setFetcher(stub);
    downloader.setInterval(0);
    QSignalSpy done(&downloader, &ac::art::Downloader::finished);
    QVERIFY(downloader.start({{"ps2-tc2", "ps2", "Time Crisis II", {}}}));
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(ac::art::choosePictures(t.path(), "ps2-tc2", "ps2", "Time Crisis II", {}, nullptr).card.isEmpty());
    QVERIFY(done.first()[0].toString().contains("failed"));
    // No key: SteamGridDB is never asked.
    for (const auto &request : stub->seen)
      QCOMPARE(request.url().host(), QString("thumbnails.libretro.com"));
  }
};
QTEST_GUILESS_MAIN(ArtDownloadTest)
#include "ArtDownloadTest.moc"
