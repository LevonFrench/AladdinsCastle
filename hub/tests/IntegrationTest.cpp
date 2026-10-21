// SPDX-License-Identifier: GPL-3.0-only
#include "core/launch/Watchdog.h"
#include "core/app/PanelAttacher.h"
#include "app/HubServices.h"
#include "core/app/LaunchOptions.h"
#include "core/catalog/CatalogLoader.h"
#include "core/install/Support.h"
#include <QTemporaryDir>
#include <QDateTime>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QScopeGuard>
#include <QtTest>
#include <optional>
#include <QtEndian>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "core/pinball/Art.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
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
// Synthetic CFB contains only an empty script terminator, no playable content.
QByteArray syntheticTable(){
    QByteArray cfb(5632,'\0');cfb.replace(0,8,QByteArray::fromHex("d0cf11e0a1b11ae1"));
    auto u16=[&](int at,quint16 n){qToLittleEndian(n,cfb.data()+at);};
    auto u32=[&](int at,quint32 n){qToLittleEndian(n,cfb.data()+at);};
    u16(26,3);u16(28,0xfffe);u16(30,9);u16(32,6);u32(44,1);u32(48,0);u32(56,4096);u32(60,0xfffffffe);u32(68,0xfffffffe);
    for(int i=0;i<109;++i)u32(76+4*i,i==0?9:0xffffffff);
    cfb[512+66]=5;u32(512+76,1);u32(512+116,0xfffffffe);
    for(int e=1;e<3;++e){const auto at=512+e*128;const QString name=e==1?"GameStg":"GameData";
        for(int i=0;i<name.size();++i){u16(at+2*i,name[i].unicode());}
        u16(at+64,static_cast<quint16>((name.size()+1)*2));
        cfb[at+66]=e==1?1:2;u32(at+68,0xffffffff);u32(at+72,0xffffffff);u32(at+76,e==1?2:0xffffffff);
        u32(at+116,e==1?0xfffffffe:1);u32(at+120,e==1?0:4096);
    }
    u32(1024,4);cfb.replace(1028,4,"ENDB");
    for(int i=0;i<128;++i)u32(5120+4*i,i==0||i==8?0xfffffffe:i==9?0xfffffffd:i<8?static_cast<quint32>(i+1):0xffffffff);
    return cfb;
}
// A tree program's process id, or 0 while its file is not written yet. Never throws: each program
// writes its file through a temporary one beside it, so the folder can list it before the id can be read.
qint64 pidIn(const QString &file){QFile f(file);return f.open(QIODevice::ReadOnly)?f.readAll().trimmed().toLongLong():0;}
}
class IntegrationTest:public QObject{
 Q_OBJECT
private slots:
 // A table whose author locked its script is listed and can start: what it needs is unknown, and said.
 void lockedPinballScriptIsANoteNotAnError(){
    QTemporaryDir t;auto cfb=syntheticTable();cfb.replace(1028,4,"SECB");
    ac::install::atomicWrite(t.filePath("Locked (Synthetic 1999).vpx"),cfb);ac::install::atomicWrite(t.filePath("Open (Synthetic 1999).vpx"),syntheticTable());
    const auto tables=ac::pinball::scan(t.path())["tables"].toList();QCOMPARE(tables.size(),2);
    const auto locked=tables[0].toMap(),open=tables[1].toMap();
    QVERIFY2(!locked.contains("error"),qPrintable(locked.value("error").toString()));QVERIFY(locked["note"].toString().contains("locked"));
    QCOMPARE(locked["romState"].toString(),QString("unknown"));QVERIFY(!locked["vrReady"].toBool());QCOMPARE(locked["year"].toInt(),1999);
    QVERIFY(!open.contains("note"));QVERIFY(!open.contains("error"));
 }
 void pinballLaunchUsesRetainedTableAndRecordsRecent(){
    QTemporaryDir t;const auto folder=t.filePath("library");QDir().mkpath(folder);
    // VPX's own settings are looked for in a folder of the test's, never the PC's.
    const auto previousAppData=qgetenv("APPDATA");qputenv("APPDATA",QDir::toNativeSeparators(t.filePath("appdata")).toUtf8());
    const auto restoreAppData=qScopeGuard([previousAppData]{qputenv("APPDATA",previousAppData);});
    const auto cfb=syntheticTable();
    const auto table=folder+"/synthetic table.vpx";ac::install::atomicWrite(table,cfb);
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("pinballExecutable",QCoreApplication::applicationFilePath());
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    services.setVrServerProbe([]{return true;});services.setGamepadProbe([](const QString &){return QVariantList{};});
    auto data=ac::pinball::scan(folder);auto row=data["tables"].toList()[0].toMap();QVERIFY(!row.contains("error"));
    ui.pinballLoaded(data);ui.selectPinball(row["id"].toString());ui.markPinball(true);
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    QVERIFY(QMetaObject::invokeMethod(&ui,"playPinball"));
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,5000);
    QCOMPARE(ui.nowPlaying()["title"].toString(),QString("synthetic table"));
    // The headset is asked for in a Hub-owned file, never beside the table.
    const auto owned=t.filePath("user/pinball/"+row["id"].toString()+".ini");
    QVERIFY(ac::install::readBytes(owned).contains("[PlayerVR]"));QVERIFY(ac::install::readBytes(owned).contains("AskToTurnOn = 0"));
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
 // Launch on desktop or in headset, each exactly that; a gamepad gets the Hub's layout in a Hub-owned copy of the user's settings.
 void pinballViewsAndGamepadReachVisualPinball(){
    QTemporaryDir t;const auto folder=t.filePath("library");QVERIFY(QDir().mkpath(folder));
    const auto table=folder+"/synthetic table.vpx";ac::install::atomicWrite(table,syntheticTable());
    // The user's own VPX settings, where VPX keeps them; they must stay byte for byte.
    const auto appData=t.filePath("appdata"),own=appData+"/VPinballX/10.8/VPinballX.ini";
    const QByteArray mine="[Player]\r\nBallTrail = 1\r\n\r\n[Input]\r\nDevices = Key\r\nMapping.LeftFlipper = Key;225\r\nMapping.Start = Key;30\r\n\r\n[PlayerVR]\r\nAskToTurnOn = 1\r\n";
    QVERIFY(QDir().mkpath(QFileInfo(own).absolutePath()));ac::install::atomicWrite(own,mine);
    const auto previousAppData=qgetenv("APPDATA");qputenv("APPDATA",QDir::toNativeSeparators(appData).toUtf8());
    const auto report=t.filePath("arguments.json");qputenv("AC_SYNTHETIC_PINBALL_ARGUMENTS",report.toUtf8());
    const auto restore=qScopeGuard([previousAppData]{qputenv("APPDATA",previousAppData);qunsetenv("AC_SYNTHETIC_PINBALL_ARGUMENTS");});
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("pinballExecutable",QCoreApplication::applicationFilePath());
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    QVariantList pads{QVariantMap{{"uid","SDLJoy_030000005e0400008e02000000007801_1"},{"name","Synthetic pad"},
        {"elements",QVariantMap{{"A",0},{"B",1},{"X",2},{"Y",3},{"LeftShoulder",4},{"RightShoulder",5},{"Back",6},{"Start",7},{"LeftStick",8},{"RightStick",9},
                                {"DPadLeft",0x100},{"DPadRight",0x101},{"DPadUp",0x102},{"DPadDown",0x103},{"LeftX",0x200},{"LeftY",0x201},{"LeftTrigger",0x202},{"RightX",0x203},{"RightY",0x204},{"RightTrigger",0x205}}}}};
    QString probed;services.setGamepadProbe([&](const QString &library){probed=library;return pads;});
    bool vr=false;services.setVrServerProbe([&]{return vr;});
    auto data=ac::pinball::scan(folder);const auto row=data["tables"].toList()[0].toMap();const auto id=row["id"].toString();
    ui.pinballLoaded(data);ui.selectPinball(id);ui.markPinball(true);
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started);
    const auto hubCopy=QDir::toNativeSeparators(t.filePath("user/pinball/VPinballX.ini")),tableFile=QDir::toNativeSeparators(t.filePath("user/pinball/"+id+".ini"));
    const auto run=[&](const QString &view){
        const int before=started.size();ui.launchPinball(id,view);
        QTRY_COMPARE_WITH_TIMEOUT(started.size(),before+1,5000);QTRY_VERIFY_WITH_TIMEOUT(!services.launcher()->busy(),5000);
    };
    // Without SteamVR, Launch in headset would start it first; a Hub with no starter (every test) starts nothing and says why.
    ui.launchPinball(id,"headset");QCOMPARE(started.size(),0);QVERIFY(ui.status().contains("could not be started"));QVERIFY(ui.status().contains("Nothing was started"));
    QVERIFY(ui.steamVrStarting().isEmpty());
    // On the desktop: VR off in the table's Hub file, the gamepad layout in the Hub's copy; SteamVR not needed.
    run("desktop");
    QCOMPARE(QJsonDocument::fromJson(ac::install::readBytes(report)).toVariant().toStringList(),(QStringList{"-Ini",hubCopy,"-Play",QFileInfo(table).absoluteFilePath(),"-TableIni",tableFile}));
    QVERIFY(ac::install::readBytes(tableFile).contains("AskToTurnOn = 2"));
    QVERIFY(probed.isEmpty()); // The stand-in has no SDL library beside it.
    const auto copy=ac::install::readBytes(hubCopy);
    for(const char *mapping:{"Mapping.LeftFlipper = Key;225 | SDLJoy_030000005e0400008e02000000007801_1;4 | SDLJoy_030000005e0400008e02000000007801_1;514;o;-0.300000",
                             "Mapping.Start = Key;30 | SDLJoy_030000005e0400008e02000000007801_1;7",
                             "Mapping.ExitGame = SDLJoy_030000005e0400008e02000000007801_1;7 & SDLJoy_030000005e0400008e02000000007801_1;6",
                             "Mapping.Plunger0.Position = SDLJoy_030000005e0400008e02000000007801_1;516;P;0.100000;1.000000;1.000000",
                             "Devices = Key;SDLJoy_030000005e0400008e02000000007801_1","[PlayerVR]\r\nAskToTurnOn = 1"})
        QVERIFY2(copy.contains(mapping),mapping);
    QCOMPARE(ac::install::readBytes(own),mine);
    QVERIFY(ui.nowPlaying().isEmpty());
    // In the headset: VR asked for; the same gamepad layout.
    vr=true;run("headset");
    QVERIFY(ac::install::readBytes(tableFile).contains("AskToTurnOn = 0"));QVERIFY(!ac::install::readBytes(tableFile).contains("AskToTurnOn = 2"));
    QCOMPARE(QJsonDocument::fromJson(ac::install::readBytes(report)).toVariant().toStringList().first(),QString("-Ini"));
    // No gamepad: VPX reads the user's own settings, so no -Ini.
    pads.clear();vr=false;run("desktop");
    QCOMPARE(QJsonDocument::fromJson(ac::install::readBytes(report)).toVariant().toStringList(),(QStringList{"-Play",QFileInfo(table).absoluteFilePath(),"-TableIni",tableFile}));
    QCOMPARE(ac::install::readBytes(own),mine);
    // Nothing is remembered: each button does its own launch every time.
    QVERIFY(!settings.values().contains("pinballViews"));
 }
 void model3LaunchUsesTheChosenViewAndLeavesFoldersAlone(){
    // The stand-in emulator is this test program: it reports what it was started with, then exits.
    QTemporaryDir t;const auto emulator=t.filePath("emu"),media=t.filePath("my games"),report=t.filePath("report.json");
    QVERIFY(QDir().mkpath(emulator+"/Config"));QVERIFY(QDir().mkpath(emulator+"/out/x64"));QVERIFY(QDir().mkpath(media));
    ac::install::atomicWrite(emulator+"/Config/Games.xml","synthetic");ac::install::atomicWrite(media+"/scud.zip","synthetic");
    const auto program=emulator+"/out/x64/program.exe";QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),program));
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model3Executable",program);settings.set("model3GameFolder",media);
    qputenv("AC_SYNTHETIC_MODEL3_REPORT",report.toUtf8());qputenv("ACVR_STALE","1");
    const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_MODEL3_REPORT");qunsetenv("ACVR_STALE");});
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    services.setVrServerProbe([]{return true;}); // A stand-in: SteamVR is running, so the game starts at once.
    const auto listing=[&]{QStringList names;for(const auto &folder:{emulator,media}){QDirIterator it(folder,QDir::AllEntries|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext())names<<it.next();}names.sort();return names;};
    const auto before=listing();
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    ui.chooseModel3View("scud","inside");ui.playModel3("scud");
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,5000);
    QCOMPARE(ui.nowPlaying()["title"].toString(),QString("Scud Race ")+QChar(0xb7)+" Inside the game");
    QVERIFY(ui.nowPlaying()["running"].toBool());QVERIFY(services.launchBusy());
    ui.playModel3("scud");QCOMPARE(started.size(),1);QVERIFY(ui.status().contains("current session"));
    QTRY_VERIFY_WITH_TIMEOUT(!services.launcher()->busy(),10000);
    QCOMPARE(finished.size(),1);QVERIFY2(finished[0][2].toString().isEmpty(),qPrintable(finished[0][2].toString()));QVERIFY(ui.nowPlaying().isEmpty());
    const auto seen=QJsonDocument::fromJson(ac::install::readBytes(report)).object();
    QCOMPARE(QFileInfo(seen["cwd"].toString()).canonicalFilePath(),QFileInfo(emulator).canonicalFilePath());
    QCOMPARE(seen["args"].toVariant().toStringList(),(QStringList{QDir::toNativeSeparators(media+"/scud.zip"),"-window","-res=1344,1344","-stretch","-ss=2","-input-system=xinput"}));
    auto env=seen["env"].toObject().toVariantMap();
    // A headset launch: SteamVR's own OpenXR runtime, when this PC has one.
    const auto steamVr=ac::launch::detectRuntime(ac::launch::systemRuntimeInputs()).steamVrManifest;
    if(!steamVr.isEmpty())QCOMPARE(env.value("XR_RUNTIME_JSON").toString(),steamVr);
    env.remove("XR_RUNTIME_JSON");
    QCOMPARE(env,(QVariantMap{{"ACVR_XR","1"},{"ACVR_VR","1"},{"ACVR_WORLD_SCALE","1"},{"ACVR_HUD_DISTANCE","1.6"},{"ACVR_HUD_WIDTH","1.6"}}));
    QCOMPARE(listing(),before);
    ui.playModel3("daytona2");QCOMPARE(started.size(),1);QVERIFY(ui.status().contains("daytona2.zip"));QVERIFY(!services.launchBusy());
    // The cabinet's wheel, asked for on the game's page: drawn inside the game, and only there.
    QCOMPARE(ui.model3Games().first().toMap().value("cabinetControl").toString(),QString("wheel"));
    ui.setModel3Control("scud",true);QVERIFY(ui.model3Games().first().toMap().value("showControl").toBool());
    QFile::remove(report);ui.playModel3("scud");QTRY_COMPARE_WITH_TIMEOUT(started.size(),2,5000);QTRY_VERIFY_WITH_TIMEOUT(!services.launcher()->busy(),10000);
    QCOMPARE(QJsonDocument::fromJson(ac::install::readBytes(report)).object()["env"].toObject()["ACVR_CONTROLS"].toString(),QString("1"));
    ui.chooseModel3View("scud","screen");QFile::remove(report);ui.playModel3("scud");QTRY_COMPARE_WITH_TIMEOUT(started.size(),3,5000);QTRY_VERIFY_WITH_TIMEOUT(!services.launcher()->busy(),10000);
    QVERIFY(!QJsonDocument::fromJson(ac::install::readBytes(report)).object()["env"].toObject().contains("ACVR_CONTROLS"));
 }
 void pinballTabShowsTheLastScanWithoutScanningAgain(){
    QTemporaryDir t;const auto folder=t.filePath("library");QDir().mkpath(folder);
    ac::install::atomicWrite(folder+"/synthetic table.vpx",syntheticTable());
    {
        ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
        ac::UiSettings settings(t.filePath("user"));settings.set("pinballFolder",folder);
        ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
        QSignalSpy scans(&ui,&ac::UiController::pinballScanRequested);
        ui.openPinball();QTRY_COMPARE_WITH_TIMEOUT(scans.size(),1,10000); // Nothing saved yet: the folder is scanned (after the restore finds nothing).
        QTRY_COMPARE_WITH_TIMEOUT(ui.pinballData()["tables"].toList().size(),1,10000);
        QVERIFY(QFileInfo::exists(t.filePath("user/cache/pinball-scan.json")));
    }
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    QSignalSpy scans(&ui,&ac::UiController::pinballScanRequested);
    // The saved scan is read off the UI thread, so it arrives a moment later, still without a new scan.
    ui.openPinball();QTRY_COMPARE_WITH_TIMEOUT(ui.pinballData()["tables"].toList().size(),1,10000);QCOMPARE(scans.size(),0);QVERIFY(ui.status().contains("last scan"));
    // Another folder is never answered from the saved scan.
    QDir().mkpath(t.filePath("other"));settings.set("pinballFolder",t.filePath("other"));
    ui.pinballLoaded({});ui.openPinball();QTRY_COMPARE_WITH_TIMEOUT(scans.size(),1,10000);
    QTRY_VERIFY_WITH_TIMEOUT(!ui.pinballScanning(),10000);
    QCOMPARE(QDir(folder).entryList(QDir::Files),QStringList{"synthetic table.vpx"});
 }
 void setupRemembersChoicesAndAddsToSteamVrOnlyWhenAsked(){
    QTemporaryDir t;const auto steam=t.filePath("steam"),list=steam+"/config/appconfig.json";
    for(const auto &name:{"roms/model3","roms/model2","steam/config"})QVERIFY(QDir(t.path()).mkpath(name));
    ac::install::atomicWrite(list,QJsonDocument(QJsonObject{{"manifest_paths",QJsonArray{}}}).toJson());
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));ac::UiController ui(&games,&filter,&settings);
    ac::HubServices services(&games,&filter,&ui,&settings,t.path(),nullptr,[&]{return steam;});
    const auto system=[&](const QString &id){for(const auto &value:ui.setupSystems())if(value.toMap()["id"]==id)return value.toMap();return QVariantMap{};};
    // Nothing is ticked on a fresh start.
    QCOMPARE(ui.setupKinds(),(QVariantMap{{"arcade",false},{"pinball",false},{"consoles",false}}));QVERIFY(!system("model3")["chosen"].toBool());
    ui.setSetupKind("arcade",true);QVERIFY(system("model3")["chosen"].toBool());QVERIFY(system("model2")["chosen"].toBool());QVERIFY(!system("pinball")["chosen"].toBool());
    ui.setSetupSystem("system22",false);QVERIFY(!system("system22")["chosen"].toBool());
    // One folder chosen: the neighbour is found and used unless skipped.
    ui.setSetupFolder("model3",t.filePath("roms/model3"));
    QCOMPARE(system("model2")["guess"].toString(),QDir(t.filePath("roms/model2")).absolutePath());QVERIFY(system("model2")["folder"].toString().isEmpty());
    ui.acceptSetupGuesses();QCOMPARE(settings.get("model2GameFolder").toString(),QDir(t.filePath("roms/model2")).absolutePath());
    ui.finishSetup();ac::UiSettings saved(t.filePath("user"));
    QVERIFY(saved.get("setupDone").toBool());QCOMPARE(saved.get("model3GameFolder").toString(),t.filePath("roms/model3"));
    QCOMPARE(QDir(t.filePath("roms/model3")).entryList(QDir::AllEntries|QDir::NoDotAndDotDot),QStringList{});
    // SteamVR: read from its list; the stand-in registrar is called only on request and only with this Hub's manifest.
    QCOMPARE(ui.steamVr()["state"].toString(),QString("absent"));
    QStringList asked;bool refuse=true;
    connect(&services,&ac::HubServices::steamVrRegistrationRequested,this,[&](const QString &manifest){
        asked<<manifest;
        if(refuse){services.steamVrAnswered("SteamVR is not running.");return;}
        ac::install::atomicWrite(list,QJsonDocument(QJsonObject{{"manifest_paths",QJsonArray{manifest}}}).toJson());services.steamVrAnswered(QString());
    });
    QVERIFY(asked.isEmpty());ui.addToSteamVr();
    QCOMPARE(asked,QStringList{services.steamVrManifest()});QCOMPARE(ui.steamVr()["state"].toString(),QString("absent"));QVERIFY(ui.steamVr()["error"].toString().contains("not running"));
    refuse=false;ui.addToSteamVr();QCOMPARE(asked.size(),2);
    QCOMPARE(ui.steamVr()["state"].toString(),QString("added"));QVERIFY(ui.steamVr()["error"].toString().isEmpty());
 }
 void model2StartsTwoProgramsAndEndsBoth(){
    // Both stand-ins are this test program: the emulator waits, the viewer reports what it was given and leaves.
    QTemporaryDir t;const auto build=t.filePath("build"),sets=t.filePath("my sets"),report=t.filePath("viewer.json");
    for(const auto &folder:{"build/mame","build/presenter","build/runtime","my sets","libraries"})QVERIFY(QDir(t.path()).mkpath(folder));
    const auto emulator=build+"/mame/emulator.exe",viewer=build+"/presenter/model2-viewer.exe";
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),emulator));QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),viewer));
    ac::install::atomicWrite(sets+"/vcop2.zip","synthetic");
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model2Executable",emulator);settings.set("model2GameFolder",sets);settings.set("model2RuntimePath",t.filePath("libraries"));
    qputenv("AC_SYNTHETIC_MODEL2_REPORT",report.toUtf8());const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_MODEL2_REPORT");});
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    services.setVrServerProbe([]{return true;}); // A stand-in: SteamVR is running, so the game starts at once.
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    ui.playOtherGame("model2-vf2");QCOMPARE(started.size(),0); // Its game file is not there: nothing starts.
    ui.playOtherGame("model2-vcop2");
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,8000);
    QCOMPARE(ui.nowPlaying()["title"].toString(),QString("Virtua Cop 2 ")+QChar(0xb7)+" 3D screen");
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000);QVERIFY2(finished[0][2].toString().isEmpty(),qPrintable(finished[0][2].toString()));
    const auto seen=QJsonDocument::fromJson(ac::install::readBytes(report)).object();
    // The viewer was handed a live process id: the emulator's, which was still running then.
    QVERIFY(seen["companion"].toInt()>0);QVERIFY(seen["companionAlive"].toBool());
    QCOMPARE(QFileInfo(seen["cwd"].toString()).canonicalFilePath(),QFileInfo(build+"/presenter").canonicalFilePath());
    QCOMPARE(seen["env"].toObject()["ACVR_M2_STEREO"].toString(),QString("1"));QCOMPARE(seen["env"].toObject()["ACVR_M2_UNITS_PER_M"].toString(),QString("0.949387005"));
    QVERIFY(seen["path"].toString().startsWith(QDir::toNativeSeparators(t.filePath("libraries"))));
    // When the viewer has gone the emulator is ended too, and only then is the Hub free again.
    QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),12000);QVERIFY(ui.nowPlaying().isEmpty());
    QCOMPARE(QDir(sets).entryList(QDir::Files,QDir::Name),QStringList{"vcop2.zip"});
    QVERIFY(QDir(build+"/runtime").isEmpty());QVERIFY(!QFileInfo::exists(build+"/captures"));
 }
 // Get artwork in the Hub: with the setting off nothing reaches the network; on, the game is asked for.
 void artworkAsksNothingWhileOff(){
    struct Counting final:ac::art::Fetcher{QList<QUrl> urls;void get(const QNetworkRequest &request,std::function<void(int,QByteArray)> done)override{urls<<request.url();QTimer::singleShot(0,[done]{done(404,{});});}};
    QTemporaryDir t;QVERIFY(QDir(t.path()).mkpath("my sets"));
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model2GameFolder",t.filePath("my sets"));
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    auto network=std::make_shared<Counting>();services.artDownloader()->setFetcher(network);services.artDownloader()->setInterval(0);
    ui.getArtwork("system:model2");ui.getArtwork("model2-vcop2");QTest::qWait(300);
    QVERIFY(network->urls.isEmpty());QVERIFY(!ui.artworkBusy());QVERIFY(ui.artworkStatus().contains("Settings"));
    settings.set("artDownload",true);
    ui.getArtwork("model2-vcop2");QVERIFY(ui.artworkBusy());
    QTRY_VERIFY_WITH_TIMEOUT(!ui.artworkBusy(),5000);
    // libretro's three MAME folders were asked for; no key, so not SteamGridDB.
    QCOMPARE(network->urls.size(),3);for(const auto &url:network->urls)QCOMPARE(url.host(),QString("thumbnails.libretro.com"));
    QVERIFY(ui.artworkStatus().contains("failed"));
    settings.set("artDownload",false);network->urls.clear();ui.getArtwork("system:model2");QTest::qWait(200);QVERIFY(network->urls.isEmpty());
 }
 // Play in headset from the window with SteamVR off: SteamVR is started first (a stand-in here; the real one
 // never), the Hub says so and waits, then launches; and the panel joins SteamVR once it runs. Cancel and a
 // failed or slow start launch nothing.
 void playInHeadsetStartsSteamVrFirst(){
    QTemporaryDir t;const auto build=t.filePath("build"),sets=t.filePath("my sets");
    for(const auto &folder:{"build/mame","build/presenter","build/runtime","my sets"})QVERIFY(QDir(t.path()).mkpath(folder));
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),build+"/mame/emulator.exe"));QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),build+"/presenter/model2-viewer.exe"));
    ac::install::atomicWrite(sets+"/vcop2.zip","synthetic");
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model2Executable",build+"/mame/emulator.exe");settings.set("model2GameFolder",sets);
    qputenv("AC_SYNTHETIC_MODEL2_REPORT",t.filePath("viewer.json").toUtf8());const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_MODEL2_REPORT");});
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    bool vr=false;int starts=0;services.setVrServerProbe([&]{return vr;});services.setSteamVrWait(5000,150);
    services.setSteamVrStarter([&](QString *){++starts;QTimer::singleShot(250,&ui,[&]{vr=true;});return true;});
    // The panel, wired as the desktop Hub wires it, with a stand-in for joining SteamVR's dashboard.
    int attaches=0;ac::PanelAttacher panel([&](QString *){++attaches;return true;},[]{});
    connect(&ui,&ac::UiController::steamVrRunningChanged,&panel,[&]{panel.setSteamVrRunning(ui.steamVrRunning());});
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    QTest::qWait(50);QVERIFY(!ui.steamVrRunning());QCOMPARE(attaches,0);
    ui.playGame("model2-vcop2","screen");
    QCOMPARE(starts,1);QCOMPARE(ui.steamVrStarting(),QString("Virtua Cop 2"));QCOMPARE(started.size(),0);
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,8000);
    QVERIFY(ui.steamVrStarting().isEmpty());QVERIFY(ui.steamVrRunning());QCOMPARE(attaches,1);QVERIFY(panel.attached());
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000);QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),10000);
    // SteamVR running already: straight to the game, nothing started.
    ui.playGame("model2-vcop2","screen");QCOMPARE(starts,1);QVERIFY(ui.steamVrStarting().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),2,8000);QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000);QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),10000);
    // SteamVR closed. Cancel while it starts: nothing is launched.
    vr=false;ui.setSteamVrRunning(false);panel.setSteamVrRunning(false);
    services.setSteamVrStarter([&](QString *){++starts;return true;});
    ui.playGame("model2-vcop2","screen");QCOMPARE(starts,2);QVERIFY(!ui.steamVrStarting().isEmpty());
    ui.cancelSteamVrStart();QVERIFY(ui.steamVrStarting().isEmpty());QVERIFY(ui.status().contains("Cancelled"));
    QTest::qWait(400);QCOMPARE(started.size(),2);
    // SteamVR cannot be started: said plainly, nothing is launched.
    services.setSteamVrStarter([&](QString *error){++starts;*error="Synthetic: no SteamVR here.";return false;});
    ui.playGame("model2-vcop2","screen");QVERIFY(ui.steamVrStarting().isEmpty());
    QVERIFY(ui.status().contains("could not be started"));QVERIFY(ui.status().contains("Nothing was started"));
    // SteamVR too slow: given up after the wait, nothing is launched.
    services.setSteamVrStarter([&](QString *){++starts;return true;});services.setSteamVrWait(400,0);
    ui.playGame("model2-vcop2","screen");QVERIFY(!ui.steamVrStarting().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(ui.steamVrStarting().isEmpty(),3000);QVERIFY(ui.status().contains("did not start within"));
    QTest::qWait(200);QCOMPARE(started.size(),2);QCOMPARE(starts,4);
 }
 // The watchdog with real programs. Programs of a set-up system that run outside any game are listed
 // and ended; the same program in a folder the settings do not point at is never touched.
 void watchdogEndsBackgroundProgramsAndLeavesOthersAlone(){
#ifndef Q_OS_WIN
    QSKIP("The watchdog lists and ends programs on Windows only; elsewhere it sees none.");
#else
    QTemporaryDir t;QVERIFY(QDir(t.path()).mkpath("emu"));QVERIFY(QDir(t.path()).mkpath("elsewhere"));
    const auto program=t.filePath("emu/TimeCrisisVR.exe"),other=t.filePath("elsewhere/TimeCrisisVR.exe");
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),program));QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),other));
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("system22Executable",program);
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());services.watchdog()->setTimings(200,300,0,800);
    qint64 stray=0,unrelated=0;
    QVERIFY(QProcess::startDetached(program,{"--synthetic-child"},{},&stray));QVERIFY(QProcess::startDetached(other,{"--synthetic-child"},{},&unrelated));
    const auto cleanup=qScopeGuard([&]{ac::launch::endProcesses({stray,unrelated});});
    const auto listed=[&](qint64 pid){for(const auto &value:ui.backgroundPrograms())if(value.toMap().value("pid").toLongLong()==pid)return true;return false;};
    QTRY_VERIFY_WITH_TIMEOUT(listed(stray),5000);QVERIFY(!listed(unrelated));
    QVERIFY(ui.backgroundPrograms().first().toMap().value("label").toString().contains("TimeCrisisVR.exe"));
    ui.endBackgroundProgram(stray);
    QTRY_VERIFY_WITH_TIMEOUT(!ac::launch::processAlive(stray),5000);QTRY_VERIFY_WITH_TIMEOUT(ui.backgroundPrograms().isEmpty(),3000);
    QTest::qWait(500);QVERIFY(ac::launch::processAlive(unrelated)); // Same name, another folder: left alone.
    ui.endAllBackgroundPrograms();QTest::qWait(1200);QVERIFY(ac::launch::processAlive(unrelated));
