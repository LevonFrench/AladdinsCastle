// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "ProductIdentity.h"
#include "core/app/LaunchFailure.h"
#include "core/app/NetworkPolicy.h"
#include "core/catalog/CatalogLoader.h"
#include "core/catalog/CatalogPaths.h"
#include "core/install/Support.h"
#include "core/launch/Launch.h"
#include "app/HubServices.h"
#include "overlay/OverlayHost.h"
#include "overlay/SpikeState.h"
#include "ui/Theme.h"
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
    if(options.help){out<<ac::product::display_name<<" Hub\nUsage: "<<ac::product::program_name<<" [--overlay [--window] | --launch <game-id>]\n  --data-root <folder>  Portable catalog\n  --spike  Diagnostic overlay input scene\n  --overlay-picture-flip-y on|off  Picture override (default off)\n  --overlay-pointer-flip-y on|off  Pointer override (default on)\n  --register-overlay | --unregister-overlay  Explicit manifest action\n  --quit-after-ms <ms>  Bounded desktop diagnostic\n";return 0;}
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
    auto catalog=QtConcurrent::run([catalogRoot]{return ac::CatalogLoader().load(catalogRoot);}).result();
    const auto count=catalog.report.records;
    ac::GameListModel games(std::move(catalog));ac::FilterSortModel filter;filter.setSourceModel(&games);
    QQuickStyle::setStyle("Basic");ac::Theme theme(games.catalog().theme);
    const auto portableRoot=options.installRoot.isEmpty()?app.applicationDirPath():QDir(options.installRoot).absolutePath();
    try{ac::install::scopedPath("user/hub-settings.toml",portableRoot);ac::install::scopedPath("user/scan-folders.json",portableRoot);ac::install::scopedPath("user/cache/scan-bindings.json",portableRoot);}catch(const std::exception &e){err<<e.what()<<'\n';return 2;}
    ac::UiSettings settings(portableRoot+"/user");ac::UiController ui(&games,&filter,&settings);
    ac::HubServices services(&games,&filter,&ui,&settings,portableRoot);
    const auto capture=qEnvironmentVariable("AC_UI_CAPTURE");
    QObject::connect(&services,&ac::HubServices::hubExitReady,&app,[&app,capture](int code){
        if(capture.isEmpty())app.exit(code);
        else QTimer::singleShot(0,&app,[&app,code]{app.exit(code);}); // Finish observers before writing the exit receipt.
    });
    QObject::connect(&app,&QGuiApplication::applicationStateChanged,&ui,[&](Qt::ApplicationState state){if(state!=Qt::ApplicationActive)ui.clearDetailControlStates();});
    qmlRegisterSingletonInstance("AladdinsCastle.Hub",1,0,"Theme",&theme);
    ac::SpikeState spike;spike.setFlipY(options.pointerFlipY);ac::LocalQmlNetworkFactory localNetwork;QQmlApplicationEngine engine;engine.setNetworkAccessManagerFactory(&localNetwork);ac::OverlayHost overlay(spike,ac::makeOpenVrRuntime(options.pictureFlipY));
    engine.addImageProvider("art",new ac::art::Provider(services.artResolver()));ui.setArtProviderReady(true);
    auto *context=engine.rootContext();context->setContextProperty("catalogGameCount",count);
    context->setContextProperty("gameModel",&games);context->setContextProperty("gameFilter",&filter);
    context->setContextProperty("uiSettings",&settings);context->setContextProperty("uiController",&ui);
    context->setContextProperty("hubServices",&services);context->setContextProperty("overlayHost",&overlay);
    context->setContextProperty("spikeState",&spike);context->setContextProperty("overlaySpikeEnabled",options.spike);
    context->setContextProperty("surfaceColor",theme.get("color.surface.window"));
    context->setContextProperty("primaryTextColor",theme.get("color.text.primary"));context->setContextProperty("brandColor",theme.get("color.brand.orange"));
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&services,[&services]{services.requestHubExit(2);},Qt::QueuedConnection);
    QObject::connect(&services,&ac::HubServices::raiseHubRequested,&engine,[&]{for(auto *object:engine.rootObjects())if(auto *window=qobject_cast<QQuickWindow*>(object)){window->show();window->raise();window->requestActivate();}});
    QObject::connect(&app,&QGuiApplication::lastWindowClosed,&app,[&]{if(ac::shouldQuitAfterLastWindow(services.launchBusy()))app.quit();});
    bool overlayInitialized=false;
    QObject::connect(services.launcher(),&ac::launch::LaunchService::finished,&app,[&](const QString &,int,const QString &,const QString &){QTimer::singleShot(0,&app,[&]{bool visible=false;for(auto *object:engine.rootObjects())if(auto *window=qobject_cast<QQuickWindow *>(object))visible=visible||window->isVisible();if(ac::shouldQuitAfterLaunch(overlayInitialized,services.launchBusy(),visible))app.quit();});});
    if(options.mode==ac::Mode::Overlay){
        QObject::connect(&overlay,&ac::OverlayHost::quitRequested,&services,[&services]{services.requestHubExit();});
        QObject::connect(&overlay,&ac::OverlayHost::failed,&app,[&](const QString &error){err<<error<<'\n';services.requestHubExit(3);});
        QString error;overlayInitialized=overlay.initialize(engine,app.applicationDirPath()+"/resources/overlay-thumbnail.png",&error);if(!overlayInitialized){const bool fallback=ac::shouldOpenDesktop(options,false);err<<error<<(fallback?"; opening desktop Hub.\n":"; exiting SteamVR-started instance.\n");overlay.shutdown();if(!fallback)return 3;ui.overlayFallback(error);}
    }
    if(ac::shouldOpenDesktop(options,overlayInitialized)){engine.loadFromModule("AladdinsCastle.Hub","DesktopShell");if(engine.rootObjects().isEmpty())return 2;}
    // Explicit operator diagnostics capture this application's own Qt surface.
    // Private owner captures are never included in packaging.
    const auto qaGame=qEnvironmentVariable("AC_UI_PLAY");
    const auto qaPinballRow=qEnvironmentVariable("AC_UI_PINBALL_ROW");
    if(!qaGame.isEmpty()&&!qaPinballRow.isEmpty())ui.showError("Diagnostic","Choose either a catalog game or one Pinball diagnostic row.");
    else if(!qaPinballRow.isEmpty())QTimer::singleShot(1000,&ui,[&ui,qaPinballRow]{
        QFile file(qaPinballRow);QJsonParseError error;
        const auto document=file.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(file.readAll(),&error):QJsonDocument{};
        if(error.error!=QJsonParseError::NoError||!document.isObject()){
            ui.showError("Diagnostic","Invalid Pinball diagnostic row; expected one cached JSON object.");return;
        }
        const auto row=document.object().toVariantMap();
        if(row.value("id").toString().isEmpty()||row.value("path").toString().isEmpty()){
            ui.showError("Diagnostic","Invalid Pinball diagnostic row; missing table identity or path.");return;
        }
        ui.pinballLoaded({{"tables",QVariantList{row}}});ui.selectPinball(row.value("id").toString());ui.playPinball();
    });
    else if(!qaGame.isEmpty())QTimer::singleShot(1000,&ui,[&ui,qaGame]{ui.primary(qaGame);});
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
    if(!capture.isEmpty())QTimer::singleShot(6000,&app,[&engine,&ui,&games,&services,capture]{
        bool imageSaved=false;
        for(auto *object:engine.rootObjects())if(auto *window=qobject_cast<QQuickWindow*>(object)){
            QDir().mkpath(QFileInfo(capture).absolutePath());imageSaved=window->grabWindow().save(capture);break;
        }
        ac::Json receipt{{"status",ui.status().toStdString()},{"imageSaved",imageSaved},{"launcherBusy",services.launchBusy()},{"nowPlaying",ac::Json::parse(QJsonDocument::fromVariant(ui.nowPlaying()).toJson(QJsonDocument::Compact).toStdString())},{"games",ac::Json::array()}};
        for(const auto &game:games.records())if(game.runtime.playing)receipt["games"].push_back({{"id",game.id.toStdString()},{"playing",true},{"stateLabel",game.roles.value("stateLabel").toString().toStdString()}});
        ac::install::atomicWrite(capture+".json",QByteArray::fromStdString(receipt.dump(2)));
    });
    out<<ac::product::display_name<<": "<<count<<" games\n";out.flush();
    if(options.quitAfterMs>0)QTimer::singleShot(options.quitAfterMs,&services,[&services]{services.requestHubExit();});
    return app.exec();
}
