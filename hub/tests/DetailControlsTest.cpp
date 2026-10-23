// SPDX-License-Identifier: GPL-3.0-only
#include "ui/DetailControlsController.h"
#include "core/catalog/CatalogLoader.h"
#include "ui/UiController.h"
#include "ui/UiSettings.h"
#include <QDirIterator>
#include <QFile>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>
#include <algorithm>
namespace {
void write(const QString &root,const QString &relative,const QByteArray &bytes){
  const auto path=QDir(root).filePath(relative);QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
  QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(bytes),bytes.size());
}
void metadataFixture(const QString &root){
  // Public project configuration only, never media or a player's folders.
  for(const auto &folder:{QString("data"),QString("games/timecris"),QString("games/vcop")}){
    QDirIterator files(QString(AC_CATALOG_ROOT)+"/"+folder,{"*.toml"},QDir::Files,QDirIterator::Subdirectories);
    while(files.hasNext()){
      const auto source=files.next(),relative=QDir(AC_CATALOG_ROOT).relativeFilePath(source),target=QDir(root).filePath(relative);
      QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));QVERIFY(QFile::copy(source,target));
    }
  }
}
void wiiFixture(const QString &root,const QString &route){
  metadataFixture(root);
  const QString game="wii-resident-evil-umbrella-chronicles";
  QDirIterator files(QString(AC_CATALOG_ROOT)+"/games/"+game,{"*.toml"},QDir::Files,QDirIterator::Subdirectories);
  while(files.hasNext()){
    const auto source=files.next(),relative=QDir(AC_CATALOG_ROOT).relativeFilePath(source);
    QFile file(source);QVERIFY(file.open(QIODevice::ReadOnly));write(root,relative,file.readAll());
  }
  QFile catalog(QString(AC_CATALOG_ROOT)+"/data/controls/wii/view-catalog.json");QVERIFY(catalog.open(QIODevice::ReadOnly));
  QJsonObject selected;
  for(const auto &entry:QJsonDocument::fromJson(catalog.readAll()).object().value("games").toArray())
    if(entry.toObject().value("game_id").toString()==game)selected=entry.toObject();
  QVERIFY(!selected.isEmpty());auto data=selected.value("data").toObject();
  if(route.isNull())data.remove("route");else data["route"]=route;
  selected["data"]=data;
  write(root,"data/controls/wii/view-catalog.json",QJsonDocument(QJsonObject{{"version","0.1"},{"games",QJsonArray{selected}}}).toJson());
}
}
class DetailControlsTest final:public QObject {
  Q_OBJECT
private slots:
  void catalogPriorityAndUserLayersReachNativeView(){
    QTemporaryDir temp;metadataFixture(temp.path());
    write(temp.path(),"packs/z-low/pack.toml","priority=1\n");write(temp.path(),"packs/a-high/pack.toml","priority=9\n");
    write(temp.path(),"packs/z-low/games/timecris/setup/controls.toml","title='Low priority'\n[future]\nlow=true\n");
    write(temp.path(),"packs/a-high/games/timecris/setup/controls.toml","title='High priority'\n[future]\nhigh=true\n");
    write(temp.path(),"user/overrides/games/timecris/setup/controls.toml","title='User controls'\n[policy]\np1_hand='left'\n[future]\nopaque=9007199254740993\n");
    write(temp.path(),"user/overrides/data/guns/arc-pistol-slide.toml","[extension]\nlayered=true\n");
    const auto catalog=ac::CatalogLoader().load(temp.path());QCOMPARE(catalog.packIds,QStringList({"z-low","a-high"}));
    ac::DetailControlsController controller;controller.configure(catalog.root,catalog.packIds);controller.selectGame("timecris","flat-test");
    QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);QCOMPARE(controller.status(),QString());QCOMPARE(controller.model()->gameId(),QString("timecris"));
    QCOMPARE(controller.model()->title(),QString("User controls"));QCOMPARE(controller.model()->primaryHand(),QString("left"));
    const auto result=controller.resolved(),future=result.value("data").toMap().value("future").toMap();
    QVERIFY(future.value("low").toBool());QVERIFY(future.value("high").toBool());QCOMPARE(future.value("opaque").toLongLong(),9007199254740993LL);
    QVERIFY(result.value("model_metadata").toMap().value("extension").toMap().value("layered").toBool());
    QCOMPARE(result.value("node_validation").toString(),QString("not-built"));QVERIFY(!result.value("gaps").toList().isEmpty());
    write(temp.path(),"user/overrides/games/timecris/setup/controls.toml","title='Edited controls'\n");
    controller.refresh();QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);QCOMPARE(controller.model()->title(),QString("Edited controls"));QCOMPARE(controller.model()->primaryHand(),QString("right"));
  }
  void staleGameResultCannotReplaceCurrentSelection(){
    auto *pool=QThreadPool::globalInstance();const auto prior=pool->maxThreadCount();pool->setMaxThreadCount(std::max(2,prior));const auto restore=qScopeGuard([&]{pool->setMaxThreadCount(prior);});
    ac::DetailControlsController controller;controller.configure(AC_CATALOG_ROOT,{});
    struct Barrier{QSemaphore entered,release,completed;std::atomic<bool> blocked=false;};
    auto barrier=std::make_shared<Barrier>();const auto release=qScopeGuard([&]{barrier->release.release();});
    controller.setOptionsSource([barrier](const QString &,const QString &){
      ac::ControlsResolveOptions options;options.useUserOverrides=false;
      options.decodedNodesForModel=[barrier](const QVariantMap &)->std::optional<QSet<QString>>{
        if(!barrier->blocked.exchange(true)){barrier->entered.release();barrier->release.acquire();barrier->completed.release();}return std::nullopt;
      };return options;
    });
    controller.selectGame("timecris","first");QPointer<QFutureWatcherBase> firstWatcher=controller.findChild<QFutureWatcherBase*>();QVERIFY(firstWatcher);
    QTRY_VERIFY_WITH_TIMEOUT(barrier->entered.available()>0,10000);
    controller.selectGame("vcop","second");QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);QCOMPARE(controller.model()->gameId(),QString("vcop"));
    barrier->release.release();QTRY_VERIFY_WITH_TIMEOUT(barrier->completed.available()>0,10000);
    QTRY_VERIFY_WITH_TIMEOUT(firstWatcher.isNull(),15000); // The stale future's queued completion has run.
    QCOMPARE(controller.model()->gameId(),QString("vcop"));QCOMPARE(controller.resolved().value("game_id").toString(),QString("vcop"));
    barrier->entered.acquire(barrier->entered.available());barrier->completed.acquire(barrier->completed.available());barrier->blocked=false;
    controller.selectGame("timecris","leaving");const auto watchers=controller.findChildren<QFutureWatcherBase*>();QVERIFY(!watchers.isEmpty());
    QPointer<QFutureWatcherBase> leavingWatcher=watchers.constLast();QTRY_VERIFY_WITH_TIMEOUT(barrier->entered.available()>0,10000);
    controller.clear();barrier->release.release();QTRY_VERIFY_WITH_TIMEOUT(barrier->completed.available()>0,10000);
    QTRY_VERIFY_WITH_TIMEOUT(leavingWatcher.isNull(),15000);
    QVERIFY(controller.model()->rows().isEmpty());QVERIFY(controller.resolved().isEmpty());QVERIFY(!controller.loading());
  }
  void nativeWiiRouteClearsDsuMapping(){
    ac::DetailControlsController controller;controller.configure(AC_CATALOG_ROOT,{});
    controller.selectGame("wii-resident-evil-umbrella-chronicles","dolphinxr","nintendo-wii");
    QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);
    QVERIFY2(controller.model()->rows().isEmpty(),"Native DolphinXR selected the DSU bridge mapping");
    QVERIFY(controller.status().contains("inside DolphinXR"));
    QVERIFY(controller.resolved().isEmpty());
    controller.clear();QVERIFY(controller.status().isEmpty());
  }
  void wiiMappingRequiresSelectedRoute_data(){
    QTest::addColumn<QString>("route");QTest::addColumn<QString>("variant");QTest::addColumn<bool>("shown");
    QTest::newRow("matching-flat")<<QString("flat-dolphin")<<QString("flat-dolphin")<<true;
    QTest::newRow("missing-route")<<QString()<<QString("flat-dolphin")<<false;
    QTest::newRow("native-catalog-on-flat")<<QString("dolphinxr")<<QString("flat-dolphin")<<false;
    QTest::newRow("unknown-catalog")<<QString("future-route")<<QString("flat-dolphin")<<false;
    QTest::newRow("unknown-selected")<<QString("future-route")<<QString("future-route")<<false;
    QTest::newRow("missing-selected")<<QString("flat-dolphin")<<QString()<<false;
  }
  void wiiMappingRequiresSelectedRoute(){
    QFETCH(QString,route);QFETCH(QString,variant);QFETCH(bool,shown);
    QTemporaryDir temp;wiiFixture(temp.path(),route);
    ac::DetailControlsController controller;controller.configure(temp.path(),{});
    controller.selectGame("wii-resident-evil-umbrella-chronicles",variant,"nintendo-wii");
    QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);
    QCOMPARE(!controller.model()->rows().isEmpty(),shown);QCOMPARE(!controller.resolved().isEmpty(),shown);
    QCOMPARE(controller.status().isEmpty(),shown);
    if(shown)QCOMPARE(controller.resolved().value("data").toMap().value("route").toString(),QString("flat-dolphin"));
  }
  void wiiRouteSwitchAndPendingResultStayNative(){
    QTemporaryDir temp;wiiFixture(temp.path(),"flat-dolphin");
    ac::DetailControlsController controller;controller.configure(temp.path(),{});
    const QString game="wii-resident-evil-umbrella-chronicles";
    controller.selectGame(game,"flat-dolphin","nintendo-wii");
    QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);QVERIFY(!controller.model()->rows().isEmpty());
    controller.selectGame(game,"dolphinxr","nintendo-wii");QVERIFY(controller.model()->rows().isEmpty());
    QVERIFY(controller.status().contains("inside DolphinXR"));QVERIFY(controller.resolved().isEmpty());
    controller.selectGame(game,"flat-dolphin","nintendo-wii");
    QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);QVERIFY(!controller.model()->rows().isEmpty());
    auto *pool=QThreadPool::globalInstance();QVERIFY2(pool->waitForDone(15000),"Global thread pool did not become idle before route test");const auto prior=pool->maxThreadCount();pool->setMaxThreadCount(1);
    struct Barrier{QSemaphore entered,release;};auto barrier=std::make_shared<Barrier>();
    const auto restore=qScopeGuard([&]{barrier->release.release();const bool done=pool->waitForDone(15000);pool->setMaxThreadCount(prior);if(!done)QTest::qFail("Global thread pool did not drain after route test",__FILE__,__LINE__);});
    pool->start([barrier]{barrier->entered.release();barrier->release.acquire();});
    QTRY_VERIFY_WITH_TIMEOUT(barrier->entered.available()>0,10000);
    controller.selectGame(game,"flat-dolphin","nintendo-wii");QVERIFY(controller.loading());
    const auto watchers=controller.findChildren<QFutureWatcherBase*>();QVERIFY(!watchers.isEmpty());QPointer<QFutureWatcherBase> staleWatcher=watchers.constLast();
    controller.selectGame(game,"dolphinxr","nintendo-wii");QVERIFY(!controller.loading());
    barrier->release.release();QVERIFY(pool->waitForDone(15000));QTRY_VERIFY_WITH_TIMEOUT(staleWatcher.isNull(),15000);
    QVERIFY(controller.model()->rows().isEmpty());QVERIFY(controller.resolved().isEmpty());
    QVERIFY(controller.status().contains("inside DolphinXR"));
  }
  void runtimeRouteTransitionClearsVisibleAndPendingMapping_data(){
    QTest::addColumn<bool>("pending");
    QTest::newRow("visible-flat")<<false;QTest::newRow("pending-flat")<<true;
  }
  void runtimeRouteTransitionClearsVisibleAndPendingMapping(){
    QFETCH(bool,pending);
    QTemporaryDir temp;wiiFixture(temp.path(),"flat-dolphin");
    ac::GameListModel games(ac::CatalogLoader().load(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(temp.path()+"/ui-settings");ac::UiController ui(&games,&filter,&settings);
    const QString game="wii-resident-evil-umbrella-chronicles";
    auto *pool=QThreadPool::globalInstance();QVERIFY2(pool->waitForDone(15000),"Global thread pool did not become idle before runtime route test");const auto prior=pool->maxThreadCount();pool->setMaxThreadCount(1);
    struct Barrier{QSemaphore entered,release;};auto barrier=std::make_shared<Barrier>();
    const auto restore=qScopeGuard([&]{barrier->release.release();const bool done=pool->waitForDone(15000);pool->setMaxThreadCount(prior);if(!done)QTest::qFail("Global thread pool did not drain after runtime route test",__FILE__,__LINE__);});
    if(pending){pool->start([barrier]{barrier->entered.release();barrier->release.acquire();});QTRY_VERIFY_WITH_TIMEOUT(barrier->entered.available()>0,10000);}
    ui.openDetail(game);ui.refreshDetailControls();QCOMPARE(ui.detail().value("variantId").toString(),QString("dolphin"));
    if(pending)QVERIFY(ui.detailControlsLoading());
    else {QTRY_VERIFY_WITH_TIMEOUT(!ui.detailControlsLoading(),10000);QVERIFY(!ui.controlsController()->model()->rows().isEmpty());}
    QPointer<QFutureWatcherBase> pendingWatcher;
    if(pending){const auto watchers=ui.controlsController()->findChildren<QFutureWatcherBase*>();QVERIFY(!watchers.isEmpty());pendingWatcher=watchers.constLast();}
    QSignalSpy changes(ui.controlsController(),&ac::DetailControlsController::changed);
    auto state=games.find(game)->runtime;state.lastPlayed+=1;games.applyRuntimeStates({state});
    auto unrelated=games.find("vcop")->runtime;unrelated.playing=true;games.applyRuntimeStates({unrelated});
    QCOMPARE(changes.count(),0); // Same effective selection must not re-resolve.
    state.selectedVariantId="dolphinxr";games.applyRuntimeStates({state});
    QCOMPARE(ui.detail().value("variantId").toString(),QString("dolphinxr"));
    QVERIFY2(ui.controlsController()->model()->rows().isEmpty(),"Runtime selected native route left flat DSU rows visible");
    QVERIFY2(ui.detailControlsStatus().contains("inside DolphinXR"),"Runtime native route did not replace pending flat mapping");
    barrier->release.release();QVERIFY(pool->waitForDone(15000));
    if(pending)QTRY_VERIFY_WITH_TIMEOUT(pendingWatcher.isNull(),15000);
    QVERIFY(ui.controlsController()->model()->rows().isEmpty());QVERIFY(!ui.detailControlsLoading());
    QVERIFY(ui.detailControlsStatus().contains("inside DolphinXR"));
    state.selectedVariantId="dolphin";games.applyRuntimeStates({state});
    QTRY_VERIFY_WITH_TIMEOUT(!ui.detailControlsLoading(),10000);QVERIFY(!ui.controlsController()->model()->rows().isEmpty());
    ui.selectVariant("dolphin");QTRY_VERIFY_WITH_TIMEOUT(!ui.detailControlsLoading(),10000);
    changes.clear();state.selectedVariantId="dolphinxr";games.applyRuntimeStates({state});
    QCOMPARE(ui.detail().value("variantId").toString(),QString("dolphin"));QCOMPARE(changes.count(),0);
    QVERIFY(!ui.controlsController()->model()->rows().isEmpty());
  }
  void canonicalRouteKeepsOriginalHostVariant(){
    QTemporaryDir temp;wiiFixture(temp.path(),"flat-dolphin");
    ac::DetailControlsController controller;controller.configure(temp.path(),{});QString selected;
    controller.setOptionsSource([&](const QString &,const QString &variant){selected=variant;return ac::ControlsResolveOptions{};});
    controller.selectGame("wii-resident-evil-umbrella-chronicles","dolphin","nintendo-wii","flat-dolphin");
    QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);
    QCOMPARE(selected,QString("dolphin"));QVERIFY(!controller.model()->rows().isEmpty());
    QCOMPARE(controller.resolved().value("data").toMap().value("route").toString(),QString("flat-dolphin"));
  }
  void explicitSyntheticEvidenceUsesExactLayeredMetadata(){
    QTemporaryDir temp;metadataFixture(temp.path());write(temp.path(),"user/overrides/data/guns/arc-pistol-slide.toml","[extension]\nrevision='synthetic-current'\n");
    ac::DetailControlsController controller;controller.configure(temp.path(),{});auto exact=std::make_shared<std::atomic<bool>>(false);
    controller.setOptionsSource([exact](const QString &game,const QString &variant){
      ac::ControlsResolveOptions options;
      if(game=="timecris"&&variant=="synthetic-backend")options.backend=QVariantMap{{"guns",1},{"players",1},{"shared_view",true},{"control",QVariantList{QVariantMap{{"kind","gun"},{"semantic","trigger"},{"player",0}}}}};
      options.decodedNodesForModel=[exact](const QVariantMap &metadata)->std::optional<QSet<QString>>{
        *exact=metadata.value("extension").toMap().value("revision")=="synthetic-current";return QSet<QString>{"pivot_trigger","muzzle","fx_laser"};
      };return options;
    });
    controller.selectGame("timecris","synthetic-backend");QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);QCOMPARE(controller.status(),QString());QVERIFY(exact->load());
    QCOMPARE(controller.resolved().value("node_validation").toString(),QString("built-references-checked"));
    controller.selectGame("missing-synthetic-game","flat");QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);QVERIFY(!controller.status().isEmpty());QVERIFY(controller.model()->rows().isEmpty());QVERIFY(controller.resolved().isEmpty());
  }
};
QTEST_GUILESS_MAIN(DetailControlsTest)
#include "DetailControlsTest.moc"