#endif
 }
 // A game the Hub started: a crash is said in words and recorded; a window that never shows is
 // reported; Windows Error Reporting opened for it is closed with it; a grandchild left behind ends.
 void watchdogCatchesCrashesHiddenGamesAndLeftovers(){
#ifndef Q_OS_WIN
    QSKIP("Windows exit codes, windows, Error Reporting and job objects.");
#else
    QTemporaryDir t;QVERIFY(QDir(t.path()).mkpath("emu"));QVERIFY(QDir(t.path()).mkpath("wer"));
    const auto program=t.filePath("emu/TimeCrisisVR.exe"),report=t.filePath("wer/WerFault.exe");
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),program));QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),report));
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("system22Executable",program);
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    services.setVrServerProbe([]{return true;});services.watchdog()->setTimings(200,400,500,800);
    QSignalSpy finished(services.launcher(),&ac::launch::LaunchService::finished);
    const auto pids=t.filePath("pids.txt");qputenv("AC_SYNTHETIC_PIDS",pids.toUtf8());qputenv("AC_SYNTHETIC_WERFAULT",report.toUtf8());
    const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_GAME");qunsetenv("AC_SYNTHETIC_PIDS");qunsetenv("AC_SYNTHETIC_WERFAULT");});
    const auto play=[&](const QByteArray &mode){qputenv("AC_SYNTHETIC_GAME",mode);const int before=finished.size();ui.playGame("system22-timecris","vr");qunsetenv("AC_SYNTHETIC_GAME");return before;};
    // A crash: the exit code in words, on the banner and in the crash records.
    int before=play("crash");
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),before+1,10000);QCOMPARE(quint32(finished.last()[1].toInt()),0xC0000005u);
    QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),10000);
    QVERIFY2(ui.lastSession().value("text").toString().contains("crashed: an access violation"),qPrintable(ui.lastSession().value("text").toString()));
    QCOMPARE(ui.lastSession().value("outcome").toString(),QString("crashed"));
    QCOMPARE(ui.crashRecords().size(),1);QCOMPARE(ui.crashRecords().first().toMap().value("exitCode").toString(),QString("0xC0000005"));
    QVERIFY(QFileInfo::exists(t.filePath("user/logs/crashes.json")));
    // No window on the monitor: said after a moment, with Bring to front and End.
    before=play("hidden");
    QTRY_COMPARE_WITH_TIMEOUT(ui.gameTrouble(),QString("hidden"),8000);
    ui.stopLaunch();QTRY_COMPARE_WITH_TIMEOUT(finished.size(),before+1,15000);QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),10000);
    QVERIFY(ui.gameTrouble().isEmpty());QCOMPARE(ui.crashRecords().size(),1); // A stop is not a crash.
    // Windows Error Reporting (a stand-in of the same name) opened by the game: both are ended, and it counts as a crash.
    before=play("werfault");
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),before+1,15000);QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),10000);
    QVERIFY2(ui.lastSession().value("text").toString().contains("Windows Error Reporting"),qPrintable(ui.lastSession().value("text").toString()));
    QCOMPARE(ui.crashRecords().size(),2);
    QTRY_VERIFY_WITH_TIMEOUT([&]{for(const auto &p:ac::launch::listProcesses())if(QDir::fromNativeSeparators(p.path).compare(report,Qt::CaseInsensitive)==0)return false;return true;}(),5000);
    // A grandchild left behind when the game closes: ended with its game, nothing left in the background.
    before=play("grandchild");
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),before+1,10000);QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),10000);
    qint64 grandchild=0;{QFile file(pids);QVERIFY(file.open(QIODevice::ReadOnly));for(const auto &line:file.readAll().split('\n'))if(line.startsWith("grandchild "))grandchild=line.mid(11).toLongLong();}
    QVERIFY(grandchild>0);QTRY_VERIFY_WITH_TIMEOUT(!ac::launch::processAlive(grandchild),10000);
    QTRY_VERIFY_WITH_TIMEOUT(ui.backgroundPrograms().isEmpty(),5000);QCOMPARE(ui.crashRecords().size(),2);
