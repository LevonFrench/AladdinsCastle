// SPDX-License-Identifier: GPL-3.0-only
#include "app/HubServices.h"
#include "core/app/LaunchOptions.h"
#include "core/catalog/CatalogLoader.h"
#include "core/install/Support.h"
#include <QTemporaryDir>
#include <QDateTime>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>
#include <QtTest>
#include <optional>
#include <QtEndian>
#include "core/pinball/Pinball.h"
namespace {
ac::CatalogData syntheticCatalog(const QString &root){
    auto data=ac::CatalogLoader().load(AC_CATALOG_ROOT);
    ac::install::atomicWrite(root+"/data/content-guard.toml",ac::install::readBytes(QString(AC_CATALOG_ROOT)+"/data/content-guard.toml"));
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
 void pinballLaunchUsesRetainedTableAndRecordsRecent(){
    QTemporaryDir t;const auto folder=t.filePath("library");QDir().mkpath(folder);
    // Synthetic CFB contains only an empty script terminator, no playable content.
    QByteArray cfb(5632,'\0');cfb.replace(0,8,QByteArray::fromHex("d0cf11e0a1b11ae1"));
    auto u16=[&](int at,quint16 n){qToLittleEndian(n,cfb.data()+at);};
    auto u32=[&](int at,quint32 n){qToLittleEndian(n,cfb.data()+at);};
    u16(26,3);u16(28,0xfffe);u16(30,9);u16(32,6);u32(44,1);u32(48,0);u32(56,4096);u32(60,0xfffffffe);u32(68,0xfffffffe);
    for(int i=0;i<109;++i)u32(76+4*i,i==0?9:0xffffffff);
    cfb[512+66]=5;u32(512+76,1);u32(512+116,0xfffffffe);
    for(int e=1;e<3;++e){const auto at=512+e*128;const QString name=e==1?"GameStg":"GameData";
        for(int i=0;i<name.size();++i)u16(at+2*i,name[i].unicode());u16(at+64,static_cast<quint16>((name.size()+1)*2));
        cfb[at+66]=e==1?1:2;u32(at+68,0xffffffff);u32(at+72,0xffffffff);u32(at+76,e==1?2:0xffffffff);
        u32(at+116,e==1?0xfffffffe:1);u32(at+120,e==1?0:4096);
    }
    u32(1024,4);cfb.replace(1028,4,"ENDB");
    for(int i=0;i<128;++i)u32(5120+4*i,i==0||i==8?0xfffffffe:i==9?0xfffffffd:i<8?static_cast<quint32>(i+1):0xffffffff);
    const auto table=folder+"/synthetic table.vpx";ac::install::atomicWrite(table,cfb);
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("pinballExecutable",QCoreApplication::applicationFilePath());
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    auto data=ac::pinball::scan(folder);auto row=data["tables"].toList()[0].toMap();QVERIFY(!row.contains("error"));
    ui.pinballLoaded(data);ui.selectPinball(row["id"].toString());ui.markPinball(true);
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    QVERIFY(QMetaObject::invokeMethod(&ui,"playPinball"));
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,5000);
    QCOMPARE(ui.nowPlaying()["title"].toString(),QString("synthetic table"));
    ui.selectPinball("");ui.restartLaunch();
#ifdef Q_OS_WIN
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),2,5000);
#else
    // Clean close is explicitly unavailable on the non-Windows route.
    QCOMPARE(started.size(),1);
