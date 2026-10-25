// SPDX-License-Identifier: GPL-3.0-only
#include "core/launch/Watchdog.h"
#include "core/launch/SteamVrStart.h"
#include "core/app/PanelAttacher.h"
#include "core/app/LaunchOptions.h"
#include "ProductIdentity.h"
#include "core/app/LaunchFailure.h"
#include "core/app/NetworkPolicy.h"
#include "core/catalog/CatalogLoader.h"
#include "core/catalog/CatalogPaths.h"
#include "core/install/Support.h"
#include "core/launch/Launch.h"
#include "core/pinball/Gamepad.h"
#include "app/HubServices.h"
#include "app/LaunchProof.h"
#include "overlay/OverlayHost.h"
#include "overlay/CompositorCapture.h"
#include "overlay/SpikeState.h"
#include "ui/Theme.h"
#include <QCryptographicHash>
#include <QLocalSocket>
#include <QLocalServer>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QQuickStyle>
#include <QTextStream>
#include <QTimer>
#include <QtConcurrent>
#include <qqml.h>
int main(int argc,char **argv){
    // The gamepad check for a pinball start, in a copy of the Hub of its own: the SDL library in the user's
    // VPX folder is loaded here, never into the open Hub. No window; the devices are printed and it quits.
    if(argc==3&&QByteArray(argv[1])=="--probe-gamepads"){
        QCoreApplication probe(argc,argv);
        QTextStream(stdout)<<QJsonDocument::fromVariant(ac::pinball::probeGamepads(probe.arguments().at(2))).toJson(QJsonDocument::Compact);
        return 0;
    }
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    ac::LaunchOptions options;
    {QCoreApplication argumentApp(argc,argv);options=ac::parseLaunchOptions(argumentApp.arguments().mid(1));}
    if(ac::wantsParentConsole(options))ac::attachParentConsole();
    QTextStream out(stdout),err(stderr);
    if(!options.error.isEmpty()){
        err<<options.error<<'\n';
        if(options.mode==ac::Mode::Launch){QCoreApplication app(argc,argv);QString log;try{log=ac::writeLaunchFailureLog(options.installRoot.isEmpty()?app.applicationDirPath():options.installRoot,options.gameId,options.error);}catch(const std::exception &){}ac::showLaunchFailure(options.error,log);}
        return 2;
    }
    if(options.help){
        out<<ac::product::display_name<<" Hub\nUsage: "<<ac::product::program_name<<" [--overlay [--window] | --launch <game-id> | --capture-compositor-dir <directory>]\n"
           <<"  --data-root <folder>  Portable catalog\n  --spike  Diagnostic overlay input scene\n"
           <<"  --overlay-picture-flip-y on|off  Picture override (default off)\n  --overlay-pointer-flip-y on|off  Pointer override (default off on Windows, on elsewhere)\n"
           <<"  --overlay-capture-dir <absolute fresh directory>  One source texture PNG/receipt under catalog .local (requires --overlay)\n"
           <<"  --show-dashboard  Explicitly select this Hub's dashboard (requires --overlay)\n  --desktop  Window only (by default the Hub is also SteamVR's dashboard panel whenever SteamVR runs)\n"
           <<"  --capture-compositor-dir <absolute fresh directory>  One compositor eye pair under catalog .local; existing service required\n"
           <<"  --compositor-dashboard any|visible|hidden  Snapshot visibility requirement (default any); never changes dashboard state\n"
           <<"  --register-overlay | --unregister-overlay  Explicit manifest action\n  --quit-after-ms <ms>  Bounded desktop diagnostic\n";return 0;
    }
    if(options.version){out<<ac::product::display_name<<" Hub 0.1.0\n";return 0;}
    if(options.mode==ac::Mode::RegisterOverlay||options.mode==ac::Mode::UnregisterOverlay){
        QCoreApplication app(argc,argv);QString error;
        if(!ac::changeOverlayRegistration(QDir(app.applicationDirPath()).filePath(QString("resources/")+ac::product::short_id+".vrmanifest"),options.mode==ac::Mode::RegisterOverlay,&error)){err<<error<<'\n';return 3;}
        out<<"Overlay manifest action completed.\n";return 0;
    }
    if(options.mode==ac::Mode::Launch){
        QCoreApplication app(argc,argv);
        const auto root=options.installRoot.isEmpty()?app.applicationDirPath():QDir(options.installRoot).absolutePath();
        const auto failure=[&](const QString &message){QString log;try{log=ac::writeLaunchFailureLog(root,options.gameId,message);}catch(const std::exception &e){err<<"Launch log unavailable: "<<e.what()<<'\n';}err<<message<<'\n';ac::showLaunchFailure(message,log);};
        try{
            const auto catalogRoot=ac::findCatalogRoot(app.applicationDirPath(),QDir::currentPath(),options.dataRoot);
            if(catalogRoot.isEmpty())throw ac::install::Error("E_CATALOG","Catalog missing");
            const auto catalog=ac::CatalogLoader().load(catalogRoot);
            const auto bindings=ac::Json::parse(ac::install::readBytes(options.bindingsFile.isEmpty()?root+"/user/cache/scan-bindings.json":options.bindingsFile).toStdString());
            ac::launch::LaunchService launcher;
            QObject::connect(&launcher,&ac::launch::LaunchService::warning,&app,[&](const QString &,const QString &text){err<<"Launch notice: "<<text<<'\n';err.flush();});
            QObject::connect(&launcher,&ac::launch::LaunchService::finished,&app,[&](const QString &,int code,const QString &message,const QString &){if(!message.isEmpty())failure(message);app.exit(code);});
            const auto request=ac::launch::gameRequest(catalog,options.gameId,root,bindings,options.variantId);
            if(!launcher.start(request))return 1;
            if(options.quitAfterMs>0)QTimer::singleShot(options.quitAfterMs,&launcher,&ac::launch::LaunchService::stop);
            return app.exec();
        }catch(const std::exception &e){failure(QString::fromUtf8(e.what()));return 2;}
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(ac::product::user_data_folder);QGuiApplication::setApplicationDisplayName(ac::product::display_name);QCoreApplication::setApplicationVersion("0.1.0");
    const auto catalogRoot=ac::findCatalogRoot(app.applicationDirPath(),QDir::currentPath(),options.dataRoot);
    if(catalogRoot.isEmpty()){err<<"Catalog missing.\n";return 2;}
    if(options.mode==ac::Mode::CompositorCapture){
        QString error;
        if(!ac::captureCompositorEyes(catalogRoot,options.compositorCaptureDir,options.compositorDashboard,&error)){err<<error<<'\n';return 3;}
        out<<"Compositor eye snapshot written; inspect content and orientation. No physical headset or exact frame-sync acceptance.\n";return 0;
    }
    // Loaded on a worker before any window exists (about half a second for the whole catalog), so there
    // is no window to freeze; the theme, the services and the art resolver are all built from it.
    auto catalog=QtConcurrent::run([catalogRoot]{return ac::CatalogLoader().load(catalogRoot);}).result();
    const auto count=catalog.report.records;
    ac::GameListModel games(std::move(catalog));ac::FilterSortModel filter;filter.setSourceModel(&games);
    QQuickStyle::setStyle("Basic");ac::Theme theme(games.catalog().theme);
    const auto portableRoot=options.installRoot.isEmpty()?app.applicationDirPath():QDir(options.installRoot).absolutePath();
    // Keep SteamVR's entry for the Hub pointing at this program, so SteamVR registers it next time it starts.
    {QString manifestError;ac::refreshOverlayManifest(QDir(app.applicationDirPath()).filePath(QString("resources/")+ac::product::short_id+".vrmanifest"),&manifestError);}
    // One Hub per folder: a second start asks the first to come to the front, and quits. A Hub that SteamVR
    // starts as its panel quits too; the open one joins SteamVR's dashboard itself. Picture and test runs are
    // left alone unless a test asks for the guard.
    std::unique_ptr<QLocalServer> instance;
    const auto instanceName=QString("%1-%2").arg(QString::fromLatin1(ac::product::program_name),
        QString::fromLatin1(QCryptographicHash::hash(QDir::cleanPath(portableRoot).toLower().toUtf8(),QCryptographicHash::Sha256).toHex().left(16)));
    if((options.mode==ac::Mode::Desktop||options.mode==ac::Mode::Overlay)&&!qEnvironmentVariableIsSet("AC_UI_CAPTURE")&&
       (qEnvironmentVariable("QT_QPA_PLATFORM")!="offscreen"||qEnvironmentVariableIsSet("AC_SINGLE_INSTANCE_TEST"))){
        QLocalSocket other;other.connectToServer(instanceName);
        if(other.waitForConnected(500)){
            other.write(options.mode==ac::Mode::Overlay?"panel\n":"raise\n");other.waitForBytesWritten(500);other.disconnectFromServer();
            out<<"Another Hub is open on this folder; it was asked to come to the front.\n";out.flush();return 0;
        }
        QLocalServer::removeServer(instanceName);
        instance=std::make_unique<QLocalServer>();instance->setSocketOptions(QLocalServer::UserAccessOption);
        if(!instance->listen(instanceName))instance.reset();
    }
    try{ac::install::scopedPath("user/hub-settings.toml",portableRoot);ac::install::scopedPath("user/scan-folders.json",portableRoot);ac::install::scopedPath("user/cache/scan-bindings.json",portableRoot);}catch(const std::exception &e){err<<e.what()<<'\n';return 2;}
    ac::UiSettings settings(portableRoot+"/user");ac::UiController ui(&games,&filter,&settings);
    settings.createAcceptedList(); // The owner's own list of games accepted in the headset, empty to begin with.
    ac::HubServices services(&games,&filter,&ui,&settings,portableRoot);
    // Play in headset may start SteamVR when the user presses it. Diagnostic and offscreen runs may not.
    if(qEnvironmentVariable("QT_QPA_PLATFORM")!="offscreen"&&!qEnvironmentVariableIsSet("AC_UI_CAPTURE"))services.setSteamVrStarter(ac::launch::startSteamVr);
    const auto capture=qEnvironmentVariable("AC_UI_CAPTURE");
    // Asked for on the set-up page only. Registration adds the manifest; it never switches on automatic start.
    QObject::connect(&services,&ac::HubServices::steamVrRegistrationRequested,&app,[&services](const QString &manifest){
        QString error;if(ac::changeOverlayRegistration(manifest,true,&error))error.clear();else if(error.isEmpty())error="SteamVR did not accept the request.";
        services.steamVrAnswered(error);
    });
    QObject::connect(&services,&ac::HubServices::hubExitReady,&app,[&app,capture](int code){
        if(capture.isEmpty())app.exit(code);
        else QTimer::singleShot(0,&app,[&app,code]{app.exit(code);}); // Finish observers before writing the exit receipt.
    });
    QObject::connect(&app,&QGuiApplication::applicationStateChanged,&ui,[&](Qt::ApplicationState state){if(state!=Qt::ApplicationActive)ui.clearDetailControlStates();});
    qmlRegisterSingletonInstance("AladdinsCastle.Hub",1,0,"Theme",&theme);
    ac::SpikeState spike;spike.setFlipY(options.pointerFlipY);ac::LocalQmlNetworkFactory localNetwork;QQmlApplicationEngine engine;engine.setNetworkAccessManagerFactory(&localNetwork);ac::OverlayHost overlay(spike,ac::makeOpenVrRuntime(options.pictureFlipY));
    if(options.mode==ac::Mode::Overlay){
        // What the headset pointer sent and what became of it, for a sitting that goes wrong.
        try{const auto log=ac::install::scopedPath("user/logs/overlay-input.log",portableRoot);QDir().mkpath(QFileInfo(log).absolutePath());overlay.setInputLog(log);}catch(const std::exception &){}
    }
    if(!options.overlayCaptureDir.isEmpty()){
        QString error;
        if(!overlay.configureCapture(catalogRoot,options.overlayCaptureDir,options.pictureFlipY,&error)){err<<error<<'\n';return 2;}
    }
    engine.addImageProvider("art",new ac::art::Provider(services.artResolver()));ui.setArtProviderReady(true);
    auto *context=engine.rootContext();context->setContextProperty("catalogGameCount",count);
    context->setContextProperty("gameModel",&games);context->setContextProperty("gameFilter",&filter);
    context->setContextProperty("uiSettings",&settings);context->setContextProperty("uiController",&ui);
    context->setContextProperty("hubServices",&services);context->setContextProperty("overlayHost",&overlay);
    // The first start opens the set-up; AC_UI_VIEW names another first page for shortcuts and diagnostics.
    // Diagnostics: a menu to open at start, and the whole catalog without changing the user's settings.
    context->setContextProperty("startMenu",qEnvironmentVariable("AC_UI_OPEN"));
    context->setContextProperty("forceShowAll",qEnvironmentVariableIsSet("AC_UI_SHOW_ALL"));
    context->setContextProperty("panelLook",qEnvironmentVariableIsSet("AC_UI_PANEL_LOOK")); // The SteamVR panel's larger controls, drawn in a desktop window.
    context->setContextProperty("startView",qEnvironmentVariableIsSet("AC_UI_VIEW")?qEnvironmentVariable("AC_UI_VIEW"):settings.get("setupDone",false).toBool()?QString():QString("Setup"));
    context->setContextProperty("spikeState",&spike);context->setContextProperty("overlaySpikeEnabled",options.spike);
    context->setContextProperty("surfaceColor",theme.get("color.surface.window"));
    context->setContextProperty("primaryTextColor",theme.get("color.text.primary"));context->setContextProperty("brandColor",theme.get("color.brand.orange"));
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&services,[&services]{services.requestHubExit(2);},Qt::QueuedConnection);
    QObject::connect(&services,&ac::HubServices::raiseHubRequested,&engine,[&]{for(auto *object:engine.rootObjects())if(auto *window=qobject_cast<QQuickWindow*>(object)){window->show();window->raise();window->requestActivate();}});
    QObject::connect(&app,&QGuiApplication::lastWindowClosed,&app,[&]{if(ac::shouldQuitAfterLastWindow(services.launchBusy()))app.quit();});
    if(instance)QObject::connect(instance.get(),&QLocalServer::newConnection,&app,[&]{
        while(auto *socket=instance->nextPendingConnection()){
            QObject::connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            QObject::connect(socket,&QLocalSocket::readyRead,&app,[&,socket]{
                if(socket->readAll().startsWith("raise")){out<<"Asked to come to the front by another start.\n";out.flush();emit services.raiseHubRequested();}
            });
        }
    });
    bool overlayInitialized=false;
    QObject::connect(services.launcher(),&ac::launch::LaunchService::finished,&app,[&](const QString &,int,const QString &,const QString &){QTimer::singleShot(0,&app,[&]{bool visible=false;for(auto *object:engine.rootObjects())if(auto *window=qobject_cast<QQuickWindow *>(object))visible=visible||window->isVisible();if(ac::shouldQuitAfterLaunch(overlayInitialized,services.launchBusy(),visible))app.quit();});});
    if(options.mode==ac::Mode::Overlay){
        QObject::connect(&overlay,&ac::OverlayHost::quitRequested,&services,[&services]{services.requestHubExit();});
        QObject::connect(&overlay,&ac::OverlayHost::failed,&app,[&](const QString &error){err<<error<<'\n';services.requestHubExit(3);});
        QString error;overlayInitialized=overlay.initialize(engine,app.applicationDirPath()+"/resources/overlay-thumbnail.png",&error);if(!overlayInitialized){const bool fallback=ac::shouldOpenDesktop(options,false);err<<error<<(fallback?"; opening desktop Hub.\n":"; exiting SteamVR-started instance.\n");overlay.shutdown();if(!fallback)return 3;ui.overlayFallback(error);}
    }
    if(ac::shouldOpenDesktop(options,overlayInitialized)){engine.loadFromModule("AladdinsCastle.Hub","DesktopShell");if(engine.rootObjects().isEmpty())return 2;}
    // The desktop Hub is also SteamVR's dashboard panel whenever SteamVR runs: attached when SteamVR is
    // seen, whether it was running first, started later, or started by Play in headset; let go when it
    // quits, with the window carrying on. The dashboard is never opened on the Hub by itself.
    // Diagnostic and offscreen runs never attach to SteamVR on their own.
    std::unique_ptr<ac::PanelAttacher> panel;
    if(ac::attachesPanelWhenSteamVrRuns(options,qEnvironmentVariable("QT_QPA_PLATFORM")=="offscreen"||qEnvironmentVariableIsSet("AC_UI_CAPTURE"))){
        try{const auto log=ac::install::scopedPath("user/logs/overlay-input.log",portableRoot);QDir().mkpath(QFileInfo(log).absolutePath());overlay.setInputLog(log);}catch(const std::exception &){}
        const auto thumbnail=app.applicationDirPath()+"/resources/overlay-thumbnail.png";
        panel=std::make_unique<ac::PanelAttacher>([&engine,&overlay,thumbnail](QString *error){
                if(overlay.initialize(engine,thumbnail,error))return true;
                overlay.shutdown();
                return false;},
            [&overlay]{QTimer::singleShot(0,&overlay,[&overlay]{overlay.shutdown();});});
        QObject::connect(panel.get(),&ac::PanelAttacher::attachedChanged,&app,[&out](bool attached){out<<(attached?"Joined SteamVR's dashboard as a panel.\n":"Left SteamVR's dashboard; the window carries on.\n");out.flush();});
        QObject::connect(panel.get(),&ac::PanelAttacher::gaveUp,&app,[&err](const QString &reason){err<<"Not a SteamVR panel this time: "<<reason<<'\n';});
        QObject::connect(&overlay,&ac::OverlayHost::quitRequested,panel.get(),&ac::PanelAttacher::steamVrQuit);
        QObject::connect(&overlay,&ac::OverlayHost::failed,panel.get(),[&err,p=panel.get()](const QString &error){err<<error<<'\n';p->steamVrQuit();});
        QObject::connect(&ui,&ac::UiController::steamVrRunningChanged,panel.get(),[&ui,p=panel.get()]{p->setSteamVrRunning(ui.steamVrRunning());});
    }
    // Explicit operator diagnostics capture this application's own Qt surface.
    // Private owner captures are never included in packaging.
    const auto qaGame=qEnvironmentVariable("AC_UI_PLAY");
    const auto qaPinballRow=qEnvironmentVariable("AC_UI_PINBALL_ROW");
    const auto qaPinballIni=qEnvironmentVariable("AC_UI_PINBALL_INI");
    if(!qaPinballIni.isEmpty()&&qaPinballRow.isEmpty())ui.showError("Diagnostic","AC_UI_PINBALL_INI requires AC_UI_PINBALL_ROW.");
    else if(!qaGame.isEmpty()&&!qaPinballRow.isEmpty())ui.showError("Diagnostic","Choose either a catalog game or one Pinball diagnostic row.");
    else if(!qaPinballRow.isEmpty())QTimer::singleShot(1000,&ui,[&ui,&services,qaPinballRow,qaPinballIni]{
        QFile file(qaPinballRow);QJsonParseError error;
        const auto document=file.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(file.readAll(),&error):QJsonDocument{};
        if(error.error!=QJsonParseError::NoError||!document.isObject()){
            ui.showError("Diagnostic","Invalid Pinball diagnostic row; expected one cached JSON object.");return;
        }
        const auto row=document.object().toVariantMap();
        if(row.value("id").toString().isEmpty()||row.value("path").toString().isEmpty()){
            ui.showError("Diagnostic","Invalid Pinball diagnostic row; missing table identity or path.");return;
        }
        try{if(!qaPinballIni.isEmpty())services.setPinballDiagnosticIni(qaPinballIni);}
        catch(const std::exception &exception){ui.showError("Diagnostic","Pinball diagnostic settings refused: "+QString::fromUtf8(exception.what()));return;}
        ui.pinballLoaded({{"tables",QVariantList{row}}});ui.selectPinball(row.value("id").toString());ui.playPinball(qEnvironmentVariable("AC_UI_PINBALL_VIEW","desktop")); // A diagnostic's settings file chooses VR itself.
    });
    else if(!qaGame.isEmpty())QTimer::singleShot(1000,&ui,[&ui,qaGame]{ui.primary(qaGame);});
    // In the headset, holding the left Y (B on Index) for 3 seconds closes a table launched in the headset, the ordinary way.
    QObject::connect(&ui,&ac::UiController::nowPlayingChanged,&overlay,[&ui,&overlay]{
        const auto session=ui.nowPlaying();
        overlay.setCloseGesture(session.value("running").toBool()&&!session.value("closing").toBool()&&session.contains("pinballId")&&session.value("view")=="headset");
    });
    QObject::connect(&overlay,&ac::OverlayHost::closeGameRequested,&ui,[&ui]{ui.stopLaunch();});
    // Diagnostic: one game started and ended through the Hub, written to AC_LAUNCH_PROOF.
    if(const auto proof=ac::launchProofOptions();!proof.output.isEmpty())ac::runLaunchProof(proof,services,ui);
    // Diagnostic, for pictures only: the now-playing screen with a sample session; nothing is started.
    if(qEnvironmentVariableIsSet("AC_UI_DEMO_NOWPLAYING"))QTimer::singleShot(400,&ui,[&ui]{
        ui.launchOtherQueued("model3-scud","screen","Scud Race · Screen","Home = set straight ahead. 1 = start, 3 = coin, hold Y for 3 seconds to quit.",QString());ui.launchStarted("model3-scud");});
    // Diagnostic: Get artwork for these games ("id,id"), one after another, as their pages' buttons do.
    // It still needs the setting on. With AC_UI_GET_ARTWORK_QUIT the Hub quits when the last one ends.
    if(qEnvironmentVariableIsSet("AC_UI_GET_ARTWORK")){
        auto queue=std::make_shared<QStringList>(qEnvironmentVariable("AC_UI_GET_ARTWORK").split(',',Qt::SkipEmptyParts));
        auto asked=std::make_shared<bool>(false);
        const auto next=[&ui,queue,asked]{
            if(!queue->isEmpty()){*asked=true;ui.getArtwork(queue->takeFirst());}
            else if(qEnvironmentVariableIsSet("AC_UI_GET_ARTWORK_QUIT"))QTimer::singleShot(0,qApp,[]{QCoreApplication::quit();});
        };
        // Only the end of the request it made itself moves it on.
        QObject::connect(&ui,&ac::UiController::artworkChanged,&ui,[&ui,&out,asked,next]{
            if(!*asked||ui.artworkBusy())return;
            *asked=false;out<<ui.artworkStatus()<<'\n';out.flush();
            QTimer::singleShot(0,&ui,next);
        });
        QTimer::singleShot(800,&ui,next);
    }
    // Diagnostic, for pictures only: the Starting SteamVR state for a sample game; nothing is started.
    if(qEnvironmentVariableIsSet("AC_UI_DEMO_STEAMVR_STARTING"))QTimer::singleShot(400,&ui,[&ui]{ui.setSteamVrStarting(qEnvironmentVariable("AC_UI_DEMO_STEAMVR_STARTING"));});
    // Diagnostic, for pictures only: the watchdog's strip, a crash and a game that is not responding,
    // from sample data ("strip", "crash" or "hung"). No process is looked at or touched.
    if(const auto demo=qEnvironmentVariable("AC_UI_DEMO_WATCHDOG");!demo.isEmpty())QTimer::singleShot(400,&ui,[&ui,demo]{
        const QVariantList strays{QVariantMap{{"pid",4242},{"label","Visual Pinball · B2SBackglassServerEXE.exe"},{"system","pinball"}},
                                  QVariantMap{{"pid",4243},{"label","Visual Pinball · VPinballX64.exe"},{"system","pinball"}},
                                  QVariantMap{{"pid",4244},{"label","Windows Error Reporting for Supermodel.exe"},{"system","model3"}}};
        if(demo=="strip")ui.setWatchdog(strays,QString());
        if(demo=="crash"){
            ui.launchOtherQueued("model3-scud","screen","Scud Race · Screen","",QString());ui.launchStarted("model3-scud");
            ui.launchFinished("model3-scud","The game stopped unexpectedly",int(0xC0000005));
            ui.markCrash("model3-scud","Scud Race · Screen crashed: "+ac::launch::exitCodeText(int(0xC0000005))+".");
            ui.setCrashRecords({QVariantMap{{"time","2026-10-15T21:04:11"},{"title","Scud Race · Screen"},{"meaning",ac::launch::exitCodeText(int(0xC0000005))},{"exitCode","0xC0000005"},
                {"logFrom","Supermodel.log"},{"log","Supermodel 0.3a\nLoading ROM set\nRenderer: OpenGL 4.6\nNetwork board: off\nFrame 18342: unhandled exception"}}});
            ui.setWatchdog({strays.first()},QString());
        }
        if(demo=="hung"){ui.launchOtherQueued("model3-scud","screen","Scud Race · Screen","Home = set straight ahead. 1 = start, 3 = coin.",QString());ui.launchStarted("model3-scud");ui.setWatchdog({},"hung");}
    });
    // Diagnostic: read the ticked systems' folders once, as the set-up's Rescan does.
    if(qEnvironmentVariableIsSet("AC_UI_SETUP_SCAN"))QTimer::singleShot(500,&ui,[&ui]{ui.scanSetup();});
    ac::Json diagnosticFinished=ac::Json::array();
    if(!capture.isEmpty()){
        QObject::connect(services.launcher(),&ac::launch::LaunchService::finished,&app,[&diagnosticFinished](const QString &id,int code,const QString &error,const QString &){
            diagnosticFinished.push_back({{"gameId",id.toStdString()},{"exitCode",code},{"error",error.toStdString()}});
        });
        QObject::connect(&app,&QCoreApplication::aboutToQuit,&app,[&ui,&services,&diagnosticFinished,capture]{
            const ac::Json receipt{{"launcherBusy",services.launchBusy()},{"nowPlaying",ac::Json::parse(QJsonDocument::fromVariant(ui.nowPlaying()).toJson(QJsonDocument::Compact).toStdString())},{"finished",diagnosticFinished}};
            ac::install::atomicWrite(capture+".exit.json",QByteArray::fromStdString(receipt.dump(2)));
        });
    }
    const auto writeCapture=[&engine,&ui,&games,&services,capture]{
        bool imageSaved=false;
        for(auto *object:engine.rootObjects())if(auto *window=qobject_cast<QQuickWindow*>(object)){
            QDir().mkpath(QFileInfo(capture).absolutePath());imageSaved=window->grabWindow().save(capture);break;
        }
        ac::Json receipt{{"status",ui.status().toStdString()},{"imageSaved",imageSaved},{"launcherBusy",services.launchBusy()},{"nowPlaying",ac::Json::parse(QJsonDocument::fromVariant(ui.nowPlaying()).toJson(QJsonDocument::Compact).toStdString())},{"games",ac::Json::array()}};
        for(const auto &game:games.records())if(game.runtime.playing)receipt["games"].push_back({{"id",game.id.toStdString()},{"playing",true},{"stateLabel",game.roles.value("stateLabel").toString().toStdString()}});
        ac::install::atomicWrite(capture+".json",QByteArray::fromStdString(receipt.dump(2)));
        // Picture sheets: quit once the picture is saved, when nothing was started.
        if(qEnvironmentVariableIsSet("AC_UI_CAPTURE_QUIT")&&!services.launchBusy())QTimer::singleShot(0,qApp,[]{QCoreApplication::quit();});
    };
    // Test-only: with AC_UI_CAPTURE_WHEN_PLAYING the picture and receipt are taken at the first running session instead
    // of at a fixed 6 s, and the Hub exits once that session has ended, so a test is decided by events, not by CPU speed.
    if(!capture.isEmpty()&&qEnvironmentVariableIsSet("AC_UI_CAPTURE_WHEN_PLAYING")){
        auto captured=std::make_shared<bool>(false);
        QObject::connect(&ui,&ac::UiController::nowPlayingChanged,&app,[&app,&ui,captured,writeCapture]{
            if(*captured||!ui.nowPlaying().value("running").toBool())return;
            *captured=true;QTimer::singleShot(0,&app,writeCapture);
        });
        QObject::connect(services.launcher(),&ac::launch::LaunchService::finished,&app,[&services](const QString &,int,const QString &,const QString &){
            QTimer::singleShot(0,&services,[&services]{services.requestHubExit();});
        });
    }
    else if(!capture.isEmpty())QTimer::singleShot(6000,&app,writeCapture);
    if(overlayInitialized&&options.showDashboard){
        QString error;if(!overlay.showDashboard(&error)){err<<error<<'\n';overlay.shutdown();return 3;}
        out<<"Own dashboard show requested; compositor presentation requires verification.\n";
    }
    out<<ac::product::display_name<<": "<<count<<" games\n";out.flush();
    if(options.quitAfterMs>0)QTimer::singleShot(options.quitAfterMs,&services,[&services]{services.requestHubExit();});
    return app.exec();
}
