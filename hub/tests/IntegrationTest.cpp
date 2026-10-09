// SPDX-License-Identifier: GPL-3.0-only
#include "app/HubServices.h"
#include "core/catalog/CatalogLoader.h"
#include "core/install/Support.h"
#include <QTemporaryDir>
#include <QDateTime>
#include <QtTest>
namespace {
ac::CatalogData syntheticCatalog(const QString &root){
    auto data=ac::CatalogLoader().load(AC_CATALOG_ROOT);
    auto game=*data.find("timecris");game.id="synthetic";game.folder=root+"/games/synthetic";
    game.raw["media"]=ac::Json::array({{{"kind","mame-romset"},{"set","synthetic"}}});
    game.raw["id"]="synthetic";game.errors.clear();game.runtime={};game.runtime.gameId=game.id;
    game.roles["id"]=game.id;game.roles["title"]="Synthetic integration game";
    game.variants.clear();ac::Variant variant;variant.id="flat-mame";variant.title="Play in MAME";
    variant.quality="flat";variant.status="stable";variant.generated=true;variant.media={"synthetic"};variant.tools={"mame"};game.variants<<variant;
    data.games={game};return data;
}
void receipt(const QString &root,const QString &media){
    const QFileInfo file(media);const QFileInfo tool(QCoreApplication::applicationFilePath());
    ac::Json json{{"bindings",ac::Json::array({{{"gameId","synthetic"},{"requirementId","synthetic"},{"path",media.toStdString()},{"identity","synthetic"},{"proof","synthetic-fixture"},{"verified",true}}})},
                  {"tools",ac::Json::array({{{"id","mame"},{"path",tool.absoluteFilePath().toStdString()},{"verified",true},{"size",tool.size()},{"mtime",tool.lastModified().toMSecsSinceEpoch()}}})},
                  {"files",ac::Json::array({{{"path",media.toStdString()},{"size",file.size()},{"mtime",file.lastModified().toMSecsSinceEpoch()}}})}};
    ac::install::atomicWrite(root+"/user/cache/scan-bindings.json",QByteArray::fromStdString(json.dump()));
    ac::install::atomicWrite(root+"/user/last-played.json","{\"synthetic\":12345}");
}
}
class IntegrationTest:public QObject{
 Q_OBJECT
private slots:
 void restoreArtAndScanSignals(){
    QTemporaryDir temp;const auto media=temp.filePath("fixture.zip");ac::install::atomicWrite(media,"synthetic metadata only");receipt(temp.path(),media);
    ac::GameListModel games(syntheticCatalog(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);
    ac::HubServices services(&games,&filter,&ui,&settings,temp.path());
    QVERIFY(games.find("synthetic")->runtime.mediaFound.contains("synthetic"));
    QVERIFY(games.find("synthetic")->runtime.toolsOk.contains("mame"));QCOMPARE(games.find("synthetic")->runtime.lastPlayed,qint64(12345));
    const auto art=services.artResolver()->resolve("synthetic","banner",{320,180});QVERIFY(!art.image.isNull());QCOMPARE(art.image.size(),QSize(320,180));
    QCOMPARE(games.find("synthetic")->roles.value("state").toInt(),int(ac::GameState::Installed));
    QSignalSpy installs(&ui,&ac::UiController::installRequested),plays(&ui,&ac::UiController::playRequested);
    ac::install::atomicWrite(media,"changed after saved proof with a different size");
    ui.primary("synthetic");QCOMPARE(installs.count(),0);QCOMPARE(plays.count(),1);QVERIFY(!services.playing());
    QSignalSpy scanChanges(&ui,&ac::UiController::scanChanged);ui.scan({temp.filePath("empty-root")});
    QTRY_VERIFY_WITH_TIMEOUT(!ui.scanning()&&scanChanges.count()>=2,10000);
    QCOMPARE(ui.status(),QString("Scan complete."));QVERIFY(ui.artRevision()>0);
    QVERIFY(games.find("synthetic")->runtime.mediaFound.isEmpty());QCOMPARE(games.find("synthetic")->runtime.lastPlayed,qint64(12345));
    ui.play("synthetic","flat-mame");QVERIFY(ui.status().contains("No flat variant"));QVERIFY(!services.playing());
 }
 void changedMediaIsNotReadyOnRestore(){
    QTemporaryDir temp;const auto media=temp.filePath("fixture.zip");ac::install::atomicWrite(media,"before");receipt(temp.path(),media);ac::install::atomicWrite(media,"different size after proof");
    ac::GameListModel games(syntheticCatalog(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);
    ac::HubServices services(&games,&filter,&ui,&settings,temp.path());QVERIFY(games.find("synthetic")->runtime.mediaFound.isEmpty());
 }
};
QTEST_MAIN(IntegrationTest)
#include "IntegrationTest.moc"