#endif
    QTRY_VERIFY_WITH_TIMEOUT(!services.launcher()->busy(),5000);
    QVERIFY(finished.size()>=1);QVERIFY(ui.nowPlaying().isEmpty());
    const auto recent=settings.get("pinballRecent",QVariantMap{}).toMap();QVERIFY(recent[row["id"].toString()].toLongLong()>0);
    ui.pinballLoaded(data);ui.selectPinball(row["id"].toString());QVERIFY(ui.pinballSelection()["lastPlayed"].toLongLong()>0);
    QCOMPARE(ac::install::readBytes(table),cfb);QCOMPARE(QDir(folder).entryList(QDir::Files),QStringList{"synthetic table.vpx"});
    ac::install::atomicWrite(table,"changed synthetic metadata");
    QVERIFY(QMetaObject::invokeMethod(&ui,"playPinball"));QCOMPARE(started.size(),finished.size());
    QVERIFY(!services.launcher()->busy());QVERIFY(ui.status().contains("scan again",Qt::CaseInsensitive));
#ifdef Q_OS_WIN
    ac::install::atomicWrite(table,cfb);data=ac::pinball::scan(folder);ui.pinballLoaded(data);ui.selectPinball(row["id"].toString());
    QVERIFY(QMetaObject::invokeMethod(&ui,"playPinball"));QTRY_COMPARE_WITH_TIMEOUT(started.size(),3,5000);
    QSignalSpy exitReady(&services,&ac::HubServices::hubExitReady);services.requestHubExit(7);
    QCOMPARE(exitReady.size(),0);QTRY_COMPARE_WITH_TIMEOUT(exitReady.size(),1,5000);QCOMPARE(exitReady[0][0].toInt(),7);
    QVERIFY(!services.launcher()->busy());QCOMPARE(ac::install::readBytes(table),cfb);