#endif
 }
 // Get artwork for one game the moment the last one ends, as the Hub's own diagnostic chains them.
 void artworkRequestsCanFollowEachOther(){
    struct Pictures final:ac::art::Fetcher{QList<QUrl> urls;void get(const QNetworkRequest &request,std::function<void(int,QByteArray)> done)override{
        urls<<request.url();QByteArray body;
        if(request.url().toString().endsWith("/"))body="<a href=\"Virtua%20Cop%202.png\">x</a><a href=\"Virtua%20Fighter%202.png\">x</a>";
        else{QImage image(40,30,QImage::Format_RGB32);image.fill(Qt::red);QBuffer buffer(&body);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");}
        QTimer::singleShot(0,[done,body]{done(200,body);});}};
    QTemporaryDir t;QVERIFY(QDir(t.path()).mkpath("my sets"));
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model2GameFolder",t.filePath("my sets"));settings.set("artDownload",true);
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    auto network=std::make_shared<Pictures>();services.artDownloader()->setFetcher(network);services.artDownloader()->setInterval(0);
    QStringList queue{"model2-vf2"},said;
    connect(&ui,&ac::UiController::artworkChanged,this,[&]{if(ui.artworkBusy())return;said<<ui.artworkStatus();if(!queue.isEmpty())ui.getArtwork(queue.takeFirst());});
    ui.getArtwork("model2-vcop2");
    QTRY_COMPARE_WITH_TIMEOUT(said.size(),2,10000);
    QVERIFY2(said.last().contains("new picture"),qPrintable(said.join(" | ")));
    const auto folder=ac::art::Downloader::downloadFolder(t.filePath("user"));
    QVERIFY(QFileInfo::exists(folder+"/model2-vcop2/snap.png"));QVERIFY(QFileInfo::exists(folder+"/model2-vf2/snap.png"));
 }
 // A game's process tree ends with whoever holds it, even if that is never told to end it.
 void aGameNeverOutlivesItsTree(){
#ifndef Q_OS_WIN
    QSKIP("Process trees are held in Windows job objects.");
#else
    qint64 pid=0;QVERIFY(QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--synthetic-child"},{},&pid));
    const auto cleanup=qScopeGuard([pid]{ac::launch::endProcesses({pid});});
    {ac::launch::ProcessTree tree;tree.adopt(pid);QVERIFY(tree.members().contains(pid));QVERIFY(ac::launch::processAlive(pid));}
    QTRY_VERIFY_WITH_TIMEOUT(!ac::launch::processAlive(pid),5000);
#endif
 }
 // Stop on a PS2 game that ignores its stop file: asked first, then ended with its whole tree.
 void ps2StopIsForcedWhenTheGameDoesNotQuit(){
    QTemporaryDir t;const auto play=t.filePath("play"),launcher=play+"/launch_ps2.py",python=t.filePath("python/pythonw.exe"),stop=t.filePath("runtime/quit.request");
    QVERIFY(QDir().mkpath(play));QVERIFY(QDir().mkpath(t.filePath("python")));QVERIFY(QDir().mkpath(t.filePath("runtime")));
    ac::install::atomicWrite(launcher,"synthetic");ac::install::atomicWrite(play+"/games.json","{\"virtua-cop\":{}}");
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),python));
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("ps2Launcher",launcher);settings.set("ps2Python",python);settings.set("ps2StopFile",stop);
    qputenv("AC_SYNTHETIC_PS2_STOP",stop.toUtf8());qputenv("AC_SYNTHETIC_PS2_IGNORE","1");
    const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_PS2_STOP");qunsetenv("AC_SYNTHETIC_PS2_IGNORE");});
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    services.setVrServerProbe([]{return true;});services.setStopGrace(1500);
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    // Inside the game is off until confirmed: one view, the default, and an Inside choice is not kept.
    const auto row=[&ui]{for(const auto &value:ui.otherGames())if(value.toMap()["id"]=="ps2-virtua-cop")return value.toMap();return QVariantMap{};};
    // No Inside view without an inside profile; Virtua Cop also has the launcher's headset check (2026-10-20).
    QCOMPARE(row()["views"].toList().size(),2);QCOMPARE(row()["views"].toList()[1].toMap()["id"].toString(),QString("check"));
    QCOMPARE(row()["view"].toString(),QString("screen"));QVERIFY(!row()["viewChosen"].toBool());
    ui.chooseGameView("ps2-virtua-cop","inside");QCOMPARE(row()["view"].toString(),QString("screen"));QVERIFY(!row()["viewChosen"].toBool());
    ui.chooseGameView("ps2-virtua-cop","screen");QVERIFY(row()["viewChosen"].toBool());
    ui.playOtherGame("ps2-virtua-cop");QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,8000);
    const auto pid=services.launcher()->processId();QVERIFY(pid>0);
    QTest::qWait(300);ui.stopLaunch();
    QVERIFY(QFileInfo::exists(stop)); // The graceful request comes first.
    QTest::qWait(800);QVERIFY(services.launchBusy()); // Not yet forced.
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,8000);QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),8000);
    QVERIFY(!ac::launch::processAlive(pid));
 }
 // One SteamVR policy: only Play in headset starts SteamVR, and the kill switch stops even that; the
 // launch service only ever checks it; headset launches get SteamVR's OpenXR runtime.
 void oneSteamVrPolicy(){
    QTemporaryDir t;const auto build=t.filePath("build"),sets=t.filePath("my sets");
    for(const auto &folder:{"build/mame","build/presenter","build/runtime","my sets"})QVERIFY(QDir(t.path()).mkpath(folder));
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),build+"/mame/emulator.exe"));QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),build+"/presenter/model2-viewer.exe"));
    ac::install::atomicWrite(sets+"/vcop2.zip","synthetic");
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model2Executable",build+"/mame/emulator.exe");settings.set("model2GameFolder",sets);
    qputenv("AC_SYNTHETIC_MODEL2_REPORT",t.filePath("viewer.json").toUtf8());const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_MODEL2_REPORT");qunsetenv("AC_NO_STEAMVR_START");});
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    bool vr=false;int starts=0;services.setVrServerProbe([&]{return vr;});services.setSteamVrStarter([&](QString *){++starts;return true;});
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    // The kill switch, in Settings: nothing is started, and it says why.
    settings.set("startSteamVr",false);
    ui.playGame("model2-vcop2","screen");QCOMPARE(starts,0);QVERIFY(ui.steamVrStarting().isEmpty());QVERIFY(ui.status().contains("turned off in Settings"));
    // The same through AC_NO_STEAMVR_START, with the setting back on.
    settings.set("startSteamVr",true);qputenv("AC_NO_STEAMVR_START","1");
    ui.playGame("model2-vcop2","screen");QCOMPARE(starts,0);QVERIFY(ui.status().contains("turned off"));
    qunsetenv("AC_NO_STEAMVR_START");QTest::qWait(200);QCOMPARE(started.size(),0);
    // The launch service never starts SteamVR itself: a SteamVR game with SteamVR off is refused, said plainly.
    ac::launch::Request request;request.root=t.path();request.gameId="synthetic-steamvr";request.variantId="vr";request.runtime="steamvr";request.prepareProfile=false;
    request.plan.executable=build+"/presenter/model2-viewer.exe";request.plan.cwd=build+"/presenter";request.plan.args={"1"};
    QTRY_VERIFY2_WITH_TIMEOUT(!services.launcher()->busy(),qPrintable(QString("busy: playing %1").arg(services.launcher()->playing())),5000);
    QVERIFY(services.launcher()->start(request));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,5000);QCOMPARE(started.size(),0);
    const auto refusal=finished.last()[2].toString();QVERIFY2(refusal.contains("not running")||refusal.contains("not found"),qPrintable(refusal));
    QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),5000);
    // With SteamVR running, a headset launch is given SteamVR's own OpenXR runtime when this PC has one.
    vr=true;ui.playGame("model2-vcop2","screen");QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,8000);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(t.filePath("viewer.json")),5000);QTest::qWait(200);
    const auto given=QJsonDocument::fromJson(ac::install::readBytes(t.filePath("viewer.json"))).object()["env"].toObject()["XR_RUNTIME_JSON"].toString();
    const auto steamVr=ac::launch::detectRuntime(ac::launch::systemRuntimeInputs()).steamVrManifest;
    if(steamVr.isEmpty())QVERIFY(given.isEmpty()||given==qEnvironmentVariable("XR_RUNTIME_JSON"));else QCOMPARE(given,steamVr);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,15000);QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),10000);QCOMPARE(starts,0);
 }
 // Exit and Stop ask Model 2 to quit through its stop file, so MAME saves; nothing is forced.
 void model2QuitsThroughItsStopFile(){
    QTemporaryDir t;const auto build=t.filePath("build"),sets=t.filePath("my sets");
    for(const auto &folder:{"build/mame","build/presenter","build/runtime","my sets"})QVERIFY(QDir(t.path()).mkpath(folder));
    const auto emulator=build+"/mame/emulator.exe",viewer=build+"/presenter/model2-viewer.exe";
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),emulator));QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),viewer));
    ac::install::atomicWrite(sets+"/vcop2.zip","synthetic");
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model2Executable",emulator);settings.set("model2GameFolder",sets);
    qputenv("AC_SYNTHETIC_MODEL2_REPORT",t.filePath("viewer.json").toUtf8());qputenv("AC_SYNTHETIC_MODEL2_WAIT_STOP","1");
    const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_MODEL2_REPORT");qunsetenv("AC_SYNTHETIC_MODEL2_WAIT_STOP");});
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    services.setVrServerProbe([]{return true;}); // A stand-in: SteamVR is running, so the game starts at once.
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    QStringList used;
    QSignalSpy exitReady(&services,&ac::HubServices::hubExitReady);
    // Closing the Hub comes last: once asked, the Hub starts nothing more.
    for(const QString way:{"exit","stop","hub"}){
        const int before=finished.size();ui.playOtherGame("model2-vcop2");QTRY_COMPARE_WITH_TIMEOUT(started.size(),before+1,8000);
        // Each launch has a stop file of its own, which does not exist until it is asked to quit.
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(t.filePath("viewer.json")),5000);QTest::qWait(200);
        const auto stop=QDir::fromNativeSeparators(QJsonDocument::fromJson(ac::install::readBytes(t.filePath("viewer.json"))).object()["env"].toObject()["ACVR_M2_STOP_FILE"].toString());
        QVERIFY2(stop.startsWith(QDir::fromNativeSeparators(t.filePath("user/runtime/model2-vcop2-"))),qPrintable(stop));QVERIFY(!used.contains(stop));used<<stop;
        QVERIFY(!QFileInfo::exists(stop));
        QVERIFY(ui.nowPlaying()["closeUnavailable"].toString().isEmpty()); // Exit and Restart are offered again.
        QTest::qWait(300);if(way=="exit")ui.closeLaunch();else if(way=="stop")ui.stopLaunch();else services.requestHubExit(3);
        QVERIFY(QFileInfo::exists(stop));
        // The viewer ends by itself, normally: the request was the stop file, not a kill.
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),before+1,15000);QCOMPARE(finished.last()[1].toInt(),0);QVERIFY2(finished.last()[2].toString().isEmpty(),qPrintable(finished.last()[2].toString()));
        QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),10000);
        QVERIFY(!QFileInfo::exists(stop)); // Cleared once its game has ended.
        QFile::remove(t.filePath("viewer.json"));
    }
    QTRY_COMPARE_WITH_TIMEOUT(exitReady.size(),1,5000);QCOMPARE(exitReady.first()[0].toInt(),3);
 }
 // Hub exit while a game runs: the exit waits for the game to finish by itself, however it was asked for.
 void hubExitWaitsForARunningGameToFinish_data(){QTest::addColumn<int>("code");QTest::newRow("normal exit")<<0;QTest::newRow("overlay failure")<<3;}
 void hubExitWaitsForARunningGameToFinish(){
    QFETCH(int,code);
    QTemporaryDir t;const auto emulator=t.filePath("emu"),media=t.filePath("games"),marker=t.filePath("saved.marker");
    QVERIFY(QDir().mkpath(emulator+"/Config"));QVERIFY(QDir().mkpath(media));
    ac::install::atomicWrite(emulator+"/Config/Games.xml","synthetic");ac::install::atomicWrite(media+"/scud.zip","synthetic");
    const auto program=emulator+"/program.exe";QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),program));
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model3Executable",program);settings.set("model3GameFolder",media);
    // The stand-in game needs 1.8 s before it has written its completion marker and ended.
    qputenv("AC_SYNTHETIC_MODEL3_REPORT",t.filePath("report.json").toUtf8());qputenv("AC_SYNTHETIC_CHILD_MS","1800");qputenv("AC_SYNTHETIC_MARKER",marker.toUtf8());
    const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_MODEL3_REPORT");qunsetenv("AC_SYNTHETIC_CHILD_MS");qunsetenv("AC_SYNTHETIC_MARKER");});
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    services.setVrServerProbe([]{return true;}); // A stand-in: SteamVR is running, so the game starts at once.
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started);
    QList<bool> markerAtExit;QList<int> codes;QElapsedTimer clock;
    connect(&services,&ac::HubServices::hubExitReady,this,[&](int value){markerAtExit<<QFileInfo::exists(marker);codes<<value;});
    ui.playModel3("scud");QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,5000);
    clock.start();services.requestHubExit(code);
    QVERIFY(codes.isEmpty());QVERIFY(services.launchBusy());QVERIFY(!QFileInfo::exists(marker));
    // No new game while the exit waits.
    ui.playModel3("scud");QCOMPARE(started.size(),1);
    QTRY_COMPARE_WITH_TIMEOUT(codes.size(),1,15000);
    QCOMPARE(codes,QList<int>{code});QCOMPARE(markerAtExit,QList<bool>{true});QVERIFY(clock.elapsed()>1000);
    QVERIFY(!services.launchBusy());QCOMPARE(ac::install::readBytes(marker),QByteArray("complete"));
    QTest::qWait(200);QCOMPARE(codes.size(),1); // Exactly once.
 }
 // Accepted in the headset is the owner's word only: an empty list accepts nothing, a found game included.
 void acceptanceComesOnlyFromTheOwnersList(){
    QTemporaryDir t;const auto emulator=t.filePath("emu"),media=t.filePath("games");
    QVERIFY(QDir().mkpath(emulator+"/Config"));QVERIFY(QDir().mkpath(media));
    ac::install::atomicWrite(emulator+"/Config/Games.xml","synthetic");ac::install::atomicWrite(emulator+"/program.exe","synthetic");ac::install::atomicWrite(media+"/scud.zip","synthetic");
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model3Executable",emulator+"/program.exe");settings.set("model3GameFolder",media);
    QVERIFY(settings.createAcceptedList());const auto empty=ac::install::readBytes(settings.acceptedPath());
    ac::UiController ui(&games,&filter,&settings);
    const auto row=[&](const QString &id){for(const auto &value:ui.model3Games())if(value.toMap()["id"]==id)return value.toMap();return QVariantMap{};};
    QVERIFY(row("scud")["found"].toBool());QVERIFY(row("scud")["launchKnown"].toBool());QVERIFY(row("scud")["accepted"].toString().isEmpty());
    QVERIFY(!row("daytona2")["found"].toBool());
    QVERIFY(settings.createAcceptedList());QCOMPARE(ac::install::readBytes(settings.acceptedPath()),empty); // Never overwritten.
    ac::install::atomicWrite(settings.acceptedPath(),empty+"\"model3-scud\" = \"2026-10-14\"\n\"daytona usa 2\" = \"2026-10-15\"\n");
    QVERIFY(row("scud")["accepted"].toString().isEmpty()); // Read again only when asked.
    settings.reloadAccepted();
    QCOMPARE(row("scud")["accepted"].toString(),QString("2026-10-14"));QCOMPARE(row("daytona2")["accepted"].toString(),QString("2026-10-15"));
    QVERIFY(!row("daytona2")["found"].toBool()); // Accepted and found stay separate facts.
 }
 // PS2 has no window to close: Exit and Stop write the launcher's stop file, and the Hub waits for the game to end.
 void ps2StopsThroughItsStopFile(){
    QTemporaryDir t;const auto play=t.filePath("play"),launcher=play+"/launch_ps2.py",python=t.filePath("python/pythonw.exe"),stop=t.filePath("runtime/quit.request");
    QVERIFY(QDir().mkpath(play));QVERIFY(QDir().mkpath(t.filePath("python")));QVERIFY(QDir().mkpath(t.filePath("runtime")));
    ac::install::atomicWrite(launcher,"synthetic");ac::install::atomicWrite(play+"/games.json","{\"virtua-cop\":{}}");
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),python));
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("ps2Launcher",launcher);settings.set("ps2Python",python);settings.set("ps2StopFile",stop);
    qputenv("AC_SYNTHETIC_PS2_STOP",stop.toUtf8());const auto restore=qScopeGuard([]{qunsetenv("AC_SYNTHETIC_PS2_STOP");});
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    bool vr=false;services.setVrServerProbe([&]{return vr;});
    QSignalSpy started(services.launcher(),&ac::launch::LaunchService::started),finished(services.launcher(),&ac::launch::LaunchService::finished);
    ui.playOtherGame("ps2-virtua-cop");QCOMPARE(started.size(),0);QVERIFY(ui.status().contains("not given a way to start it"));
    vr=true;ui.playOtherGame("ps2-virtua-cop");QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,8000);
    QVERIFY(!QFileInfo::exists(stop));ui.stopLaunch();
    QVERIFY(QFileInfo::exists(stop)); // Asked to quit, not killed:
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000);QCOMPARE(finished[0][1].toInt(),0);QVERIFY(finished[0][2].toString().isEmpty());
    QCOMPARE(ui.lastSession()["outcome"].toString(),QString("stopped"));QTRY_VERIFY_WITH_TIMEOUT(!services.launchBusy(),5000);
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
 // Artwork beside the owner's ROMs is found without adding an artwork folder: a game folder's snap subfolder for
 // Model 2, and the picture beside the disc a launcher's games.json names for Dreamcast.
 void romFoldersFeedLocalArt(){
    QTemporaryDir t;const auto model2=t.filePath("model2 roms"),discs=t.filePath("dreamcast discs"),play=t.filePath("play/dreamcast");
    for(const auto &folder:{model2+"/snap",discs,play})QVERIFY(QDir().mkpath(folder));
    ac::install::atomicWrite(model2+"/snap/vcop2.png","synthetic picture name only");
    ac::install::atomicWrite(discs+"/Confidential Mission.cdi","synthetic");ac::install::atomicWrite(discs+"/Confidential Mission.png","synthetic picture name only");
    ac::install::atomicWrite(play+"/launch-dreamcast.ps1","synthetic");
    ac::install::atomicWrite(play+"/games.json",QJsonDocument(QJsonObject{{"confmiss",QJsonObject{{"title","Synthetic Mission"},{"file",QDir::toNativeSeparators(discs+"/Confidential Mission.cdi")},{"views",QJsonArray{"theatre"}}}}}).toJson());
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);
    ac::UiSettings settings(t.filePath("user"));settings.set("model2GameFolder",model2);settings.set("dreamcastLauncher",play+"/launch-dreamcast.ps1");
    ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,t.path());
    QTRY_COMPARE_WITH_TIMEOUT(ui.localArtFiles(),2,10000);
    const auto art=[&](const QString &id){for(const auto &value:ui.otherGames())if(value.toMap()["id"]==id)return value.toMap()["art"].toString();return QString();};
    QVERIFY2(art("model2-vcop2").endsWith("snap/vcop2.png"),qPrintable(art("model2-vcop2")));
    QVERIFY2(art("dreamcast-confmiss").endsWith("Confidential Mission.png"),qPrintable(art("dreamcast-confmiss")));
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
 // Every process a game started ends with it: by Stop, when the game ends by itself, and by Stop everything.
 // A table's backglass file gives its pictures, made once in the Hub's cache; the table's folder is only read.
 void pinballPicturesComeFromTheBackglassFile(){
    QTemporaryDir t;const auto folder=t.filePath("tables"),cache=t.filePath("hub/user/cache/pinball-art");QVERIFY(QDir().mkpath(folder));
    const auto table=folder+"/Synthetic Table (Maker 1990).vpx";ac::install::atomicWrite(table,syntheticTable());
    const auto image=[](const QColor &colour){QImage picture(64,48,QImage::Format_RGB32);picture.fill(colour);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);picture.save(&buffer,"PNG");return bytes.toBase64();};
    const auto backglass=folder+"/Synthetic Table (Maker 1990).DirectB2S";
    ac::install::atomicWrite(backglass,"<DirectB2SData Version=\"1.2\"><Name Value=\"Synthetic\" /><Images><ThumbnailImage Value=\""+image(Qt::blue)+"\" /><BackglassImage Value=\""+image(Qt::red)+"\" /></Images></DirectB2SData>");
    const auto listing=[&]{auto names=QDir(folder).entryList(QDir::Files);names.sort();return names;};const auto before=listing();
    QCOMPARE(ac::pinball::backglassFile(table),QFileInfo(backglass).absoluteFilePath());
    const auto id=QString::fromLatin1(QCryptographicHash::hash(QFileInfo(table).absoluteFilePath().toUtf8(),QCryptographicHash::Sha256).toHex());
    std::atomic_bool cancel=false;const auto art=ac::pinball::tableArt(table,id,cache,cancel);
    QVERIFY(QFileInfo(art["thumb"].toString()).isFile());QVERIFY(QFileInfo(art["backglass"].toString()).isFile());
    QVERIFY(art["backglass"].toString().startsWith(cache));
    // The backglass picture, not the thumbnail: red.
    const QImage made(art["backglass"].toString());QVERIFY(!made.isNull());QVERIFY(qRed(made.pixel(10,10))>200&&qBlue(made.pixel(10,10))<60);
    QCOMPARE(listing(),before); // Nothing written beside the table.
    // Made once: the same files are used again.
    const auto stamp=QFileInfo(art["thumb"].toString()).lastModified();QCOMPARE(ac::pinball::tableArt(table,id,cache,cancel),art);QCOMPARE(QFileInfo(art["thumb"].toString()).lastModified(),stamp);
    // A changed backglass file is read again and replaces the old pictures.
    QTest::qWait(1100);ac::install::atomicWrite(backglass,"<DirectB2SData><Images><BackglassImage Value=\""+image(Qt::green)+"\" /></Images></DirectB2SData>");
    const auto again=ac::pinball::tableArt(table,id,cache,cancel);QVERIFY(again["thumb"]!=art["thumb"]);QVERIFY(!QFileInfo::exists(art["thumb"].toString()));QCOMPARE(QDir(cache).entryList(QDir::Files).size(),2);
    // No backglass file, or one without a picture: no pictures, and today's look stays.
    const auto other=folder+"/Other.vpx";ac::install::atomicWrite(other,syntheticTable());QVERIFY(ac::pinball::tableArt(other,QString(64,'a'),cache,cancel).isEmpty());
    ac::install::atomicWrite(folder+"/Other.directb2s","<DirectB2SData><Images><BackglassImage Value=\"bm90IGFuIGltYWdl\" /></Images></DirectB2SData>");
    QVERIFY(ac::pinball::tableArt(other,QString(64,'a'),cache,cancel).isEmpty());
    // The controller puts the pictures on the table's row.
    ac::GameListModel games(syntheticCatalog(t.path()));ac::FilterSortModel filter;filter.setSourceModel(&games);ac::UiSettings settings(t.filePath("user"));ac::UiController ui(&games,&filter,&settings);
    ui.pinballLoaded(ac::pinball::scan(folder));ui.setPinballArt({{id,again}});ui.selectPinball(id);
    QCOMPARE(ui.pinballSelection()["artBackglass"].toString(),again["backglass"].toString());
 }
 // The race the tree test below lost on Windows CI: an id file still being written is listed in its folder
 // (its temporary file sits beside it) with no id in it yet. The Hub's reader throws for it, and that
 // exception ended the whole run inside QTest; the test's own reader waits instead.
 void aPidFileBeingWrittenIsNotYetAnId(){
    QTemporaryDir t;for(const auto &name:{"launcher","child1","child2"})ac::install::atomicWrite(t.filePath(QString(name)+".pid"),"4242");
    const auto last=t.filePath("grandchild.pid");QSaveFile pending(last);QVERIFY(pending.open(QIODevice::WriteOnly));QCOMPARE(pending.write("4343"),qint64(4));
#ifdef Q_OS_WIN
    QCOMPARE(QDir(t.path()).entryList(QDir::Files).size(),4); // Four files listed, three ids written.
#endif
    bool threw=false;try{ac::install::readBytes(last);}catch(const ac::install::Error &){threw=true;}QVERIFY(threw);
    QCOMPARE(pidIn(last),qint64(0));QCOMPARE(pidIn(t.filePath("child1.pid")),qint64(4242));
    QVERIFY(pending.commit());QCOMPARE(pidIn(last),qint64(4343));
 }
 void stopLeavesNoProcessOfTheGameRunning(){
#ifndef Q_OS_WIN
    QSKIP("Process trees are held in Windows job objects.");
#else
    for(const QString way:{"stop","leave","everything"}){
        QTemporaryDir temp;const auto folder=temp.filePath("pids");QVERIFY(QDir().mkpath(folder));
        ac::launch::LaunchService launcher;QSignalSpy finished(&launcher,&ac::launch::LaunchService::finished);
        ac::launch::Request request;request.root=temp.path();request.gameId="synthetic-tree";request.variantId="flat";request.prepareProfile=false;
        request.plan.executable=QCoreApplication::applicationFilePath();request.plan.cwd=temp.path();
        request.plan.args={"--spawn-tree",folder,"launcher",way=="leave"?"leave":"stay"};
        QVERIFY(launcher.start(request));
        const auto pids=[&]{QList<qint64> list;for(const auto &name:{"launcher","child1","child2","grandchild"})list<<pidIn(folder+"/"+name+".pid");return list;};
        // Waited for by id, not by file count: a file still being written counts in the folder but cannot be read.
        QTRY_VERIFY2_WITH_TIMEOUT(!pids().contains(0),qPrintable(way+" "+QDir(folder).entryList(QDir::Files).join(' ')),15000);
        const auto all=pids();for(const auto pid:all)QVERIFY(pid>0);
        if(way=="stop"){QVERIFY(ac::launch::processAlive(all[3]));const auto owned=launcher.ownedProcesses();for(const auto pid:all)QVERIFY(owned.contains(pid));launcher.stop();}
        else if(way=="everything")launcher.endEverything();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,15000);
        for(const auto pid:all)QTRY_VERIFY2_WITH_TIMEOUT(!ac::launch::processAlive(pid),qPrintable(way+" "+QString::number(pid)),10000);
        QTRY_VERIFY_WITH_TIMEOUT(launcher.ownedProcesses().isEmpty(),10000);
    }
