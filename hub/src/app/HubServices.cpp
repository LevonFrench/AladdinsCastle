// SPDX-License-Identifier: GPL-3.0-only
#include "HubServices.h"
#include "core/install/Support.h"
#include "core/pinball/Pinball.h"
#include "core/model3/Model3.h"
#include "core/setup/Setup.h"
#include "core/setup/Report.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/model2/Model2.h"
#include "ProductIdentity.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QPointer>
namespace ac {
namespace {
QString privatePinballIni(const QString &path,const QString &root) {
    if(!QDir::isAbsolutePath(path))throw install::Error("E_PINBALL_DIAGNOSTIC_INI","Use an absolute private settings path");
    const auto candidate=install::scopedPath(path,QDir(root).filePath(".local"));
    const QFileInfo file(candidate);
    if(!file.isFile()||!file.isReadable())throw install::Error("E_PINBALL_DIAGNOSTIC_INI","Private settings file is missing or unreadable");
    return candidate;
}
scan::ScanOptions scanOptions(const QString &root) {
    install::scopedPath("user/scan-folders.json",root);
    install::scopedPath("user/cache/scan.toml",root);
    scan::ScanOptions options; options.userRoot=QDir(root).filePath("user");
    options.toolRoots={QDir(root).filePath("emulators")}; return options;
}
Json readJson(const QString &path,const QString &root) {
    QFile file(install::scopedPath(path,root));
    if (!file.open(QIODevice::ReadOnly)) return Json::object();
    if (file.size()>128*1024*1024) throw install::Error("E_STATE_SIZE","Hub state is too large");
    return Json::parse(file.readAll().toStdString());
}
QString text(const Json &j,const char *key){return install::string(j,key);}
QStringList strings(const Json &j,const char *key){QStringList out;for(const auto &v:j.value(key,Json::array()))if(v.is_string())out<<QString::fromStdString(v.get<std::string>());return out;}
}
HubServices::HubServices(GameListModel *games,FilterSortModel *filter,UiController *ui,
                        UiSettings *settings,QString root,QObject *parent,std::function<QString()> steamRootSource,std::function<bool()> steamRunningProbe)
    :QObject(parent),games_(games),filter_(filter),ui_(ui),settings_(settings),root_(std::move(root)),
      scanner_(games,scanOptions(root_)),art_(std::make_shared<art::Resolver>(games->catalog(),art::ArtOptions{root_+"/user",{}})) {
    steamRootSource_=steamRootSource?std::move(steamRootSource):[] { return launch::systemRuntimeInputs().steamPath; };
    steamRunningProbe_=std::move(steamRunningProbe);
    gunRoomModels_=gunroom::models(games_->catalog().root);
    connect(&gunRoom_,&gunroom::Service::changed,this,[this]{
        if(gunRoom_.closing())ui_->launchClosing();
        emit playingChanged();
        emit gunRoomChanged();
    });
    connect(&gunRoom_,&gunroom::Service::started,this,[this]{childPlaying_=true;emit playingChanged();ui_->launchStarted("__gun_room");});
    connect(&gunRoom_,&gunroom::Service::finished,this,[this](const QString &error){
        childPlaying_=false;emit playingChanged();
        // Shutdown consumes a pending restart before notifying the shared card.
        if(hubExitPending_){ui_->cancelPendingRestart();ui_->launchFinished("__gun_room",error);emit hubExitReady(hubExitCode_);return;}
        ui_->launchFinished("__gun_room",error);if(!error.isEmpty())ui_->showError("Gun room",error);
    });
    installer_.setRuntimeStateSource([this](const QString &id){return current(id);});
    connect(&launcher_,&launch::LaunchService::warning,ui_,[this](const QString &,const QString &text){ui_->showError("Launch notice",text);});
    launcher_.setRuntimeStateSource([this](const QString &id){return current(id);});
    // Only journal-proven finished quarantine runs may be swept, off the GUI thread.
    auto *cleanup = new QFutureWatcher<QStringList>(this);
    connect(cleanup,&QFutureWatcher<QStringList>::finished,this,[this,cleanup]{
        const auto warnings=cleanup->result();cleanup->deleteLater();
        if(!warnings.isEmpty())ui_->showError("Cleanup notice",warnings.join("\n"));
    });
    cleanup->setFuture(QtConcurrent::run([root=root_]{
        try{return install::Engine().sweepFinishedMediaRemovals(root);}
        catch(const std::exception &){return QStringList{"Finished media cleanup could not complete; retained files will be retried at the next startup."};}
    }));
    connect(ui_,&UiController::writeConfigRequested,this,[this](const QString &id,const QVariantMap &){ui_->settingsSaved(id);});
    connect(ui_,&UiController::pinballScanRequested,this,[this](const QString &folder,const QString &database,const QString &roms,const QString &media){
        auto *watcher=new QFutureWatcher<QVariantMap>(this);
        connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,folder,database,roms,media]{
            const auto result=watcher->result();watcher->deleteLater();
            if(result.contains("error")){ui_->pinballFailed(result["error"].toString());return;}
            // Kept in Hub's own data so the tab opens at once next time; a table is still rechecked when it is started.
            try{install::atomicWrite(install::scopedPath("user/cache/pinball-scan.json",root_),QJsonDocument::fromVariant(QVariantMap{{"folder",folder},{"database",database},{"roms",roms},{"media",media},{"result",result}}).toJson(QJsonDocument::Compact));}
            catch(const std::exception &){ui_->showError("Pinball","The scan could not be saved; the next start scans again.");}
            ui_->pinballLoaded(result);
        });
        watcher->setFuture(QtConcurrent::run([folder,database,roms,media]{try{return pinball::scan(folder,database,roms,media);}catch(const std::exception &e){return QVariantMap{{"error",QString::fromUtf8(e.what())}};}}));
    });
    connect(ui_,&UiController::pinballRestoreRequested,this,[this](const QString &folder,const QString &database,const QString &roms,const QString &media){
        QVariantMap saved;
        try{QFile file(install::scopedPath("user/cache/pinball-scan.json",root_));if(file.size()<=64*1024*1024&&file.open(QIODevice::ReadOnly))saved=QJsonDocument::fromJson(file.readAll()).toVariant().toMap();}catch(const std::exception &){}
        const auto result=saved.value("result").toMap();
        if(saved.value("folder")==folder&&saved.value("database")==database&&saved.value("roms")==roms&&saved.value("media")==media&&result.contains("tables")){
            ui_->pinballLoaded(result);ui_->showError("Pinball","Showing your last scan. Use Scan after adding or changing tables.");
        }else ui_->scanPinball();
    });
    connect(ui_,&UiController::pinballPlanRequested,this,[this](const QString &table,const QString &executable,const QString &){
        try{const auto plan=pinball::launchPlan(table,executable,pinballVrRoot(),true);ui_->setPinballPlan(QString::fromUtf8(QJsonDocument::fromVariant(plan).toJson()));}catch(const std::exception &e){ui_->setPinballPlan(QString::fromUtf8(e.what()));}
    });
    connect(ui_,&UiController::pinballPlayRequested,this,[this](const QString &id){
        if(launchBusy()||hubExitPending_){ui_->showError("Pinball","Close the current session before playing a table.");return;}
        for(const auto &value:ui_->pinballData().value("tables").toList()){
            const auto row=value.toMap();if(row.value("id")==id){startPinball(row,settings_->get("pinballExecutable",QString()).toString());return;}
        }
        ui_->showError("Pinball","Table is no longer in the scan; scan again.");
    });
    connect(ui_,&UiController::model3PlayRequested,this,&HubServices::startModel3);
    // The Hub never registers itself unasked: the set-up page asks, and whoever owns the VR runtime call answers.
    connect(ui_,&UiController::steamVrRequested,this,[this]{emit steamVrRegistrationRequested(steamVrManifest());});
    refreshSteamVr();
    connect(ui_,&UiController::setupScanRequested,this,&HubServices::scanSetup);
    connect(ui_,&UiController::otherPlayRequested,this,&HubServices::startOtherGame);
    try{QFile saved(install::scopedPath("user/cache/setup-report.json",root_));if(saved.size()<=64*1024*1024&&saved.open(QIODevice::ReadOnly))ui_->setSetupReport(QJsonDocument::fromJson(saved.readAll()).toVariant().toMap());}catch(const std::exception &){}
    connect(ui_,&UiController::scanRequested,this,&HubServices::scan);
    connect(ui_,&UiController::cancelScanRequested,&scanner_,&scan::ScanController::cancel);
    connect(&scanner_,&scan::ScanController::scanStarted,ui_,&UiController::scanStarted);
    connect(&scanner_,&scan::ScanController::progress,ui_,&UiController::scanProgress);
    connect(&scanner_,&scan::ScanController::scanFinished,this,[this](bool cancelled){ui_->scanFinished(!cancelled);if(rescanPending_){rescanPending_=false;QTimer::singleShot(0,this,[this]{scan({});});}});
    connect(&scanner_,&scan::ScanController::scanFailed,ui_,[this](const QString &message){ui_->showError("Scan failed",message);});
    connect(&scanner_,&scan::ScanController::resultsReady,this,[this](const scan::ScanResult &result){
        bindings_=result.toJson(); art_->setBindings(result.bindings);
        auto roots=scanner_.options().artRoots+scanner_.options().mediaRoots;
        for(const auto &tool:result.tools)roots<<QFileInfo(tool.path).absolutePath();
        art_->setRoots(roots); filter_->setScanComplete(true);
    });
    connect(&scanner_,&scan::ScanController::artRevisionChanged,this,[this]{ui_->setArtRevision(static_cast<int>(scanner_.artRevision()%2147483647));});
    connect(&installer_,&install::InstallService::event,this,[this](QVariantMap event){if(event.value("kind")=="fail"){const auto context=retryContext();for(auto it=context.begin();it!=context.end();++it)event[it.key()]=it.value();}ui_->installEvent(event);});
    connect(&installer_,&install::InstallService::finished,this,[this](bool ok,const QString &message){ui_->installFinished(ok,message);if(ok)scan({});});
    connect(&installer_,&install::InstallService::runtimeStateReady,this,[this](const RuntimeState &state){if(games_->find(state.gameId))games_->applyRuntimeStates({state});});
    connect(ui_,&UiController::cancelInstallRequested,&installer_,&install::InstallService::cancel);
    connect(ui_,&UiController::installRequested,this,[this](const QString &id,const QString &variant){
        try{auto request=gameRequest(id,variant);startInstallRequest(request,"game");}catch(const std::exception &e){report(e);}
    });
    connect(ui_,&UiController::retryInstallRequested,this,[this](const QString &id,const QString &variant,bool,const QString &handover){
        try{
            const auto *game=games_->find(id);bool available=false;
            if(game)for(const auto &v:game->variants)if(v.id==variant&&(v.generated||authoredLaunchSupported(v)))available=true;
            if(!available||!game->install.contains("variant")||!game->install["variant"].contains(variant.toStdString())){ui_->scanProgress({{"text","Nothing to retry: the selected game and variant have no M1 owned installation recipe."}});return;}
            auto request=gameRequest(id,variant);request.handover=handover;
            startInstallRequest(request,"game");
        }catch(const std::exception &e){report(e);}
    });
    connect(ui_,&UiController::retryToolInstallRequested,this,[this](const QString &id,const QString &variant,bool,const QString &handover){
        const auto recovery=ui_->recovery();
        if(recovery.value("retryKind")!="tool"||recovery.value("retryOperation","install")!="install"||recovery.value("retryGameId")!=id||recovery.value("retryVariantId")!=variant){ui_->scanProgress({{"text","There is no matching failed tool installation to retry."}});return;}
        installTool(id,variant,handover);
    });
    connect(ui_,&UiController::uninstallPreviewRequested,this,[this](const QString &id,const QString &variant){
        try{auto request=gameRequest(id,variant);request.operation="uninstall";request.catalogRoot=catalogRoot();previewRemoval(request);}catch(const std::exception &e){report(e);}
    });
    connect(ui_,&UiController::playRequested,this,[this](const QString &id,const QString &variant){
        if(launchBusy()){ui_->scanProgress({{"text","Play refused: a game is already running or a launch is being prepared. Use Stop first."}});return;}
        if(id=="__gun_room"){
            if(variant!=retainedGunRoom_.model+"|"+retainedGunRoom_.hand+(retainedGunRoom_.calibrateGrip?"|calibrate":"")){ui_->showError("Gun room","The retained model, hand and action differ from this restart request.");return;}
            startGunRoom(retainedGunRoom_);return;
        }
        if(id.startsWith("pinball-")){
            if(id!="pinball-"+retainedPinball_.value("id").toString()||variant!="vpx"){ui_->showError("Pinball","Restart does not match the retained table.");return;}
            startPinball(retainedPinball_,retainedPinballExecutable_);return;
        }
        if(id.startsWith("model3-")){startModel3(id.mid(7),variant);return;}
        if(id.startsWith("dreamcast-")||id.startsWith("system22-")||id.startsWith("model2-")){ui_->chooseGameView(id,variant);ui_->playOtherGame(id);return;}
        try{
            const auto request=launch::gameRequest(games_->catalog(),id,root_,bindings_,variant);
#ifdef Q_OS_WIN
            ui_->launchQueued(id,variant,QString());
#else
            ui_->launchQueued(id,variant,"Clean window close is not supported on this platform yet.");
#endif
            launcher_.start(request);
        }catch(const std::exception &e){ui_->showError("Launch",QString::fromUtf8(e.what()));}
    });
    connect(ui_,&UiController::stopLaunchRequested,this,[this]{if(gunRoom_.busy())gunRoom_.close();else launcher_.stop();});
    connect(ui_,&UiController::closeLaunchRequested,this,[this]{if(gunRoom_.busy())gunRoom_.close();else launcher_.requestClose();});
    connect(&launcher_,&launch::LaunchService::starting,ui_,&UiController::launchPreparing);
    connect(&launcher_,&launch::LaunchService::started,this,[this](const QString &id){childPlaying_=true;emit playingChanged();ui_->launchStarted(id);});
    connect(&launcher_,&launch::LaunchService::finished,this,[this](const QString &id,int code,const QString &error,const QString &){
        childPlaying_=false;emit playingChanged();
        // A Hub exit waiting on this game goes ahead only now that the game has finished by itself.
        if(hubExitPending_){ui_->cancelPendingRestart();ui_->launchFinished(id,error,code);finishPendingExit();return;}
        ui_->launchFinished(id,error,code);if(!error.isEmpty()&&!launcher_.busy())emit raiseHubRequested();
    });
    connect(&launcher_,&launch::LaunchService::runtimeStateReady,this,[this](const RuntimeState &state){games_->applyRuntimeStates({state});});
    connect(&launcher_,&launch::LaunchService::playingChanged,this,&HubServices::playingChanged);
    // A companion program can end after the main one; the exit waits for it too.
    connect(&launcher_,&launch::LaunchService::playingChanged,this,&HubServices::finishPendingExit);
    connect(&launcher_,&launch::LaunchService::raiseHubRequested,this,&HubServices::raiseHubRequested);
    connect(ui_,&UiController::locationRequested,this,[this](const QString &kind){
        QString path;if(kind=="log")path=root_+"/user/logs/install";else if(kind=="install")path=root_+"/emulators";else if(kind=="downloads")path=root_+"/user/cache";else return;
        try{path=install::scopedPath(path,root_);QDir().mkpath(path);QDesktopServices::openUrl(QUrl::fromLocalFile(path));}catch(const std::exception &e){report(e);}
    });
    restore();
}
bool HubServices::beginSteam(const QString &id,const QString &variantId,bool remove){
    pendingSteamRemoval_=false;steamPreviewReady_=false;steamText_.clear();steamAccounts_.clear();steamRequest_={};steamRequest_.remove=remove;
    try{
        const auto *game=games_->find(id);if(!game)throw install::Error("E_GAME_INVALID","Unknown game");
        if(!remove){
            steamRequest_.shortcut=steam::prepareGameShortcut(games_->catalog(),id,variantId,root_,QCoreApplication::applicationFilePath(),bindings_);
        }
        steamRequest_.steamRoot=steamRootSource_();
        steamRequest_.userRoot=install::scopedPath("user",root_);
        if(remove)steamRequest_.shortcut={id,game->roles.value("title").toString(),QCoreApplication::applicationFilePath(),root_,false,0,game->runtime.lastPlayed,variantId};
        if(!remove){
        steamRequest_.art["header"]=art_->resolve(id,"banner",{460,215}).image;
        steamRequest_.art["capsule"]=art_->resolve(id,"portrait",{600,900}).image;
        steamRequest_.art["hero"]=art_->resolve(id,"hero",{1920,620}).image;
        steamRequest_.art["logo"]=art_->resolve(id,"logo",{640,360}).image;
        }
        steamAccounts_=steam::accounts(steamRequest_.steamRoot);
        if(steamAccounts_.isEmpty())throw install::Error("E_STEAM_ACCOUNT","No Steam userdata shortcut folder found");
        steamText_=remove?"Select an account folder, then preview removal of the Hub-owned shortcut and art. Close Steam yourself before approving removal.":"Select an account folder, then preview the exact shortcut and art changes. Close Steam before saving.";
    }catch(const std::exception &e){steamAccounts_.clear();steamText_=QString::fromUtf8(e.what());ui_->showError("Steam",steamText_);}
    emit steamPreviewChanged();
    return !steamAccounts_.isEmpty();
}
void HubServices::previewSteam(const QString &account){
    steamPreviewReady_=false;
    try{
        if(!steamAccounts_.contains(account))throw install::Error("E_STEAM_ACCOUNT","Select a listed account folder");
        steamRequest_.accountId=account;steamPreview_=steam::preview(steamRequest_);
        steamText_=QString::fromStdString(steamPreview_.json.dump(2));steamPreviewReady_=true;
    }catch(const std::exception &e){steamText_=QString::fromUtf8(e.what());ui_->showError("Steam",steamText_);}
    emit steamPreviewChanged();
}
void HubServices::approveSteamWrite(){
    if(!steamPreviewReady_)return;
    if(pendingSteamRemoval_&&installer_.busy()){ui_->scanProgress({{"text","An installation is already running. Wait before approving Steam removal."}});return;}
    try{
        const auto result=steam::apply(steamRequest_,steamPreview_,true,steamRunningProbe_);
        steamText_=(steamRequest_.remove?"Steam shortcut removal reviewed and applied. ":"Steam shortcut saved. ")+result.warnings.join("\n");steamPreviewReady_=false;
        if(pendingSteamRemoval_&&steamRequest_.remove&&pendingRemoval_.gameId==steamRequest_.shortcut.gameId&&pendingRemoval_.variantId==steamRequest_.shortcut.variantId){
            const auto request=pendingRemoval_;pendingRemoval_={};pendingSteamRemoval_=false;startInstallRequest(request,"game");
        }
    }catch(const std::exception &e){steamText_=QString::fromUtf8(e.what());steamPreviewReady_=false;ui_->showError("Steam",steamText_+(pendingSteamRemoval_?" Installation preserved; review Steam again or explicitly uninstall without Steam removal.":""));}
    emit steamPreviewChanged();
}
RuntimeState HubServices::current(const QString &id)const{auto game=games_->find(id);return game?game->runtime:RuntimeState{};}
void HubServices::report(const std::exception &e){ui_->showError("Hub",QString::fromUtf8(e.what()));}
void HubServices::scan(const QStringList &folders){
    if(scanner_.running()){rescanPending_=true;return;}
    QStringList roots=folders;
    if(roots.isEmpty())roots=scanner_.options().mediaRoots+scanner_.options().toolRoots+scanner_.options().artRoots;
    roots<<QDir(root_).filePath("emulators");roots.removeDuplicates();scanner_.scan(roots);
}
install::Request HubServices::gameRequest(const QString &id,const QString &variant){
    auto game=games_->find(id);if(!game)throw install::Error("E_GAME_INVALID","Unknown game");
    install::Request request;request.root=root_;request.catalogRoot=QDir::cleanPath(QFileInfo(game->folder).dir().absolutePath()+"/..");
    request.gameId=id;request.variantId=variant;request.recipe=game->install;request.runtime=current(id);request.runtime.selectedVariantId=variant;
    request.bindings=launch::normalizeBindings(bindings_,id);request.settings=Json::parse(QJsonDocument(QJsonObject::fromVariantMap(settings_->game(id))).toJson(QJsonDocument::Compact).toStdString());return request;
}
HubServices::~HubServices(){
    disconnect(&gunRoom_,nullptr,this,nullptr);
    gunRoom_.close();
}
void HubServices::setPinballDiagnosticIni(const QString &path){
    pinballDiagnosticIni_=path.isEmpty()?QString():privatePinballIni(path,root_);
}
void HubServices::startOtherGame(const QString &id,const QString &view,const QString &file){
    if(launchBusy()||hubExitPending_){ui_->showError("Launch","Close the current session before starting a game.");return;}
    try{
        launch::Request request;request.root=root_;request.gameId=id;request.variantId=view;request.prepareProfile=false;
        QString title,hint;
        if(id.startsWith("dreamcast-")){
            // The emulator keeps its settings beside itself, so its own folder is the working folder.
            const auto plan=dreamcast::launchPlan(file,view,settings_->get("dreamcastExecutable",QString()).toString());
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            for(const auto &value:dreamcast::games())if("dreamcast-"+value.toMap().value("id").toString()==id)title=value.toMap().value("title").toString()+" · "+dreamcast::viewLabel(view);
            hint=view=="headset"?"New and not yet tried in a headset. The picture is also on the PC monitor.":"The mouse is the gun.";
        }else if(id.startsWith("model2-")){
            // Two programs: the emulator first, then the viewer given the emulator's process id.
            const auto game=id.mid(7);
            const auto plan=model2::launchPlan(game,settings_->get("model2Executable",QString()).toString(),settings_->get("model2GameFolder",QString()).toString());
            const auto emulator=plan.value("companion").toMap();
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            request.companion.executable=emulator.value("executable").toString();request.companion.cwd=emulator.value("cwd").toString();request.companion.args=emulator.value("args").toStringList();
            auto &environment=request.runtimeInputs.environment;
            for(const auto &name:environment.keys())if(name.startsWith("ACVR_",Qt::CaseInsensitive))environment.remove(name);
            const auto variables=plan.value("env").toMap();
            for(auto it=variables.begin();it!=variables.end();++it)environment.insert(it.key(),it.value().toString());
            // The build's own runtime libraries, when the user's set-up names where they are.
            const auto libraries=settings_->get("model2RuntimePath",QString()).toString();
            if(!libraries.isEmpty())environment.insert("PATH",QDir::toNativeSeparators(libraries)+QDir::listSeparator()+environment.value("PATH"));
            for(const auto &value:model2::games())if(value.toMap().value("id")==game){title=value.toMap().value("title").toString()+" · 3D screen";hint=value.toMap().value("controls").toString();}
            hint+=" Home = put the screen in front of you again. Use Stop game here to end it.";
        }else if(id=="system22-timecris"){
            const QFileInfo program(settings_->get("system22Executable",QString()).toString());
            if(!program.isFile())throw install::Error("E_SYSTEM22_SETUP","The Time Crisis VR build is not set up. Choose its program in Set-up.");
            request.plan.executable=QDir::toNativeSeparators(program.absoluteFilePath());request.plan.cwd=QDir::toNativeSeparators(program.absolutePath());
            title="Time Crisis · In the headset";hint="A = credits, either trigger = start and fire, hold a grip to leave cover, X = recentre. Close its window on the PC to quit.";
        }else throw install::Error("E_ID","Unknown game");
#ifdef Q_OS_WIN
        ui_->launchOtherQueued(id,view,title,hint,QString());
#else
        ui_->launchOtherQueued(id,view,title,hint,"Clean window close is not supported on this platform yet.");
#endif
        launcher_.start(request);
    }catch(const std::exception &error){ui_->showError("Launch",QString::fromUtf8(error.what()));}
}
void HubServices::scanSetup(const QStringList &systems){
    // Off the GUI thread; only the user's folders are read, and only Hub's own cache is written.
    const auto values=settings_->values();const auto root=root_;const QPointer<UiController> ui=ui_;
    const QStringList pinballKey{values.value("pinballFolder").toString(),values.value("pinballDatabase").toString(),values.value("pinballRoms").toString(),values.value("pinballMedia").toString()};
    auto *watcher=new QFutureWatcher<QVariantMap>(this);
    connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher]{const auto report=watcher->result();watcher->deleteLater();ui_->setSetupReport(report);if(!report.value("error").toString().isEmpty())ui_->showError("Set-up",report.value("error").toString());});
    watcher->setFuture(QtConcurrent::run([systems,values,root,ui,pinballKey]{
        QVariantMap report;QVariantList results;std::atomic_bool cancel=false;
        try{
            const auto cachePath=install::scopedPath("user/cache/pinball-scan.json",root);
            QVariantMap context;
            {QFile file(cachePath);if(file.size()<=64*1024*1024&&file.open(QIODevice::ReadOnly)){const auto saved=QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
                if(QStringList{saved.value("folder").toString(),saved.value("database").toString(),saved.value("roms").toString(),saved.value("media").toString()}==pinballKey)context["pinballScan"]=saved.value("result");}}
            for(const auto &id:systems){
                QString title=id;for(const auto &value:setup::systems())if(value.toMap().value("id")==id)title=value.toMap().value("title").toString();
                const auto progress=[ui,title](const QString &text){if(ui)QMetaObject::invokeMethod(ui.data(),"setupScanProgress",Qt::QueuedConnection,Q_ARG(QString,title+": "+text));};
                auto result=setup::scanSystem(id,values,context,progress,cancel);
                const auto tables=result.take("pinballScan").toMap();
                if(tables.contains("tables")&&!context.contains("pinballScan"))install::atomicWrite(cachePath,QJsonDocument::fromVariant(QVariantMap{{"folder",pinballKey[0]},{"database",pinballKey[1]},{"roms",pinballKey[2]},{"media",pinballKey[3]},{"result",tables}}).toJson(QJsonDocument::Compact));
                results<<result;
            }
            report={{"scanned",QDateTime::currentDateTime().toString("d MMMM yyyy, HH:mm")},{"systems",results}};
            install::atomicWrite(install::scopedPath("user/cache/setup-report.json",root),QJsonDocument::fromVariant(report).toJson(QJsonDocument::Compact));
        }catch(const std::exception &e){report["systems"]=results;report["error"]=QString::fromUtf8(e.what());}
        return report;
    }));
}
QString HubServices::steamVrManifest()const{return QDir(QCoreApplication::applicationDirPath()).filePath(QString("resources/")+product::short_id+".vrmanifest");}
void HubServices::steamVrAnswered(const QString &error){refreshSteamVr(error);}
void HubServices::refreshSteamVr(const QString &error){
    // Read from SteamVR's own application list; AC_STEAMVR_APPCONFIG names another file for diagnostics.
    auto list=qEnvironmentVariable("AC_STEAMVR_APPCONFIG");const auto steam=steamRootSource_();
    if(list.isEmpty()&&!steam.isEmpty())list=QDir(steam).filePath("config/appconfig.json");
    const auto state=list.isEmpty()?"no-steam":setup::steamVrLists(list,steamVrManifest())?"added":"absent";
    ui_->setSteamVr({{"state",QString::fromLatin1(state)},{"error",error}});
}
QString HubServices::pinballVrRoot()const{return settings_->get("pinballHeadset",true).toBool()?root_:QString();}
void HubServices::startPinball(const QVariantMap &row,const QString &executable){
    if(launchBusy()||hubExitPending_){ui_->showError("Pinball","Close the current session before playing a table.");return;}
    try{
        const auto id=row.value("id").toString(),table=row.value("path").toString();const QFileInfo file(table);
        const auto identity=QString::fromLatin1(QCryptographicHash::hash(file.absoluteFilePath().toUtf8(),QCryptographicHash::Sha256).toHex());
        if(!QRegularExpression("^[a-f0-9]{64}$").match(id).hasMatch()||id!=identity||!file.isFile()||
           file.size()!=row.value("size",-1).toLongLong()||file.lastModified().toMSecsSinceEpoch()!=row.value("mtime",-1).toLongLong())
            throw install::Error("E_PINBALL_CHANGED","Table changed or is no longer available; scan again.");
        if(!row.value("vrReady").toBool()||!row.value("error").toString().isEmpty()||row.value("romState")=="ROM missing")
            throw install::Error("E_PINBALL_NOT_READY","Choose a VR-ready table with no scan error or missing known ROM.");
        // Headset on is asked for explicitly, through a Hub-owned settings file for this table.
        // A diagnostic settings file chooses its own mode, so nothing is layered over it.
        const auto plan=pinball::launchPlan(table,executable,pinballDiagnosticIni_.isEmpty()?pinballVrRoot():QString(),true);
        pinball::writeVrSettings(plan);
        launch::Request request;request.root=root_;request.gameId="pinball-"+id;request.variantId="vpx";request.prepareProfile=false;
        request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
        if(!pinballDiagnosticIni_.isEmpty())request.plan.args<<"-Ini"<<privatePinballIni(pinballDiagnosticIni_,root_);
        retainedPinball_=row;retainedPinballExecutable_=request.plan.executable;
#ifdef Q_OS_WIN
        ui_->launchPinballQueued(row,QString());
#else
        ui_->launchPinballQueued(row,"Clean window close is not supported on this platform yet.");
#endif
        launcher_.start(request);
    }catch(const std::exception &error){ui_->showError("Pinball",QString::fromUtf8(error.what()));}
}
void HubServices::startModel3(const QString &id,const QString &view){
    if(launchBusy()||hubExitPending_){ui_->showError("Model 3","Close the current session before starting a game.");return;}
    try{
        // The same program, working folder, arguments and variables as the user's
        // own launcher files. Nothing is written beside the emulator or game files.
        const auto plan=model3::launchPlan(id,view,settings_->get("model3Executable",QString()).toString(),settings_->get("model3GameFolder",QString()).toString());
        launch::Request request;request.root=root_;request.gameId="model3-"+id;request.variantId=view;request.prepareProfile=false;
        request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
        auto &environment=request.runtimeInputs.environment;
        for(const auto &name:environment.keys())if(name.startsWith("ACVR_",Qt::CaseInsensitive))environment.remove(name);
        const auto variables=plan.value("env").toMap();
        for(auto it=variables.begin();it!=variables.end();++it)environment.insert(it.key(),it.value().toString());
#ifdef Q_OS_WIN
        ui_->launchModel3Queued(id,view,QString());
#else
        ui_->launchModel3Queued(id,view,"Clean window close is not supported on this platform yet.");
#endif
        launcher_.start(request);
    }catch(const std::exception &error){ui_->showError("Model 3",QString::fromUtf8(error.what()));}
}
gunroom::Inputs HubServices::gunRoomInputs(const QString &model,const QString &hand,const QString &executable) const{
    auto inputs=gunroom::systemInputs(games_->catalog().root,executable,model,hand);
    const auto loader=settings_->get("gunRoomLoader",QString()).toString();
    if(!loader.isEmpty())inputs.loader=loader;
    return inputs;
}
void HubServices::previewGunRoom(const QString &model,const QString &hand,const QString &executable){
    const auto candidate=gunroom::plan(gunRoomInputs(model,hand,executable));
    gunRoomAvailable_=candidate.available()&&!launchBusy()&&!hubExitPending_;
    gunRoomReason_=candidate.error;
    if(candidate.available()&&!gunRoomAvailable_)gunRoomReason_="Close the current session before opening the gun room.";
    gunRoomCalibrationAvailable_=gunRoomAvailable_&&candidate.calibrationCapable;
    gunRoomCalibrationReason_=!gunRoomAvailable_?gunRoomReason_:candidate.calibrationCapable?QString():
        QString("This reviewed build does not admit grip calibration. Normal room launch is available.");
    emit gunRoomChanged();
}
void HubServices::launchGunRoom(const QString &model,const QString &hand,const QString &executable){
    const auto inputs=gunRoomInputs(model,hand,executable);
    startGunRoom(inputs);
}
void HubServices::calibrateGunRoom(const QString &model,const QString &hand,const QString &executable){
    auto inputs=gunRoomInputs(model,hand,executable);inputs.calibrateGrip=true;
    startGunRoom(inputs);
}
void HubServices::startGunRoom(const gunroom::Inputs &inputs){
    if(launchBusy()||hubExitPending_){ui_->showError("Gun room","Close the current session before opening the gun room.");return;}
    // Revalidate the selected paths and frozen bytes at the actual click/restart.
    const auto candidate=gunroom::plan(inputs);
    if(!candidate.available()){gunRoomAvailable_=false;gunRoomCalibrationAvailable_=false;gunRoomReason_=candidate.error;gunRoomCalibrationReason_=candidate.error;emit gunRoomChanged();ui_->showError("Gun room",candidate.error);return;}
    retainedGunRoom_=inputs;
    ui_->launchRoomQueued(inputs.model,inputs.hand,inputs.calibrateGrip);
    if(!gunRoom_.start(candidate))ui_->launchFinished("__gun_room","Gun-room process could not be queued.");
}
void HubServices::requestHubExit(int code){
    if(launcher_.busy()&&ui_->nowPlaying().contains("pinballId")){
#ifdef Q_OS_WIN
        hubExitPending_=true;hubExitCode_=code;ui_->cancelPendingRestart();ui_->launchClosing();launcher_.requestClose();
#else
        ui_->showError("Pinball","Exit the table in VPX before closing Hub on this platform.");
#endif
        return;
    }
    if(launcher_.busy()){
        // Any other game keeps its lease: the Hub exits once the game has ended by itself, so a slow
        // save or close dialog is never cut short. Ending it now is the explicit Stop action, not this.
        hubExitPending_=true;hubExitCode_=code;ui_->cancelPendingRestart();
        ui_->showError("Closing","The Hub closes when the running game has ended. Use Stop game to end it now.");
        return;
    }
    if(!gunRoom_.busy()){emit hubExitReady(code);return;}
    hubExitPending_=true;hubExitCode_=code;ui_->cancelPendingRestart();gunRoom_.close();
}
void HubServices::finishPendingExit(){
    if(!hubExitPending_||launcher_.busy()||gunRoom_.busy())return;
    hubExitPending_=false;emit hubExitReady(hubExitCode_);
}
QString HubServices::catalogRoot()const{
    if(games_->records().isEmpty())throw install::Error("E_CATALOG_EMPTY","Catalog has no games. Load a valid catalog before installing or removing emulator tools.");
    return QDir::cleanPath(QFileInfo(games_->records().first().folder).dir().absolutePath()+"/..");
}
QString HubServices::toolPlan()const{
    try{const auto catalog=catalogRoot();auto request=install::Engine::emulatorRequest(root_,games_->catalog().emulators.at("supermodel"));request.catalogRoot=catalog;return install::Engine().plan(request).text;}
    catch(const std::exception &e){return QString::fromUtf8(e.what());}
}
QVariantMap HubServices::retryContext()const{
    return {{"retryKind",activeInstallKind_},{"retryOperation",activeInstall_.operation},{"retryGameId",activeInstall_.gameId},{"retryVariantId",activeInstall_.variantId},{"retryLabel",activeInstallKind_=="tool"?"Supermodel":"Selected game"}};
}
void HubServices::startInstallRequest(const install::Request &request,const QString &kind){
    if(installer_.busy()){ui_->scanProgress({{"text","An installation is already running. Wait for it to finish before retrying."}});return;}
    activeInstall_=request;activeInstallKind_=kind;ui_->installStarted();installer_.start(request);
}
void HubServices::installTool(const QString &id,const QString &variant,const QString &handover){
    if(installer_.busy()){ui_->scanProgress({{"text","An installation is already running. Wait for it to finish before retrying."}});return;}
    activeInstall_={};activeInstall_.gameId=id;activeInstall_.variantId=variant;activeInstallKind_="tool";
    try{
        if(id!="tool-supermodel"||variant!="windows-x64")throw install::Error("E_TOOL_INVALID","Only the identified Supermodel M1 tool installation can be retried");
        const auto catalog=catalogRoot();auto request=install::Engine::emulatorRequest(root_,games_->catalog().emulators.at("supermodel"));request.catalogRoot=catalog;request.handover=handover;startInstallRequest(request,"tool");
    }catch(const std::exception &e){auto event=retryContext();event["kind"]="fail";event["text"]=QString::fromUtf8(e.what());ui_->installEvent(event);ui_->installFinished(false,QString::fromUtf8(e.what()));}
}
void HubServices::installSupermodel(){installTool("tool-supermodel","windows-x64");}
void HubServices::previewSupermodelRemoval(){
    try{install::Request request;request.root=root_;request.gameId="tool-supermodel";request.variantId="windows-x64";request.operation="uninstall";request.catalogRoot=catalogRoot();previewRemoval(request);}catch(const std::exception &e){report(e);}
}
void HubServices::previewRemoval(const install::Request &request){
    const bool hadSteamPreview=steamPreviewReady_;pendingRemoval_={};pendingSteamRemoval_=false;steamPreviewReady_=false;
    if(hadSteamPreview)emit steamPreviewChanged();
    try{const auto preview=QString::fromStdString(install::Engine().uninstallPreview(request).dump(2));pendingRemoval_=request;removalText_=preview;emit removalPlanChanged();}catch(const std::exception &e){report(e);}
}
bool HubServices::confirmRemoval(bool removeSteam){
    const auto request=pendingRemoval_;
    if(request.gameId.isEmpty())return false;
    if(installer_.busy()){ui_->scanProgress({{"text","An installation is already running. Wait before confirming removal."}});return false;}
    if(removeSteam){
        if(!games_->find(request.gameId)){ui_->scanProgress({{"text","Tool removal cannot remove a game Steam shortcut. Select the game and preview its owned shortcut separately."}});return false;}
        if(!beginSteam(request.gameId,request.variantId,true)){ui_->scanProgress({{"text",steamText_}});return false;}
        pendingSteamRemoval_=true;ui_->scanProgress({{"text","Awaiting Steam review; installation preserved."}});return true;
    }
    pendingRemoval_={};pendingSteamRemoval_=false;if(steamPreviewReady_){steamPreviewReady_=false;emit steamPreviewChanged();}
    startInstallRequest(request,request.gameId.startsWith("tool-")?"tool":"game");return false;
}
void HubServices::restore(){
    try{
        bindings_=readJson(root_+"/user/cache/scan-bindings.json",root_);
        QVector<RuntimeState> states;QVector<scan::Binding> artBindings;
        const auto times=readJson(root_+"/user/last-played.json",root_);
        for(const auto &game:games_->records()){
            auto state=game.runtime;state.gameId=game.id;
            state.lastPlayed=times.value(game.id.toStdString(),qint64(0));
            for(const auto &b:bindings_.value("bindings",Json::array()))if(text(b,"gameId")==game.id){
                scan::Binding binding;binding.gameId=game.id;binding.requirementId=text(b,"requirementId");binding.path=text(b,"path");binding.identity=text(b,"identity");binding.proof=text(b,"proof");binding.verified=b.value("verified",false)&&scan::hasValidatedChdBounds(b,bindings_.value("files",Json::array()));if(binding.proof=="mame-header-crc"&&(!b.contains("bios")||!b["bios"].is_string()))binding.verified=false;binding.supportPaths=strings(b,"supportPaths");
                const QFileInfo currentFile(binding.path);
                if(!currentFile.isFile())binding.verified=false;
                for(const auto &file:bindings_.value("files",Json::array()))if(text(file,"path")==binding.path&&(file.value("size",qint64(-1))!=currentFile.size()||file.value("mtime",qint64(-1))!=currentFile.lastModified().toMSecsSinceEpoch()))binding.verified=false;
                const auto validSupport=[&](const QString &path){
                    const QFileInfo support(path);if(!support.isFile())return false;
                    for(const auto &file:bindings_.value("files",Json::array()))if(text(file,"path")==path&&(file.value("size",qint64(-1))!=support.size()||file.value("mtime",qint64(-1))!=support.lastModified().toMSecsSinceEpoch()))return false;
                    return true;
                };
                for(const auto &support:binding.supportPaths)if(!validSupport(support))binding.verified=false;
                for(const auto &support:b.value("supportRequirements",Json::array()))if(!validSupport(text(support,"sourcePath")))binding.verified=false;
                if(binding.verified)state.mediaFound<<binding.requirementId;
                artBindings<<binding;
            }
            for(const auto &tool:bindings_.value("tools",Json::array())) {
                const QFileInfo executable(text(tool,"path"));
                if(tool.value("verified",false)&&executable.isFile()&&(!tool.contains("size")||tool["size"].get<qint64>()==executable.size())&&(!tool.contains("mtime")||tool["mtime"].get<qint64>()==executable.lastModified().toMSecsSinceEpoch()))state.toolsOk<<text(tool,"id");
            }
            for(const auto &variant:game.variants)if(authoredLaunchSupported(variant)) {
                install::Request saved;saved.root=root_;saved.gameId=game.id;saved.variantId=variant.id;saved.recipe=game.install;saved.runtime=state;
                state=install::Engine().runtimeState(saved);
                for(const auto &installed:state.variants)
                    if(installed.id==variant.id&&installed.verified&&installed.manifestExists&&installed.installedWhenExists&&state.selectedVariantId.isEmpty())
                        state.selectedVariantId=variant.id;
            }
            states<<state;
        }
        games_->applyRuntimeStates(states);art_->setBindings(artBindings);
        art_->setRoots(scanner_.options().artRoots+scanner_.options().mediaRoots);
        filter_->setScanComplete(bindings_.contains("bindings"));
    }catch(const std::exception &e){ui_->scanProgress({{"text",QString("Saved scan needs refresh: ")+QString::fromUtf8(e.what())}});}
}
}
