// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogLoader.h"
#include "ui/Theme.h"
#include "ui/UiController.h"
#include "ui/UiSettings.h"
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTimer>
#include <QQuickImageProvider>
#include <QPainter>
#include <QAtomicInteger>
#include <QJsonDocument>
#include <QJsonObject>
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
 void componentLoading(){
  const QStringList components{"PillButton","UiText","GameArt","GradientText","GameCard","SectionHeader","FeaturedBanner","Header","FilterBar","ScanProgress","FiltersDrawer","GameGrid","LibraryTile","RecentlyPlayedRow","ExplorePage","DetailPage","InstallConsole","RecoveryPanel","SettingsPage","SortMenu","HelpPanel"};
  for(const auto &name:components){QQmlComponent c(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/"+name+".qml"));QVERIFY2(c.isReady(),qPrintable(c.errorString()));std::unique_ptr<QObject> o(c.create());QVERIFY2(o!=nullptr,qPrintable(c.errorString()));}
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
  QSignalSpy scans(ui.get(),&ac::UiController::scanRequested);ui->scan({"synthetic-root"});QCOMPARE(scans.count(),1);QVERIFY(!ui->scanning());ui->scanStarted();QVERIFY(ui->scanning());ui->scanFinished(true);QVERIFY(filter.scanComplete());
  ui->installEvent({{"kind","step"},{"step",2},{"total",4},{"text","Extract"}});ui->installEvent({{"kind","ok"},{"text","Verified archive"}});QCOMPARE(ui->consoleEvents().first().toMap().value("line").toString(),QString("--- [2/4] Extract ---"));ui->installEvent({{"kind","fail"},{"step",2},{"total",4},{"text","Synthetic failure"}});QVERIFY(!ui->recovery().isEmpty());QVERIFY(!ui->installing());QSignalSpy retries(ui.get(),&ac::UiController::retryInstallRequested);ui->retryInstall(false,"synthetic-file");QCOMPARE(retries.count(),1);ui->installFinished(true,"Synthetic service completion without proof");
  for(const auto &g:games->records())QVERIFY(g.roles.value("state").toInt()!=static_cast<int>(ac::GameState::Installed));
  ui->openDetail("timecris");ui->selectVariant("dr89-pcvr");QSignalSpy install(ui.get(),&ac::UiController::installRequested);ui->startInstall("timecris","dr89-pcvr");QCOMPARE(install.count(),0);
 }
 void keyboardFocusAndOverlay(){
  window->requestActivate();QTest::keyClick(window,Qt::Key_K,Qt::ControlModifier);QTest::qWait(30);auto search=window->findChild<QQuickItem *>("searchField");QVERIFY(search);QVERIFY(search->hasActiveFocus());for(char letter:std::string("scud"))QTest::keyClick(window,letter);QTest::qWait(30);QCOMPARE(filter.query(),QString("scud"));QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(30);QCOMPARE(filter.query(),QString());
  root->setProperty("vrOverlayMode",true);QTest::qWait(30);QQmlComponent c(engine.get(),QUrl("qrc:/qt/qml/AladdinsCastle/Hub/PillButton.qml"));std::unique_ptr<QObject> button(c.create());button->setProperty("vrOverlayMode",true);QVERIFY(button->property("implicitHeight").toDouble()>=40);root->setProperty("vrOverlayMode",false);
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