#endif
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
    // Synthetic games for the watchdog, chosen by AC_SYNTHETIC_GAME (set only around one test):
    // "crash" ends with an access violation's code (no real fault, so no error dialog ever shows),
    // "hidden" runs with a window that is never shown, "werfault" starts a stand-in Windows Error
    // Reporting program and waits, "grandchild" starts a copy of itself and leaves it behind.
    if(const auto mode=qEnvironmentVariable("AC_SYNTHETIC_GAME");!mode.isEmpty()&&argc==1){
        QCoreApplication game(argc,argv);
        if(const auto pidFile=qEnvironmentVariable("AC_SYNTHETIC_PIDS");!pidFile.isEmpty()){QFile file(pidFile);if(file.open(QIODevice::Append))file.write(QByteArray::number(QCoreApplication::applicationPid())+"\n");}
#ifdef Q_OS_WIN
        if(mode=="crash")QTimer::singleShot(600,&game,[]{TerminateProcess(GetCurrentProcess(),0xC0000005);});
        if(mode=="hidden"){
            WNDCLASSW kind{};kind.lpfnWndProc=DefWindowProcW;kind.hInstance=GetModuleHandleW(nullptr);kind.lpszClassName=L"AcSyntheticGame";RegisterClassW(&kind);
            CreateWindowExW(0,L"AcSyntheticGame",L"Synthetic game",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,kind.hInstance,nullptr); // Never shown.
        }
#endif
        if(mode=="werfault")QProcess::startDetached(qEnvironmentVariable("AC_SYNTHETIC_WERFAULT"),{"--synthetic-child"});
        if(mode=="grandchild"){
            qint64 pid=0;QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--synthetic-child"},{},&pid);
            if(const auto pidFile=qEnvironmentVariable("AC_SYNTHETIC_PIDS");!pidFile.isEmpty()){QFile file(pidFile);if(file.open(QIODevice::Append))file.write("grandchild "+QByteArray::number(pid)+"\n");}
            QTimer::singleShot(800,&game,&QCoreApplication::quit);
        }
        QTimer::singleShot(30000,&game,&QCoreApplication::quit);return game.exec();
    }
    // A launcher that starts two children, the first of which starts a grandchild. Each writes its
    // process id and waits; the launcher waits too ("stay") or ends after a moment ("leave").
    if(argc>=4&&(QByteArray(argv[1])=="--spawn-tree"||QByteArray(argv[1])=="--tree-child")){
        QCoreApplication child(argc,argv);const auto folder=QString::fromLocal8Bit(argv[2]),name=QString::fromLocal8Bit(argv[3]);
        const auto self=QCoreApplication::applicationFilePath();
        ac::install::atomicWrite(folder+"/"+name+".pid",QByteArray::number(QCoreApplication::applicationPid()));
        if(QByteArray(argv[1])=="--spawn-tree"){
            QProcess::startDetached(self,{"--tree-child",folder,"child1","grand"});QProcess::startDetached(self,{"--tree-child",folder,"child2"});
            if(argc>=5&&QByteArray(argv[4])=="leave"){QTimer::singleShot(2500,&child,&QCoreApplication::quit);return child.exec();}
        }else if(argc>=5&&QByteArray(argv[4])=="grand")QProcess::startDetached(self,{"--tree-child",folder,"grandchild"});
        QTimer::singleShot(60000,&child,&QCoreApplication::quit);return child.exec();
    }
    for(int i=1;i<argc;++i)if(QByteArray(argv[i])=="--synthetic-child"){QCoreApplication app(argc,argv);QTimer::singleShot(30000,&app,&QCoreApplication::quit);return app.exec();}
    // PS2 stand-in for the Python running the launcher: it waits for the stop file, then ends normally.
    if(argc==3&&QByteArray(argv[1]).endsWith("launch_ps2.py")){
        QCoreApplication child(argc,argv);const auto stop=qEnvironmentVariable("AC_SYNTHETIC_PS2_STOP");QTimer poll;
        // With AC_SYNTHETIC_PS2_IGNORE the game takes no notice of the stop file, as a stuck one would.
        const bool ignore=qEnvironmentVariableIsSet("AC_SYNTHETIC_PS2_IGNORE");
        QObject::connect(&poll,&QTimer::timeout,&child,[&]{if(!ignore&&QFileInfo::exists(stop))child.exit(0);});poll.start(100);
        QTimer::singleShot(20000,&child,[&]{child.exit(7);});return child.exec();
    }
    // Model 2 stand-ins. The emulator: any "-rompath" run, which waits to be ended. The viewer: one numeric argument.
    for(int i=1;i<argc;++i)if(QByteArray(argv[i])=="-rompath"){QCoreApplication child(argc,argv);QTimer::singleShot(30000,&child,&QCoreApplication::quit);return child.exec();}
    if(argc==2&&QByteArray(argv[1]).toLongLong()>0){
        QCoreApplication child(argc,argv);QJsonObject variables;const auto environment=QProcessEnvironment::systemEnvironment();
        for(const auto &name:environment.keys())if(name.startsWith("ACVR_")||name=="XR_RUNTIME_JSON")variables[name]=environment.value(name);
        const auto id=QByteArray(argv[1]).toLongLong();bool alive=false;
#ifdef Q_OS_WIN
        if(const auto handle=OpenProcess(SYNCHRONIZE,FALSE,static_cast<DWORD>(id))){alive=WaitForSingleObject(handle,0)==WAIT_TIMEOUT;CloseHandle(handle);}
#else
        alive=QFileInfo::exists("/proc/"+QString::number(id));
#endif
        QFile file(qEnvironmentVariable("AC_SYNTHETIC_MODEL2_REPORT"));if(!file.open(QIODevice::WriteOnly))return 3;
        file.write(QJsonDocument(QJsonObject{{"companion",id},{"companionAlive",alive},{"cwd",QDir::currentPath()},{"env",variables},{"path",environment.value("PATH")}}).toJson());file.close();
        // Like the real viewer: with AC_SYNTHETIC_MODEL2_WAIT_STOP it plays until its stop file appears, then ends normally.
        if(qEnvironmentVariableIsSet("AC_SYNTHETIC_MODEL2_WAIT_STOP")){const auto stop=qEnvironmentVariable("ACVR_M2_STOP_FILE");QTimer poll;
            QObject::connect(&poll,&QTimer::timeout,&child,[&]{if(QFileInfo::exists(stop))child.exit(0);});poll.start(100);QTimer::singleShot(20000,&child,[&]{child.exit(7);});return child.exec();}
        QTimer::singleShot(600,&child,&QCoreApplication::quit);return child.exec();
    }
    for(int i=1;i<argc;++i)if(QByteArray(argv[i]).startsWith("-input-system=")){
        QCoreApplication child(argc,argv);QJsonObject variables;
        const auto environment=QProcessEnvironment::systemEnvironment();
        for(const auto &name:environment.keys())if(name.startsWith("ACVR_")||name=="XR_RUNTIME_JSON")variables[name]=environment.value(name);
        QFile file(qEnvironmentVariable("AC_SYNTHETIC_MODEL3_REPORT"));if(!file.open(QIODevice::WriteOnly))return 3;
        file.write(QJsonDocument(QJsonObject{{"cwd",QDir::currentPath()},{"args",QJsonArray::fromStringList(child.arguments().mid(1))},{"env",variables}}).toJson());file.close();
        // A slow game: it writes its completion marker only after the given time, then ends.
        const auto duration=qEnvironmentVariableIntValue("AC_SYNTHETIC_CHILD_MS");const auto marker=qEnvironmentVariable("AC_SYNTHETIC_MARKER");
        QTimer::singleShot(duration>0?duration:600,&child,[&child,marker]{if(!marker.isEmpty()){QFile done(marker);if(done.open(QIODevice::WriteOnly))done.write("complete");}child.quit();});
        return child.exec();
    }
    // VPX stand-in: any run with -Play, which -Ini may come before.
    bool play=false;for(int i=1;i<argc;++i)play=play||QByteArray(argv[i])=="-Play";
    if(play){
        QGuiApplication child(argc,argv);QWindow window;window.show();
        const auto arguments=qEnvironmentVariable("AC_SYNTHETIC_PINBALL_ARGUMENTS");
        if(!arguments.isEmpty()){
            QFile file(arguments);if(!file.open(QIODevice::WriteOnly))return 3;
            file.write(QJsonDocument::fromVariant(child.arguments().mid(1)).toJson(QJsonDocument::Compact));
        }
        const auto duration=qEnvironmentVariableIntValue("AC_SYNTHETIC_PINBALL_CHILD_MS");
        QTimer::singleShot(duration>0?duration:600,&child,&QCoreApplication::quit);return child.exec();
    }
    QGuiApplication app(argc,argv);IntegrationTest test;return QTest::qExec(&test,argc,argv);
}
#include "IntegrationTest.moc"
