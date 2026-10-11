// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogLoader.h"
#include "core/steam/Steam.h"
#include "AuthoredSteamFixture.h"
#include <optional>
#include "ui/Theme.h"
#include "ui/UiController.h"
#include "ui/UiSettings.h"
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlIncubationController>
#include <QScopeGuard>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTimer>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QJSValue>
#include <QQuickImageProvider>
#include <QPainter>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QAtomicInteger>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDirIterator>
#include <QImageReader>
#include <ctime>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif
namespace {
QStringList warnings;
int cardCount(QQuickItem *item,bool visibleOnly=false){if(!item)return 0;int count=0;if(item->objectName()=="cardPrimary"){auto p=item->mapToScene(QPointF());if(!visibleOnly||(item->isVisible()&&p.y()>100&&p.y()<690))++count;}for(auto child:item->childItems())count+=cardCount(child,visibleOnly);return count;}
QList<QQuickItem *> namedItems(QQuickItem *item,const QString &name){QList<QQuickItem *> result;if(!item)return result;if(item->objectName()==name)result<<item;for(auto child:item->childItems())result.append(namedItems(child,name));return result;}
void logMessage(QtMsgType type,const QMessageLogContext &,const QString &message){if(type==QtWarningMsg||type==QtCriticalMsg)warnings<<message;fprintf(stderr,"%s\n",qPrintable(message));}
double cpuSeconds(){
#ifdef Q_OS_WIN
 FILETIME creation{},exit{},kernel{},user{};if(GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user)){ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return static_cast<double>(k.QuadPart+u.QuadPart)/10000000.0;}
#endif
 return static_cast<double>(std::clock())/CLOCKS_PER_SEC;
}
qint64 memoryBytes(){
#ifdef Q_OS_WIN
 PROCESS_MEMORY_COUNTERS counters{};if(GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters)))return static_cast<qint64>(counters.WorkingSetSize);