#endif
 }
 void requiredM1GamesOfferFlatRoutes(){
    const auto catalog=ac::CatalogLoader().load(AC_CATALOG_ROOT);
    for(const auto &pair:QList<QPair<QString,QString>>{{"timecris","mame"},{"scud","supermodel"},{"ps2-time-crisis-2","pcsx2"}}){
        const auto *game=catalog.find(pair.first);QVERIFY(game);bool offered=false;
        for(const auto &v:game->variants)if(v.generated&&v.quality=="flat"&&v.tools.contains(pair.second))offered=true;
        QVERIFY2(offered,qPrintable("Required M1 flat route missing: "+pair.first));
    }
 }
 void authoredInstallationRestoresAfterHubRestart(){
    QTemporaryDir t;auto catalog=syntheticCatalog(t.path());auto &game=catalog.games[0];
    game.raw["hardware"]="nintendo-wii";game.raw["media"]=ac::Json::array({{{"kind","disc"},{"id","disc"}}});
    ac::Variant v;v.id="dolphinxr";v.title="DolphinXR (experimental VR)";v.status="wip";v.quality="true3d";v.media={"disc"};
    const auto dir=t.filePath("installed/synthetic/dolphinxr"),exe=dir+"/Dolphin.exe";
    ac::install::atomicWrite(exe,"synthetic application; never run");QDir().mkpath(dir+"/User");
    const auto hash=ac::install::hashFile(exe);
    v.raw={{"status","wip"},{"quality","true3d"},{"version","v0.4"},{"installed_when","file:${install_dir}/Dolphin.exe"},{"step",ac::Json::array()},
      {"launch",{{"exe","${install_dir}/Dolphin.exe"},{"exe_sha256",hash.toStdString()},{"args",ac::Json::array({"--user","${install_dir}/User"})},{"runtime","none"}}}};
    game.variants.prepend(v);game.install={{"format",1},{"default","dolphinxr"},{"variant",{{"dolphinxr",v.raw}}}};game.hasRecipe=true;
    const auto media=t.filePath("synthetic.iso");ac::install::atomicWrite(media,"synthetic disc");receipt(t.path(),media);
    auto bindings=ac::Json::parse(ac::install::readBytes(t.filePath("user/cache/scan-bindings.json")).toStdString());bindings["bindings"][0]["requirementId"]="disc";
    ac::install::atomicWrite(t.filePath("user/cache/scan-bindings.json"),QByteArray::fromStdString(bindings.dump()));
    ac::install::Request r;r.root=t.path();r.gameId=game.id;r.variantId=v.id;
    const auto base=ac::install::stateBase(r),recipeHash=ac::install::sha256(QByteArray::fromStdString(game.install.dump()));
    ac::install::writeEnvelope(base+".toml",{{"game","synthetic"},{"variant","dolphinxr"},{"state","installed"},{"installed_version","v0.4"},{"recipe_sha256",recipeHash.toStdString()}});
    ac::install::writeEnvelope(base+".manifest.toml",{{"game","synthetic"},{"variant","dolphinxr"},{"install_dir","installed/synthetic/dolphinxr"},{"installed_when","file:${install_dir}/Dolphin.exe"},{"recipe_sha256",recipeHash.toStdString()},{"file",ac::Json::array({{{"path","Dolphin.exe"},{"sha256",hash.toStdString()}}})}});
    ac::GameListModel model(catalog);ac::FilterSortModel filter;filter.setSourceModel(&model);ac::UiSettings settings(t.filePath("user"));ac::UiController ui(&model,&filter,&settings);
    ac::HubServices services(&model,&filter,&ui,&settings,t.path());
    QCOMPARE(model.find("synthetic")->runtime.selectedVariantId,QString("dolphinxr"));
    QCOMPARE(model.find("synthetic")->roles.value("stateLabel").toString(),QString("Setup installed"));
    ui.openDetail("synthetic");QCOMPARE(ui.detail().value("variantId").toString(),QString("dolphinxr"));
    std::optional<ac::launch::Request> launch;
    try { launch=ac::launch::gameRequest(catalog,"synthetic",t.path(),bindings); }catch(const ac::install::Error &) {}
    QVERIFY2(launch.has_value(),"Default launch did not retain the installed authored setup");
    QCOMPARE(launch->variantId,QString("dolphinxr"));
 }
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
 void retryUsesCurrentGameAndVariant(){
    QTemporaryDir temp;auto catalog=syntheticCatalog(temp.path());auto &game=catalog.games.first();
    // Catalog policy resolution must not depend on an intermediate games directory.
    QVERIFY(!QFileInfo::exists(temp.filePath("games")));QVERIFY(ac::install::loadContentGuard(temp.path()).contains("extensions"));
    ac::Variant alternate=game.variants.first();alternate.id="flat-alternate";game.variants<<alternate;game.install={{"format",1},{"variant",ac::Json::object()}};
    for(const auto &v:game.variants){const auto name=v.id.toStdString();game.install["variant"][name]={{"version","synthetic-v1"},{"installed_when","file:${install_dir}/synthetic.txt"},{"step",ac::Json::array({{{"id","generate"},{"do","write-config"},{"file","${install_dir}/synthetic.txt"},{"format","ini"},{"create",true},{"set",ac::Json::array({{{"section","Synthetic"},{"key","variant"},{"value",name}},{{"section","Synthetic"},{"key","setting"},{"from","settings.syntheticChoice"}}})}}})}};}
    ac::GameListModel games(std::move(catalog));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path());
    QVERIFY(settings.saveGame("synthetic",{{"syntheticChoice","desired-value"}}));QVERIFY(ui.status().contains("Settings saved"));ui.openDetail("synthetic");ui.selectVariant("flat-alternate");QSignalSpy changed(&ui,&ac::UiController::installChanged);ui.retryInstall(false,{});
    QTRY_VERIFY_WITH_TIMEOUT(!ui.installing()&&changed.count()>1,10000);
    const auto states=QDir(temp.filePath("user/state/installs/synthetic")).entryList(QDir::Files);QVERIFY2(!states.isEmpty(),qPrintable(ui.status()));
    QVERIFY(!QFileInfo::exists(temp.filePath("user/state/installs/tool-supermodel")));
    // No prior session install is required; the selected variant gets its own persisted state.
    bool selectedState=false;for(const auto &file:states)if(file.contains("flat-alternate"))selectedState=true;QVERIFY(selectedState);
    QVERIFY(ac::install::readBytes(temp.filePath("installed/synthetic/flat-alternate/synthetic.txt")).contains("flat-alternate"));QVERIFY(ac::install::readBytes(temp.filePath("installed/synthetic/flat-alternate/synthetic.txt")).contains("desired-value"));QVERIFY(!QFileInfo::exists(temp.filePath("installed/synthetic/flat-mame/synthetic.txt")));
 }
 void launchPreparationBusyAndStopAreDistinct(){
    QTemporaryDir temp;ac::GameListModel games(syntheticCatalog(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path());
    ac::launch::Request request;request.root=temp.path();request.gameId="synthetic";request.variantId="flat-mame";request.plan.executable=QCoreApplication::applicationFilePath();request.plan.cwd=temp.path();request.plan.args={"--synthetic-child"};
    bool prepared=false,busyMessage=false;auto connection=connect(services.launcher(),&ac::launch::LaunchService::starting,&ui,[&](const QString &,const QString &){prepared=!ac::shouldQuitAfterLastWindow(services.launchBusy())&&services.preparing()&&services.launchBusy()&&!services.playing()&&ui.status().contains("Preparing");ui.play("synthetic","flat-mame");busyMessage=ui.status().contains("Play refused");ui.stopLaunch();});
    services.launcher()->start(request);QVERIFY(prepared);QVERIFY(busyMessage);QVERIFY(!services.launchBusy());disconnect(connection);
    request.prepareProfile=false;QVERIFY(services.launcher()->start(request));QTRY_VERIFY_WITH_TIMEOUT(services.playing(),5000);QVERIFY(services.launchBusy());QVERIFY(!services.preparing());ui.stopLaunch();QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy()&&!services.playing(),6000);
 }
 void closedHubReopensOnLaunchFailure(){
    QTemporaryDir temp;ac::GameListModel games(syntheticCatalog(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path());
    QWindow window;window.show();window.hide();QVERIFY(!window.isVisible());QSignalSpy raised(&services,&ac::HubServices::raiseHubRequested);
    connect(&services,&ac::HubServices::raiseHubRequested,&window,[&]{window.show();});
    ac::launch::Request request;request.root=temp.path();request.gameId="synthetic";request.variantId="flat-mame";request.prepareProfile=false;request.plan.executable=temp.filePath("missing-synthetic-tool.exe");request.plan.cwd=temp.path();
    QVERIFY(services.launcher()->start(request));QTRY_COMPARE(raised.count(),1);QVERIFY(!services.launchBusy());QVERIFY(window.isVisible());QVERIFY(ui.status().contains("missing",Qt::CaseInsensitive));QVERIFY(!ac::shouldQuitAfterLaunch(false,services.launchBusy(),window.isVisible()));window.hide();
 }
 void uninstallSteamRemovalRequiresSeparateApproval(){
    QTemporaryDir temp;const auto steamRoot=temp.filePath("synthetic-steam");const auto target=steamRoot+"/userdata/123/config/shortcuts.vdf";QDir().mkpath(QFileInfo(target).absolutePath());
    ac::steam::Shortcut shortcut;shortcut.gameId="synthetic";shortcut.variantId="flat-mame";shortcut.title="Synthetic integration game";shortcut.executable=QCoreApplication::applicationFilePath();shortcut.startDir=temp.path();const auto before=ac::steam::edit({},shortcut).bytes;ac::install::atomicWrite(target,before);
    ac::GameListModel games(syntheticCatalog(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path(),nullptr,[&]{return steamRoot;});
    QSignalSpy steamChanges(&services,&ac::HubServices::steamPreviewChanged);ui.uninstall("synthetic","flat-mame");QVERIFY(services.canRemoveSteam());services.confirmRemoval();QCOMPARE(steamChanges.count(),0);QCOMPARE(ac::install::readBytes(target),before);QTRY_VERIFY_WITH_TIMEOUT(!ui.installing(),5000);
    ui.uninstall("synthetic","flat-mame");services.confirmRemoval(true);QCOMPARE(steamChanges.count(),1);QVERIFY(services.steamRemoving());QVERIFY(!services.steamWriteReady());QCOMPARE(ac::install::readBytes(target),before);QCOMPARE(services.steamAccounts(),QStringList{"123"});
    services.previewSteam("123");QVERIFY2(services.steamWriteReady(),qPrintable(services.steamPreview()));const auto preview=ac::Json::parse(services.steamPreview().toStdString());QCOMPARE(QString::fromStdString(preview["operation"].get<std::string>()),QString("remove"));QVERIFY(QString::fromStdString(preview["launchOptions"].get<std::string>()).contains("--variant flat-mame"));QCOMPARE(ac::install::readBytes(target),before);
    services.confirmRemoval(false);QVERIFY(!services.steamWriteReady());services.approveSteamWrite();QCOMPARE(ac::install::readBytes(target),before);QTRY_VERIFY_WITH_TIMEOUT(!ui.installing(),5000);
    QVERIFY(services.beginSteam("synthetic","flat-mame",true));services.previewSteam("123");QVERIFY(services.steamWriteReady());QVERIFY(services.steamPreview().contains("--variant flat-mame"));QCOMPARE(ac::install::readBytes(target),before);
    shortcut.variantId="retired-vr";shortcut.vr=true;const auto legacy=ac::steam::edit(before,shortcut).bytes;ac::install::atomicWrite(target,legacy);
    QVERIFY(services.beginSteam("synthetic","retired-vr",true));services.previewSteam("123");QVERIFY(services.steamWriteReady());QVERIFY(services.steamPreview().contains("--variant retired-vr"));QCOMPARE(ac::install::readBytes(target),legacy);
    QVERIFY(!services.beginSteam("synthetic","unavailable-route",false));QVERIFY(!services.steamWriteReady());QVERIFY(services.steamPreview().contains("available flat or pinned authored SteamVR variant"));QCOMPARE(ac::install::readBytes(target),legacy);
 }
 void shippedFlatHasNothingToRetryAndFailedToolIsExplicit(){
    QTemporaryDir temp;auto catalog=ac::CatalogLoader().load(AC_CATALOG_ROOT);catalog.emulators["supermodel"]["gate"]="locate-only";
    ac::GameListModel games(std::move(catalog));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path());
    ui.openDetail("timecris");for(const auto &v:games.find("timecris")->variants)if(v.generated){ui.selectVariant(v.id);break;}QSignalSpy gameRetries(&ui,&ac::UiController::retryInstallRequested);ui.retryInstall(false,{});QCOMPARE(gameRetries.count(),0);QVERIFY(ui.status().contains("Nothing to retry"));QVERIFY(!ui.installing());
    services.installSupermodel();QCOMPARE(ui.recovery().value("retryKind").toString(),QString("tool"));QCOMPARE(ui.recovery().value("retryGameId").toString(),QString("tool-supermodel"));QCOMPARE(ui.recovery().value("retryVariantId").toString(),QString("windows-x64"));
    QSignalSpy toolRetries(&ui,&ac::UiController::retryToolInstallRequested);ui.retryFailedTool(false,{});QCOMPARE(toolRetries.count(),1);QCOMPARE(toolRetries.first().at(0).toString(),QString("tool-supermodel"));QVERIFY(!ui.installing());QVERIFY(!ui.status().isEmpty());QCOMPARE(ui.recovery().value("retryGameId").toString(),QString("tool-supermodel"));QVERIFY(!QFileInfo::exists(temp.filePath("user/state/installs/timecris")));
 }
 void failedSteamPreparationKeepsPendingUninstall(){
    QTemporaryDir temp;ac::GameListModel games(syntheticCatalog(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path(),nullptr,[&]{return temp.filePath("no-steam-accounts");});
    ui.uninstall("synthetic","flat-mame");QVERIFY(services.canRemoveSteam());QSignalSpy installs(&ui,&ac::UiController::installChanged);services.confirmRemoval(true);QVERIFY(services.canRemoveSteam());QCOMPARE(installs.count(),0);QVERIFY(!ui.installing());QVERIFY(ui.status().contains("No Steam userdata shortcut folder found"));QVERIFY(!QFileInfo::exists(temp.filePath("user/state/installs/synthetic")));
    services.previewSupermodelRemoval();services.confirmRemoval(true);QCOMPARE(installs.count(),0);QVERIFY(ui.status().contains("Select the game"));
 }
 void steamApprovalPrecedesOwnedPayloadRemoval(){
    QTemporaryDir temp;auto catalog=syntheticCatalog(temp.path());ac::install::Request request;request.root=temp.path();request.catalogRoot=temp.path();request.gameId="synthetic";request.variantId="flat-mame";
    request.recipe={{"format",1},{"variant",{{"flat-mame",{{"version","synthetic-v1"},{"installed_when","file:${install_dir}/synthetic.txt"},{"step",ac::Json::array({{{"id","config"},{"do","write-config"},{"file","${install_dir}/synthetic.txt"},{"format","ini"},{"create",true},{"set",ac::Json::array({{{"section","Synthetic"},{"key","owned"},{"value",true}}})}}})}}}}}};
    QVERIFY(ac::install::Engine().install(request).success);const auto payload=temp.filePath("installed/synthetic/flat-mame/synthetic.txt");const auto payloadBytes=ac::install::readBytes(payload);
    const auto steamRoot=temp.filePath("synthetic-steam");const auto target=steamRoot+"/userdata/123/config/shortcuts.vdf";ac::install::atomicWrite(target,"malformed-vdf");bool running=true;
    ac::GameListModel games(std::move(catalog));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path(),nullptr,[&]{return steamRoot;},[&]{return running;});
    ui.uninstall("synthetic","flat-mame");QSignalSpy installs(&ui,&ac::UiController::installChanged);QVERIFY(services.confirmRemoval(true));QVERIFY(ui.status().contains("Awaiting Steam review; installation preserved"));QCOMPARE(installs.count(),0);QCOMPARE(ac::install::readBytes(payload),payloadBytes);
    services.previewSteam("123");QVERIFY(!services.steamWriteReady());services.approveSteamWrite();QCOMPARE(installs.count(),0);QCOMPARE(ac::install::readBytes(payload),payloadBytes);QVERIFY(services.canRemoveSteam());
    ac::steam::Shortcut shortcut;shortcut.gameId="synthetic";shortcut.variantId="flat-mame";shortcut.title="Synthetic integration game";shortcut.executable=QCoreApplication::applicationFilePath();shortcut.startDir=temp.path();const auto before=ac::steam::edit({},shortcut).bytes;ac::install::atomicWrite(target,before);
    services.previewSteam("123");QVERIFY(services.steamWriteReady());services.approveSteamWrite();QVERIFY(!services.steamWriteReady());QCOMPARE(installs.count(),0);QCOMPARE(ac::install::readBytes(payload),payloadBytes);QCOMPARE(ac::install::readBytes(target),before);QVERIFY(services.canRemoveSteam());
    running=false;services.previewSteam("123");QVERIFY(services.steamWriteReady());services.approveSteamWrite();QTRY_VERIFY_WITH_TIMEOUT(!ui.installing()&&!QFileInfo::exists(payload),5000);QVERIFY(installs.count()>0);QVERIFY(!ac::steam::edit(ac::install::readBytes(target),shortcut,true).ownedFound);
 }
 void savedSettingsPreserveUnknownKeysAndNonInstallErrors(){
    QTemporaryDir temp;ac::GameListModel games(syntheticCatalog(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path());
    QVERIFY(settings.saveGame("synthetic",{{"futureKey","keep"},{"futureModes",QVariantList{"a","b"}},{"laser","off"}}));QVERIFY(settings.saveGame("synthetic",{{"laser","on"}}));ac::UiSettings saved(temp.filePath("user"));QCOMPARE(saved.game("synthetic").value("futureKey").toString(),QString("keep"));QCOMPARE(saved.game("synthetic").value("futureModes").toList(),QVariantList({"a","b"}));QCOMPARE(saved.game("synthetic").value("laser").toString(),QString("on"));QVERIFY(ui.status().contains("Settings saved"));
    QVERIFY(!services.beginSteam("unknown-game",{},true));QVERIFY(ui.status().startsWith("Steam:"));QVERIFY(ui.recovery().isEmpty());ui.uninstall("synthetic","../invalid");QVERIFY(ui.status().startsWith("Hub:"));QVERIFY(ui.recovery().isEmpty());QVERIFY(!ui.installing());
 }
 void oldMameReceiptNeedsBiosRefresh(){
    QTemporaryDir temp;
    for(const auto &shape:{"missing","invalid","present"}){
        const auto root=temp.filePath(shape);const auto media=root+"/synthetic.zip";ac::install::atomicWrite(media,"synthetic");receipt(root,media);
        const auto path=root+"/user/cache/scan-bindings.json";auto saved=ac::Json::parse(ac::install::readBytes(path).toStdString());saved["bindings"][0]["proof"]="mame-header-crc";
        if(QString(shape)!="missing")saved["bindings"][0]["bios"]=QString(shape)=="present"?ac::Json(""):ac::Json(true);
        ac::install::atomicWrite(path,QByteArray::fromStdString(saved.dump()));ac::GameListModel games(syntheticCatalog(root));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(root+"/user");ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,root);
        QCOMPARE(games.find("synthetic")->runtime.mediaFound.contains("synthetic"),QString(shape)=="present");
        services.launcher()->warning("synthetic","Synthetic advisory");QVERIFY(ui.status().contains("Launch notice"));QVERIFY(ui.recovery().isEmpty());
    }
 }
 void legacyChdReceiptsRequireFreshBounds(){
    QTemporaryDir temp;
    for(const auto &shape:{"primary","support","renamed"}){
        const auto root=temp.filePath(shape);const auto primary=root+"/synthetic.zip",disk=root+(QString(shape)=="renamed"?"/synthetic-disk.dat":"/synthetic-disk.chd");ac::install::atomicWrite(primary,"synthetic archive");ac::install::atomicWrite(disk,"synthetic old truncated CHD");receipt(root,QString(shape)=="primary"?disk:primary);
        const auto path=root+"/user/cache/scan-bindings.json";auto saved=ac::Json::parse(ac::install::readBytes(path).toStdString());auto &binding=saved["bindings"][0];binding["proof"]="mame-header-crc";binding["bios"]="";
        if(QString(shape)!="primary")binding["supportPaths"]=ac::Json::array({disk.toStdString()});
        if(QString(shape)=="renamed")saved["files"]=ac::Json::array({{{"path",disk.toStdString()},{"kind","chd-sha1"}}});
        ac::install::atomicWrite(path,QByteArray::fromStdString(saved.dump()));ac::GameListModel games(syntheticCatalog(root));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(root+"/user");ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,root);
        QVERIFY(games.find("synthetic")->runtime.mediaFound.isEmpty());
    }
 }
 void startupSweepsOnlyFinishedMediaRemovals(){
    QTemporaryDir temp;auto data=syntheticCatalog(temp.path());const auto setup=temp.filePath("games/synthetic/setup/example.exe"),media=temp.filePath("synthetic.media");ac::install::atomicWrite(setup,"synthetic tool bytes");ac::install::atomicWrite(media,"synthetic copied medium");
    ac::install::Request request;request.root=temp.path();request.gameId="synthetic";request.variantId="flat";request.allowMediaCopy=true;
    request.bindings["media"]["disc"]={{"path",media.toStdString()},{"sha256",ac::install::hashFile(media).toStdString()},{"verified",true}};
    request.recipe={{"format",1},{"variant",{{"flat",{{"status","stable"},{"version","v1"},{"installed_when","file:${install_dir}/example.exe"},{"step",ac::Json::array({{{"id","copy"},{"do","copy"},{"from",setup.toStdString()},{"to","${install_dir}/example.exe"}},{{"id","media"},{"do","copy-media"},{"media","disc"},{"mode","copy"},{"to","${install_dir}/disc.media"}}})}}}}}};
    ac::install::Options options;options.survivalMs=0;QVERIFY(ac::install::Engine(options).install(request).success);options.mediaRemovalPurge=[](const QString &){return false;};QVERIFY(ac::install::Engine(options).uninstall(request).success);
    const auto removals=temp.filePath("user/state/media-removals");QVERIFY(!QDir(removals).entryList(QDir::Dirs|QDir::NoDotAndDotDot).isEmpty());
    const auto orphan=removals+"/aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee/ffffffff-aaaa-bbbb-cccc-dddddddddddd";ac::install::atomicWrite(orphan,"synthetic unrecorded file");
    ac::GameListModel games(std::move(data));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,temp.path());
    QTRY_COMPARE_WITH_TIMEOUT(QDir(removals).entryList(QDir::Dirs|QDir::NoDotAndDotDot),QStringList{"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"},5000);QCOMPARE(ac::install::readBytes(orphan),QByteArray("synthetic unrecorded file"));
 }
 void emptyCatalogToolActionsFailSafely(){
    QTemporaryDir temp;ac::CatalogData catalog;ac::GameListModel games(std::move(catalog));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);
    std::unique_ptr<ac::HubServices> services;
    try{services=std::make_unique<ac::HubServices>(&games,&filter,&ui,&settings,temp.path());}
    catch(const std::exception &error){QFAIL(qPrintable(QString("Empty-catalog service construction threw: ")+error.what()));}
    QVERIFY(services->toolPlan().contains("Catalog has no games"));services->installSupermodel();QVERIFY(ui.status().contains("Catalog has no games"));QVERIFY(!ui.installing());services->previewSupermodelRemoval();QVERIFY(ui.status().startsWith("Hub:"));QVERIFY(ui.status().contains("Catalog has no games"));QVERIFY(!QFileInfo::exists(temp.filePath("emulators")));
    QVERIFY(services->gunRoomModels().isEmpty());services->previewGunRoom("synthetic-pistol","left",temp.filePath("absent-room.exe"));
    QVERIFY(!services->gunRoomAvailable());QVERIFY(!services->gunRoomReason().isEmpty());
 }
 void changedMediaIsNotReadyOnRestore(){
    QTemporaryDir temp;const auto media=temp.filePath("fixture.zip");ac::install::atomicWrite(media,"before");receipt(temp.path(),media);ac::install::atomicWrite(media,"different size after proof");
    ac::GameListModel games(syntheticCatalog(temp.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(temp.filePath("user"));ac::UiController ui(&games,&filter,&settings);
    ac::HubServices services(&games,&filter,&ui,&settings,temp.path());QVERIFY(games.find("synthetic")->runtime.mediaFound.isEmpty());
 }
};
int main(int argc,char **argv){
    for(int i=1;i<argc;++i)if(QByteArray(argv[i])=="--synthetic-child"){QCoreApplication app(argc,argv);QTimer::singleShot(30000,&app,&QCoreApplication::quit);return app.exec();}
    if(argc==3&&QByteArray(argv[1])=="-Play"){
        QGuiApplication child(argc,argv);QWindow window;window.show();
        QTimer::singleShot(600,&child,&QCoreApplication::quit);return child.exec();
    }
    QGuiApplication app(argc,argv);IntegrationTest test;return QTest::qExec(&test,argc,argv);
}
#include "IntegrationTest.moc"
