// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogLoader.h"
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
 void desktopDetailEmbedsNativeControls(){
  ui->openDetail("timecris");root->setProperty("view","Detail");
  QTRY_VERIFY_WITH_TIMEOUT(!namedItems(window->contentItem(),"universalControlsView").isEmpty(),3000);
  const auto view=namedItems(window->contentItem(),"universalControlsView").first();
  auto model=view->property("controlsModel").value<QObject *>();QVERIFY(model);
  QCOMPARE(model->property("gameId").toString(),QString("timecris"));
  QVERIFY(!model->property("rows").toList().isEmpty());
  QVERIFY(!model->property("previewAvailable").toBool());
  root->setProperty("view","List");
  QTRY_VERIFY(model->property("gameId").toString().isEmpty());
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