#endif
 return 0;
}
}
class OwnedArtProvider:public QQuickImageProvider {
 public:
 explicit OwnedArtProvider(QColor color):QQuickImageProvider(QQuickImageProvider::Image),accent(color){}
 QAtomicInteger<int> requests=0;
 QImage requestImage(const QString &,QSize *size,const QSize &requested)override{
  requests.fetchAndAddRelaxed(1);QImage image(requested.isValid()?requested:QSize(320,160),QImage::Format_ARGB32_Premultiplied);image.fill(accent);QPainter painter(&image);painter.setPen(accent.lighter(160));painter.drawEllipse(image.rect().adjusted(20,20,-20,-20));if(size)*size=image.size();return image;
 }
 private:QColor accent;
};
// Observe the real QML call and run the production preparation seam. Steam
// discovery/account enumeration and file preview are deliberately absent here.
class SteamPreparationProbe:public QObject {
 Q_OBJECT
 Q_PROPERTY(QString toolPlan MEMBER empty CONSTANT)
 Q_PROPERTY(QString removalPlan MEMBER empty CONSTANT)
 Q_PROPERTY(bool canRemoveSteam MEMBER inactive CONSTANT)
 Q_PROPERTY(bool launchBusy MEMBER inactive CONSTANT)
 Q_PROPERTY(bool preparing MEMBER inactive CONSTANT)
 Q_PROPERTY(bool steamRemoving MEMBER inactive CONSTANT)
 Q_PROPERTY(QStringList steamAccounts MEMBER accounts CONSTANT)
 Q_PROPERTY(QString steamPreview MEMBER preview CONSTANT)
 Q_PROPERTY(bool steamWriteReady MEMBER inactive CONSTANT)
public:
 bool inactive=false;
 QString empty;
 QStringList accounts;
 QString preview="Synthetic shortcut preparation only";
 ac::test::AuthoredSteamFixture *fixture=nullptr;
 QString requestedGame,requestedVariant,error;
 std::optional<ac::steam::Shortcut> prepared;
 Q_INVOKABLE bool beginSteam(const QString &gameId,const QString &variantId) {
  requestedGame=gameId;requestedVariant=variantId;
  try {
   prepared=ac::steam::prepareGameShortcut(fixture->catalog,gameId,variantId,
       fixture->temp.path(),fixture->hubExecutable,fixture->bindings);
  } catch(const ac::install::Error &e) { error=e.code; }
  return prepared.has_value();
 }
 Q_INVOKABLE void previewSteam(const QString &) {}
 Q_INVOKABLE void approveSteamWrite() {}
signals:
 void removalPlanChanged();
};
class UiTest:public QObject {
 Q_OBJECT
 QTemporaryDir user;
 std::unique_ptr<ac::GameListModel> games;
 ac::FilterSortModel filter;
 std::unique_ptr<ac::Theme> theme;
 std::unique_ptr<ac::UiSettings> settings;
 std::unique_ptr<ac::UiController> ui;
 std::unique_ptr<QQmlApplicationEngine> engine;
 QQuickWindow *window=nullptr;
 QObject *root=nullptr;
 private slots:
 void initTestCase(){
  qInstallMessageHandler(logMessage);
  auto catalog=ac::CatalogLoader().load(AC_CATALOG_ROOT);QCOMPARE(catalog.games.size(),413);
  theme=std::make_unique<ac::Theme>(catalog.theme);qmlRegisterSingletonInstance("AladdinsCastle.Hub",1,0,"Theme",theme.get());
  games=std::make_unique<ac::GameListModel>(std::move(catalog));filter.setSourceModel(games.get());settings=std::make_unique<ac::UiSettings>(user.path());ui=std::make_unique<ac::UiController>(games.get(),&filter,settings.get());engine=std::make_unique<QQmlApplicationEngine>();
  auto context=engine->rootContext();context->setContextProperty("catalogGameCount",413);context->setContextProperty("gameModel",games.get());context->setContextProperty("gameFilter",&filter);context->setContextProperty("uiSettings",settings.get());context->setContextProperty("uiController",ui.get());
  engine->load(QUrl("qrc:/qt/qml/AladdinsCastle/Hub/DesktopShell.qml"));QVERIFY(!engine->rootObjects().isEmpty());window=qobject_cast<QQuickWindow *>(engine->rootObjects().first());QVERIFY(window);root=window->findChild<QObject *>("hubRoot");QVERIFY(root);QTest::qWait(300);
  auto list=window->findChild<QQuickItem *>("sectionedGrid");QVERIFY(list);QVERIFY(list->mapToScene(QPointF()).y()<200);QVERIFY(list->height()>400);
  int visibleCards=cardCount(window->contentItem(),true);QVERIFY(visibleCards>0);
  if(!qEnvironmentVariable("AC_UI_BENCH_OUTPUT").isEmpty()){auto out=QFileInfo(qEnvironmentVariable("AC_UI_BENCH_OUTPUT")).dir();QVERIFY(window->grabWindow().save(out.filePath("d-startup.png")));}
 }
 void themeAndSettings(){QCOMPARE(theme->get("motion.hover_dwell").toInt(),600);QCOMPARE(theme->get("motion.preview_scale").toDouble(),1.4);QVERIFY(theme->get("gradient.explore_banner_fade.stop").toList().size()==8);QVERIFY(settings->set("reduceMotion",true));QVERIFY(settings->saveGame("synthetic",{{"laser","on"},{"gun_pitch",-10}}));ac::UiSettings copy(user.path());QCOMPARE(copy.get("reduceMotion").toBool(),true);QCOMPARE(copy.game("synthetic").value("gun_pitch").toInt(),-10);settings->set("reduceMotion",false);}
 void pinballTabRealScene(){
  root->setProperty("view","List");root->setProperty("vrOverlayMode",true);
  const auto tabs=namedItems(window->contentItem(),"pinballTab");QCOMPARE(tabs.size(),1);
  const auto tab=tabs.first();QVERIFY(tab->height()>=48);
  auto p=tab->mapToScene(QPointF(tab->width()/2,tab->height()/2));QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());
  QCOMPARE(root->property("view").toString(),QString("Pinball"));
  QVariantList rows;for(int i=0;i<25;++i)rows<<QVariantMap{{"id",QString::number(i)},{"title",QString("Fixture %1").arg(i)},{"maker","Synthetic"},{"year",2000},{"vrReady",i!=24},{"evidence",QStringList{"marked by you"}},{"romState","needs no ROM"}};
  QVariantMap data{{"total",25},{"vrReady",24},{"tables",rows}};
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"pinballLoaded",Q_ARG(QVariantMap,data)));
  QTRY_VERIFY(!namedItems(window->contentItem(),"pinballGrid").isEmpty());
  auto grid=namedItems(window->contentItem(),"pinballGrid").first();QTRY_COMPARE(grid->property("count").toInt(),24);
  auto tiles=namedItems(window->contentItem(),"pinballTile");QVERIFY(!tiles.isEmpty());
  auto tile=tiles.first();p=tile->mapToScene(QPointF(tile->width()/2,tile->height()/2));
  QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());QTest::mouseMove(window,p.toPoint()-QPoint(0,140),80);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,p.toPoint()-QPoint(0,140));
  QVERIFY(namedItems(window->contentItem(),"pinballDetailTitle").isEmpty());
  grid->setProperty("contentY",0);QTest::qWait(40);tiles=namedItems(window->contentItem(),"pinballTile");tile=tiles.first();p=tile->mapToScene(QPointF(tile->width()/2,tile->height()/2));
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());QTRY_VERIFY(!namedItems(window->contentItem(),"pinballDetailTitle").isEmpty());
  QCOMPARE(ui->property("pinballSelection").toMap().value("romState").toString(),QString("needs no ROM"));
  QTRY_VERIFY(!namedItems(window->contentItem(),"universalControlsView").isEmpty());
  auto model=ui->property("pinballControls").value<QObject *>();QVERIFY(model);
  QCOMPARE(model->property("gameId").toString(),QString("pinball-vpx-openxr"));
  QVERIFY(!model->property("rows").toList().isEmpty());
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"selectPinball",Q_ARG(QString,"")));
  root->setProperty("vrOverlayMode",false);root->setProperty("view","List");
 }
 void sharedNowPlayingRealScene(){
  root->setProperty("view","List"); root->setProperty("vrOverlayMode",true);
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"launchQueued",Q_ARG(QString,"timecris"),Q_ARG(QString,"flat"),Q_ARG(QString,"")));
  ui->launchStarted("timecris");
  QTRY_VERIFY(!namedItems(window->contentItem(),"nowPlayingExit").isEmpty());
  auto exit=namedItems(window->contentItem(),"nowPlayingExit").first();
  QVERIFY(exit->isVisible()); QVERIFY(exit->height()>=48);
  auto recentre=namedItems(window->contentItem(),"nowPlayingRecentre").first();
  QVERIFY(!recentre->isEnabled());
  QSignalSpy close(ui.get(),SIGNAL(closeLaunchRequested()));
  auto p=exit->mapToScene(QPointF(exit->width()/2,exit->height()/2));
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());
  QCOMPARE(close.size(),1); QVERIFY(!exit->isEnabled());
  ui->launchFinished("unrelated",QString()); QVERIFY(!ui->property("nowPlaying").toMap().isEmpty());
  ui->launchFinished("timecris",QString()); QTRY_VERIFY(!exit->isVisible());
  QCOMPARE(root->property("view").toString(),QString("List"));
  QSignalSpy replay(ui.get(),&ac::UiController::playRequested);
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"launchQueued",Q_ARG(QString,"timecris"),Q_ARG(QString,"flat"),Q_ARG(QString,"")));
  ui->launchStarted("timecris");
  auto restart=namedItems(window->contentItem(),"nowPlayingRestart").first();
  p=restart->mapToScene(QPointF(restart->width()/2,restart->height()/2));
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());
  ui->launchFinished("timecris","Close failed"); QCOMPARE(replay.size(),0);
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"launchQueued",Q_ARG(QString,"timecris"),Q_ARG(QString,"flat"),Q_ARG(QString,"")));
  ui->launchStarted("timecris");
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"restartLaunch"));
  ui->launchFinished("timecris",QString()); QCOMPARE(replay.size(),1);
  QCOMPARE(replay[0][1].toString(),QString("flat")); root->setProperty("vrOverlayMode",false);
 }
 void desktopDetailEmbedsNativeControls(){
  ui->openDetail("timecris");root->setProperty("view","Detail");
  QTRY_VERIFY_WITH_TIMEOUT(!namedItems(window->contentItem(),"universalControlsView").isEmpty(),3000);
  const auto view=namedItems(window->contentItem(),"universalControlsView").first();
  auto model=view->property("controlsModel").value<QObject *>();QVERIFY(model);
  QCOMPARE(model->property("gameId").toString(),QString("timecris"));
  QVERIFY(!model->property("rows").toList().isEmpty());
  QVERIFY(model->property("previewAvailable").toBool());
  QCOMPARE(model->property("modelId").toString(),QString("arc-pistol-slide"));
  const auto preview=QDir(AC_CATALOG_ROOT).absoluteFilePath("assets/guns/preview/arc-pistol-slide.png");
  QCOMPARE(model->property("previewImage").toUrl(),QUrl::fromLocalFile(preview));
  QImageReader reader(preview);QVERIFY(reader.canRead());QCOMPARE(reader.size(),QSize(512,512));
  QVERIFY(!reader.read().isNull());
  const auto images=namedItems(view,"gunPreviewImage");QCOMPARE(images.size(),1);
  QTRY_VERIFY(images.first()->isVisible());
  QCOMPARE(images.first()->property("source").toUrl(),model->property("previewImage").toUrl());
  QVERIFY(!model->property("markers").toList().isEmpty());
  root->setProperty("view","List");
  QTRY_VERIFY(model->property("gameId").toString().isEmpty());
 }
 void desktopControlsKeepMappingsWhenPreviewIsMissing(){
  ui->openDetail("timecris");root->setProperty("view","Detail");
  auto controls=ui->controlsController();
  QTRY_COMPARE_WITH_TIMEOUT(controls->model()->gameId(),QString("timecris"),10000);
  QTemporaryDir emptyPreviews(user.filePath("empty-previews-XXXXXX"));QVERIFY(emptyPreviews.isValid());
  ac::ControlsViewModel missingPreview;
  QVERIFY(missingPreview.loadResolved(controls->resolved(),emptyPreviews.path()));
  QVERIFY(!missingPreview.previewAvailable());QVERIFY(missingPreview.previewImage().isEmpty());
  QVERIFY(missingPreview.markers().isEmpty());QVERIFY(!missingPreview.rows().isEmpty());
  for(const auto &row:missingPreview.rows())QVERIFY(!row.toMap().value("anchorAvailable").toBool());
  QQmlComponent component(engine.get(),QUrl("qrc:/controls/ControlsView.qml"));
  std::unique_ptr<QObject> object(component.createWithInitialProperties({{"controlsModel",QVariant::fromValue(&missingPreview)}}));
  QVERIFY2(object,qPrintable(component.errorString()));auto view=qobject_cast<QQuickItem *>(object.get());QVERIFY(view);
  QCOMPARE(namedItems(view,"controlsCallout").size(),missingPreview.rows().size());
  const auto images=namedItems(view,"gunPreviewImage");QCOMPARE(images.size(),1);QVERIFY(!images.first()->isVisible());
  root->setProperty("view","List");QTRY_VERIFY(controls->model()->gameId().isEmpty());
 }
 void selectedNativeWiiRouteExplainsMappingOnRealPage(){
  QTemporaryDir fixture;
  const QString game="wii-resident-evil-umbrella-chronicles";
  const auto originalRuntime=games->find(game)->runtime;
  const auto restoreRuntime=qScopeGuard([&]{games->applyRuntimeStates({originalRuntime});});
  for(const auto &folder:{QString("data"),QString("games/timecris"),QString("games/vcop"),"games/"+game}){
   QDirIterator files(QString(AC_CATALOG_ROOT)+"/"+folder,{"*.toml"},QDir::Files,QDirIterator::Subdirectories);
   while(files.hasNext()){
    const auto source=files.next(),target=QDir(fixture.path()).filePath(QDir(AC_CATALOG_ROOT).relativeFilePath(source));
    QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));QVERIFY(QFile::copy(source,target));
   }
  }
  QFile catalog(QString(AC_CATALOG_ROOT)+"/data/controls/wii/view-catalog.json");QVERIFY(catalog.open(QIODevice::ReadOnly));
  QJsonObject selected;
  for(const auto &row:QJsonDocument::fromJson(catalog.readAll()).object().value("games").toArray())
   if(row.toObject().value("game_id").toString()==game)selected=row.toObject();
  QVERIFY(!selected.isEmpty());auto data=selected.value("data").toObject();data["route"]="flat-dolphin";selected["data"]=data;
  QFile output(QDir(fixture.path()).filePath("data/controls/wii/view-catalog.json"));QVERIFY(output.open(QIODevice::WriteOnly));
  output.write(QJsonDocument(QJsonObject{{"version","0.1"},{"games",QJsonArray{selected}}}).toJson());output.close();
  ui->controlsController()->configure(fixture.path(),{});
  const auto restore=qScopeGuard([&]{root->setProperty("view","List");ui->controlsController()->configure(AC_CATALOG_ROOT,{});});
  ui->openDetail("wii-resident-evil-umbrella-chronicles");ui->selectVariant("dolphin");
  root->setProperty("view","Detail");
  QTRY_VERIFY2_WITH_TIMEOUT(!ui->controlsController()->model()->rows().isEmpty(),qPrintable(ui->detailControlsStatus()),10000);
  QTRY_COMPARE(namedItems(window->contentItem(),"universalControlsView").size(),1);
  QQuickItem *native=nullptr;
  QTRY_VERIFY(!namedItems(window->contentItem(),"detailVariant").isEmpty());
  for(auto *item:namedItems(window->contentItem(),"detailVariant"))
    if(item->property("modelData").toMap().value("id")=="dolphinxr")native=item;
  QVERIFY(native);QVERIFY(native->isVisible());
  auto page=window->findChild<QQuickItem *>("detailPage");QVERIFY(page);
  auto flick=page->property("contentItem").value<QQuickItem *>();QVERIFY(flick);
  auto click=[&](QQuickItem *button){
   const auto before=button->mapToScene(QPointF(button->width()/2,button->height()/2));
   flick->setProperty("contentY",qMax(0.0,before.y()+flick->property("contentY").toDouble()-window->height()/2));
   QTest::qWait(50);
   const auto point=button->mapToScene(QPointF(button->width()/2,button->height()/2));
   QVERIFY(point.y()>0&&point.y()<window->height());
   QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
  };
  click(native);
  QTRY_COMPARE(ui->detail().value("variantId").toString(),QString("dolphinxr"));
  QTRY_VERIFY_WITH_TIMEOUT(!ui->detailControlsLoading(),10000);
  QVERIFY2(ui->controlsController()->model()->rows().isEmpty(),"Native route displays DSU bridge rows");
  QTRY_VERIFY(window->findChild<QObject *>("detailControlsStatus"));
  auto explanation=window->findChild<QObject *>("detailControlsStatus");
  QVERIFY(explanation->property("visible").toBool());
  QVERIFY(explanation->property("text").toString().contains("inside DolphinXR"));
  QVERIFY(namedItems(window->contentItem(),"universalControlsView").isEmpty());
  const auto rows=ui->detail().value("controls").toList();QCOMPARE(rows.size(),1);
  QVERIFY(rows.first().toMap().value("notes").toString().contains("inside DolphinXR"));
  auto clickRoute=[&](const QString &id){
   for(auto *item:namedItems(window->contentItem(),"detailVariant"))
    if(item->property("modelData").toMap().value("id").toString()==id){
     click(item);return;
    }
   QFAIL("Route button not found");
  };
  clickRoute("dolphin");QTRY_COMPARE(ui->detail().value("variantId").toString(),QString("dolphin"));
  QTRY_VERIFY_WITH_TIMEOUT(!ui->detailControlsLoading(),10000);QVERIFY(!ui->controlsController()->model()->rows().isEmpty());
  QTRY_COMPARE(namedItems(window->contentItem(),"universalControlsView").size(),1);
  clickRoute("dolphinxr");QTRY_COMPARE(ui->detail().value("variantId").toString(),QString("dolphinxr"));
  QTRY_VERIFY(ui->controlsController()->model()->rows().isEmpty());QVERIFY(ui->detailControlsStatus().contains("inside DolphinXR"));
  QVERIFY(namedItems(window->contentItem(),"universalControlsView").isEmpty());
  ui->openDetail(game); // Clear the explicit choice: follow effective runtime selection.
  QTRY_VERIFY_WITH_TIMEOUT(!ui->controlsController()->model()->rows().isEmpty(),10000);
  auto runtime=originalRuntime;runtime.selectedVariantId="dolphinxr";games->applyRuntimeStates({runtime});
  QTRY_COMPARE(ui->detail().value("variantId").toString(),QString("dolphinxr"));
  QTRY_VERIFY(ui->controlsController()->model()->rows().isEmpty());
  QTRY_VERIFY(explanation->property("text").toString().contains("inside DolphinXR"));
  QVERIFY(namedItems(window->contentItem(),"universalControlsView").isEmpty());
  root->setProperty("view","List");
 }
 void catalogBadgesDescribeMetadata(){
  QCOMPARE(ui->vrLabel(1),QString("TRUE 3D · CATALOG"));
  QCOMPARE(ui->vrLabel(2),QString("THEATRE · CATALOG"));
 }
 void catalogMetadataFitsSmallCard(){
  QQmlComponent component(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/GameCard.qml"));
  std::unique_ptr<QObject> object(component.create());QVERIFY2(object,qPrintable(component.errorString()));
  auto card=qobject_cast<QQuickItem *>(object.get());QVERIFY(card);card->setParentItem(window->contentItem());
  card->setProperty("sizeValue","S");card->setWidth(card->implicitWidth());card->setHeight(card->implicitHeight());
  const auto pad=theme->get("space.card_inner_pad").toDouble()*card->property("factor").toDouble();
  double furthest=0;QString longest;int qualified=0;
  for(const auto &game:games->records()){
   const auto badge=game.roles.value("vrBadge").toInt();if(badge!=1&&badge!=2)continue;
   ++qualified;card->setProperty("game",game.roles);QCoreApplication::processEvents();
   const auto players=game.roles.value("players").toInt();
   const auto text=ui->vrLabel(badge)+"  "+(players>1?QString::number(players)+"P":QString())+"  "+game.roles.value("controlsLabel").toString();
   QQuickItem *metadata=nullptr;for(auto item:namedItems(card,QString()))if(item->property("text").toString()==text){metadata=item;break;}
   QVERIFY(metadata);const auto right=metadata->mapToItem(card,QPointF(metadata->width(),0)).x();
   if(right>furthest){furthest=right;longest=game.id+": "+text;}
   QVERIFY(QFontMetricsF(metadata->property("font").value<QFont>()).horizontalAdvance(ui->vrLabel(badge))<=card->width()-2*pad);
  }
  QVERIFY(qualified>0);qInfo().noquote()<<"Furthest qualified small-card metadata:"<<longest<<"right edge"<<furthest<<"inner edge"<<card->width()-pad;
  QVERIFY2(furthest<=card->width()-pad+0.5,qPrintable(longest));
 }
 void pinnedAuthoredSetupIsAvailableOnRealDetailPage(){
  auto catalog=ac::CatalogLoader().load(AC_CATALOG_ROOT);
  auto game=*catalog.find("wii-resident-evil-umbrella-chronicles");
  ac::Variant authored;authored.id="dolphinxr";authored.title="DolphinXR (experimental VR)";authored.status="wip";authored.quality="true3d";authored.media={"disc"};
  authored.raw={{"status","wip"},{"quality","true3d"},{"version","v0.4"},{"launch",{{"exe","${install_dir}/Dolphin.exe"},{"exe_sha256",std::string(64,'a')},{"args",ac::Json::array({"--user","${install_dir}/User"})},{"runtime","steamvr"}}}};
  game.variants.prepend(authored);game.install={{"format",1},{"variant",{{"dolphinxr",authored.raw}}}};game.hasRecipe=true;game.runtime.mediaFound={"disc"};
  catalog.games={game};ac::GameListModel model(catalog);ac::FilterSortModel localFilter;localFilter.setSourceModel(&model);
  ac::UiController localUi(&model,&localFilter,settings.get());localUi.openDetail(game.id);localUi.selectVariant(authored.id);
  QVERIFY(localUi.detail().value("m1Available").toBool());
  QSignalSpy install(&localUi,&ac::UiController::installRequested),play(&localUi,&ac::UiController::playRequested);
  QQmlContext context(engine->rootContext());context.setContextProperty("uiController",&localUi);
  QQmlComponent component(engine.get());component.setData("import QtQuick; import AladdinsCastle.Hub; DetailPage {width:1120;height:720}",QUrl());
  std::unique_ptr<QQuickItem> page(qobject_cast<QQuickItem *>(component.create(&context)));QVERIFY2(page,qPrintable(component.errorString()));
  page->setParentItem(window->contentItem());page->setZ(100);QTest::qWait(50);
  auto primary=page->findChild<QQuickItem *>("detailPrimary");QVERIFY(primary);QVERIFY(primary->isEnabled());
  auto flick=page->property("contentItem").value<QQuickItem *>();QVERIFY(flick);QVERIFY(flick->property("contentY").isValid());
  const auto initial=primary->mapToScene({primary->width()/2,primary->height()/2});
  flick->setProperty("contentY",qMax(0.0,initial.y()-window->height()/2));QTest::qWait(50);
  const auto point=primary->mapToScene({primary->width()/2,primary->height()/2});
  QVERIFY(point.x()>0&&point.x()<window->width()&&point.y()>0&&point.y()<window->height());
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
  QCOMPARE(install.count(),1);localUi.play(game.id,authored.id);QCOMPARE(play.count(),1);
 }
 void authoredSteamActionUsesActualQmlSelection(){
  ac::test::AuthoredSteamFixture fixture;
  ac::GameListModel localGames(fixture.catalog);ac::FilterSortModel localFilter;localFilter.setSourceModel(&localGames);
  ac::UiSettings localSettings(fixture.temp.filePath("user"));ac::UiController localUi(&localGames,&localFilter,&localSettings);
  localUi.openDetail("wii-synthetic");localUi.selectVariant("flat-dolphin");
  SteamPreparationProbe probe;probe.fixture=&fixture;
  QQmlContext context(engine->rootContext());
  context.setContextProperty("gameModel",&localGames);context.setContextProperty("gameFilter",&localFilter);
  context.setContextProperty("uiController",&localUi);context.setContextProperty("uiSettings",&localSettings);
  context.setContextProperty("hubServices",&probe);context.setContextProperty("catalogGameCount",1);
  QQmlComponent component(engine.get());
  component.setData("import QtQuick; import AladdinsCastle.Hub; HubRoot {width:1120;height:720;view: \"Detail\"}",QUrl());
  std::unique_ptr<QQuickItem> scene(qobject_cast<QQuickItem *>(component.create(&context)));
  QVERIFY2(scene,qPrintable(component.errorString()));scene->setParentItem(window->contentItem());scene->setZ(100);
  QTRY_VERIFY(scene->findChild<QQuickItem *>("detailPage"));
  auto page=scene->findChild<QQuickItem *>("detailPage");
  auto flick=page->property("contentItem").value<QQuickItem *>();QVERIFY(flick);
  const auto click=[&](QQuickItem *button){
   const auto before=button->mapToScene({button->width()/2,button->height()/2});
   flick->setProperty("contentY",qMax(0.0,before.y()+flick->property("contentY").toDouble()-window->height()/2));
   QTest::qWait(50);
   const auto point=button->mapToScene({button->width()/2,button->height()/2});
   QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
  };
  QQuickItem *variantButton=nullptr;
  for(auto item:namedItems(page,QString()))
   if(item->property("text").toString().startsWith("DolphinXR (experimental VR)")) { variantButton=item;break; }
  QVERIFY(variantButton);click(variantButton);
  QTRY_COMPARE(localUi.detail().value("variantId").toString(),QString("dolphinxr"));
  QCOMPARE(localUi.detail().value("state").toInt(),int(ac::GameState::Installed));
  QQuickItem *steamButton=nullptr;
  for(auto item:namedItems(page,QString()))
   if(item->property("text").toString()=="Add to Steam library") { steamButton=item;break; }
  QVERIFY(steamButton);QVERIFY(steamButton->isVisible());click(steamButton);
  QTRY_COMPARE(probe.requestedVariant,QString("dolphinxr"));
  QCOMPARE(probe.requestedGame,QString("wii-synthetic"));
  QVERIFY2(probe.prepared.has_value(),qPrintable("Selected authored Steam shortcut refused: "+probe.error));
  QVERIFY(probe.prepared->vr);QCOMPARE(probe.prepared->executable,fixture.hubExecutable);
  auto dialog=scene->findChild<QObject *>("steamDialog");QVERIFY(dialog);
  QTRY_VERIFY(dialog->property("opened").toBool());QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
 }
 void authoredVrCopyKeepsInstallAndPlayBlocked(){
  ui->openDetail("timecris");ui->selectVariant("dr89-pcvr");
  QVERIFY(!ui->detail().value("m1Available").toBool());
  QSignalSpy install(ui.get(),&ac::UiController::installRequested),play(ui.get(),&ac::UiController::playRequested);
  ui->startInstall("timecris","dr89-pcvr");QCOMPARE(install.count(),0);QVERIFY(ui->status().contains("not enabled"));
  ui->play("timecris","dr89-pcvr");QCOMPARE(play.count(),0);QVERIFY(ui->status().contains("not enabled"));
  const auto components=ui->detail().value("components").toList();QVERIFY(!components.isEmpty());
  QVERIFY(components.first().toMap().value("role").toString().contains("recipe"));
  root->setProperty("view","Detail");QTRY_VERIFY(window->findChild<QObject *>("detailStateReason"));
  const auto reason=window->findChild<QObject *>("detailStateReason")->property("text").toString();
  QVERIFY(reason.contains("not enabled"));QVERIFY(!reason.contains("M1"));
  root->setProperty("view","List");
 }
 void desktopControlsSwitchClearAndKeepEvidenceHonest(){
  ui->openDetail("timecris");root->setProperty("view","Detail");
  auto controls=ui->controlsController();auto model=controls->model();
  QTRY_COMPARE_WITH_TIMEOUT(model->gameId(),QString("timecris"),10000);
  QCOMPARE(controls->resolved().value("node_validation").toString(),QString("not-built"));
  for(const auto &row:model->rows())QVERIFY(row.toMap().value("availability").toString()!="available");
  const auto policy=controls->resolved().value("data").toMap().value("policy").toMap();
  QVERIFY(settings->saveGame("timecris",{{"left_handed",true}}));
  QTRY_VERIFY_WITH_TIMEOUT(!controls->loading(),10000);
  QCOMPARE(controls->resolved().value("data").toMap().value("policy").toMap(),policy);
  auto pressed=[&]{for(const auto &row:model->rows())if(row.toMap().value("pressed").toBool())return true;return false;};
  QFocusEvent focusIn(QEvent::FocusIn,Qt::OtherFocusReason);QCoreApplication::sendEvent(window,&focusIn);qobject_cast<QQuickItem *>(root)->forceActiveFocus();
  model->setBindingState("right","trigger",true);QVERIFY(pressed());
  QFocusEvent focusOut(QEvent::FocusOut,Qt::OtherFocusReason);QCoreApplication::sendEvent(window,&focusOut);QTRY_VERIFY(!pressed());
  QCoreApplication::sendEvent(window,&focusIn);
  model->setBindingState("right","trigger",true);QVERIFY(pressed());ui->launchFinished("timecris",{});QVERIFY(!pressed());
  ui->openDetail("vcop");QTRY_COMPARE_WITH_TIMEOUT(model->gameId(),QString("vcop"),10000);QVERIFY(!pressed());
  QCOMPARE(controls->resolved().value("node_validation").toString(),QString("not-built"));
  QString racing;for(const auto &game:games->records())if(game.roles.value("genreId")=="racing"){racing=game.id;break;}QVERIFY(!racing.isEmpty());
  ui->openDetail(racing);QTRY_VERIFY_WITH_TIMEOUT(!controls->loading(),10000);QVERIFY(model->rows().isEmpty());QVERIFY(!ui->detailControlsStatus().isEmpty());
  QVERIFY(namedItems(window->contentItem(),"universalControlsView").isEmpty());
  root->setProperty("view","List");QTRY_VERIFY(model->gameId().isEmpty());
 }
 void componentLoading(){
  const QStringList components{"PillButton","UiText","GameArt","GradientText","GameCard","SectionHeader","FeaturedBanner","Header","FilterBar","ScanProgress","FiltersDrawer","GameGrid","LibraryTile","RecentlyPlayedRow","ExplorePage","DetailPage","InstallConsole","RecoveryPanel","SettingsPage","SortMenu","HelpPanel"};
  for(const auto &name:components){QQmlComponent c(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/"+name+".qml"));QVERIFY2(c.isReady(),qPrintable(c.errorString()));std::unique_ptr<QObject> o(c.create());QVERIFY2(o!=nullptr,qPrintable(c.errorString()));}
 }
 void resizingSurvivesUnrelatedSettings(){
  const auto original=window->size();window->resize(1104,688);QCoreApplication::processEvents();settings->set("reduceMotion",true);QCOMPARE(window->size(),QSize(1104,688));settings->set("reduceMotion",false);window->resize(original);
 }
 void persistentGridKeepsCardsFocusAndScroll(){
  filter.clearFacets();filter.setSortMode("title");root->setProperty("view","List");QTest::qWait(100);auto list=window->findChild<QQuickItem *>("sectionedGrid");QVERIFY(list);list->setProperty("contentY",280.0);QTest::qWait(50);
  auto cards=namedItems(list,"gameCard");QVERIFY(!cards.isEmpty());QPointer<QQuickItem> kept=cards.first();const auto value=kept->property("game");const auto game=value.metaType().id()==qMetaTypeId<QJSValue>()?value.value<QJSValue>().toVariant().toMap():value.toMap();const auto id=game.value("gameId").toString();QVERIFY(games->find(id));
  kept->forceActiveFocus();QVERIFY(kept->hasActiveFocus());const auto y=list->property("contentY").toDouble();const auto modelValue=list->property("model");auto object=modelValue.value<QObject *>();if(!object&&modelValue.metaType().id()==qMetaTypeId<QJSValue>())object=modelValue.value<QJSValue>().toQObject();auto model=qobject_cast<ac::SectionedGridModel *>(object);QVERIFY(model);
  QPersistentModelIndex index(model->index(1));QSignalSpy resets(model,&QAbstractItemModel::modelReset),changes(model,&QAbstractItemModel::dataChanged);const auto originalState=games->find(id)->runtime;auto state=originalState;state.playing=true;games->applyRuntimeStates({state});QTRY_VERIFY(changes.count()>0);QVERIFY(index.isValid());QCOMPARE(resets.count(),0);QVERIFY(kept);QVERIFY(namedItems(list,"gameCard").contains(kept.data()));QVERIFY(kept->hasActiveFocus());QCOMPARE(list->property("contentY").toDouble(),y);
  games->applyRuntimeStates({originalState});QTest::qWait(50);list->setProperty("contentY",0.0);
 }
 void runtimeBurstCoalescesFacetNotifications(){
  filter.clearFacets();filter.setSortMode("title");QCoreApplication::processEvents();QSignalSpy facets(ui.get(),&ac::UiController::facetsChanged);QVector<ac::RuntimeState> originals;
  for(int i=0;i<12;++i){auto state=games->records()[i].runtime;originals<<state;state.lastPlayed=9000+i;games->applyRuntimeStates({state});}
  QCOMPARE(facets.count(),0);QTRY_COMPARE(facets.count(),1);games->applyRuntimeStates(originals);QTest::qWait(30);
 }
 void hardwareExpansionSurvivesRecount(){
  filter.setFacet("genre",QStringList{"gun"});QTest::qWait(50);const auto kind=ui->hardwareTree().first().toMap();const auto family=kind.value("children").toList().first().toMap();
  auto drawer=window->findChild<QObject *>("filtersDrawer");QVERIFY(drawer);QVERIFY(QMetaObject::invokeMethod(drawer,"open"));QTest::qWait(50);
  QVERIFY(QMetaObject::invokeMethod(drawer,"setExpanded",Q_ARG(QVariant,kind.value("id")),Q_ARG(QVariant,QVariant(false))));
  QVERIFY(QMetaObject::invokeMethod(drawer,"setExpanded",Q_ARG(QVariant,family.value("id")),Q_ARG(QVariant,QVariant(true))));filter.clearFacets();QTest::qWait(70);
  auto kinds=namedItems(window->contentItem(),"hardwareKind-"+kind.value("id").toString());QCOMPARE(kinds.size(),1);QVERIFY(!kinds.first()->property("expanded").toBool());
  QVERIFY(QMetaObject::invokeMethod(drawer,"setExpanded",Q_ARG(QVariant,kind.value("id")),Q_ARG(QVariant,QVariant(true))));QTest::qWait(50);
  auto families=namedItems(window->contentItem(),"hardwareFamily-"+family.value("id").toString());QCOMPARE(families.size(),1);QVERIFY(families.first()->property("expanded").toBool());QVERIFY(QMetaObject::invokeMethod(drawer,"close"));
 }
 void recoveryRetryIgnoresOpenPage(){
  auto failed=*games->find("timecris");failed.id="synthetic-failed";auto page=failed;page.id="synthetic-page";
  ac::Variant route;route.id="flat-synthetic";route.title="Synthetic route";route.generated=true;route.quality="flat";
  for(auto *game:{&failed,&page}){game->runtime.jobStatus=ac::JobStatus::Failed;game->variants={route};game->install={{"variant",{{"flat-synthetic",ac::Json::object()}}}};}
  ac::CatalogData data;data.games={failed,page};ac::GameListModel source(std::move(data));ac::FilterSortModel proxy;proxy.setSourceModel(&source);ac::UiSettings state(user.filePath("retry-state"));ac::UiController controller(&source,&proxy,&state);
  controller.installEvent({{"kind","fail"},{"retryKind","game"},{"retryOperation","install"},{"retryGameId","synthetic-failed"},{"retryVariantId","flat-synthetic"}});controller.openDetail("synthetic-page");QSignalSpy retry(&controller,&ac::UiController::retryInstallRequested);controller.retryInstall(false,{});
  QCOMPARE(retry.count(),1);QCOMPARE(retry.first()[0].toString(),QString("synthetic-failed"));QCOMPARE(retry.first()[1].toString(),QString("flat-synthetic"));
  controller.primary("synthetic-page");QCOMPARE(retry.count(),2);QCOMPARE(retry.last()[0].toString(),QString("synthetic-page"));QCOMPARE(retry.last()[1].toString(),QString("flat-synthetic"));
  QQmlContext context(engine->rootContext());context.setContextProperty("uiController",&controller);QQmlComponent component(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/RecoveryPanel.qml"));std::unique_ptr<QObject> panel(component.create(&context));QVERIFY(panel);auto button=panel->findChild<QObject *>("retrySelectedGame");QVERIFY(button);QVERIFY(button->property("text").toString().contains("synthetic-failed"));
 }
 void overlayFallbackShowsReason(){
  ui->overlayFallback("Synthetic overlay reason");QVERIFY(ui->status().contains("Overlay unavailable"));QVERIFY(ui->status().contains("Synthetic overlay reason"));QVERIFY(ui->recovery().isEmpty());
 }
 void recoveryRetryRequiresInstallIds(){
  ac::UiController controller(games.get(),&filter,settings.get());QQmlContext context(engine->rootContext());context.setContextProperty("uiController",&controller);QQmlComponent component(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/RecoveryPanel.qml"));std::unique_ptr<QObject> panel(component.create(&context));QVERIFY(panel);auto item=qobject_cast<QQuickItem *>(panel.get());QVERIFY(item);item->setParentItem(window->contentItem());item->setWidth(1000);auto gameButton=panel->findChild<QObject *>("retrySelectedGame");auto toolButton=panel->findChild<QObject *>("retryFailedTool");QVERIFY(gameButton);QVERIFY(toolButton);
  for(const QString operation:{"uninstall","repair",""}){
   controller.installEvent({{"kind","fail"},{"retryKind","game"},{"retryOperation",operation},{"retryGameId","synthetic"},{"retryVariantId","flat"}});QVERIFY(!controller.retryGameAvailable());QVERIFY(!gameButton->property("visible").toBool());QSignalSpy retry(&controller,&ac::UiController::retryInstallRequested);controller.retryInstall(false,{});QCOMPARE(retry.count(),0);
  }
  for(const auto missing:{"retryGameId","retryVariantId"}){QVariantMap event{{"kind","fail"},{"retryKind","game"},{"retryOperation","install"},{"retryGameId","synthetic"},{"retryVariantId","flat"}};event.remove(missing);controller.installEvent(event);QVERIFY(!controller.retryGameAvailable());QVERIFY(!gameButton->property("visible").toBool());}
  controller.installEvent({{"kind","fail"},{"retryKind","game"},{"retryOperation","install"},{"retryGameId","synthetic"},{"retryVariantId","flat"}});QVERIFY(controller.retryGameAvailable());QTRY_VERIFY(gameButton->property("visible").toBool());QVERIFY(!toolButton->property("visible").toBool());
  controller.installEvent({{"kind","fail"},{"retryKind","tool"},{"retryOperation","uninstall"},{"retryGameId","synthetic"},{"retryVariantId","flat"}});QVERIFY(!controller.retryToolAvailable());QVERIFY(!toolButton->property("visible").toBool());QSignalSpy toolRetry(&controller,&ac::UiController::retryToolInstallRequested);controller.retryFailedTool(false,{});QCOMPARE(toolRetry.count(),0);
  controller.installEvent({{"kind","fail"},{"retryKind","tool"},{"retryOperation","install"},{"retryGameId","synthetic"},{"retryVariantId","flat"}});QVERIFY(controller.retryToolAvailable());QTRY_VERIFY(toolButton->property("visible").toBool());
  controller.installEvent({{"kind","step"}});controller.installFinished(false,"Generic synthetic failure");QVERIFY(!controller.retryGameAvailable());QVERIFY(!controller.retryToolAvailable());
 }
 void recentReordersWhenLastPlayedChanges(){
  filter.setSortMode("recent");const auto id=games->records().last().id;const auto original=games->find(id)->runtime;auto state=original;state.lastPlayed=123456;games->applyRuntimeStates({state});QTRY_COMPARE(ui->filteredGame(0).value("gameId").toString(),id);games->applyRuntimeStates({original});filter.setSortMode("title");
 }
 void exploreTilesBindLocalArt_data(){
  QTest::addColumn<bool>("delayedIncubation");
  QTest::newRow("window-incubation")<<false;
  QTest::newRow("delayed-delegate-incubation")<<true;
 }
 void exploreTilesBindLocalArt(){
  QFETCH(bool,delayedIncubation);
  QQmlIncubationController incubation;
  auto original=engine->incubationController();
  if(delayedIncubation) engine->setIncubationController(&incubation);
  const auto restore=qScopeGuard([&]{if(delayedIncubation)engine->setIncubationController(original);});
  QTimer frames;frames.setInterval(5);
  connect(&frames,&QTimer::timeout,this,[&]{incubation.incubateFor(2);});
  if(delayedIncubation) QTimer::singleShot(250,&frames,qOverload<>(&QTimer::start));
  // Sorting queues facetsChanged; exercise a model refresh during incubation.
  QSignalSpy refreshed(ui.get(),&ac::UiController::facetsChanged);
  if(delayedIncubation){filter.setSortMode("recent");filter.setSortMode("title");}
  QQmlComponent component(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/ExplorePage.qml"));
  std::unique_ptr<QObject> object(component.create());QVERIFY2(object,qPrintable(component.errorString()));
  auto page=qobject_cast<QQuickItem *>(object.get());QVERIFY(page);
  page->setParentItem(window->contentItem());page->setWidth(1000);page->setHeight(650);
  if(delayedIncubation) QTRY_VERIFY(refreshed.count()>0);
  else QCoreApplication::processEvents();
  QString diagnostic;
  auto boundTiles=[&]{
   // Model replacement/incubation can destroy items between event turns.
   // Reacquire all delegates each time instead of retaining transient pointers.
   const auto tiles=namedItems(page,"libraryTile");
   if(tiles.isEmpty()){diagnostic="ExplorePage has no libraryTile delegates yet";return false;}
   for(auto tile:tiles){
    const auto value=tile->property("game");
    const auto game=value.metaType().id()==qMetaTypeId<QJSValue>()?value.value<QJSValue>().toVariant().toMap():value.toMap();
    const auto id=game.value("gameId").toString();
    const auto source=tile->property("source").toUrl().toString();
    const auto expected="image://art/"+id+"/portrait";
    if(id.isEmpty()||!games->find(id)||source!=expected){
     diagnostic=QString("item=%1 game=%2 source=%3 expected=%4 tiles=%5 incubating=%6")
         .arg(tile->objectName(),id,source,expected).arg(tiles.size()).arg(engine->incubationController()?engine->incubationController()->incubatingObjectCount():0);
     return false;
    }
   }
   return true;
  };
  QTRY_VERIFY2(boundTiles(),qPrintable(diagnostic));
  page->setParentItem(nullptr);
 }
 void nonInstallErrorsHaveNoInstallRecovery(){
  ui->installFinished(true,"Synthetic completion");ui->showError("Steam","Synthetic failure");QCOMPARE(ui->status(),QString("Steam: Synthetic failure"));QVERIFY(ui->recovery().isEmpty());QVERIFY(!ui->installing());
 }
 void hardwareFacetsHaveCountsAndNoUndefinedText(){
  filter.clearFacets();const auto tree=ui->hardwareTree();QVERIFY(!tree.isEmpty());
  for(const auto &kindValue:tree){const auto kind=kindValue.toMap();QVERIFY(kind.value("count").toInt()>0);const auto families=kind.value("children").toList();QVERIFY(!families.isEmpty());
   for(const auto &familyValue:families){const auto family=familyValue.toMap();QVERIFY(family.value("count").toInt()>0);const auto boards=family.value("children").toList();QVERIFY(!boards.isEmpty());for(const auto &boardValue:boards)QVERIFY(boardValue.toMap().value("count").toInt()>0);}
  }
  auto drawer=window->findChild<QObject *>("filtersDrawer");QVERIFY(drawer);QVERIFY(QMetaObject::invokeMethod(drawer,"open"));QTest::qWait(100);
  std::function<void(QQuickItem *)> inspect=[&](QQuickItem *item){QVERIFY(!item->property("text").toString().contains("undefined",Qt::CaseInsensitive));for(auto child:item->childItems())inspect(child);};inspect(window->contentItem());
  QVERIFY(QMetaObject::invokeMethod(drawer,"close"));
  // Manufacturer choices ignore their own selected value while obeying the genre facet.
  filter.setFacet("genre",QStringList{"gun"});const auto choices=filter.choices("manufacturerIds");QVERIFY(!choices.isEmpty());
  for(const auto &choiceValue:choices){auto choice=choiceValue.toMap();int expected=0;for(const auto &g:games->records())if(g.roles.value("genreId")=="gun"&&g.roles.value("manufacturerId")==choice.value("id"))++expected;QCOMPARE(choice.value("count").toInt(),expected);}
  filter.setFacet("manufacturerIds",QStringList{choices.first().toMap().value("id").toString()});QCOMPARE(filter.choices("manufacturerIds"),choices);
  filter.clearFacets();filter.setQuery("no-synthetic-game-has-this-title");QVERIFY(ui->hardwareTree().isEmpty());for(const auto &choice:filter.choices("hardwareIds"))QCOMPARE(choice.toMap().value("count").toInt(),0);filter.clearFacets();
 }
 void retryCarriesSelectedIdentifiers(){
  ac::UiController noSelection(games.get(),&filter,settings.get());QSignalSpy noRetry(&noSelection,&ac::UiController::retryInstallRequested);noSelection.retryInstall(false,{});QCOMPARE(noRetry.count(),0);QVERIFY(noSelection.status().contains("Nothing to retry",Qt::CaseInsensitive));
  ui->openDetail("timecris");for(const auto &v:games->find("timecris")->variants)if(v.generated){ui->selectVariant(v.id);break;}auto d=ui->detail();const auto variant=d.value("variantId").toString();QVERIFY(!variant.isEmpty());QSignalSpy retry(ui.get(),&ac::UiController::retryInstallRequested);
  ui->retryInstall(false,"synthetic handover");QCOMPARE(retry.count(),0);QVERIFY(ui->status().contains("Nothing to retry"));
  for(const auto &v:games->find("timecris")->variants)if(!v.generated){ui->selectVariant(v.id);ui->retryInstall(false,{});QCOMPARE(retry.count(),0);QVERIFY(ui->status().contains("Nothing to retry",Qt::CaseInsensitive));break;}
 }
 void stopControlAndRemovalOptIn(){
  auto stop=window->findChild<QObject *>("globalStop");QVERIFY(stop);QSignalSpy stops(ui.get(),&ac::UiController::stopLaunchRequested);QVERIFY(QMetaObject::invokeMethod(stop,"clicked"));QCOMPARE(stops.count(),1);QVERIFY(ui->status().contains("Stop requested"));
  auto remove=window->findChild<QObject *>("removeSteamOption");QVERIFY(remove);QVERIFY(!remove->property("selected").toBool());
  ui->launchPreparing("timecris","Preparing emulator profile");QVERIFY(ui->status().contains("Preparing"));QVERIFY(!games->find("timecris")->roles.value("playing").toBool());
 }
 void shippedFlatRemovalControlIsReachable(){
  ui->openDetail("timecris");for(const auto &v:games->find("timecris")->variants)if(v.generated){ui->selectVariant(v.id);break;}
  root->setProperty("view","Detail");QCoreApplication::processEvents();auto button=window->findChild<QObject *>("detailOwnedRemoval");QVERIFY(button);QVERIFY(button->property("visible").toBool());QVERIFY(ui->detail().value("m1Available").toBool());QVERIFY(!ui->detail().value("reinstallVisible").toBool());
  QSignalSpy removals(ui.get(),&ac::UiController::uninstallPreviewRequested);QVERIFY(QMetaObject::invokeMethod(button,"clicked"));QCOMPARE(removals.count(),1);QCOMPARE(removals.first().at(0).toString(),QString("timecris"));QCOMPARE(removals.first().at(1).toString(),ui->detail().value("variantId").toString());root->setProperty("view","List");
 }
 void failedToolRecoveryHasExplicitButton(){
  ui->installEvent({{"kind","fail"},{"text","Synthetic tool failure"},{"retryKind","tool"},{"retryOperation","install"},{"retryGameId","tool-supermodel"},{"retryVariantId","windows-x64"},{"retryLabel","Supermodel"}});
  QQmlComponent component(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/RecoveryPanel.qml"));std::unique_ptr<QObject> panel(component.create());QVERIFY(panel);auto button=panel->findChild<QObject *>("retryFailedTool");QVERIFY(button);QVERIFY(button->property("visible").toBool());QVERIFY(button->property("text").toString().contains("tool-supermodel"));QVERIFY(button->property("text").toString().contains("windows-x64"));QSignalSpy retry(ui.get(),&ac::UiController::retryToolInstallRequested);QVERIFY(QMetaObject::invokeMethod(button,"clicked"));QCOMPARE(retry.count(),1);QCOMPARE(retry.first().at(0).toString(),QString("tool-supermodel"));QCOMPARE(retry.first().at(1).toString(),QString("windows-x64"));ui->installEvent({{"kind","fail"},{"text","Synthetic tool removal failure"},{"retryKind","tool"},{"retryOperation","uninstall"},{"retryGameId","tool-supermodel"},{"retryVariantId","windows-x64"}});ui->retryFailedTool(false,{});QCOMPARE(retry.count(),1);ui->installFinished(true,"Synthetic completion");
 }
 void readmePrivacy(){
  QString raw="![inline](https://example.invalid/inline.png)\n![reference][cover]\n[cover]: https://example.invalid/ref.png\n![shortcut]\n[shortcut]: https://example.invalid/shortcut.png\n<img\n src=\"https://example.invalid/html.png\">\n[Upstream](https://example.invalid/project)";
  auto safe=ac::UiController::safeMarkdown(raw);QVERIFY(!safe.contains("!["));QVERIFY(!safe.contains("<img",Qt::CaseInsensitive));QVERIFY(safe.contains("[Upstream]"));
#ifdef Q_OS_WIN
  const auto syntheticPath=user.filePath("synthetic folder");QCOMPARE(ui->localPath(QUrl::fromLocalFile(syntheticPath)),syntheticPath);
#else
  QCOMPARE(ui->localPath(QUrl("file:///tmp/synthetic%20folder")),QString("/tmp/synthetic folder"));
#endif
 }
 void ownedLocalArt(){
  auto provider=new OwnedArtProvider(QColor(theme->get("color.genre.racing").toString()));engine->addImageProvider("ownedtest",provider);
  QQmlComponent c(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/GameArt.qml"));std::unique_ptr<QObject> art(c.create());QVERIFY(art);art->setProperty("width",320);art->setProperty("height",160);art->setProperty("source",QUrl("image://ownedtest/synthetic/banner"));QTRY_VERIFY(provider->requests.loadRelaxed()>0);
  auto image=provider->requestImage("synthetic",nullptr,{});QVERIFY(!image.isNull());QVERIFY(image.pixelColor(0,0).alpha()>0);
 }
 void artRevisionInvalidatesCache(){
  engine->addImageProvider("art",new OwnedArtProvider(QColor(theme->get("color.brand.orange").toString())));
  QQmlComponent c(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/GameArt.qml"));std::unique_ptr<QObject> art(c.create());QVERIFY(art);art->setProperty("source",QUrl("image://art/synthetic/banner"));QCOMPARE(art->property("effectiveSource").toUrl(),QUrl());ui->setArtProviderReady(true);QCOMPARE(art->property("effectiveSource").toUrl().toString(),QString("image://art/synthetic/banner?v=0"));ui->setArtRevision(2);QCOMPARE(art->property("effectiveSource").toUrl().toString(),QString("image://art/synthetic/banner?v=2"));ui->setArtProviderReady(false);
 }
 void filtersAndDetails413(){
  qInfo("UI facet interaction checks");
  for(const auto &key:{"graphicsIds","manufacturerIds","hardwareIds","vrKeys","playersBuckets","controlsTypes","decades"}){auto choices=filter.choices(key);QVERIFY(!choices.isEmpty());for(const auto &choice:QVariantList{choices.first(),choices.last()}){filter.setFacet(key,QStringList{choice.toMap().value("id").toString()});QVERIFY(filter.visibleCount()>0);filter.clearFacets();}}
  root->setProperty("view","Detail");
  int rendered=0;
  for(const auto &g:games->records()){if(rendered++%50==0)qInfo("UI detail %d / 413",rendered);ui->openDetail(g.id);auto d=ui->detail();QCOMPARE(d.value("gameId").toString(),g.id);QVERIFY(!d.value("readme").toString().isEmpty());QVERIFY(!d.value("controls").toList().isEmpty());QCoreApplication::processEvents(QEventLoop::AllEvents,1);}
  filter.setQuery("scud");QVERIFY(filter.visibleCount()>0);filter.setQuery("unlikely-no-title-synthetic");QCOMPARE(filter.visibleCount(),0);filter.setQuery("");root->setProperty("view","List");QCOMPARE(filter.visibleCount(),413);
 }
 void signalsAndProofGates(){
  QSignalSpy scans(ui.get(),&ac::UiController::scanRequested),cancels(ui.get(),&ac::UiController::cancelScanRequested);ui->startScan({"synthetic-root"});ui->startScan({"synthetic-root"});QCOMPARE(scans.count(),1);QCOMPARE(cancels.count(),0);QVERIFY(ui->scanning());ui->cancelScan();QCOMPARE(cancels.count(),1);ui->scanStarted();QVERIFY(ui->scanning());ui->scanFinished(true);QVERIFY(filter.scanComplete());
  ui->openDetail("timecris");for(const auto &v:games->find("timecris")->variants)if(v.generated){ui->selectVariant(v.id);break;}
  const auto eventStart=ui->consoleEvents().size();ui->installEvent({{"kind","step"},{"step",2},{"total",4},{"text","Extract"}});ui->installEvent({{"kind","ok"},{"text","Verified archive"}});QCOMPARE(ui->consoleEvents().at(eventStart).toMap().value("line").toString(),QString("--- [2/4] Extract ---"));ui->installEvent({{"kind","fail"},{"step",2},{"total",4},{"text","Synthetic failure"}});QVERIFY(!ui->recovery().isEmpty());QVERIFY(!ui->installing());QSignalSpy retries(ui.get(),&ac::UiController::retryInstallRequested);ui->retryInstall(false,"synthetic-file");QCOMPARE(retries.count(),0);QVERIFY(ui->status().contains("Nothing to retry"));ui->installFinished(true,"Synthetic service completion without proof");
  for(const auto &g:games->records())QVERIFY(g.roles.value("state").toInt()!=static_cast<int>(ac::GameState::Installed));
  ui->openDetail("timecris");ui->selectVariant("dr89-pcvr");QSignalSpy install(ui.get(),&ac::UiController::installRequested);ui->startInstall("timecris","dr89-pcvr");QCOMPARE(install.count(),0);
 }
 void keyboardFocusAndOverlay(){
  window->requestActivate();QTest::keyClick(window,Qt::Key_K,Qt::ControlModifier);QTest::qWait(30);auto search=window->findChild<QQuickItem *>("searchField");QVERIFY(search);QVERIFY(search->hasActiveFocus());for(char letter:std::string("scud"))QTest::keyClick(window,letter);QTest::qWait(30);QCOMPARE(filter.query(),QString("scud"));QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(30);QCOMPARE(filter.query(),QString());
  root->setProperty("vrOverlayMode",true);QTest::qWait(30);QQmlComponent c(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/PillButton.qml"));std::unique_ptr<QObject> button(c.create());button->setProperty("vrOverlayMode",true);QVERIFY(button->property("implicitHeight").toDouble()>=40);root->setProperty("vrOverlayMode",false);
 }
 void allOverlayPillsHaveMinimumTargets(){
  root->setProperty("vrOverlayMode",true);int checked=0;std::function<void(QQuickItem *)> inspect=[&](QQuickItem *item){if(item->property("targetHeight").isValid()){++checked;QVERIFY2(item->property("vrOverlayMode").toBool(),qPrintable(item->property("text").toString()));QVERIFY(item->height()>=56);QVERIFY(item->width()>=44);}for(auto child:item->childItems())inspect(child);};
  ui->openDetail("timecris");for(const auto &view:{"List","Detail","Explore","Settings"}){root->setProperty("view",view);QTest::qWait(60);inspect(window->contentItem());}
  for(const auto &name:{"scanDialog","toolDialog","steamDialog","removeDialog"}){auto dialog=window->findChild<QObject *>(name);QVERIFY(dialog);QVERIFY(QMetaObject::invokeMethod(dialog,"open"));QTest::qWait(60);inspect(window->contentItem());QVERIFY(QMetaObject::invokeMethod(dialog,"close"));}
  QVERIFY(checked>50);root->setProperty("view","List");root->setProperty("vrOverlayMode",false);
 }
 void sceneWarnings(){QTest::qWait(100);QStringList invalid;for(const auto &w:warnings)if(w.contains("ReferenceError")||w.contains("TypeError")||w.contains("Unable to assign")||w.contains("Binding loop")||w.contains("is not a type"))invalid<<w;QVERIFY2(invalid.isEmpty(),qPrintable(invalid.join('\n')));}
 void selfBenchmark(){
  const auto output=qEnvironmentVariable("AC_UI_BENCH_OUTPUT");if(output.isEmpty())QSKIP("Owner-hardware headed timing runs explicitly through AC_UI_BENCH_OUTPUT.");
  root->setProperty("view","List");filter.clearFacets();filter.setQuery("");window->resize(1120,720);QTest::qWait(500);
  auto list=window->findChild<QObject *>("sectionedGrid");QVERIFY(list);
  for(auto p=qobject_cast<QQuickItem *>(list);p;p=p->parentItem()){qInfo()<<"Ancestor"<<p->metaObject()->className()<<p->position()<<p->size()<<p->implicitHeight();if(p->parentItem())for(auto c:p->parentItem()->childItems())qInfo()<<"Sibling"<<c->metaObject()->className()<<c->position()<<c->size()<<c->implicitHeight();}
  QJsonObject report;report["catalogGames"]=413;report["renderer"]=QStringLiteral("OpenGL");report["initialDelegateCount"]=cardCount(window->contentItem());report["viewportHeight"]=list->property("height").toDouble();
  QQmlComponent card(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/GameCard.qml"));QElapsedTimer creation;creation.start();for(int i=0;i<100;++i){std::unique_ptr<QObject> object(card.create());QVERIFY(object);}report["delegateCreateMs"]=static_cast<double>(creation.nsecsElapsed())/1000000.0/100.0;
  for(bool cheap:{false,true}){
   settings->set("cheapEffects",cheap);QTest::qWait(200);QElapsedTimer elapsed;elapsed.start();QVector<double> deltas;double last=0;int frames=0;double cpu=cpuSeconds();
   auto connection=connect(window,&QQuickWindow::frameSwapped,this,[&]{double t=static_cast<double>(elapsed.nsecsElapsed())/1000000.0;if(last>0)deltas<<t-last;last=t;++frames;});
   QTimer scroll;scroll.setInterval(8);connect(&scroll,&QTimer::timeout,this,[&]{double max=std::max(0.0,list->property("contentHeight").toDouble()-list->property("height").toDouble());double phase=static_cast<double>(elapsed.elapsed()%6000)/6000.0;list->setProperty("contentY",phase*max);window->update();});scroll.start();QEventLoop loop;QTimer::singleShot(6000,&loop,&QEventLoop::quit);loop.exec();scroll.stop();disconnect(connection);
   std::sort(deltas.begin(),deltas.end());double seconds=static_cast<double>(elapsed.elapsed())/1000.0;QJsonObject run;run["fps"]=static_cast<double>(frames)/seconds;run["frames"]=frames;run["p95FrameMs"]=deltas.isEmpty()?0:deltas[static_cast<qsizetype>(deltas.size()*0.95)];run["cpuSeconds"]=cpuSeconds()-cpu;run["cpuMsPerFrame"]=frames>0?(cpuSeconds()-cpu)*1000/frames:0;run["workingSetBytes"]=static_cast<double>(memoryBytes());report[cheap?"cheapBorder":"multiEffectBaseline"]=run;
  }
  QFile f(output);QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(report).toJson());
  settings->set("cheapEffects",true);QMetaObject::invokeMethod(list,"positionViewAtBeginning");QTest::qWait(300);auto item=qobject_cast<QQuickItem *>(list);qInfo()<<"List geometry"<<item->mapToScene(QPointF())<<item->size()<<"content"<<list->property("contentY")<<list->property("contentHeight");QVERIFY(window->grabWindow().save(QFileInfo(output).dir().filePath("d-library.png")));
  ui->openDetail("timecris");root->setProperty("view","Detail");QTest::qWait(300);auto detail=window->findChild<QQuickItem *>("detailPage");if(detail)qInfo()<<"Detail geometry"<<detail->mapToScene(QPointF())<<detail->size();QVERIFY(window->grabWindow().save(QFileInfo(output).dir().filePath("d-detail.png")));
 }
};
int main(int argc,char **argv){QQuickWindow::setGraphicsApi(qEnvironmentVariable("QT_QUICK_BACKEND")=="software"?QSGRendererInterface::Software:QSGRendererInterface::OpenGL);QGuiApplication app(argc,argv);QCoreApplication::setApplicationVersion("0.1.0");QQuickStyle::setStyle("Basic");UiTest test;return QTest::qExec(&test,argc,argv);}
#include "UiTest.moc"
