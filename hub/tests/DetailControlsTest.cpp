// SPDX-License-Identifier: GPL-3.0-only
#include "ui/DetailControlsController.h"
#include "core/catalog/CatalogLoader.h"
#include <QDirIterator>
#include <QFile>
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
    controller.selectGame("timecris","first");QTRY_VERIFY_WITH_TIMEOUT(barrier->entered.available()>0,10000);
    controller.selectGame("vcop","second");QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(),10000);QCOMPARE(controller.model()->gameId(),QString("vcop"));
    barrier->release.release();QTRY_VERIFY_WITH_TIMEOUT(barrier->completed.available()>0,10000);QTest::qWait(50);
    QCOMPARE(controller.model()->gameId(),QString("vcop"));QCOMPARE(controller.resolved().value("game_id").toString(),QString("vcop"));
    barrier->entered.acquire(barrier->entered.available());barrier->completed.acquire(barrier->completed.available());barrier->blocked=false;
    controller.selectGame("timecris","leaving");QTRY_VERIFY_WITH_TIMEOUT(barrier->entered.available()>0,10000);
    controller.clear();barrier->release.release();QTRY_VERIFY_WITH_TIMEOUT(barrier->completed.available()>0,10000);QTest::qWait(50);
    QVERIFY(controller.model()->rows().isEmpty());QVERIFY(controller.resolved().isEmpty());QVERIFY(!controller.loading());
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
