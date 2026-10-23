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
bool diagnosticEnabled(){return qEnvironmentVariableIsSet("AC_UI_GEOMETRY_DIAGNOSTIC");}
QJsonObject geometry(QQuickItem *item){
 const auto point=item->mapToScene({item->width()/2,item->height()/2});
 QJsonArray chain;
 for(auto parent=item;parent;parent=parent->parentItem()){
  const auto local=parent->mapFromScene(point);
  chain<<QJsonObject{{"name",parent->objectName()},{"type",parent->metaObject()->className()},
   {"width",parent->width()},{"height",parent->height()},{"implicitHeight",parent->implicitHeight()},
   {"clip",parent->clip()},{"containsPoint",parent->contains(local)},
   {"visible",parent->isVisible()},{"enabled",parent->isEnabled()}};
 }
 return {{"pointX",point.x()},{"pointY",point.y()},
  {"mode",QJsonValue::fromVariant(item->property("vrOverlayMode"))},
  {"targetHeight",QJsonValue::fromVariant(item->property("targetHeight"))},{"chain",chain}};
}
void snapshot(const QString &tag,QQuickItem *item){
 if(diagnosticEnabled())qInfo().noquote()<<"GEOMETRY "+tag+" "+QString::fromUtf8(QJsonDocument(geometry(item)).toJson(QJsonDocument::Compact));
}
bool softwareFrame(QQuickWindow *window){
 QSignalSpy rendered(window,&QQuickWindow::afterRendering);
 window->update();return QTest::qWaitFor([&]{return !rendered.isEmpty();},2000);
}
bool withinClips(QQuickItem *item){
 if(item->width()<=0||item->height()<=0||!item->isVisible())return false;
 const auto point=item->mapToScene({item->width()/2,item->height()/2});
 for(auto parent=item;parent;parent=parent->parentItem())
  if(parent->clip()&&!parent->contains(parent->mapFromScene(point)))return false;
 return true;
}
void hitSnapshot(QQuickWindow *window,const QPointF &point){
 if(!diagnosticEnabled())return;
 QJsonArray path;auto item=window->contentItem();
 for(int depth=0;item&&depth<32;++depth){
  path<<QJsonObject{{"name",item->objectName()},{"type",item->metaObject()->className()},{"z",item->z()}};
  const auto local=item->mapFromScene(point);item=item->childAt(local.x(),local.y());
 }
 const auto grabber=window->mouseGrabberItem();
 qInfo().noquote()<<"HIT "+QString::fromUtf8(QJsonDocument(QJsonObject{{"path",path},{"grabber",grabber?grabber->objectName():QString()}}).toJson(QJsonDocument::Compact));
}
int cardCount(QQuickItem *item,bool visibleOnly=false){if(!item)return 0;int count=0;if(item->objectName()=="cardPrimary"){auto p=item->mapToScene(QPointF());if(!visibleOnly||(item->isVisible()&&p.y()>100&&p.y()<690))++count;}for(auto child:item->childItems())count+=cardCount(child,visibleOnly);return count;}
QList<QQuickItem *> namedItems(QQuickItem *item,const QString &name){QList<QQuickItem *> result;if(!item)return result;if(item->objectName()==name)result<<item;for(auto child:item->childItems())result.append(namedItems(child,name));return result;}
void logMessage(QtMsgType type,const QMessageLogContext &,const QString &message){if(type==QtWarningMsg||type==QtCriticalMsg)warnings<<message;fprintf(stderr,"%s: %s\n",QTest::currentTestFunction()?QTest::currentTestFunction():"teardown",qPrintable(message));}
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
class GunRoomProbe:public SteamPreparationProbe {
 Q_OBJECT
 Q_PROPERTY(QVariantList gunRoomModels MEMBER choices CONSTANT)
 Q_PROPERTY(bool gunRoomAvailable READ available NOTIFY gunRoomChanged)
 Q_PROPERTY(QString gunRoomReason READ reason NOTIFY gunRoomChanged)
 Q_PROPERTY(bool gunRoomCalibrationAvailable READ calibrationAvailable NOTIFY gunRoomChanged)
 Q_PROPERTY(QString gunRoomCalibrationReason READ calibrationReason NOTIFY gunRoomChanged)
 Q_PROPERTY(bool gunRoomBusy MEMBER inactive CONSTANT)
public:
 QVariantList choices{QVariantMap{{"id","synthetic-pistol"},{"title","Synthetic pistol"}}};
 QString model,hand,binary,explanation="Choose a hand.";
 bool admitted=false,calibrationCapability=false;int launches=0,calibrations=0;
 bool calibrationAvailable()const{return admitted&&calibrationCapability;}
 QString calibrationReason()const{return "This reviewed build does not admit grip calibration.";}
 bool available()const{return admitted;}QString reason()const{return explanation;}
 Q_INVOKABLE void previewGunRoom(const QString &m,const QString &h,const QString &b){
  model=m;hand=h;binary=b;admitted=!model.isEmpty()&&(hand=="left"||hand=="right")&&!binary.isEmpty();
  explanation=admitted?QString():"Choose a gun model and a hand; a reviewed executable is required.";emit gunRoomChanged();
 }
 Q_INVOKABLE void launchGunRoom(const QString &m,const QString &h,const QString &b){previewGunRoom(m,h,b);if(admitted)++launches;}
 Q_INVOKABLE void calibrateGunRoom(const QString &m,const QString &h,const QString &b){previewGunRoom(m,h,b);if(calibrationAvailable())++calibrations;}
signals:void gunRoomChanged();
};
class LaunchLifecycleProbe:public GunRoomProbe {
 Q_OBJECT
 Q_PROPERTY(bool launchBusy READ busy CONSTANT)
 Q_PROPERTY(bool preparing READ isPreparing CONSTANT)
public:
 bool startup=false;
 bool busy()const{return true;}
 bool isPreparing()const{return startup;}
};
class IdentityUiProbe:public ac::UiController {
 Q_OBJECT
 Q_PROPERTY(QString appName READ syntheticName CONSTANT)
public:
 using ac::UiController::UiController;
 QString syntheticName()const{return QStringLiteral("Synthetic Product");}
};
// Stands in for the overlay host: the B button arrives as its backRequested signal, as from OverlayHost.
class OverlayHostProbe:public QObject {
 Q_OBJECT
public:
 int keyboards=0;
 Q_INVOKABLE void requestKeyboard(QQuickItem *){++keyboards;}
 Q_INVOKABLE void dismissKeyboard(){}
signals:
 void backRequested();
};
// Synthetic panel rows: titles only, one Model 3 row with a real id so Play reaches the real launch signal.
QVariantList syntheticPanelRows(int count){
 const QStringList systems{"","model2","ps1","ps2","wii","dreamcast","system22"};
 QVariantList rows;
 for(int i=0;i<count;++i){
  const auto system=systems[i%systems.size()];
  QVariantMap row{{"id",i==0?QString("scud"):system.isEmpty()?QString("synthetic%1").arg(i):system+QString("-synthetic%1").arg(i)},
   {"title",i==0?QString("Scud Race"):QString("Synthetic game %1").arg(i,2,10,QChar('0'))},{"year",1990+i%10},{"problem",QString()},{"warning",QString()},{"found",true}};
  if(!system.isEmpty()){
   row["system"]=system;row["view"]="screen";
   row["views"]=QVariantList{QVariantMap{{"id","screen"},{"label","3D screen"},{"help","A 3D screen in the headset."}},QVariantMap{{"id","theatre"},{"label","Theatre screen"},{"help","A big screen in the headset."}}};
  }
  rows<<row;
 }
 return rows;
}
class UiTest:public QObject {
 Q_OBJECT
 QTemporaryDir user;
 std::unique_ptr<ac::GameListModel> games;
 ac::FilterSortModel filter;
 std::unique_ptr<ac::Theme> theme;
 std::unique_ptr<ac::UiSettings> settings;
 std::unique_ptr<ac::UiController> ui;
 OverlayHostProbe overlayProbe;
 std::unique_ptr<QQmlApplicationEngine> engine;
 QQuickWindow *window=nullptr;
 QObject *root=nullptr;
 private slots:
 void initTestCase(){
  qInstallMessageHandler(logMessage);
  auto catalog=ac::CatalogLoader().load(AC_CATALOG_ROOT);QCOMPARE(catalog.games.size(),443);
  theme=std::make_unique<ac::Theme>(catalog.theme);qmlRegisterSingletonInstance("AladdinsCastle.Hub",1,0,"Theme",theme.get());
  games=std::make_unique<ac::GameListModel>(std::move(catalog));filter.setSourceModel(games.get());settings=std::make_unique<ac::UiSettings>(user.path());settings->set("showAllGames",true); /* the catalog tests below need the whole catalog */ui=std::make_unique<ac::UiController>(games.get(),&filter,settings.get());engine=std::make_unique<QQmlApplicationEngine>();
  auto context=engine->rootContext();context->setContextProperty("overlayHost",&overlayProbe);context->setContextProperty("catalogGameCount",443);context->setContextProperty("gameModel",games.get());context->setContextProperty("gameFilter",&filter);context->setContextProperty("uiSettings",settings.get());context->setContextProperty("uiController",ui.get());
  engine->load(QUrl("qrc:/qt/qml/AladdinsCastle/Hub/DesktopShell.qml"));QVERIFY(!engine->rootObjects().isEmpty());window=qobject_cast<QQuickWindow *>(engine->rootObjects().first());QVERIFY(window);root=window->findChild<QObject *>("hubRoot");QVERIFY(root);QTest::qWait(300);
  auto list=window->findChild<QQuickItem *>("sectionedGrid");QVERIFY(list);QVERIFY(list->mapToScene(QPointF()).y()<200);QVERIFY(list->height()>400);
  int visibleCards=cardCount(window->contentItem(),true);QVERIFY(visibleCards>0);
  if(!qEnvironmentVariable("AC_UI_BENCH_OUTPUT").isEmpty()){auto out=QFileInfo(qEnvironmentVariable("AC_UI_BENCH_OUTPUT")).dir();QVERIFY(window->grabWindow().save(out.filePath("d-startup.png")));}
 }
 void themeAndSettings(){QCOMPARE(theme->get("motion.hover_dwell").toInt(),600);QCOMPARE(theme->get("motion.preview_scale").toDouble(),1.4);QVERIFY(theme->get("gradient.explore_banner_fade.stop").toList().size()==8);QVERIFY(settings->set("reduceMotion",true));QVERIFY(settings->saveGame("synthetic",{{"laser","on"},{"gun_pitch",-10}}));ac::UiSettings copy(user.path());QCOMPARE(copy.get("reduceMotion").toBool(),true);QCOMPARE(copy.game("synthetic").value("gun_pitch").toInt(),-10);settings->set("reduceMotion",false);}
 void alternateProductNameInInstallerAndFoundationScenes(){
  IdentityUiProbe alternate(games.get(),&filter,settings.get());
  alternate.installEvent({{"kind","step"},{"step",1},{"total",1},{"text","Synthetic event"}});
  QQmlContext context(engine->rootContext());context.setContextProperty("uiController",&alternate);
  context.setContextProperty("surfaceColor","#10131c");context.setContextProperty("primaryTextColor","#ffffff");
  context.setContextProperty("brandColor","#ee892a");context.setContextProperty("overlaySpikeEnabled",false);
  for(const auto &file:{QStringLiteral("InstallConsole.qml"),QStringLiteral("SpikeRoot.qml")}){
   QQmlComponent component(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/"+file));
   std::unique_ptr<QQuickItem> scene(qobject_cast<QQuickItem *>(component.create(&context)));
   QVERIFY2(scene,qPrintable(component.errorString()));scene->setParentItem(window->contentItem());scene->setSize({1000,700});
   QVERIFY(softwareFrame(window));
   const bool installer=file=="InstallConsole.qml";
   const auto labels=namedItems(scene.get(),installer?"installConsoleTitle":"spikeFoundationTitle");
   QCOMPARE(labels.size(),1);
   QCOMPARE(labels.first()->property("text").toString(),installer?QString("Synthetic Product installer · Last install"):QString("Synthetic Product: 443 games"));
  }
 }
 void captureMarkersAreOptInInRealHubScene(){
  const auto previous=root->property("captureMarkers");const auto cleanup=qScopeGuard([&]{root->setProperty("captureMarkers",previous);});
  QVERIFY(!previous.toBool());QVERIFY(namedItems(window->contentItem(),"overlayCaptureCorner0").isEmpty());
  QVERIFY(root->setProperty("captureMarkers",true));QVERIFY(softwareFrame(window));
  const auto scene=qobject_cast<QQuickItem *>(root);QVERIFY(scene);
  const QStringList labels{"TL 1","TR 2","BL 3","BR 4"};
  const QStringList colors{"#ff00ff","#00ff00","#0000ff","#ff8800"};
  for(int i=0;i<4;++i){
   const auto corners=namedItems(scene,"overlayCaptureCorner"+QString::number(i));QCOMPARE(corners.size(),1);const auto corner=corners.first();
   QCOMPARE(corner->x(),i%2?scene->width()-corner->width()-12:12.0);
   QCOMPARE(corner->y(),i<2?12.0:scene->height()-corner->height()-12);QVERIFY(corner->isVisible());
   QCOMPARE(corner->property("color").value<QColor>(),QColor(colors[i]));
   const auto texts=namedItems(corner,"overlayCaptureLabel"+QString::number(i));QCOMPARE(texts.size(),1);QCOMPARE(texts.first()->property("text").toString(),labels[i]);
  }
  QVERIFY(root->setProperty("captureMarkers",false));QVERIFY(softwareFrame(window));QVERIFY(namedItems(scene,"overlayCaptureCorner0").isEmpty());
 }
 void emptyArtModelDuringNavigation(){
  QQmlComponent component(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/GameArt.qml"));
  std::unique_ptr<QObject> art(component.create());QVERIFY(art);const auto before=warnings.size();
  QVERIFY(art->setProperty("game",QVariant()));QTest::qWait(40);
  QStringList invalid;for(const auto &warning:warnings.mid(before))if(warning.contains("TypeError")||warning.contains("ReferenceError"))invalid<<warning;
  QVERIFY2(invalid.isEmpty(),qPrintable(invalid.join('\n')));
 }
 void setupPagesRealScene(){
  const auto previousView=root->property("view");
  const auto cleanup=qScopeGuard([&]{settings->set("setupKinds",QVariantMap{});settings->set("setupSystems",QVariantMap{});settings->set("setupDone",false);root->setProperty("view",previousView);});
  const auto centre=[](QQuickItem *item){return item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();};
  const auto one=[&](const QString &name){const auto items=namedItems(window->contentItem(),name);return items.size()==1?items.first():nullptr;};
  // A click on an item missing from the page, or on it twice, fails the test with its name instead of crashing it.
  const auto press=[&](const QString &name){const auto item=one(name);if(!item){qWarning().noquote()<<name<<"is not on the page exactly once";return false;}
   QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,centre(item));return true;};
  root->setProperty("view","Setup");QVERIFY(softwareFrame(window));
  QTRY_VERIFY(one("setupWelcome"));QVERIFY(one("setupWelcome")->property("text").toString().contains(ui->appName()));
  // Five pages, reached with Next; the choices are saved as they are made.
  QVERIFY(press("setupNext"));QVERIFY(softwareFrame(window));
  QVERIFY(press("setupNext"));QVERIFY(softwareFrame(window));
  QTRY_VERIFY(one("setupKind-arcade"));QVERIFY(!ui->setupKinds()["arcade"].toBool());
  QVERIFY(press("setupKind-arcade"));
  QTRY_VERIFY(ui->setupKinds()["arcade"].toBool());
  ac::UiSettings saved(user.path());QVERIFY(saved.get("setupKinds").toMap()["arcade"].toBool());
  QVERIFY(softwareFrame(window));QVERIFY(press("setupSystem-model2"));
  QTRY_VERIFY(!ui->setupSystems()[1].toMap()["chosen"].toBool());
  QVERIFY(press("setupNext"));QVERIFY(softwareFrame(window));
  QTRY_VERIFY(one("setupFolder-model3"));QVERIFY(!one("setupFolder-model2"));
  QVERIFY(one("setupFolder-model3")->property("text").toString().contains("not chosen"));
  QVERIFY(!settings->get("setupDone",false).toBool());
  // Leaving the folders page asks for one read of the ticked systems, then shows what came back.
  QSignalSpy scans(ui.get(),SIGNAL(setupScanRequested(QStringList)));
  QVERIFY(press("setupNext"));QVERIFY(softwareFrame(window));
  QCOMPARE(scans.size(),1);QCOMPARE(scans[0][0].toStringList(),(QStringList{"model3","system22"}));
  QTRY_VERIFY(one("setupProgress"));QVERIFY(one("setupProgress")->isVisible());QVERIFY(!one("setupResult-model3"));
  const QVariantMap entry{{"id","scud"},{"title","Synthetic Racer"},{"detail","Checked."},{"reason","Two files differ."},{"expected","synthetic.zip"}};
  ui->setSetupReport({{"scanned","today"},{"systems",QVariantList{QVariantMap{{"id","model3"},{"title","Sega Model 3"},{"note",""},{"ready",QVariantList{entry}},{"notRight",QVariantList{entry}},{"missing",QVariantList{entry}}}}}});
  QVERIFY(softwareFrame(window));QTRY_VERIFY(one("setupResult-model3"));QVERIFY(!one("setupProgress")->isVisible());
  // Running a found emulator for its list is asked once; "No" is kept, and the question goes.
  QVERIFY(one("setupQueryEmulators")&&one("setupQueryEmulators")->isVisible());
  QVERIFY(press("setupQueryNo"));
  QTRY_VERIFY(settings->values().contains("queryEmulators"));QVERIFY(!settings->get("queryEmulators").toBool());
  QVERIFY(softwareFrame(window));QVERIFY(!one("setupQueryEmulators")->isVisible());QCOMPARE(scans.size(),1);
  settings->remove("queryEmulators");
  // The same finding greys the game on its own tab, with the reason.
  QVERIFY(ui->model3Games()[0].toMap()["problem"].toString().isEmpty()==false);
  QVERIFY(press("setupNext"));
  QTRY_COMPARE(root->property("view").toString(),QString("List"));QVERIFY(settings->get("setupDone").toBool());
  ui->setSetupReport({});
 }
 // A SteamGridDB key saved in the clear by an earlier Hub is protected at the next start, and is never shown.
 void steamGridKeyIsKeptProtected(){
#ifdef Q_OS_WIN
  QTemporaryDir own;QVERIFY(own.isValid());const QString key="synthetic-key-abcdef0123456789";
  {ac::UiSettings old(own.path());old.set("steamGridDbKey",key);}
  ac::UiSettings kept(own.path());ac::UiController controller(games.get(),&filter,&kept);
  QVERIFY(!kept.values().contains("steamGridDbKey"));QVERIFY(controller.hasSteamGridDbKey());QCOMPARE(controller.steamGridDbKey(),key);
  QDirIterator files(own.path(),QDir::Files,QDirIterator::Subdirectories);
  while(files.hasNext()){QFile file(files.next());QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY2(!file.readAll().contains(key.toUtf8()),qPrintable(file.fileName()));}
  QVERIFY(controller.setSteamGridDbKey("  another-key "));QCOMPARE(controller.steamGridDbKey(),QString("another-key"));
  QVERIFY(controller.setSteamGridDbKey(""));QVERIFY(!controller.hasSteamGridDbKey());QVERIFY(controller.steamGridDbKey().isEmpty());
#else
  QSKIP("Windows data protection only");
#endif
 }
 // Starting, stopping and ending are shown at once, with the reason when a game crashed.
 // Only playable games by default, and every menu opens and goes away again.
 void playableOnlyAndEveryMenuCloses(){
  const auto previousView=root->property("view");
  const auto cleanup=qScopeGuard([&]{QMetaObject::invokeMethod(root,"closeMenus");settings->set("showAllGames",true);root->setProperty("history",QVariantList{});root->setProperty("view",previousView);});
  const auto one=[&](const QString &name){const auto items=namedItems(window->contentItem(),name);return items.size()==1?items.first():nullptr;};
  const auto centre=[](QQuickItem *item){return item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();};
  const auto click=[&](QQuickItem *item){QVERIFY(item);QVERIFY(item->isVisible());QVERIFY(softwareFrame(window));QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,centre(item));};
  const auto popup=[&](const QString &name){return root->findChild<QObject *>(name);};
  // Switching the catalog off leaves the catalog page for the playable list and hides what leads to it.
  root->setProperty("view","List");QVERIFY(softwareFrame(window));
  settings->set("showAllGames",false);QVERIFY(softwareFrame(window));
  QTRY_COMPARE(root->property("view").toString(),QString("Model3"));QVERIFY(softwareFrame(window));
  QTRY_VERIFY(one("model3Grid"));QCOMPARE(one("model3Grid")->property("count").toInt(),int(ui->model3Games().size()+ui->otherGames().size()+ui->readyCatalogGames().size()));
  for(const auto &hidden:{"filterBar","libraryButton","orderButton","searchField"})QVERIFY2(!one(hidden)->isVisible(),hidden);
  QVERIFY(one("model3Tab")->isVisible());QVERIFY(one("pinballTab")->isVisible());QVERIFY(one("helpButton")->isVisible());
  // Pinball lists every table and every readable one plays in the headset; Ready to play and VR room only narrow it, and a tile says why a table cannot play.
  QVERIFY(QMetaObject::invokeMethod(root,"navigate",Q_ARG(QVariant,"Pinball"),Q_ARG(QVariant,QVariant())));
  {QVariantList tables;const QStringList states{"ready","needs no ROM","ROM missing","unknown"};
   for(int i=0;i<8;++i)tables<<QVariantMap{{"id",QString::number(i)},{"title",QString("Fixture %1").arg(i)},{"vrReady",i<6},{"romState",states[i%4]}};
   QVERIFY(QMetaObject::invokeMethod(ui.get(),"pinballLoaded",Q_ARG(QVariantMap,(QVariantMap{{"total",8},{"vrReady",6},{"tables",tables}}))));}
  QTRY_VERIFY(one("pinballGrid"));QTRY_COMPARE(one("pinballGrid")->property("count").toInt(),8);
  const QString dot=QString("  ")+QChar(0xb7)+"  ";
  QCOMPARE(one("pinballCount")->property("text").toString(),"8 tables"+dot+"4 ready to play"+dot+"4 ready in the headset");
  QVERIFY(one("pinballReady")->isVisible());QVERIFY(one("pinballVrOnly")->isVisible());
  one("pinballReady")->setProperty("checked",true);QTRY_COMPARE(one("pinballGrid")->property("count").toInt(),4); // 0, 1, 4 and 5: startable.
  one("pinballReady")->setProperty("checked",false);one("pinballVrOnly")->setProperty("checked",true);QTRY_COMPARE(one("pinballGrid")->property("count").toInt(),6);
  QCOMPARE(one("pinballCount")->property("text").toString(),"6 of 8 tables"+dot+"4 ready to play"+dot+"4 ready in the headset");
  one("pinballVrOnly")->setProperty("checked",false);
  {QObject *pinballPage=one("pinballGrid");while(pinballPage&&pinballPage->metaObject()->indexOfMethod("tileOf(QVariant)")<0)pinballPage=pinballPage->parent();QVERIFY(pinballPage);
   QVariantMap desktop{{"id","9"},{"title","Fixture 9"},{"vrReady",false},{"romState","ready"}},broken{{"id","10"},{"title","Fixture 10"},{"error","unreadable"},{"romState","unknown"}},
    locked{{"id","11"},{"title","Fixture 11"},{"note","Its script is locked by its author."},{"romState","unknown"}};
   QStringList badges;QVariant tile;auto list=ui->pinballData()["tables"].toList();list<<desktop<<broken<<locked;
   for(const auto &row:list){QVERIFY(QMetaObject::invokeMethod(pinballPage,"tileOf",Q_RETURN_ARG(QVariant,tile),Q_ARG(QVariant,row)));badges<<tile.toMap()["badge"].toString();}
   QCOMPARE(badges,(QStringList{"VR room","VR room","ROM missing","ROM not checked","VR room","VR room","ROM missing","ROM not checked","Excluded","Can't read","Locked script"}));}
  QVERIFY(QMetaObject::invokeMethod(root,"navigate",Q_ARG(QVariant,"Model3"),Q_ARG(QVariant,QVariant())));QVERIFY(softwareFrame(window));
  // The help menu: closed by its Close button, by Escape and by a click outside it; only items that work are shown.
  const auto help=popup("helpPanel");QVERIFY(help);
  for(int way=0;way<3;++way){
   click(one("helpButton"));QTRY_VERIFY(help->property("opened").toBool());
   QVERIFY(one("helpSettings")->isVisible());QVERIFY(!one("helpGunRoom")->isVisible());QVERIFY(!one("helpSuggest")->isVisible());
   if(way==0)click(one("helpClose"));
   else if(way==1){QVERIFY(softwareFrame(window));QTest::keyClick(window,Qt::Key_Escape);}
   else{QVERIFY(softwareFrame(window));QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,QPoint(20,window->height()-20));}
   QTRY_VERIFY2(!help->property("visible").toBool(),qPrintable(QString::number(way)));
  }
  // Scrolling over the help menu leaves it open; it used to close it.
  click(one("helpButton"));QTRY_VERIFY(help->property("opened").toBool());
  {const auto inside=centre(one("helpSettings"));QWheelEvent wheel(inside,window->mapToGlobal(inside),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(window,&wheel);}
  QTest::qWait(100);QVERIFY(help->property("opened").toBool());click(one("helpClose"));QTRY_VERIFY(!help->property("visible").toBool());
  // A help item closes the menu and opens its page.
  click(one("helpButton"));QTRY_VERIFY(help->property("opened").toBool());click(one("helpSettings"));
  QTRY_VERIFY(!help->property("visible").toBool());QCOMPARE(root->property("view").toString(),QString("Settings"));
  // The setting brings the catalog back, with its filters, sort and search; each of those menus closes too.
  click(one("settingShowAll"));QTRY_VERIFY(settings->get("showAllGames").toBool());
  root->setProperty("view","List");QVERIFY(softwareFrame(window));QVERIFY(one("filterBar")->isVisible());QVERIFY(one("orderButton")->isVisible());
  const auto sort=popup("sortMenu"),filters=popup("filtersDrawer");QVERIFY(sort);QVERIFY(filters);
  click(one("orderButton"));QTRY_VERIFY(sort->property("opened").toBool());click(one("sortClose"));QTRY_VERIFY(!sort->property("visible").toBool());
  QVERIFY(QMetaObject::invokeMethod(filters,"open"));QTRY_VERIFY(filters->property("opened").toBool());
  // Scrolling inside the drawer scrolls it; it used to close it.
  QVERIFY(softwareFrame(window));const auto inside=window->contentItem()->mapFromItem(qobject_cast<QQuickItem *>(filters->property("contentItem").value<QObject *>()),QPointF(40,40)).toPoint();
  QWheelEvent wheel(inside,window->mapToGlobal(inside),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(window,&wheel);
  QTest::qWait(100);QVERIFY(filters->property("opened").toBool());
  click(one("filtersDone"));QTRY_VERIFY(!filters->property("visible").toBool());
  // Escape closes a dialog and stays on the page under it.
  const auto scan=popup("scanDialog");QVERIFY(scan);QVERIFY(QMetaObject::invokeMethod(scan,"open"));QTRY_VERIFY(scan->property("opened").toBool());
  QVERIFY(softwareFrame(window));QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY(!scan->property("visible").toBool());QCOMPARE(root->property("view").toString(),QString("List"));
  // Changing page closes whatever is open.
  click(one("orderButton"));QTRY_VERIFY(sort->property("opened").toBool());QVERIFY(QMetaObject::invokeMethod(root,"navigate",Q_ARG(QVariant,"Model3"),Q_ARG(QVariant,QVariant())));
  QTRY_VERIFY(!sort->property("visible").toBool());
 }
 void lifecycleIsShownAtOnce(){
  const auto one=[&](const QString &name){const auto items=namedItems(window->contentItem(),name);return items.size()==1?items.first():nullptr;};
  const auto cleanup=qScopeGuard([&]{ui->dismissLastSession();});
  ui->launchQueued("synthetic-game","flat",QString());QVERIFY(softwareFrame(window));
  QTRY_VERIFY(one("nowPlayingPhase"));QVERIFY(one("nowPlayingPhase")->property("text").toString().startsWith("Starting: "));
  ui->launchStarted("synthetic-game");QVERIFY(softwareFrame(window));QVERIFY(one("nowPlayingPhase")->property("text").toString().startsWith("Now playing: "));
  QSignalSpy stops(ui.get(),SIGNAL(stopLaunchRequested()));ui->stopLaunch();QCOMPARE(stops.size(),1);
  QVERIFY(ui->nowPlaying()["stopping"].toBool());QVERIFY(softwareFrame(window));QVERIFY(one("nowPlayingPhase")->property("text").toString().startsWith("Stopping: "));
  ui->launchFinished("synthetic-game","The game stopped unexpectedly\nsynthetic first line\nsynthetic last line",3);
  QCOMPARE(ui->lastSession()["outcome"].toString(),QString("stopped"));
  // A stop the player asked for is not reported as a crash on the status line either.
  QVERIFY(ui->status().endsWith("was stopped."));QVERIFY(!ui->status().contains("unexpectedly"));
  // A queued game is named from the start, never as another game for a moment.
  {QSignalSpy cards(ui.get(),SIGNAL(nowPlayingChanged()));ui->launchOtherQueued("synthetic-game","flat","Synthetic title","hint",QString());
   QCOMPARE(cards.size(),1);QCOMPARE(ui->nowPlaying()["title"].toString(),QString("Synthetic title"));ui->launchFinished("synthetic-game",QString(),0);}
  // A crash, not asked for: the exit code and the end of the game's output.
  ui->launchQueued("synthetic-game","flat",QString());QVERIFY(ui->lastSession().isEmpty());ui->launchStarted("synthetic-game");
  ui->launchFinished("synthetic-game","The game stopped unexpectedly\nsynthetic first line\nsynthetic last line",3);
  QCOMPARE(ui->lastSession()["outcome"].toString(),QString("crashed"));
  QVERIFY(softwareFrame(window));QTRY_VERIFY(one("lastSessionText"));QVERIFY(one("lastSession")->isVisible());
  const auto shown=one("lastSessionText")->property("text").toString();
  QVERIFY(shown.contains("stopped unexpectedly (exit code 3)"));QVERIFY(shown.contains("synthetic last line"));
  // An ordinary close, and a start that failed.
  ui->launchQueued("synthetic-game","flat",QString());ui->launchStarted("synthetic-game");ui->launchFinished("synthetic-game",QString(),0);
  QCOMPARE(ui->lastSession()["outcome"].toString(),QString("closed"));QVERIFY(ui->lastSession()["text"].toString().endsWith("has closed."));
  ui->launchQueued("synthetic-game","flat",QString());ui->launchFinished("synthetic-game","Launch executable or working directory missing",1);
  QCOMPARE(ui->lastSession()["outcome"].toString(),QString("failed"));QVERIFY(ui->lastSession()["text"].toString().contains("could not start"));
  QVERIFY(softwareFrame(window));QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,one("lastSessionDismiss")->mapToScene(QPointF(10,10)).toPoint());
  QTRY_VERIFY(ui->lastSession().isEmpty());
 }
 void model3TabRealScene(){
  const auto previousView=root->property("view");
  const auto cleanup=qScopeGuard([&]{settings->set("model3Executable",QString());settings->set("model3GameFolder",QString());settings->set("model3Views",QVariantMap{});root->setProperty("view",previousView);});
  // A synthetic emulator folder and one synthetic game file: one card can start, the others name what is missing.
  QTemporaryDir fixture;QVERIFY(QDir().mkpath(fixture.filePath("emu/Config")));QVERIFY(QDir().mkpath(fixture.filePath("games")));
  for(const auto &name:{"emu/program.exe","emu/Config/Games.xml","games/scud.zip"}){QFile file(fixture.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write("synthetic");}
  const auto centre=[](QQuickItem *item){return item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();};
  root->setProperty("view","List");QVERIFY(softwareFrame(window));
  const auto tabs=namedItems(window->contentItem(),"model3Tab");QCOMPARE(tabs.size(),1);
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,centre(tabs.first()));
  QCOMPARE(root->property("view").toString(),QString("Model3"));
  QTRY_VERIFY(!namedItems(window->contentItem(),"model3Grid").isEmpty());
  QCOMPARE(namedItems(window->contentItem(),"model3Grid").first()->property("count").toInt(),11); // with Sega Rally 2 DX
  for(const auto &row:ui->model3Games())QVERIFY(row.toMap().value("problem").toString().contains("Choose it in Set-up"));
  QVERIFY(settings->set("model3Executable",fixture.filePath("emu/program.exe")));QVERIFY(settings->set("model3GameFolder",fixture.filePath("games")));
  auto rows=ui->model3Games();QCOMPARE(rows.size(),11); // with Sega Rally 2 DX
  QCOMPARE(rows[0].toMap().value("view").toString(),QString("screen"));
  QVERIFY(rows[0].toMap().value("problem").toString().isEmpty());QVERIFY(rows[1].toMap().value("problem").toString().contains("daytona2.zip"));
  QVERIFY(softwareFrame(window));QTRY_VERIFY(!namedItems(window->contentItem(),"model3Card").isEmpty());
  const auto card=namedItems(window->contentItem(),"model3Card").first();
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,card->mapToScene(QPointF(card->width()/2,20)).toPoint());
  // Scud Race's views from its old launchers: screen, open frame, wide, depth trial, inside, inside with the wheel.
  QTRY_COMPARE(namedItems(window->contentItem(),"model3View").size(),6);
  QCOMPARE(namedItems(window->contentItem(),"model3Title").first()->property("text").toString(),QString("Scud Race"));
  QVERIFY(softwareFrame(window));
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,centre(namedItems(window->contentItem(),"model3View")[4]));
  QTRY_COMPARE(ui->model3Games()[0].toMap().value("view").toString(),QString("inside"));
  ac::UiSettings saved(user.path());QCOMPARE(saved.get("model3Views").toMap().value("scud").toString(),QString("inside"));
  QSignalSpy play(ui.get(),SIGNAL(model3PlayRequested(QString,QString)));
  QTRY_VERIFY(!namedItems(window->contentItem(),"model3Start").isEmpty());
  const auto start=namedItems(window->contentItem(),"model3Start").first();QTRY_VERIFY(start->isEnabled());
  QVERIFY(softwareFrame(window));QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,centre(start));
  QCOMPARE(play.size(),1);QCOMPARE(play[0][0].toString(),QString("scud"));QCOMPARE(play[0][1].toString(),QString("inside"));
 }
 void pinballTabRealScene(){
  const auto previousView=root->property("view"),previousMode=root->property("vrOverlayMode");
  const auto cleanup=qScopeGuard([&]{ui->selectPinball(QString());root->setProperty("view",previousView);root->setProperty("vrOverlayMode",previousMode);});
  root->setProperty("view","List");root->setProperty("vrOverlayMode",true);
  const auto tabs=namedItems(window->contentItem(),"pinballTab");QCOMPARE(tabs.size(),1);
  const auto tab=tabs.first();snapshot("pinball-before-frame",tab);
  QVERIFY(softwareFrame(window));snapshot("pinball-after-frame",tab);
  QVERIFY(tab->height()>=48);
  auto p=tab->mapToScene(QPointF(tab->width()/2,tab->height()/2));QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());
  QCOMPARE(root->property("view").toString(),QString("Pinball"));
  QVariantList rows;for(int i=0;i<25;++i)rows<<QVariantMap{{"id",QString::number(i)},{"title",QString("Fixture %1").arg(i)},{"maker","Synthetic"},{"year",2000},{"vrReady",i!=24},{"evidence",QStringList{"marked by you"}},{"romState","needs no ROM"}};
  QVariantMap data{{"total",25},{"vrReady",24},{"tables",rows}};
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"pinballLoaded",Q_ARG(QVariantMap,data)));
  QTRY_VERIFY(!namedItems(window->contentItem(),"pinballGrid").isEmpty());
  auto grid=namedItems(window->contentItem(),"pinballGrid").first();QTRY_COMPARE(grid->property("count").toInt(),25);
  auto tiles=namedItems(window->contentItem(),"pinballTile");QVERIFY(!tiles.isEmpty());
  auto tile=tiles.first();p=tile->mapToScene(QPointF(tile->width()/2,24));
  QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());QTest::mouseMove(window,p.toPoint()-QPoint(0,140),80);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,p.toPoint()-QPoint(0,140));
  QVERIFY(namedItems(window->contentItem(),"pinballDetailTitle").isEmpty());
  grid->setProperty("contentY",0);QTest::qWait(40);tiles=namedItems(window->contentItem(),"pinballTile");tile=tiles.first();p=tile->mapToScene(QPointF(tile->width()/2,24));
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());QTRY_VERIFY(!namedItems(window->contentItem(),"pinballDetailTitle").isEmpty());
  QCOMPARE(ui->property("pinballSelection").toMap().value("romState").toString(),QString("needs no ROM"));
  QTRY_VERIFY(!namedItems(window->contentItem(),"pinballLaunchHelp").isEmpty());
  const auto launchHelp=namedItems(window->contentItem(),"pinballLaunchHelp").first();
  const auto helpText=ui->property("pinballLaunchHelp").toString();
  QVERIFY(helpText.contains("normal settings"));QVERIFY(helpText.contains("scores"));
  QCOMPARE(launchHelp->property("text").toString(),helpText);
  // Two launches, each exactly what it says. With SteamVR off, Launch in headset still works: it says SteamVR starts first.
  QSignalSpy play(ui.get(),SIGNAL(pinballPlayRequested(QString,QString)));
  QTRY_VERIFY(!namedItems(window->contentItem(),"pinballLaunchDesktop").isEmpty());
  const auto playButton=namedItems(window->contentItem(),"pinballLaunchDesktop").first(),headsetButton=namedItems(window->contentItem(),"pinballLaunchHeadset").first();
  ui->setSteamVrRunning(false);QVERIFY(softwareFrame(window));
  QVERIFY(playButton->height()>=48);QVERIFY(playButton->isEnabled());QVERIFY(headsetButton->isEnabled());
  QVERIFY(namedItems(window->contentItem(),"pinballHeadsetReason").first()->property("text").toString().contains("SteamVR"));
  QVERIFY(QMetaObject::invokeMethod(playButton,"clicked"));QCOMPARE(play.size(),1);
  QCOMPARE(play[0][0].toString(),ui->pinballSelection()["id"].toString());QCOMPARE(play[0][1].toString(),QString("desktop"));
  QVERIFY(QMetaObject::invokeMethod(headsetButton,"clicked"));QCOMPARE(play.size(),2);QCOMPARE(play[1][1].toString(),QString("headset"));
  // While SteamVR is starting for one game, no other headset launch is offered.
  ui->setSteamVrStarting("Synthetic table");QVERIFY(softwareFrame(window));QVERIFY(!headsetButton->isEnabled());QVERIFY(playButton->isEnabled());
  ui->setSteamVrStarting({});ui->setSteamVrRunning(true);QVERIFY(softwareFrame(window));QVERIFY(headsetButton->isEnabled());
  QVERIFY(namedItems(window->contentItem(),"pinballHeadsetReason").first()->property("text").toString().isEmpty());
  ui->setSteamVrRunning(false);
  root->setProperty("vrOverlayMode",false);QVERIFY(softwareFrame(window));
  QCOMPARE(launchHelp->property("text").toString(),helpText);
  root->setProperty("vrOverlayMode",true);QVERIFY(softwareFrame(window));
  QTRY_VERIFY(!namedItems(window->contentItem(),"universalControlsView").isEmpty());
  auto model=ui->property("pinballControls").value<QObject *>();QVERIFY(model);
  QCOMPARE(model->property("gameId").toString(),QString("pinball-vpx-openxr"));
  QVERIFY(!model->property("rows").toList().isEmpty());
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"selectPinball",Q_ARG(QString,"")));
  root->setProperty("vrOverlayMode",false);root->setProperty("view","List");
 }
 void pinballGlobalStopRealScene_data(){
  QTest::addColumn<QString>("phase");
  for(const auto &phase:{"preparing","running","restart-pending","unsupported-close","ordinary"})
   QTest::newRow(phase)<<QString::fromLatin1(phase);
 }
 void pinballGlobalStopRealScene(){
  QFETCH(QString,phase);
  QTemporaryDir temporary;ac::UiSettings localSettings(temporary.path());localSettings.set("showAllGames",true);
  ac::UiController localUi(games.get(),&filter,&localSettings);
  LaunchLifecycleProbe probe;probe.startup=phase=="preparing";
  QQmlContext context(engine->rootContext());context.setContextProperty("hubServices",&probe);
  context.setContextProperty("uiController",&localUi);context.setContextProperty("uiSettings",&localSettings);
  QQmlComponent component(engine.get());component.setData("import QtQuick; import AladdinsCastle.Hub; HubRoot {width:1120;height:720;view: \"List\"}",QUrl());
  std::unique_ptr<QQuickItem> scene(qobject_cast<QQuickItem *>(component.create(&context)));
  QVERIFY2(scene,qPrintable(component.errorString()));scene->setParentItem(window->contentItem());scene->setZ(100);
  const auto id=phase=="ordinary"?QString("timecris"):QString("pinball-synthetic");
  if(phase=="ordinary")localUi.launchQueued(id,"flat",QString());
  else localUi.launchPinballQueued({{"id","synthetic"},{"title","Synthetic table"}},phase=="unsupported-close"?QString("Close is unavailable here"):QString());
  if(!probe.startup)localUi.launchStarted(id);
  QSignalSpy close(&localUi,&ac::UiController::closeLaunchRequested),forced(&localUi,&ac::UiController::stopLaunchRequested);
  QSignalSpy replay(&localUi,&ac::UiController::playRequested);
  if(phase=="restart-pending")localUi.restartLaunch();
  auto stop=scene->findChild<QQuickItem *>("globalStop");QVERIFY(stop);QTRY_VERIFY(stop->isVisible());
  QVERIFY(softwareFrame(window));QVERIFY(withinClips(stop));
  const auto point=stop->mapToScene({stop->width()/2,stop->height()/2});
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
  if(phase=="ordinary"){
   QCOMPARE(forced.count(),1);QCOMPARE(close.count(),0);QVERIFY(localUi.status().contains("forced"));
  }else if(phase=="unsupported-close"){
   QCOMPARE(forced.count(),0);QCOMPARE(close.count(),0);QVERIFY(!localUi.nowPlaying().value("closing").toBool());
   QVERIFY(localUi.status().contains("VPX"));
  }else{
   // Pinball's Stop is Stop like any other: VPX is asked to close, then the Hub ends it if it has not.
   // A pending Restart had already asked it to close once; Stop cancels the restart and forces the end.
   const int asked=phase=="restart-pending"?1:0;
   QCOMPARE(forced.count(),1);QCOMPARE(close.count(),asked);QVERIFY(localUi.nowPlaying().value("closing").toBool());
   QVERIFY(localUi.status().contains(phase=="preparing"?"cancellation":"VPX is asked to close"));
   QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
   QCOMPARE(forced.count(),1);QCOMPARE(close.count(),asked); // A second press does not start the wait over.
  }
  localUi.launchFinished(id,QString());QVERIFY(localUi.nowPlaying().isEmpty());QCOMPARE(replay.count(),0);
 }
 void sharedNowPlayingRealScene(){
  const auto previousView=root->property("view"),previousMode=root->property("vrOverlayMode");
  const auto cleanup=qScopeGuard([&]{ui->launchFinished("timecris","Synthetic fixture cleanup");root->setProperty("view",previousView);root->setProperty("vrOverlayMode",previousMode);});
  root->setProperty("view","List"); root->setProperty("vrOverlayMode",true);
  QVERIFY(QMetaObject::invokeMethod(ui.get(),"launchQueued",Q_ARG(QString,"timecris"),Q_ARG(QString,"flat"),Q_ARG(QString,"")));
  ui->launchStarted("timecris");
  QTRY_VERIFY(!namedItems(window->contentItem(),"nowPlayingExit").isEmpty());
  auto exit=namedItems(window->contentItem(),"nowPlayingExit").first();
  QVERIFY(exit->isVisible());snapshot("card-before-frame",exit);
  QVERIFY(softwareFrame(window));snapshot("card-after-frame",exit);
  QVERIFY(exit->height()>=48);
  QVERIFY(namedItems(window->contentItem(),"nowPlayingRecentre").isEmpty()); // A button that did nothing is gone, not greyed out.
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
 void gunRoomDesktopRealScene(){
  QTemporaryDir temporary;ac::UiSettings localSettings(temporary.path());localSettings.set("showAllGames",true);
  localSettings.set("gunRoomExecutable",temporary.filePath("reviewed room.exe"));
  ac::UiController localUi(games.get(),&filter,&localSettings);GunRoomProbe probe;
  QQmlContext context(engine->rootContext());context.setContextProperty("hubServices",&probe);
  context.setContextProperty("uiController",&localUi);context.setContextProperty("uiSettings",&localSettings);
  QQmlComponent component(engine.get());component.setData("import QtQuick; import AladdinsCastle.Hub; HubRoot {width:1120;height:720;view: \"List\"}",QUrl());
  std::unique_ptr<QQuickItem> scene(qobject_cast<QQuickItem *>(component.create(&context)));
  QVERIFY2(scene,qPrintable(component.errorString()));scene->setParentItem(window->contentItem());scene->setZ(100);
  auto help=scene->findChild<QObject *>("helpPanel");QVERIFY(help);QVERIFY(QMetaObject::invokeMethod(help,"open"));
  auto entry=scene->findChild<QQuickItem *>("helpGunRoom");QVERIFY(entry);QTRY_VERIFY(entry->isVisible());
  auto entryPoint=entry->mapToScene({entry->width()/2,entry->height()/2});
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,entryPoint.toPoint());
  QTRY_COMPARE(scene->property("view").toString(),QString("GunRoom"));
  QTRY_VERIFY(!help->property("visible").toBool());
  QTRY_VERIFY(scene->findChild<QQuickItem *>("gunRoomLaunch"));
  auto launch=scene->findChild<QQuickItem *>("gunRoomLaunch");
  auto model=scene->findChild<QObject *>("gunRoomModel"),hand=scene->findChild<QObject *>("gunRoomHand");
  QVERIFY(model);QVERIFY(hand);QCOMPARE(hand->property("currentIndex").toInt(),-1);QVERIFY(!launch->isEnabled());
  model->setProperty("currentIndex",0);QVERIFY(!launch->isEnabled());hand->setProperty("currentIndex",0);
  QTRY_VERIFY(launch->isEnabled());
  snapshot("gun-room-before-frame",launch);
  QVERIFY(softwareFrame(window));snapshot("gun-room-after-frame",launch);
  auto point=launch->mapToScene({launch->width()/2,launch->height()/2});
  auto page=scene->findChild<QQuickItem *>("gunRoomPage");QVERIFY(page);
  page->setProperty("contentY",qMax(0.0,point.y()-window->height()/2));
  QVERIFY(softwareFrame(window));snapshot("gun-room-after-scroll",launch);
  QTRY_VERIFY(withinClips(launch));
  point=launch->mapToScene({launch->width()/2,launch->height()/2});
  QSignalSpy presses(launch,SIGNAL(pressed())),releases(launch,SIGNAL(released())),clicks(launch,SIGNAL(clicked()));
  hitSnapshot(window,point);
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
  if(diagnosticEnabled())qInfo()<<"DISPATCH"<<presses.size()<<releases.size()<<clicks.size()<<probe.launches;
  QCOMPARE(probe.launches,1);QCOMPARE(probe.model,QString("synthetic-pistol"));QCOMPARE(probe.hand,QString("left"));
  QCOMPARE(probe.binary,localSettings.get("gunRoomExecutable").toString());
  auto calibrate=scene->findChild<QQuickItem *>("gunRoomCalibrate");QVERIFY(calibrate);
  QVERIFY(!calibrate->isVisible());QCOMPARE(probe.calibrations,0);
  probe.calibrationCapability=true;emit probe.gunRoomChanged();
  QTRY_VERIFY(calibrate->isVisible());QTRY_VERIFY(calibrate->isEnabled());
  QVERIFY(softwareFrame(window));point=calibrate->mapToScene({calibrate->width()/2,calibrate->height()/2});
  page->setProperty("contentY",page->property("contentY").toReal()+point.y()-window->height()/2);
  QVERIFY(softwareFrame(window));QTRY_VERIFY(withinClips(calibrate));
  point=calibrate->mapToScene({calibrate->width()/2,calibrate->height()/2});
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
  QCOMPARE(probe.calibrations,1);QCOMPARE(probe.launches,1);
  QCOMPARE(probe.hand,QString("left"));
  probe.calibrationCapability=false;emit probe.gunRoomChanged();QTRY_VERIFY(!calibrate->isVisible());
  QVERIFY(QMetaObject::invokeMethod(&localUi,"launchRoomQueued",Q_ARG(QString,probe.model),Q_ARG(QString,probe.hand)));
  localUi.launchStarted("__gun_room");
  auto exit=scene->findChild<QQuickItem *>("nowPlayingExit");QVERIFY(exit);QTRY_VERIFY(exit->isVisible());
  QSignalSpy close(&localUi,&ac::UiController::closeLaunchRequested);
  point=exit->mapToScene({exit->width()/2,exit->height()/2});QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
  QCOMPARE(close.size(),1);QVERIFY(!exit->isEnabled());
  QVERIFY(localUi.nowPlaying().value("closing").toBool());
  localUi.launchFinished("__gun_room",QString());QTRY_VERIFY(!exit->isVisible());
  QVERIFY(QMetaObject::invokeMethod(&localUi,"launchRoomQueued",Q_ARG(QString,probe.model),Q_ARG(QString,probe.hand),Q_ARG(bool,true)));
  localUi.launchStarted("__gun_room");
  QVERIFY(localUi.nowPlaying().value("calibrateGrip").toBool());
  // A later form edit must not change the retained card/restart action.
  hand->setProperty("currentIndex",1);QCOMPARE(localUi.nowPlaying().value("variantId").toString(),QString("synthetic-pistol|left|calibrate"));
  QSignalSpy replay(&localUi,&ac::UiController::playRequested);
  auto restart=scene->findChild<QQuickItem *>("nowPlayingRestart");QVERIFY(restart);
  QTRY_VERIFY(restart->isVisible());QVERIFY(softwareFrame(window));QVERIFY(withinClips(restart));
  point=restart->mapToScene({restart->width()/2,restart->height()/2});
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
  QVERIFY(localUi.nowPlaying().value("closing").toBool());QCOMPARE(replay.size(),0);
  localUi.launchFinished("__gun_room",QString());QCOMPARE(replay.size(),1);
  QCOMPARE(replay[0][1].toString(),QString("synthetic-pistol|left|calibrate"));
  scene->setProperty("vrOverlayMode",true);QTRY_VERIFY(!scene->findChild<QQuickItem *>("gunRoomLaunch"));
  QVERIFY(!entry->isVisible());
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
 void adoptedTimeCrisisControlsOnRealPage(){
  const auto restore=qScopeGuard([&]{root->setProperty("view","List");});
  ui->openDetail("timecris"); ui->selectVariant("dr89-pcvr"); root->setProperty("view","Detail");
  auto controls=ui->controlsController();
  QTRY_VERIFY_WITH_TIMEOUT(!controls->loading(),10000);
  QTRY_VERIFY_WITH_TIMEOUT(!namedItems(window->contentItem(),"universalControlsView").isEmpty(),3000);
  const auto data=controls->resolved().value("data").toMap();
  QCOMPARE(data.value("schema").toString(),QString("system22-native/1"));
  QCOMPARE(data.value("route").toString(),QString("namco22-vr"));
  QCOMPARE(data.value("policy").toMap().value("native_players").toInt(),1);
  QCOMPARE(data.value("policy").toMap().value("two_guns").toString(),QString("off"));
  QVERIFY(!controls->model()->rows().isEmpty());
  for(const auto &value:controls->model()->rows()) {
    const auto row=value.toMap(); QCOMPARE(row.value("player").toInt(),0);
    QVERIFY(row.contains("runtimeValidated")); QVERIFY(!row.value("runtimeValidated").toBool());
    QVERIFY(!row.value("action").toString().contains("offscreen",Qt::CaseInsensitive));
  }
  ui->selectVariant("mame");
  QVERIFY2(controls->model()->rows().isEmpty(),"Adopted native rows survived the immediate flat-route switch");
  QTRY_VERIFY_WITH_TIMEOUT(!controls->loading(),10000);
  QVERIFY(controls->resolved().value("data").toMap().value("route").toString()!="namco22-vr");
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
  ac::UiSettings localSettings(fixture.temp.filePath("user"));localSettings.set("showAllGames",true);ac::UiController localUi(&localGames,&localFilter,&localSettings);
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
  // Set up and ready here: listed on the VR games page even with the catalog switched off.
  {bool listed=false;for(const auto &row:localUi.readyCatalogGames())listed=listed||row.toMap().value("id")=="wii-synthetic";QVERIFY(listed);}
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
 // The Time Crisis VR copy is admitted through its pinned local build (the recipe locates the user's own
 // build and downloads nothing); a parked authored copy with no admitted launch stays blocked and says so.
 void authoredVrCopyAdmittedOnlyWithItsPinnedLocalBuild(){
  ui->openDetail("timecris");ui->selectVariant("dr89-pcvr");
  QVERIFY(ui->detail().value("m1Available").toBool());
  QSignalSpy install(ui.get(),&ac::UiController::installRequested),play(ui.get(),&ac::UiController::playRequested);
  ui->startInstall("timecris","dr89-pcvr");QCOMPARE(install.count(),1);QVERIFY(!ui->status().contains("not enabled"));
  QVERIFY(!ui->detail().value("components").toList().isEmpty());
  ui->openDetail("timecris");ui->selectVariant("namco22-vr");
  QVERIFY(!ui->detail().value("m1Available").toBool());
  const auto components=ui->detail().value("components").toList();QVERIFY2(!components.isEmpty(),"The parked copy still lists what it is made of.");
  QVERIFY2(components.first().toMap().value("role").toString().contains("recipe"),qPrintable(components.first().toMap().value("role").toString()));
  ui->startInstall("timecris","namco22-vr");QCOMPARE(install.count(),1);QVERIFY(ui->status().contains("not enabled"));
  ui->play("timecris","namco22-vr");QCOMPARE(play.count(),0);QVERIFY(ui->status().contains("not enabled"));
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
 void filtersAndDetails418(){
  qInfo("UI facet interaction checks");
  for(const auto &key:{"graphicsIds","manufacturerIds","hardwareIds","vrKeys","playersBuckets","controlsTypes","decades"}){auto choices=filter.choices(key);QVERIFY(!choices.isEmpty());for(const auto &choice:QVariantList{choices.first(),choices.last()}){filter.setFacet(key,QStringList{choice.toMap().value("id").toString()});QVERIFY(filter.visibleCount()>0);filter.clearFacets();}}
  root->setProperty("view","Detail");
  int rendered=0;
  for(const auto &g:games->records()){if(rendered++%50==0)qInfo("UI detail %d / 443",rendered);ui->openDetail(g.id);auto d=ui->detail();QCOMPARE(d.value("gameId").toString(),g.id);QVERIFY(!d.value("readme").toString().isEmpty());QVERIFY(!d.value("controls").toList().isEmpty());QCoreApplication::processEvents(QEventLoop::AllEvents,1);}
  filter.setQuery("scud");QVERIFY(filter.visibleCount()>0);filter.setQuery("unlikely-no-title-synthetic");QCOMPARE(filter.visibleCount(),0);filter.setQuery("");root->setProperty("view","List");QCOMPARE(filter.visibleCount(),443);
 }
 void signalsAndProofGates(){
  QSignalSpy scans(ui.get(),&ac::UiController::scanRequested),cancels(ui.get(),&ac::UiController::cancelScanRequested);ui->startScan({"synthetic-root"});ui->startScan({"synthetic-root"});QCOMPARE(scans.count(),1);QCOMPARE(cancels.count(),0);QVERIFY(ui->scanning());ui->cancelScan();QCOMPARE(cancels.count(),1);ui->scanStarted();QVERIFY(ui->scanning());ui->scanFinished(true);QVERIFY(filter.scanComplete());
  ui->openDetail("timecris");for(const auto &v:games->find("timecris")->variants)if(v.generated){ui->selectVariant(v.id);break;}
  const auto eventStart=ui->consoleEvents().size();ui->installEvent({{"kind","step"},{"step",2},{"total",4},{"text","Extract"}});ui->installEvent({{"kind","ok"},{"text","Verified archive"}});QCOMPARE(ui->consoleEvents().at(eventStart).toMap().value("line").toString(),QString("--- [2/4] Extract ---"));ui->installEvent({{"kind","fail"},{"step",2},{"total",4},{"text","Synthetic failure"}});QVERIFY(!ui->recovery().isEmpty());QVERIFY(!ui->installing());QSignalSpy retries(ui.get(),&ac::UiController::retryInstallRequested);ui->retryInstall(false,"synthetic-file");QCOMPARE(retries.count(),0);QVERIFY(ui->status().contains("Nothing to retry"));ui->installFinished(true,"Synthetic service completion without proof");
  for(const auto &g:games->records())QVERIFY(g.roles.value("state").toInt()!=static_cast<int>(ac::GameState::Installed));
  ui->openDetail("timecris");ui->selectVariant("namco22-vr");QSignalSpy install(ui.get(),&ac::UiController::installRequested);ui->startInstall("timecris","namco22-vr");QCOMPARE(install.count(),0);
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
 void artDelayedRecordKeepsFallbackSafe(){
  const auto before=warnings.size();QQmlComponent component(engine.get());
  component.setData("import QtQuick; import AladdinsCastle.Hub; GameArt { property bool recordReady:false; readonly property var safeGame: recordReady ? ({accent: \"#123456\"}) : undefined }",QUrl());
  std::unique_ptr<QQuickItem> art(qobject_cast<QQuickItem *>(component.create(engine->rootContext())));
  QVERIFY2(art,qPrintable(component.errorString()));art->setParentItem(window->contentItem());art->setSize({160,120});
  QVERIFY(softwareFrame(window));QVERIFY(art->setProperty("recordReady",true));
  QVERIFY(art->property("recordReady").toBool());
  QCOMPARE(art->property("safeGame").toMap().value("accent").toString(),QString("#123456"));
  QVERIFY(softwareFrame(window));
  QColor expected("#123456");expected.setAlphaF(0.17f);int checked=0;
  for(auto child:art->childItems())if(child->property("rotation").toDouble()==28){
   QCOMPARE(child->property("color").value<QColor>(),expected);++checked;
  }
  QCOMPARE(checked,8);
  QStringList invalid;for(qsizetype i=before;i<warnings.size();++i)if(warnings[i].contains("TypeError"))invalid<<warnings[i];
  QVERIFY2(invalid.isEmpty(),qPrintable(invalid.join('\n')));
 }
 private:
 // Shown and inside every clipping parent: a delegate kept in a list's cache is not counted.
 QQuickItem *visibleNamed(const QString &name){for(auto *item:namedItems(window->contentItem(),name))if(item->isVisible()&&withinClips(item))return item;return nullptr;}
 QSize desktopSize;
 QQuickItem *enterPanel(const QVariantList &rows){
  // The panel's own size.
  if(desktopSize.isEmpty())desktopSize=window->size();
  window->resize(1280,800);
  root->setProperty("history",QVariantList{});root->setProperty("vrOverlayMode",true);root->setProperty("overlayPresentation",true);
  if(!QMetaObject::invokeMethod(root,"navigate",Q_ARG(QVariant,"Model3"),Q_ARG(QVariant,QVariant())))return nullptr;
  QQuickItem *panel=nullptr;
  if(!QTest::qWaitFor([&]{panel=visibleNamed("panelLibrary");return panel!=nullptr;},3000))return nullptr;
  panel->setProperty("rows",rows);root->setProperty("history",QVariantList{});QTest::qWait(350);softwareFrame(window);
  return panel;
 }
 void leavePanel(){
  QMetaObject::invokeMethod(root,"closeMenus");
  if(auto loader=window->findChild<QObject *>("model3Loader"))loader->setProperty("keep",false);
  root->setProperty("overlayPresentation",false);root->setProperty("vrOverlayMode",false);root->setProperty("history",QVariantList{});root->setProperty("view","List");
  if(!desktopSize.isEmpty()){window->resize(desktopSize);}desktopSize={};QTest::qWait(50);
 }
 void sendWheel(QPointF at,int dy){QWheelEvent event(at,window->mapToGlobal(at),{},QPoint(0,dy),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(window,&event);}
 private slots:
 // The SteamVR panel: one grid that scrolls as one surface, a row at a time, with a dead zone and no jitter.
 void panelGridScrollsByRowsWithDeadZone(){
  const auto cleanup=qScopeGuard([&]{leavePanel();});
  auto panel=enterPanel(syntheticPanelRows(70));QVERIFY(panel);
  // No split: no hero over the grid; the grid takes the panel below the slim bar.
  QVERIFY(namedItems(window->contentItem(),"model3Hero").isEmpty());QVERIFY(!visibleNamed("brand"));
  auto grid=visibleNamed("panelGrid");QVERIFY(grid);QVERIFY(grid->height()>window->height()*0.6);
  QVERIFY(visibleNamed("panelTopBar")->height()<=80);QVERIFY(visibleNamed("panelSearchButton"));QVERIFY(visibleNamed("panelFilter"));
  for(const auto &tab:{"all","ps1","model2","model3","system22","dreamcast","ps2","wii","pinball"})QVERIFY2(visibleNamed(QString("panelTab-")+tab),tab);
  QVERIFY(!visibleNamed("panelTab-n64"));QVERIFY(visibleNamed("panelBackHint"));
  // Large laser targets.
  auto tile=visibleNamed("panelTile");QVERIFY(tile);QVERIFY(tile->width()>=200);QVERIFY(tile->height()>=110);
  auto scroller=panel->findChild<QObject *>("panelScroller");QVERIFY(scroller);
  const double row=grid->property("cellHeight").toDouble(),minY=grid->property("originY").toDouble()-grid->property("topMargin").toDouble();
  const auto maxY=[&]{return grid->property("originY").toDouble()+grid->property("contentHeight").toDouble()+grid->property("bottomMargin").toDouble()-grid->height();};
  const auto y=[&]{return grid->property("contentY").toDouble();};
  const auto aligned=[&]{const double r=(y()-minY)/row;return std::abs(r-std::round(r))<0.01||std::abs(y()-maxY())<0.5;};
  const QPointF over=grid->mapToScene(QPointF(grid->width()/2,grid->height()/2));
  QCOMPARE(y(),minY);
  // A resting stick: weak packets never move the page.
  for(int i=0;i<40;++i){sendWheel(over,-10);QTest::qWait(8);}
  QTest::qWait(250);QCOMPARE(y(),minY);QCOMPARE(scroller->property("steps").toInt(),0);
  // Noise flipping between up and down never moves it either.
  for(int i=0;i<40;++i){sendWheel(over,i%2?150:-150);QTest::qWait(8);}
  QTest::qWait(250);QCOMPARE(y(),minY);QCOMPARE(scroller->property("steps").toInt(),0);
  // Held down: rows one after another, never backwards, faster the longer it is held, each on a row boundary.
  QList<qint64> stepTimes;QElapsedTimer held;held.start();double last=y();bool backwards=false;int steps=0;
  while(held.elapsed()<1300){sendWheel(over,-150);QTest::qWait(16);if(y()<last-0.5)backwards=true;last=y();const int now=scroller->property("steps").toInt();if(now>steps){steps=now;stepTimes<<held.elapsed();}}
  QTest::qWait(300);
  QVERIFY(!backwards);QVERIFY2(stepTimes.size()>=5,qPrintable(QString::number(stepTimes.size())));QVERIFY(aligned());QVERIFY(y()>minY+row*4-0.5);
  QList<qint64> gaps;for(int i=1;i<stepTimes.size();++i)gaps<<stepTimes[i]-stepTimes[i-1];
  QVERIFY2(gaps.last()<gaps.first(),qPrintable(QString("first gap %1 ms, last %2 ms").arg(gaps.first()).arg(gaps.last())));
  // One notch up after a pause: exactly one row.
  const double before=y();const int stepsBefore=scroller->property("steps").toInt();
  sendWheel(over,120);QTest::qWait(300);
  QCOMPARE(scroller->property("steps").toInt(),stepsBefore+1);QVERIFY(std::abs(y()-(before-row))<0.5);QVERIFY(aligned());
  // Scrolling opened nothing.
  QCOMPARE(panel->property("selectedId").toString(),QString());
  // The keyboard moves the highlight a row at a time and keeps it in view, still on row boundaries.
  panel->forceActiveFocus();for(int i=0;i<12;++i){QTest::keyClick(window,Qt::Key_Down);QTest::qWait(20);}
  QTest::qWait(300);QVERIFY(aligned());QCOMPARE(panel->property("selectedId").toString(),QString());
 }
 // B goes back exactly one level per press: menu, game page, tab, page; never two at once and never out of the Hub.
 void panelBackStepsOneLevelPerPress(){
  const auto cleanup=qScopeGuard([&]{leavePanel();});
  auto panel=enterPanel(syntheticPanelRows(30));QVERIFY(panel);
  const auto centre=[](QQuickItem *item){return item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();};
  // The double-click guard drops presses for a moment after each page change; a slow PC can stretch a fixed wait
  // past it or short of it, so a click waits until the guard's time has passed.
  const auto click=[&](QQuickItem *item){QVERIFY(item);QVERIFY(QTest::qWaitFor([&]{return QDateTime::currentMSecsSinceEpoch()>=root->property("clickGuardUntil").toDouble();},3000));QVERIFY(softwareFrame(window));QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,centre(item));};
  const auto steps=[&]{return root->property("backSteps").toInt();};
  const auto b=[&]{emit overlayProbe.backRequested();};
  // At the top: nothing happens and the Hub stays.
  int start=steps();b();QTest::qWait(50);QCOMPARE(steps(),start);QCOMPARE(root->property("view").toString(),QString("Model3"));QVERIFY(window->isVisible());
  // A tab, then a game in it.
  click(visibleNamed("panelTab-ps1"));QTRY_COMPARE(panel->property("tab").toString(),QString("ps1"));
  QCOMPARE(panel->property("shown").toList().size(),4);
  QTest::qWait(350);click(visibleNamed("panelTile"));QTRY_VERIFY(!panel->property("selectedId").toString().isEmpty());
  QTRY_VERIFY(visibleNamed("panelGamePage"));
  // Spammed B, five presses in 100 ms: one level only. The game page closes; the tab stays.
  QTest::qWait(300);start=steps();for(int i=0;i<5;++i){b();QTest::qWait(20);}
  QCOMPARE(steps(),start+1);QCOMPARE(panel->property("selectedId").toString(),QString());QCOMPARE(panel->property("tab").toString(),QString("ps1"));
  // The next press, after the page change: the tab goes back to All.
  QTest::qWait(300);b();QCOMPARE(steps(),start+2);QCOMPARE(panel->property("tab").toString(),QString("all"));
  // Then the top: nothing, however often.
  for(int i=0;i<4;++i){QTest::qWait(300);b();}
  QCOMPARE(steps(),start+2);QCOMPARE(root->property("view").toString(),QString("Model3"));QVERIFY(window->isVisible());
  // A search counts as a filter: B clears it.
  panel->setProperty("query","synthetic game 0");QTRY_VERIFY(panel->property("shown").toList().size()<30);QTest::qWait(300);b();QCOMPARE(panel->property("query").toString(),QString());
  // The filter menu closes first and leaves the page as it was.
  QTest::qWait(300);click(visibleNamed("panelFilter"));auto menu=window->findChild<QObject *>("panelFilterMenu");QVERIFY(menu);QTRY_VERIFY(menu->property("opened").toBool());
  QTest::qWait(300);start=steps();b();QTRY_VERIFY(!menu->property("visible").toBool());QCOMPARE(steps(),start+1);QCOMPARE(root->property("view").toString(),QString("Model3"));
  // The Hub's own menu too.
  QTest::qWait(300);click(visibleNamed("panelMenu"));auto help=root->findChild<QObject *>("helpPanel");QVERIFY(help);QTRY_VERIFY(help->property("opened").toBool());
  QTest::qWait(300);b();QTRY_VERIFY(!help->property("visible").toBool());QCOMPARE(root->property("view").toString(),QString("Model3"));
  // Pinball is a page of its own: B comes back to the library, on the same tab.
  QTest::qWait(300);click(visibleNamed("panelTab-pinball"));QTRY_COMPARE(root->property("view").toString(),QString("Pinball"));
  QTest::qWait(300);b();QTRY_COMPARE(root->property("view").toString(),QString("Model3"));QCOMPARE(root->property("history").toList().size(),0);
  QVERIFY(visibleNamed("panelLibrary")==panel); // Kept: the same library, not a new one.
  // Escape on a keyboard is B too.
  QTest::qWait(350);click(visibleNamed("panelTile"));QTRY_VERIFY(!panel->property("selectedId").toString().isEmpty());
  QTest::qWait(300);window->requestActivate();QTest::keyClick(window,Qt::Key_Escape);QTRY_COMPARE(panel->property("selectedId").toString(),QString());
  // While a game starts its card is the page: B leaves it alone.
  QTest::qWait(350);click(visibleNamed("panelTile"));QTRY_VERIFY(!panel->property("selectedId").toString().isEmpty());
  ui->launchQueued("synthetic-game","flat",QString());QTest::qWait(300);start=steps();b();QCOMPARE(steps(),start);QVERIFY(!panel->property("selectedId").toString().isEmpty());
  ui->launchFinished("synthetic-game","Launch executable or working directory missing",1);ui->dismissLastSession();
  QTest::qWait(300);b();QCOMPARE(panel->property("selectedId").toString(),QString());
 }
 // Double clicks and spammed buttons: one launch, a message for the refused ones, single-shot Stop and Install.
 void panelDoubleClickAndSpamStartOnce(){
  const auto cleanup=qScopeGuard([&]{if(!ui->nowPlaying().isEmpty())ui->launchFinished(ui->nowPlaying()["gameId"].toString(),QString(),0);ui->dismissLastSession();leavePanel();});
  int plays=0;
  // Stands in for the services: a Model 3 Play is queued at once, as HubServices does.
  auto connection=connect(ui.get(),&ac::UiController::model3PlayRequested,this,[&](const QString &id,const QString &view){++plays;ui->launchModel3Queued(id,view,QString());});
  const auto disconnectLater=qScopeGuard([&]{disconnect(connection);});
  auto panel=enterPanel(syntheticPanelRows(12));QVERIFY(panel);
  const auto centre=[](QQuickItem *item){return item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();};
  QQuickItem *first=nullptr;for(auto *tile:namedItems(window->contentItem(),"panelTile"))if(tile->property("title").toString()=="Scud Race"&&withinClips(tile))first=tile;QVERIFY(first);
  // A double click on the tile: the first click opens the page, the second lands in the click guard.
  QVERIFY(softwareFrame(window));const auto at=centre(first);
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,at);QTest::qWait(60);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,at);
  QTRY_VERIFY(visibleNamed("panelGamePage"));QCOMPARE(panel->property("selectedId").toString(),QString("scud"));QCOMPARE(plays,0);
  // A click on Play right after the page appears is also dropped.
  auto play=visibleNamed("panelPlay");QVERIFY(play);QCOMPARE(namedItems(window->contentItem(),"panelPlay").size(),3); // Model 3: three headset views.
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,centre(play));QTest::qWait(30);QCOMPARE(plays,0);
  // After the guard: Play, spammed five times, starts once.
  QTest::qWait(350);QVERIFY(play->isEnabled());const auto playAt=centre(play);
  for(int i=0;i<5;++i){QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,playAt);QTest::qWait(30);}
  QCOMPARE(plays,1);QCOMPARE(ui->nowPlaying()["gameId"].toString(),QString("model3-scud"));
  // Every Play button is off and says it is starting.
  for(auto *button:namedItems(window->contentItem(),"panelPlay")){QVERIFY(!button->isEnabled());QCOMPARE(button->property("text").toString(),QString::fromUtf8("Starting…"));}
  // A second launch through any door is refused with a message, not silently.
  const int refused=ui->property("refusedLaunches").toInt();
  ui->playGame("scud","screen");ui->playModel3("scud");ui->playOtherGame("ps1-synthetic2");
  QCOMPARE(plays,1);QCOMPARE(ui->property("refusedLaunches").toInt(),refused+3);QVERIFY2(ui->status().startsWith("Not started: "),qPrintable(ui->status()));QVERIFY(ui->status().contains("current session"));
  // Stop pressed five times asks once.
  ui->launchStarted("model3-scud");QSignalSpy stops(ui.get(),SIGNAL(stopLaunchRequested()));
  for(int i=0;i<5;++i)ui->stopLaunch();
  QCOMPARE(stops.size(),1);
  ui->launchFinished("model3-scud",QString(),0);QTRY_VERIFY(ui->nowPlaying().isEmpty());QVERIFY(ui->property("launchGateOpen").toBool());
  QVERIFY(softwareFrame(window));QTRY_VERIFY(visibleNamed("panelPlay")&&visibleNamed("panelPlay")->isEnabled());
  // Install while one runs: refused, said, nothing queued.
  QSignalSpy installs(ui.get(),&ac::UiController::installRequested);ui->installStarted();
  for(int i=0;i<3;++i)ui->startInstall("timecris","dr89-pcvr");
  QCOMPARE(installs.size(),0);QVERIFY(ui->status().contains("already running"));
  ui->installFinished(true,"Synthetic install finished");
 }
 // Pictures of the panel at its own size, for review; only when asked for.
 void panelPictures(){
  const auto output=qEnvironmentVariable("AC_UI_PANEL_PICTURES");if(output.isEmpty())QSKIP("Panel pictures are taken only when AC_UI_PANEL_PICTURES names a folder.");
  const auto cleanup=qScopeGuard([&]{leavePanel();});
  // Real catalog titles, sorted into systems; no game files and no artwork in this test, so tiles show their system colour.
  QVariantList rows=ui->model3Games();for(auto &value:rows){auto row=value.toMap();row["problem"]=QString();row["found"]=true;value=row;}
  const QMap<QString,QString> systems{{"sega-model-2","model2"},{"sony-ps1","ps1"},{"sony-ps2","ps2"},{"nintendo-wii","wii"},{"sega-dreamcast","dreamcast"}};
  QMap<QString,int> taken;
  for(const auto &record:games->records()){
   const auto hw=record.roles.value("hardwareId").toString();const auto system=systems.value(hw);
   if(system.isEmpty()||taken[system]>=6){continue;}++taken[system];
   rows<<QVariantMap{{"id",system+"-"+record.id},{"system",system},{"title",record.roles.value("title")},{"year",record.roles.value("year")},{"problem",QString()},{"warning",QString()},{"found",true},{"view","screen"},
    {"views",QVariantList{QVariantMap{{"id","screen"},{"label","3D screen"},{"help","A 3D screen in the headset."}},QVariantMap{{"id","theatre"},{"label","Theatre screen"},{"help","A big screen in the headset, with your controller as the gun."}}}}};
  }
  auto panel=enterPanel(rows);QVERIFY(panel);QDir().mkpath(output);
  QVERIFY(softwareFrame(window));QVERIFY(window->grabWindow().save(QDir(output).filePath("panel-1-grid.png")));
  auto grid=visibleNamed("panelGrid");const QPointF over=grid->mapToScene(QPointF(grid->width()/2,grid->height()/2));
  QElapsedTimer held;held.start();while(held.elapsed()<700){sendWheel(over,-150);QTest::qWait(16);}QTest::qWait(400);
  QVERIFY(softwareFrame(window));QVERIFY(window->grabWindow().save(QDir(output).filePath("panel-2-grid-scrolled.png")));
  panel->setProperty("selectedId","scud");QTest::qWait(400);
  QVERIFY(softwareFrame(window));QVERIFY(window->grabWindow().save(QDir(output).filePath("panel-3-game-page.png")));
  const auto save=[&](const QString &name){QVERIFY(softwareFrame(window));QVERIFY(window->grabWindow().save(QDir(output).filePath(name)));};
  const auto b=[&]{QTest::qWait(300);emit overlayProbe.backRequested();QTest::qWait(300);};
  // B at each level: a game page in the PS1 tab, then the PS1 grid, then the whole grid, then the top (no change).
  panel->setProperty("selectedId","");QTest::qWait(300);panel->setProperty("tab","ps1");QTest::qWait(300);
  save("panel-4-tab-ps1.png");
  QQuickItem *ps1Tile=visibleNamed("panelTile");QVERIFY(ps1Tile);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,ps1Tile->mapToScene(QPointF(ps1Tile->width()/2,ps1Tile->height()/2)).toPoint());
  QTRY_VERIFY(visibleNamed("panelGamePage"));QTest::qWait(300);save("panel-5-b0-game-page-in-ps1-tab.png");
  b();QVERIFY(!visibleNamed("panelGamePage"));save("panel-6-b1-back-to-ps1-grid.png");
  b();QCOMPARE(panel->property("tab").toString(),QString("all"));save("panel-7-b2-back-to-all-grid.png");
  b();QCOMPARE(root->property("view").toString(),QString("Model3"));save("panel-8-b3-top-nothing-happens.png");
  // The filter menu open, then closed by B.
  QQuickItem *filterButton=visibleNamed("panelFilter");QVERIFY(filterButton);QTest::qWait(300);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,filterButton->mapToScene(QPointF(filterButton->width()/2,filterButton->height()/2)).toPoint());
  QTest::qWait(300);save("panel-9-filter-menu.png");b();save("panel-10-b-closed-filter-menu.png");
  // Double click on a tile, then Play spammed: one start, every Play shows Starting, a refused second launch is said.
  int plays=0;auto connection=connect(ui.get(),&ac::UiController::model3PlayRequested,this,[&](const QString &id,const QString &view){++plays;ui->launchModel3Queued(id,view,QString());});
  const auto finish=qScopeGuard([&]{disconnect(connection);if(!ui->nowPlaying().isEmpty())ui->launchFinished(ui->nowPlaying()["gameId"].toString(),QString(),0);ui->dismissLastSession();});
  QQuickItem *scud=nullptr;for(auto *tile:namedItems(window->contentItem(),"panelTile"))if(tile->property("title").toString()=="Scud Race"&&withinClips(tile))scud=tile;QVERIFY(scud);
  const auto at=scud->mapToScene(QPointF(scud->width()/2,scud->height()/2)).toPoint();
  QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,at);QTest::qWait(60);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,at);
  QTRY_VERIFY(visibleNamed("panelGamePage"));QCOMPARE(plays,0);save("panel-11-after-double-click-page-open-nothing-started.png");
  QTest::qWait(350);QQuickItem *play=visibleNamed("panelPlay");QVERIFY(play);const auto playAt=play->mapToScene(QPointF(play->width()/2,play->height()/2)).toPoint();
  for(int i=0;i<5;++i){QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,playAt);QTest::qWait(30);}
  QCOMPARE(plays,1);save("panel-12-play-spammed-starting-card.png");
  ui->playGame("scud","screen");QCOMPARE(plays,1);save("panel-13-second-launch-refused-message.png");
 }
 void sceneWarnings(){QTest::qWait(100);QStringList invalid;for(const auto &w:warnings)if(w.contains("ReferenceError")||w.contains("TypeError")||w.contains("Unable to assign")||w.contains("Binding loop")||w.contains("is not a type"))invalid<<w;QVERIFY2(invalid.isEmpty(),qPrintable(invalid.join('\n')));}
 void selfBenchmark(){
  const auto output=qEnvironmentVariable("AC_UI_BENCH_OUTPUT");if(output.isEmpty())QSKIP("Owner-hardware headed timing runs explicitly through AC_UI_BENCH_OUTPUT.");
  root->setProperty("view","List");filter.clearFacets();filter.setQuery("");window->resize(1120,720);QTest::qWait(500);
  auto list=window->findChild<QObject *>("sectionedGrid");QVERIFY(list);
  for(auto p=qobject_cast<QQuickItem *>(list);p;p=p->parentItem()){qInfo()<<"Ancestor"<<p->metaObject()->className()<<p->position()<<p->size()<<p->implicitHeight();if(p->parentItem())for(auto c:p->parentItem()->childItems())qInfo()<<"Sibling"<<c->metaObject()->className()<<c->position()<<c->size()<<c->implicitHeight();}
  QJsonObject report;report["catalogGames"]=443;report["renderer"]=QStringLiteral("OpenGL");report["initialDelegateCount"]=cardCount(window->contentItem());report["viewportHeight"]=list->property("height").toDouble();
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
