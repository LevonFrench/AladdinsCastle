// SPDX-License-Identifier: GPL-3.0-only
#include "core/library/Library.h"
#include "core/wii/Wii.h"
#include "HubServices.h"
#include "core/install/Support.h"
#include "core/pinball/Pinball.h"
#include <QElapsedTimer>
#include "core/model3/Model3.h"
#include "core/setup/Setup.h"
#include "core/setup/Report.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/model2/Model2.h"
#include "core/ps2/Ps2.h"
#include "core/ps1/Ps1.h"
#include "core/n64/N64.h"
#include "core/saturn/Saturn.h"
#include "core/scripted/Scripted.h"
#include "core/system22/System22.h"
#include "core/pinball/Gamepad.h"
#include "core/pinball/Art.h"
#include <QJsonArray>
#include <algorithm>
#include <QThread>
#include <QScopeGuard>
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
#include <QProcess>
namespace ac {
namespace {
QString privatePinballIni(const QString &path,const QString &root) {
    if(!QDir::isAbsolutePath(path))throw install::Error("E_PINBALL_DIAGNOSTIC_INI","Use an absolute private settings path");
    const auto candidate=install::scopedPath(path,QDir(root).filePath(".local"));
    const QFileInfo file(candidate);
    if(!file.isFile()||!file.isReadable())throw install::Error("E_PINBALL_DIAGNOSTIC_INI","Private settings file is missing or unreadable");
    return candidate;
}
// The devices SDL sees, asked of a copy of the Hub run for that alone: the user's SDL library is never loaded
// into this one. Hidden, a few seconds at most, and only what a device list can hold is kept.
QVariantList probeGamepadsApart(const QString &sdl){
    if(!QFileInfo(sdl).isFile())return {};
    auto program=QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1(product::program_name));
#ifdef Q_OS_WIN
    program+=".exe";
#endif
    if(!QFileInfo(program).isFile())return {};
    QProcess child;child.setProgram(program);child.setArguments({"--probe-gamepads",sdl});
    child.setWorkingDirectory(QFileInfo(sdl).absolutePath());child.setStandardErrorFile(QProcess::nullDevice());child.start();
    if(!child.waitForStarted(3000))return {};
    if(!child.waitForFinished(8000)){child.kill();child.waitForFinished(2000);return {};}
    if(child.exitStatus()!=QProcess::NormalExit||child.exitCode()!=0)return {};
    QVariantList pads;
    for(const auto &value:QJsonDocument::fromJson(child.readAllStandardOutput().left(256*1024)).array()){
        const auto pad=value.toObject();if(!pad.value("uid").isString())continue;
        QVariantMap elements;const auto given=pad.value("elements").toObject();
        for(auto it=given.begin();it!=given.end();++it)if(it.value().isDouble())elements[it.key()]=it.value().toInt();
        pads<<QVariantMap{{"uid",pad.value("uid").toString().left(128)},{"name",pad.value("name").toString().left(128)},{"elements",elements}};
    }
    return pads;
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
        connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher]{
            auto result=watcher->result();watcher->deleteLater();
            if(result.contains("error")){ui_->pinballFailed(result["error"].toString());return;}
            const bool unsaved=result.take("cacheUnsaved").toBool();
            if(unsaved)ui_->showError("Pinball","The scan could not be saved; the next start scans again.");
            ui_->pinballLoaded(result);startPinballArt(result);
        });
        // The scan and its cache file are both written off the UI thread: a large library is many megabytes of JSON.
        const auto cachePath=[this]{try{return install::scopedPath("user/cache/pinball-scan.json",root_);}catch(const std::exception &){return QString();}}();
        watcher->setFuture(QtConcurrent::run([folder,database,roms,media,cachePath]{
            QVariantMap result;
            try{result=pinball::scan(folder,database,roms,media);}catch(const std::exception &e){return QVariantMap{{"error",QString::fromUtf8(e.what())}};}
            // Kept in Hub's own data so the tab opens at once next time; a table is still rechecked when it is started.
            bool saved=false;
            if(!cachePath.isEmpty())try{install::atomicWrite(cachePath,QJsonDocument::fromVariant(QVariantMap{{"folder",folder},{"database",database},{"roms",roms},{"media",media},{"result",result}}).toJson(QJsonDocument::Compact));saved=true;}catch(const std::exception &){}
            if(!saved)result["cacheUnsaved"]=true;
            return result;
        }));
    });
    connect(ui_,&UiController::pinballRestoreRequested,this,[this](const QString &folder,const QString &database,const QString &roms,const QString &media){
        // The saved scan can be tens of megabytes: it is read and parsed off the UI thread.
        const auto cachePath=[this]{try{return install::scopedPath("user/cache/pinball-scan.json",root_);}catch(const std::exception &){return QString();}}();
        auto *watcher=new QFutureWatcher<QVariantMap>(this);
        connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,folder,database,roms,media]{
            const auto saved=watcher->result();watcher->deleteLater();
            const auto result=saved.value("result").toMap();
            if(saved.value("folder")==folder&&saved.value("database")==database&&saved.value("roms")==roms&&saved.value("media")==media&&result.contains("tables")){
                ui_->pinballLoaded(result);startPinballArt(result);ui_->showError("Pinball","Showing your last scan. Use Scan after adding or changing tables.");
            }else ui_->scanPinball();
        });
        watcher->setFuture(QtConcurrent::run([cachePath]{
            QVariantMap saved;
            if(!cachePath.isEmpty()){QFile file(cachePath);if(file.size()<=64*1024*1024&&file.open(QIODevice::ReadOnly))saved=QJsonDocument::fromJson(file.readAll()).toVariant().toMap();}
            return saved;
        }));
    });
    connect(ui_,&UiController::pinballPlanRequested,this,[this](const QString &table,const QString &executable,const QString &){
        try{const auto plan=pinball::launchPlan(table,executable,root_,QString("headset"));ui_->setPinballPlan(QString::fromUtf8(QJsonDocument::fromVariant(plan).toJson()));}catch(const std::exception &e){ui_->setPinballPlan(QString::fromUtf8(e.what()));}
    });
    connect(ui_,&UiController::stopEverythingRequested,this,[this]{if(gunRoom_.busy())gunRoom_.close();launcher_.endEverything();});
    connect(ui_,&UiController::pinballPlayRequested,this,[this](const QString &id,const QString &view){
        if(launchBusy()||hubExitPending_){ui_->showError("Pinball","Close the current session before playing a table.");return;}
        for(const auto &value:ui_->pinballData().value("tables").toList()){
            const auto row=value.toMap();if(row.value("id")==id){
                const auto start=[this,row,view]{startPinball(row,settings_->get("pinballExecutable",QString()).toString(),view);};
                if(view=="headset")launchInHeadset(row.value("title").toString(),start);else start();
                return;}
        }
        ui_->showError("Pinball","Table is no longer in the scan; scan again.");
    });
    // Every Model 3 view is in the headset.
    connect(ui_,&UiController::model3PlayRequested,this,[this](const QString &id,const QString &view){
        QString title=id;for(const auto &value:ui_->model3Games())if(value.toMap().value("id")==id)title=value.toMap().value("title").toString();
        if(model3::isMonitorView(view)){startModel3(id,view);return;}
        launchInHeadset(title,[this,id,view]{startModel3(id,view);});
    });
    connect(ui_,&UiController::cancelSteamVrStartRequested,this,[this]{
        endSteamVrWait("Cancelled: nothing was started. SteamVR may still be opening; close it from its own window if you do not need it.",false);});
    steamVrStartWatch_.setInterval(500);
    connect(&steamVrStartWatch_,&QTimer::timeout,this,[this]{
        if(!pendingHeadset_){steamVrStartWatch_.stop();return;}
        const auto now=steamVrStartClock_.elapsed();
        if(vrServerRunning()){
            if(steamVrSeenAt_<0)steamVrSeenAt_=now;
            // Ready once it has been running a moment: its compositor and dashboard come up after the server.
            if(now-steamVrSeenAt_>=steamVrSettleMs_){auto launch=std::move(pendingHeadset_);endSteamVrWait({},false);ui_->setSteamVrRunning(true);launch();return;}
        }else steamVrSeenAt_=-1;
        if(now>=steamVrTimeoutMs_)endSteamVrWait(QString("SteamVR did not start within %1 seconds. Nothing was started; check that the headset is connected, then try again.").arg(steamVrTimeoutMs_/1000),true);
    });
    // The Hub never registers itself unasked: the set-up page asks, and whoever owns the VR runtime call answers.
    connect(ui_,&UiController::steamVrRequested,this,[this]{emit steamVrRegistrationRequested(steamVrManifest());});
    refreshSteamVr();
    // Whether SteamVR is running, for the Launch in headset buttons.
    steamVrWatch_.setInterval(3000);connect(&steamVrWatch_,&QTimer::timeout,this,[this]{ui_->setSteamVrRunning(vrServerRunning());});
    steamVrWatch_.start();QTimer::singleShot(0,this,[this]{ui_->setSteamVrRunning(vrServerRunning());});
    connect(ui_,&UiController::setupScanRequested,this,&HubServices::scanSetup);
    // Every game but a Dreamcast disc on the monitor runs in the headset.
    connect(ui_,&UiController::otherPlayRequested,this,[this](const QString &id,const QString &view,const QString &file){
        if((id.startsWith("dreamcast-")&&view=="monitor")||onMonitor(id,view)){startOtherGame(id,view,file);return;}
        QString title=id;for(const auto &value:ui_->otherGames())if(value.toMap().value("id")==id)title=value.toMap().value("title").toString();
        launchInHeadset(title,[this,id,view,file]{startOtherGame(id,view,file);});
    });
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
            startPinball(retainedPinball_,retainedPinballExecutable_,retainedPinballView_);return;
        }
        if(id.startsWith("model3-")){startModel3(id.mid(7),variant);return;}
        if(id.startsWith("dreamcast-")||id.startsWith("system22-")||id.startsWith("model2-")||id.startsWith("ps2-")||id.startsWith("ps1-")||id.startsWith("n64-")||id.startsWith("saturn-")||id.startsWith("wii-")||id.startsWith("mamegun-")||id.startsWith("gamecube-")){ui_->chooseGameView(id,variant);ui_->playOtherGame(id);return;}
        try{
            const auto request=launch::gameRequest(games_->catalog(),id,root_,bindings_,variant);
            const auto start=[this,request,id,variant]{
#ifdef Q_OS_WIN
                ui_->launchQueued(id,variant,QString());
#else
                ui_->launchQueued(id,variant,"Clean window close is not supported on this platform yet.");
#endif
                launcher_.start(request);
            };
            if(request.runtime=="steamvr"){const auto *record=games_->find(id);launchInHeadset(record?record->roles.value("title").toString():id,start);}
            else start();
        }catch(const std::exception &e){ui_->showError("Launch",QString::fromUtf8(e.what()));}
    });
    // A PS2 game has no window to close: both Exit and Stop ask it to quit through its stop file, and the Hub waits.
    connect(ui_,&UiController::stopLaunchRequested,this,&HubServices::stopRunning);
    connect(ui_,&UiController::closeLaunchRequested,this,[this]{if(gunRoom_.busy())gunRoom_.close();else if(!askPs2ToQuit()&&!askPs1ToQuit()&&!askModel2ToQuit(false))launcher_.requestClose();});
    // Artwork from the internet: read from the settings at each request, so off is off at once.
    artDownloader_=std::make_unique<art::Downloader>(settings_->userDir(),[this]{return settings_->get("artDownload",false).toBool();},
        [this]{return ui_->steamGridDbKey();},this);
    connect(ui_,&UiController::artworkRequested,this,[this](const QVariantList &rows){
        QVector<art::Target> targets;
        for(const auto &value:rows){const auto row=value.toMap();targets<<art::Target{row.value("id").toString(),row.value("system").toString(),row.value("title").toString(),row.value("names").toStringList()};}
        if(!artDownloader_->start(targets))ui_->setArtwork(false,artDownloader_->busy()?"Artwork is already being fetched.":"Artwork download is off. Turn it on in Settings first.");
    });
    connect(artDownloader_.get(),&art::Downloader::progress,this,[this](const QString &text){ui_->setArtwork(true,text);});
    connect(artDownloader_.get(),&art::Downloader::finished,this,[this](const QString &summary,const QStringList &){ui_->setArtwork(false,summary);});
    connect(settings_,&UiSettings::changed,this,[this]{scanLocalArt();applyWatchdogRules();});
    // The Game library: the last result from the cache, and a new search on request, off the GUI thread.
    try{QFile cached(install::scopedPath("user/cache/library.json",root_));if(cached.size()<=64*1024*1024&&cached.open(QIODevice::ReadOnly))ui_->setLibrary(QJsonDocument::fromJson(cached.readAll()).toVariant().toMap());}catch(const std::exception &){}
    connect(ui_,&UiController::libraryScanRequested,this,[this](const QStringList &roots){
        const auto root=root_;const QPointer<UiController> ui=ui_;
        auto *watcher=new QFutureWatcher<QVariantMap>(this);
        connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher]{
            const auto result=watcher->result();watcher->deleteLater();ui_->setLibrary(result);
            int games=0;for(const auto &system:result.value("systems").toMap())games+=system.toMap().value("games").toInt();
            ui_->showError("Game library",QString("%1 games found in %2 files, in %3 seconds.").arg(games).arg(result.value("files").toInt()).arg(result.value("seconds").toDouble(),0,'f',1));
        });
        watcher->setFuture(QtConcurrent::run([roots,root,ui]{
            std::atomic_bool cancel=false;
            const auto progress=[ui](const QString &text){if(ui)QMetaObject::invokeMethod(ui.data(),"libraryScanProgress",Qt::QueuedConnection,Q_ARG(QString,text));};
            const auto result=library::scan(roots,progress,cancel);
            try{install::atomicWrite(install::scopedPath("user/cache/library.json",root),QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));}catch(const std::exception &){}
            return result;
        }));
    });
    scanLocalArt();
    // The watchdog: on start and every few seconds, the set-up systems' programs outside any game.
    applyWatchdogRules();
    watchdog_.setGraceful([this](const QString &system,const QList<qint64> &pids){
        // The system's own way to quit first, then every window asked to close; the tree ends after.
        if(system=="ps2"){const auto stop=settings_->get("ps2StopFile",QString()).toString();if(!stop.isEmpty()){QFile file(stop);if(file.open(QIODevice::WriteOnly))file.write("stop\n");}}
        if((system=="ps1"||system=="n64"||system=="saturn")&&!ps1StopFile_.isEmpty()){QFile file(ps1StopFile_);if(file.open(QIODevice::WriteOnly))file.write("stop\n");}
        if(system=="model2"){
            // The stop files of the Hub's own recent launches, and the viewer's default beside itself
            // (model2-<emulator id>.stop) for one started some other way.
            QStringList stops;
            try{QFile list(install::scopedPath("user/runtime/model2-stop-files.txt",root_));if(list.size()<64*1024&&list.open(QIODevice::ReadOnly))stops=QString::fromUtf8(list.readAll()).split('\n',Qt::SkipEmptyParts);}catch(const std::exception &){}
            QString viewerFolder;QList<qint64> emulators;
            for(const auto &p:launch::listProcesses())if(pids.contains(p.pid)){
                if(p.name.startsWith("model2",Qt::CaseInsensitive))viewerFolder=QFileInfo(p.path).absolutePath();
                else if(p.name.startsWith("mame",Qt::CaseInsensitive))emulators<<p.pid;}
            if(!viewerFolder.isEmpty())for(const auto pid:emulators)stops<<viewerFolder+"/model2-"+QString::number(pid)+".stop";
            for(const auto &stop:stops){QFile file(stop);if(file.open(QIODevice::WriteOnly))file.write("stop\n");}
        }
        launch::closeProcessWindows(pids);
    });
    connect(&watchdog_,&launch::Watchdog::straysChanged,this,[this]{ui_->setWatchdog(watchdog_.strays(),watchdog_.trouble());});
    connect(&watchdog_,&launch::Watchdog::troubleChanged,this,[this]{ui_->setWatchdog(watchdog_.strays(),watchdog_.trouble());});
    connect(ui_,&UiController::endBackgroundRequested,&watchdog_,&launch::Watchdog::end);
    connect(ui_,&UiController::endAllBackgroundRequested,&watchdog_,&launch::Watchdog::endAll);
    connect(ui_,&UiController::bringGameToFrontRequested,this,[this]{if(!watchdog_.bringSessionToFront())ui_->showError("Game","The game has no window to bring to the front.");});
    connect(&watchdog_,&launch::Watchdog::errorReportSeen,this,[this](qint64 report,qint64 forPid,const QString &){
        // Only the running game's: a background program's report shows on the strip instead.
        if(!launcher_.busy()||crashReported_)return;
        const auto own=launcher_.sessionProcesses();bool ours=own.contains(forPid);
        if(!ours)for(const auto &p:launch::listProcesses())if(p.pid==forPid&&own.contains(p.parent))ours=true;
        if(!ours)return;
        crashReported_=true;
        ui_->showError("Game",(sessionTitle_.isEmpty()?QString("The game"):sessionTitle_)+" crashed; Windows Error Reporting opened for it. Ending it.");
        launch::endProcesses({report});
        QTimer::singleShot(1500,this,[this]{if(launcher_.busy())launcher_.endEverything();});
    });
    try{QFile records(install::scopedPath("user/logs/crashes.json",root_));if(records.size()<=4*1024*1024&&records.open(QIODevice::ReadOnly))ui_->setCrashRecords(QJsonDocument::fromJson(records.readAll()).array().toVariantList());}catch(const std::exception &){}
    QTimer::singleShot(0,this,[this]{watchdog_.start();});
    connect(&launcher_,&launch::LaunchService::starting,ui_,&UiController::launchPreparing);
    connect(&launcher_,&launch::LaunchService::starting,this,[this](const QString &id,const QString &){
        if(sessionSystem_.isEmpty()||!launcher_.playing()){sessionStartMs_=QDateTime::currentMSecsSinceEpoch()-1000;crashReported_=false;}
        static const QStringList systems{"pinball","model3","model2","ps2","ps1","n64","saturn","dreamcast","system22","wii","mamegun","gamecube"};
        sessionSystem_="catalog";for(const auto &system:systems)if(id.startsWith(system+"-")||id==system)sessionSystem_=system;
    });
    connect(&launcher_,&launch::LaunchService::started,this,[this](const QString &id){
        childPlaying_=true;emit playingChanged();ui_->launchStarted(id);
        sessionTitle_=ui_->nowPlaying().value("title",id).toString();
        // The watchdog follows the running game: its own processes, and whether its window is all right.
        watchdog_.beginSession({id,sessionTitle_,sessionSystem_,sessionStartMs_,sessionSystem_!="model2",[this]{return launcher_.sessionProcesses();}});
    });
    connect(&launcher_,&launch::LaunchService::finished,this,[this](const QString &id,int code,const QString &error,const QString &logPath){
        childPlaying_=false;emit playingChanged();
        // A crash: a crash exit code, or Windows Error Reporting opened for it while it ran.
        const bool crashCode=launch::isCrashCode(quint32(code)),crashed=crashCode||crashReported_;
        const auto how=crashCode?launch::exitCodeText(code):QString("Windows Error Reporting opened for it");
        const auto system=sessionSystem_;const auto since=sessionStartMs_;
        if(crashed)recordCrash(id,code,how,logPath);
        watchdog_.endSession();crashReported_=false;
        if(id.startsWith("model2-")&&!model2StopFile_.isEmpty()){QFile::remove(model2StopFile_);model2StopFile_.clear();}
        // What it left running outside its own tree ends with it.
        if(crashed)QTimer::singleShot(300,this,[this,system,since]{watchdog_.endSince(system,since);});
        const auto said=[this,id,crashed,how]{if(crashed)ui_->markCrash(id,(sessionTitle_.isEmpty()?QString("The game"):sessionTitle_)+" crashed: "+how+".");};
        // A Hub exit waiting on this game goes ahead only now that the game has finished by itself.
        if(hubExitPending_){ui_->cancelPendingRestart();ui_->launchFinished(id,error,code);said();finishPendingExit();return;}
        ui_->launchFinished(id,error,code);said();if(!error.isEmpty()&&!launcher_.busy())emit raiseHubRequested();
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
    roots<<QDir(root_).filePath("emulators");roots.removeDuplicates();
    scanner_.setQueryTools(settings_->get("queryEmulators",false).toBool());scanner_.scan(roots);
}
install::Request HubServices::gameRequest(const QString &id,const QString &variant){
    auto game=games_->find(id);if(!game)throw install::Error("E_GAME_INVALID","Unknown game");
    install::Request request;request.root=root_;request.catalogRoot=QDir::cleanPath(QFileInfo(game->folder).dir().absolutePath()+"/..");
    request.gameId=id;request.variantId=variant;request.recipe=game->install;request.runtime=current(id);request.runtime.selectedVariantId=variant;
    request.bindings=launch::normalizeBindings(bindings_,id);request.settings=Json::parse(QJsonDocument(QJsonObject::fromVariantMap(settings_->game(id))).toJson(QJsonDocument::Compact).toStdString());return request;
}
HubServices::~HubServices(){
    if(artCancel_)*artCancel_=true;
    if(localArtCancel_)localArtCancel_->store(true);
    artPool_.waitForDone();
    disconnect(&gunRoom_,nullptr,this,nullptr);
    gunRoom_.close();
}
void HubServices::setPinballDiagnosticIni(const QString &path){
    pinballDiagnosticIni_=path.isEmpty()?QString():privatePinballIni(path,root_);
}
void HubServices::applyDiagnosticChild(launch::Request &request) const{
    for(const auto &pair:diagnosticEnv_.split(';',Qt::SkipEmptyParts)){
        const auto name=pair.section('=',0,0).trimmed(),value=pair.section('=',1);
        if(name.isEmpty())continue;
        if(value.isEmpty())request.runtimeInputs.environment.remove(name);else request.runtimeInputs.environment.insert(name,value);
    }
    for(const auto &argument:diagnosticArgs_.split(';',Qt::SkipEmptyParts))request.plan.args<<argument;
}
void HubServices::startForDiagnostic(const QString &id,const QString &view){
    if(id.startsWith("model3-")){startModel3(id.mid(7),view);return;}
    for(const auto &value:ui_->otherGames()){const auto row=value.toMap();if(row.value("id")==id){startOtherGame(id,view,row.value("file").toString());return;}}
    ui_->showError("Launch","This game is not in the VR games list on this PC.");
}
bool HubServices::onMonitor(const QString &id,const QString &view)const{
    for(const auto &value:ui_->otherGames()){const auto row=value.toMap();if(row.value("id")!=id)continue;
        for(const auto &v:row.value("views").toList())if(v.toMap().value("id")==view)return v.toMap().value("monitor").toBool();}
    return false;
}
void HubServices::startOtherGame(const QString &id,const QString &view,const QString &file){
    if(launchBusy()||hubExitPending_){ui_->showError("Launch","Close the current session before starting a game.");return;}
    try{
        launch::Request request;request.root=root_;request.gameId=id;request.variantId=view;request.prepareProfile=false;
        QString title,hint,closeNote;
        bool monitor=false;
        QVariantMap row;for(const auto &value:ui_->otherGames())if(value.toMap().value("id")==id)row=value.toMap();
        if(!row.value("launcher").toString().isEmpty()){
            // A game of the user's own launcher (arcade guns, Dreamcast and Atomiswave, Wii, GameCube): its key and view.
            const auto system=id.section('-',0,0),key=id.mid(system.size()+1),launcher=row.value("launcher").toString();
            const auto plan=scripted::launchPlan(launcher,key,view);
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            QVariantMap chosen;for(const auto &value:row.value("views").toList())if(value.toMap().value("id")==view)chosen=value.toMap();
            monitor=scripted::isMonitor(chosen);
            title=row.value("title").toString()+" · "+chosen.value("label").toString();
            hint=chosen.value("controls").toString();
            if(!monitor&&!(vrServerProbe_?vrServerProbe_():launch::processRunning({"vrserver.exe","vrserver"})))
                throw install::Error("E_STEAMVR_NOT_RUNNING","Start SteamVR first, with the headset connected, then start the game again.");
            if(system=="mamegun"&&monitor)ps1StopFile_.clear(); // its own program on the monitor: closed by its window
            if(system=="mamegun"&&!monitor){
                // A stop file of the Hub's own for this launch; MAME saves and quits when it appears.
                ps1StopFile_=install::scopedPath("user/runtime/mamegun-"+key+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+".stop",root_);
                if(!QDir().mkpath(QFileInfo(ps1StopFile_).absolutePath()))throw install::Error("E_WRITE_DENIED","Cannot create the Hub's runtime folder");
                if(QFileInfo::exists(ps1StopFile_)&&!QFile::remove(ps1StopFile_))throw install::Error("E_WRITE_DENIED","An old stop request could not be cleared");
                request.runtimeInputs.environment.insert("AC_HUB_STOP_FILE",QDir::toNativeSeparators(ps1StopFile_));
                if(!monitor)hint+=" Exit game here, or hold left Y for 3 seconds, quits and saves.";
            }
        }else if(id.startsWith("dreamcast-")){
            // The emulator keeps its settings beside itself, so its own folder is the working folder.
            const auto plan=dreamcast::launchPlan(file,view,settings_->get("dreamcastExecutable",QString()).toString());
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            for(const auto &value:dreamcast::games())if("dreamcast-"+value.toMap().value("id").toString()==id)title=value.toMap().value("title").toString()+" · "+dreamcast::viewLabel(view);
            hint=view=="headset"?"New and not yet tried in a headset. The picture is also on the PC monitor.":"The mouse is the gun.";
        }else if(id.startsWith("wii-")){
            // The authored DolphinXR route: the build's own User folder, OpenXR on, the disc where it is.
            const auto plan=wii::launchPlan(file,settings_->get("wiiExecutable",QString()).toString());
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            for(const auto &value:wii::games())if("wii-"+value.toMap().value("id").toString()==id)title=value.toMap().value("title").toString()+" · In the headset";
            hint="Aim with your controller. Both thumbsticks pressed together, or Shift+R, recentre the view. Exit game here closes Dolphin.";
        }else if(id.startsWith("model2-")){
            // Two programs: the emulator first, then the viewer given the emulator's process id.
            const auto game=id.mid(7);
            const auto plan=model2::launchPlan(game,settings_->get("model2Executable",QString()).toString(),ui_->gameFolderFor("model2",game),view);
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
            for(const auto &value:model2::games())if(value.toMap().value("id")==game)title=value.toMap().value("title").toString()+" · "+model2::viewLabel(view);
            for(const auto &value:model2::views(game))if(value.toMap().value("id")==view)hint=value.toMap().value("controls").toString();
            hint+=" Home = put the screen in front of you again. Exit game here, or hold Y for 3 seconds, quits and saves.";
            // A stop file of the Hub's own for this launch, cleared first; the viewer watches for it.
            model2StopFile_=install::scopedPath("user/runtime/model2-"+game+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+".stop",root_);
            if(!QDir().mkpath(QFileInfo(model2StopFile_).absolutePath()))throw install::Error("E_WRITE_DENIED","Cannot create the Hub's runtime folder");
            if(QFileInfo::exists(model2StopFile_)&&!QFile::remove(model2StopFile_))throw install::Error("E_WRITE_DENIED","An old Model 2 stop request could not be cleared");
            environment.insert("ACVR_M2_STOP_FILE",QDir::toNativeSeparators(model2StopFile_));++model2Session_;
            {const auto list=install::scopedPath("user/runtime/model2-stop-files.txt",root_);QStringList known;
             {QFile in(list);if(in.size()<64*1024&&in.open(QIODevice::ReadOnly))known=QString::fromUtf8(in.readAll()).split('\n',Qt::SkipEmptyParts);} // Closed before it is replaced.
             known<<model2StopFile_;while(known.size()>20)known.removeFirst();install::atomicWrite(list,known.join('\n').toUtf8());}
        }else if(id.startsWith("ps2-")){
            // The launcher refuses without SteamVR; say so here instead of starting it to fail.
            if(!(vrServerProbe_?vrServerProbe_():launch::processRunning({"vrserver.exe","vrserver"})))
                throw install::Error("E_STEAMVR_NOT_RUNNING","Start SteamVR first, with the headset connected, then start the game again.");
            const auto game=id.mid(4);
            const auto plan=ps2::launchPlan(game,settings_->get("ps2Launcher",QString()).toString(),settings_->get("ps2Python",QString()).toString(),settings_->get("ps2StopFile",QString()).toString(),view);
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            for(const auto &value:ps2::games())if(value.toMap().value("id")==game)title=value.toMap().value("title").toString()+" · "+ps2::viewLabel(view);
            hint="Gamepad: Start, A = Cross, B = Circle. Home recentres the screen. Use Exit game here to quit: the game is asked to close and the Hub waits for it.";
        }else if(id.startsWith("ps1-")){
            // The launcher refuses without SteamVR; say so here instead of starting it to fail.
            if(!(vrServerProbe_?vrServerProbe_():launch::processRunning({"vrserver.exe","vrserver"})))
                throw install::Error("E_STEAMVR_NOT_RUNNING","Start SteamVR first, with the headset connected, then start the game again.");
            const auto game=id.mid(4);
            const auto plan=ps1::launchPlan(game,settings_->get("ps1Launcher",QString()).toString(),view);
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            auto &environment=request.runtimeInputs.environment;
            for(const auto &name:environment.keys())if(name.startsWith("ACVR_",Qt::CaseInsensitive))environment.remove(name);
            // A stop file of the Hub's own for this launch, cleared first; acvr-ps1 watches for it and saves on the way out.
            ps1StopFile_=install::scopedPath("user/runtime/ps1-"+game+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+".stop",root_);
            if(!QDir().mkpath(QFileInfo(ps1StopFile_).absolutePath()))throw install::Error("E_WRITE_DENIED","Cannot create the Hub's runtime folder");
            if(QFileInfo::exists(ps1StopFile_)&&!QFile::remove(ps1StopFile_))throw install::Error("E_WRITE_DENIED","An old PS1 stop request could not be cleared");
            environment.insert("ACVR_PS1_STOP_FILE",QDir::toNativeSeparators(ps1StopFile_));
            for(const auto &value:ps1::games())if(value.toMap().value("id")==game){title=value.toMap().value("title").toString()+" · "+ps1::viewLabel(game,view);hint=value.toMap().value("controls").toString();}
            hint+=" Home = put the screen in front of you again. Exit game here, or hold left Y for 3 seconds, quits and saves.";
        }else if(id.startsWith("n64-")){
            if(!(vrServerProbe_?vrServerProbe_():launch::processRunning({"vrserver.exe","vrserver"})))
                throw install::Error("E_STEAMVR_NOT_RUNNING","Start SteamVR first, with the headset connected, then start the game again.");
            const auto game=id.mid(4);
            const auto plan=n64::launchPlan(game,settings_->get("n64Launcher",QString()).toString(),view);
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            auto &environment=request.runtimeInputs.environment;
            for(const auto &name:environment.keys())if(name.startsWith("ACVR_",Qt::CaseInsensitive))environment.remove(name);
            // The same stop file as PS1: acvr-ps1 runs these too, and saves on the way out when it appears.
            ps1StopFile_=install::scopedPath("user/runtime/n64-"+game+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+".stop",root_);
            if(!QDir().mkpath(QFileInfo(ps1StopFile_).absolutePath()))throw install::Error("E_WRITE_DENIED","Cannot create the Hub's runtime folder");
            if(QFileInfo::exists(ps1StopFile_)&&!QFile::remove(ps1StopFile_))throw install::Error("E_WRITE_DENIED","An old N64 stop request could not be cleared");
            environment.insert("ACVR_PS1_STOP_FILE",QDir::toNativeSeparators(ps1StopFile_));
            for(const auto &value:n64::games())if(value.toMap().value("id")==game){title=value.toMap().value("title").toString()+" · "+n64::viewLabel(game,view);hint=value.toMap().value("controls").toString();}
            hint+=" Home = put the screen in front of you again. Exit game here quits and saves.";
        }else if(id.startsWith("saturn-")){
            if(!(vrServerProbe_?vrServerProbe_():launch::processRunning({"vrserver.exe","vrserver"})))
                throw install::Error("E_STEAMVR_NOT_RUNNING","Start SteamVR first, with the headset connected, then start the game again.");
            const auto game=id.mid(7);
            const auto plan=saturn::launchPlan(game,ui_->launcherFor("saturn"));
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            auto &environment=request.runtimeInputs.environment;
            for(const auto &name:environment.keys())if(name.startsWith("ACVR_",Qt::CaseInsensitive))environment.remove(name);
            // The same stop file as PS1: acvr-ps1 runs these too, and saves on the way out when it appears.
            ps1StopFile_=install::scopedPath("user/runtime/saturn-"+game+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+".stop",root_);
            if(!QDir().mkpath(QFileInfo(ps1StopFile_).absolutePath()))throw install::Error("E_WRITE_DENIED","Cannot create the Hub's runtime folder");
            if(QFileInfo::exists(ps1StopFile_)&&!QFile::remove(ps1StopFile_))throw install::Error("E_WRITE_DENIED","An old Saturn stop request could not be cleared");
            environment.insert("ACVR_PS1_STOP_FILE",QDir::toNativeSeparators(ps1StopFile_));
            for(const auto &value:saturn::games())if(value.toMap().value("id")==game){title=value.toMap().value("title").toString()+" · Theatre screen";hint=value.toMap().value("controls").toString();}
            hint+=" Home = put the screen in front of you again. Exit game here, or hold left Y for 3 seconds, quits and saves.";
        }else if(id=="system22-timecris"){
            const auto plan=system22::launchPlan(settings_->get("system22Executable",QString()).toString(),view);
            request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
            auto &environment=request.runtimeInputs.environment;
            for(const auto &name:environment.keys())if(name.startsWith("TCVR_",Qt::CaseInsensitive))environment.remove(name);
            const auto variables=plan.value("env").toMap();
            for(auto it=variables.begin();it!=variables.end();++it)environment.insert(it.key(),it.value().toString());
            monitor=view=="monitor";
            title="Time Crisis · "+system22::viewLabel(view);
            for(const auto &value:system22::views(settings_->get("system22Executable",QString()).toString()))if(value.toMap().value("id")==view)hint=value.toMap().value("controls").toString();
        }else throw install::Error("E_ID","Unknown game");
#ifdef Q_OS_WIN
        ui_->launchOtherQueued(id,view,title,hint,closeNote);
#else
        // Model 2 and PS2 are asked to quit through a stop file, never a window, so their Exit works here too.
        const bool stopFile=id.startsWith("model2-")||id.startsWith("ps2-");
        ui_->launchOtherQueued(id,view,title,hint,stopFile?closeNote:"Clean window close is not supported on this platform yet.");
#endif
        applyDiagnosticChild(request);
        request.pinSteamVrOpenXr=!(id.startsWith("dreamcast-")&&view=="monitor")&&!monitor; // Every one but a view on the monitor is in the headset.
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
            {QFile cached(install::scopedPath("user/cache/library.json",root));if(cached.size()<=64*1024*1024&&cached.open(QIODevice::ReadOnly))context["library"]=QJsonDocument::fromJson(cached.readAll()).toVariant().toMap();}
            context["libraryChoice"]=values.value("libraryChoice").toMap();
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
void HubServices::startPinballArt(const QVariantMap &scan){
    if(artCancel_)*artCancel_=true;
    // Only the pinball art job is replaced. The game artwork scan is a separate job on the global pool; cancelling
    // it here left it marked done for good, so artwork from the owner's folders never appeared.
    auto cancel=std::make_shared<std::atomic_bool>(false);artCancel_=cancel;
    QString cache;try{cache=install::scopedPath("user/cache/pinball-art/x",root_);cache=QFileInfo(cache).absolutePath();}catch(const std::exception &){return;}
    // Tables that can be started first, so the pictures that are seen come first.
    QVariantList first,later;
    for(const auto &value:scan.value("tables").toList()){const auto row=value.toMap();(row.value("error").toString().isEmpty()&&row.value("romState")!="ROM missing"&&row.value("romState")!="unknown"?first:later)<<row;}
    const auto rows=first+later;const QPointer<UiController> ui=ui_;
    artPool_.setMaxThreadCount(1);
    artPool_.start([rows,cache,cancel,ui]{
        QThread::currentThread()->setPriority(QThread::LowestPriority);
        // Each batch rebuilds the pinball page's lists on the UI thread, so batches go at most twice a second.
        QVariantMap batch;QElapsedTimer sinceSend;sinceSend.start();
        const auto send=[&]{if(batch.isEmpty()||!ui)return;QMetaObject::invokeMethod(ui.data(),[ui,batch]{if(ui)ui->setPinballArt(batch);},Qt::QueuedConnection);batch.clear();sinceSend.restart();};
        for(const auto &value:rows){
            if(*cancel)return;
            const auto row=value.toMap();
            try{const auto art=pinball::tableArt(row.value("path").toString(),row.value("id").toString(),cache,*cancel);if(!art.isEmpty())batch[row.value("id").toString()]=art;}catch(const std::exception &){}
            if(sinceSend.elapsed()>=500)send();
        }
        send();
    });
}
bool HubServices::vrServerRunning()const{return vrServerProbe_?vrServerProbe_():launch::processRunning({"vrserver.exe","vrserver"});}
void HubServices::startPinball(const QVariantMap &row,const QString &executable,const QString &view,const QVariantList *probed){
    if((launchBusy()&&!(probed&&preparing_))||hubExitPending_){ui_->showError("Pinball","Close the current session before playing a table.");return;}
    preparing_=false;
    try{
        const auto id=row.value("id").toString(),table=row.value("path").toString();const QFileInfo file(table);
        const auto identity=QString::fromLatin1(QCryptographicHash::hash(file.absoluteFilePath().toUtf8(),QCryptographicHash::Sha256).toHex());
        if(!QRegularExpression("^[a-f0-9]{64}$").match(id).hasMatch()||id!=identity||!file.isFile()||
           file.size()!=row.value("size",-1).toLongLong()||file.lastModified().toMSecsSinceEpoch()!=row.value("mtime",-1).toLongLong())
            throw install::Error("E_PINBALL_CHANGED","Table changed or is no longer available; scan again.");
        if(!row.value("error").toString().isEmpty()||row.value("romState")=="ROM missing")
            throw install::Error("E_PINBALL_NOT_READY","Choose a table with no scan error or missing known ROM.");
        if(view=="headset"&&!row.value("vrReady").toBool())
            throw install::Error("E_PINBALL_NOT_READY","This table is not marked VR-ready. Use Launch on desktop, or mark it VR-ready first.");
        // In the headset or on the monitor, asked for explicitly through a Hub-owned settings file for this table.
        // A diagnostic settings file chooses its own mode, so nothing is layered over it.
        if(view!="desktop"&&view!="headset")throw install::Error("E_PINBALL_VIEW","Choose Launch on desktop or Launch in headset.");
        // The headset needs SteamVR already running; the desktop never needs or starts it.
        if(view=="headset"&&!vrServerRunning())throw install::Error("E_STEAMVR_NOT_RUNNING","Start SteamVR first, with the headset connected, then use Launch in headset.");
        const bool diagnostic=!pinballDiagnosticIni_.isEmpty();
        // A gamepad gets the Hub's layout through a Hub-owned copy of the user's VPX settings, made again at
        // each start from the user's own file, which is only read. Without a gamepad VPX uses the user's file.
        QString appSettings,gamepad;
        if(!diagnostic){
            QString sdl;for(const auto &name:{"SDL364.dll","SDL3.dll"}){const QFileInfo candidate(QFileInfo(executable).absoluteDir().filePath(QString::fromLatin1(name)));if(sdl.isEmpty()&&candidate.isFile())sdl=candidate.absoluteFilePath();}
            if(!probed){
                // The gamepad probe loads SDL and asks the devices: off the GUI thread, then this start again.
                preparing_=true;emit playingChanged();
                const auto probe=gamepadProbe_;auto *watcher=new QFutureWatcher<QVariantList>(this);
                connect(watcher,&QFutureWatcher<QVariantList>::finished,this,[this,watcher,row,executable,view]{
                    const auto pads=watcher->result();watcher->deleteLater();startPinball(row,executable,view,&pads);emit playingChanged();});
                watcher->setFuture(QtConcurrent::run([probe,sdl]{return probe?probe(sdl):probeGamepadsApart(sdl);}));
                return;
            }
            const auto pads=*probed;
            // In the headset a single Y tap must not exit (holding Y in the Hub closes the table), so the copy is always made there.
            if(!pads.isEmpty()||view=="headset"){
                const auto own=pinball::userSettingsFile(executable);
                const QByteArray mine=own.isEmpty()||QFileInfo(own).size()>4*1024*1024?QByteArray():install::readBytes(own);
                appSettings=install::scopedPath("user/pinball/VPinballX.ini",root_);
                if(!QDir().mkpath(QFileInfo(appSettings).absolutePath()))throw install::Error("E_WRITE_DENIED","Cannot create the Hub's Pinball settings folder");
                auto settings=pinball::gamepadSettings(mine,pads);
                if(view=="headset")settings=pinball::exitOnChord(settings);
                install::atomicWrite(appSettings,settings);
                if(!pads.isEmpty())gamepad=pads.first().toMap().value("name").toString();
            }
        }
        const auto plan=pinball::launchPlan(table,executable,diagnostic?QString():root_,diagnostic?QString():view,appSettings);
        pinball::writeVrSettings(plan);
        launch::Request request;request.root=root_;request.gameId="pinball-"+id;request.variantId="vpx";request.prepareProfile=false;
        request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
        if(!pinballDiagnosticIni_.isEmpty())request.plan.args<<"-Ini"<<privatePinballIni(pinballDiagnosticIni_,root_);
        // Windows starts these for VPX outside its process tree; those started during the table end with it.
        request.helperPrograms={"B2SBackglassServerEXE.exe","B2SBackglassServer.exe","dmdext.exe","PinUpDisplay.exe","DOF2DMD.exe"};
        retainedPinball_=row;retainedPinballExecutable_=request.plan.executable;retainedPinballView_=view;
        request.pinSteamVrOpenXr=view=="headset"; // VPX's OpenXR build finds SteamVR's runtime, never another.
#ifdef Q_OS_WIN
        ui_->launchPinballQueued(row,QString(),diagnostic?QString():view,gamepad);
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
        const auto plan=model3::launchPlan(id,view,settings_->get("model3Executable",QString()).toString(),ui_->gameFolderFor("model3",id));
        launch::Request request;request.root=root_;request.gameId="model3-"+id;request.variantId=view;request.prepareProfile=false;
        request.plan.executable=plan.value("executable").toString();request.plan.cwd=plan.value("cwd").toString();request.plan.args=plan.value("args").toStringList();
        auto &environment=request.runtimeInputs.environment;
        for(const auto &name:environment.keys())if(name.startsWith("ACVR_",Qt::CaseInsensitive))environment.remove(name);
        const auto variables=plan.value("env").toMap();
        for(auto it=variables.begin();it!=variables.end();++it)environment.insert(it.key(),it.value().toString());
        // The cabinet's wheel or handlebars in front of you, inside the game only, when asked for.
        if(view=="inside"&&model3::hasCabinetControl(id)&&settings_->get("model3Controls",QVariantMap{}).toMap().value(id).toBool())environment.insert("ACVR_CONTROLS","1");
#ifdef Q_OS_WIN
        ui_->launchModel3Queued(id,view,QString());
#else
        ui_->launchModel3Queued(id,view,"Clean window close is not supported on this platform yet.");
#endif
        applyDiagnosticChild(request);
        request.pinSteamVrOpenXr=!model3::isMonitorView(view); // Every Model 3 view but side by side on the monitor is in the headset.
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
void HubServices::startGunRoom(const gunroom::Inputs &inputs,const gunroom::Plan *checked){
    if((launchBusy()&&!(checked&&preparing_))||hubExitPending_){ui_->showError("Gun room","Close the current session before opening the gun room.");return;}
    preparing_=false;
    // Revalidate the selected paths and frozen bytes at the actual click/restart, hashing off the GUI thread.
    if(!checked){
        preparing_=true;emit playingChanged();
        auto *watcher=new QFutureWatcher<gunroom::Plan>(this);
        connect(watcher,&QFutureWatcher<gunroom::Plan>::finished,this,[this,watcher,inputs]{
            const auto plan=watcher->result();watcher->deleteLater();startGunRoom(inputs,&plan);emit playingChanged();});
        watcher->setFuture(QtConcurrent::run([inputs]{return gunroom::plan(inputs);}));
        return;
    }
    const auto candidate=*checked;
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
        // Model 2 can be asked to quit cleanly, so it is; the Hub still waits for it to save and end.
        if(askModel2ToQuit(false))return;
        ui_->showError("Closing","The Hub closes when the running game has ended. Use Stop game to end it now.");
        return;
    }
    if(!gunRoom_.busy()){emit hubExitReady(code);return;}
    hubExitPending_=true;hubExitCode_=code;ui_->cancelPendingRestart();gunRoom_.close();
}
void HubServices::stopRunning(){
    // Stop, the same from the window and the headset panel: the system's own graceful quit first (a stop
    // file, VPX's close, the room's close), then everything of the game is ended after a short wait.
    const auto session=++stopSession_;
    const auto later=[this,session](std::function<void()> force){QTimer::singleShot(stopGraceMs_,this,[this,session,force]{if(session==stopSession_)force();});};
    if(gunRoom_.busy()){gunRoom_.close();later([this]{if(gunRoom_.busy())gunRoom_.forceEnd();});return;}
    if(!launcher_.busy())return;
    const auto id=ui_->nowPlaying().value("gameId").toString();
    if(id.startsWith("model2-")&&askModel2ToQuit(true))return; // Its own wait: the viewer gives MAME 30 s to save.
    if(id.startsWith("ps1-")||id.startsWith("n64-")||id.startsWith("saturn-")||id.startsWith("mamegun-")){
        QFile request(ps1StopFile_);
        if(!ps1StopFile_.isEmpty()&&request.open(QIODevice::WriteOnly)&&request.write("stop\n")==5){request.close();ui_->launchClosing();later([this]{if(launcher_.busy())launcher_.endEverything();});return;}
        launcher_.stop();return;
    }
    if(id.startsWith("ps2-")){
        const auto stopFile=settings_->get("ps2StopFile",QString()).toString();QFile request(stopFile);
        if(!stopFile.isEmpty()&&request.open(QIODevice::WriteOnly)&&request.write("stop\n")==5){request.close();ui_->launchClosing();later([this]{if(launcher_.busy())launcher_.endEverything();});return;}
        launcher_.stop();return;
    }
    if(ui_->nowPlaying().contains("pinballId")){launcher_.requestClose();later([this]{if(launcher_.busy())launcher_.endEverything();});return;}
    launcher_.stop();
}
bool HubServices::askPs2ToQuit(){
    if(!ui_->nowPlaying().value("gameId").toString().startsWith("ps2-")||!launcher_.busy())return false;
    const auto stopFile=settings_->get("ps2StopFile",QString()).toString();
    QFile request(stopFile);
    if(stopFile.isEmpty()||!request.open(QIODevice::WriteOnly)||request.write("stop\n")!=5){ui_->showError("PS2","The stop request could not be written. Use the launcher's own stop to quit the game.");return true;}
    request.close();ui_->launchClosing();return true;
}
bool HubServices::askPs1ToQuit(){
    const auto playing=ui_->nowPlaying().value("gameId").toString();
    if(!(playing.startsWith("ps1-")||playing.startsWith("n64-")||playing.startsWith("saturn-")||(playing.startsWith("mamegun-")&&!ps1StopFile_.isEmpty()))||!launcher_.busy())return false;
    QFile request(ps1StopFile_);
    if(ps1StopFile_.isEmpty()||!request.open(QIODevice::WriteOnly)||request.write("stop\n")!=5){ui_->showError("PS1","The stop request could not be written. Hold left Y for 3 seconds in the headset to quit the game.");return true;}
    request.close();ui_->launchClosing();return true;
}
bool HubServices::mayStartSteamVr() const{
    return settings_->get("startSteamVr",true).toBool()&&!qEnvironmentVariableIsSet("AC_NO_STEAMVR_START");
}
void HubServices::launchInHeadset(const QString &title,std::function<void()> launch){
    if(vrServerRunning()){launch();return;}
    if(!mayStartSteamVr()){ui_->showError("SteamVR","Starting SteamVR from the Hub is turned off in Settings. Start SteamVR yourself, then press Play in headset again. Nothing was started.");return;}
    if(pendingHeadset_){ui_->showError("SteamVR","SteamVR is already starting for "+pendingHeadsetTitle_+".");return;}
    if(launchBusy()||hubExitPending_){ui_->showError("Launch","Close the current session before starting a game.");return;}
    // Only ever because Play in headset was pressed: SteamVR is never started for anything else.
    QString error;
    // Only the real Hub is given the real starter (main.cpp); a Hub without one, as in every test, starts nothing.
    if(!steamVrStarter_)error="this Hub was not given a way to start it.";
    const bool asked=steamVrStarter_&&steamVrStarter_(&error);
    if(!asked){ui_->showError("SteamVR","SteamVR could not be started"+(error.isEmpty()?QString("."):": "+error)+" Nothing was started.");return;}
    pendingHeadset_=std::move(launch);pendingHeadsetTitle_=title;steamVrSeenAt_=-1;steamVrStartClock_.start();
    ui_->setSteamVrStarting(title.isEmpty()?QString("the game"):title);
    steamVrStartWatch_.start();
}
void HubServices::endSteamVrWait(const QString &message,bool error){
    pendingHeadset_=nullptr;pendingHeadsetTitle_.clear();steamVrStartWatch_.stop();ui_->setSteamVrStarting({});
    // Said on the status line either way; error only marks the ones that went wrong, for the log.
    if(!message.isEmpty())ui_->showError("SteamVR",message);
    if(error)qWarning("SteamVR wait ended: %s",qPrintable(message));
}
void HubServices::applyWatchdogRules(){
    // Only what the settings point at: each system's program folder, and its known programs in the
    // folders around it. Nothing else on the PC is ever the watchdog's to end.
    const auto get=[this](const char *key){return settings_->get(key,QString()).toString();};
    const auto folderOf=[](const QString &file){return file.isEmpty()?QString():QFileInfo(file).absolutePath();};
    const auto parentOf=[](const QString &folder){return folder.isEmpty()?QString():QFileInfo(folder).absolutePath();};
    QList<launch::SystemRule> rules;
    const auto add=[&](const QString &system,const QString &label,const QString &own,const QStringList &folders,const QStringList &names){
        if(own.isEmpty())return;
        rules<<launch::SystemRule{system,label,{own},folders,names};};
    const auto pinball=folderOf(get("pinballExecutable"));
    add("pinball","Visual Pinball",pinball,{get("pinballFolder"),parentOf(pinball)},
        {"vpinball*","pinmame*","vpinmame*","b2sbackglassserver*","dmdext*","flexdmd*","ultradmd*","freezy*","pinupdisplay*","pupdmdcontrol*","dof*"});
    add("model3","Sega Model 3",folderOf(get("model3Executable")),{},{"supermodel*"});
    const auto model2=folderOf(get("model2Executable"));
    add("model2","Sega Model 2",model2,{parentOf(model2)},{"mame*","model2*"});
    const auto ps2=folderOf(get("ps2Launcher"));
    add("ps2","PlayStation 2",ps2,{parentOf(ps2)},{"pcsx2*"});
    const auto ps1=folderOf(get("ps1Launcher"));
    add("ps1","PlayStation",ps1,{parentOf(ps1)},{"acvr-ps1*"});
    const auto n64=folderOf(get("n64Launcher"));
    add("n64","Nintendo 64",n64,{parentOf(n64)},{"acvr-ps1*"});
    const auto saturn=folderOf(get("saturnLauncher"));
    add("saturn","Sega Saturn",saturn,{parentOf(saturn)},{"acvr-ps1*"});
    add("dreamcast","Sega Dreamcast",folderOf(get("dreamcastExecutable")),{},{"flycast*"});
    add("system22","Namco System 22",folderOf(get("system22Executable")),{},{"timecrisis*"});
    add("wii","Nintendo Wii",folderOf(get("wiiExecutable")),{},{"dolphin*"});
    watchdog_.setRules(rules);
}
void HubServices::recordCrash(const QString &id,int code,const QString &how,const QString &logPath){
    // A short record in the Hub's folder: when, which game, the exit code in words, and the end of
    // the emulator's own log when it keeps one (else the Hub's launch log).
    const auto system=sessionSystem_;
    QStringList candidates;
    const auto beside=[this](const char *key,const QString &name){const auto file=settings_->get(key,QString()).toString();return file.isEmpty()?QString():QFileInfo(file).absolutePath()+"/"+name;};
    if(system=="model3")candidates<<beside("model3Executable","Supermodel.log");
    if(system=="pinball")candidates<<beside("pinballExecutable","VPinballX.log");
    if(system=="dreamcast")candidates<<beside("dreamcastExecutable","flycast.log");
    if(system=="wii")candidates<<beside("wiiExecutable","User/Logs/dolphin.log");
    QString tail,from;
    for(const auto &file:candidates){const QFileInfo info(file);if(!file.isEmpty()&&info.isFile()&&info.lastModified().toMSecsSinceEpoch()>=sessionStartMs_-5000){tail=launch::lastLines(file,12);from=info.fileName();break;}}
    if(tail.isEmpty()&&!logPath.isEmpty()&&QFileInfo(logPath).isFile()){tail=launch::lastLines(logPath,12);from="the Hub's launch log";}
    QJsonObject record{{"time",QDateTime::currentDateTime().toString(Qt::ISODate)},{"gameId",id},{"title",sessionTitle_.isEmpty()?id:sessionTitle_},{"system",system},
        {"exitCode",QString("0x%1").arg(quint32(code),8,16,QLatin1Char('0')).toUpper().replace("0X","0x")},{"meaning",how},{"log",install::shareable(tail)},{"logFrom",from}};
    try{
        const auto path=install::scopedPath("user/logs/crashes.json",root_);
        QJsonArray list;{QFile file(path);if(file.size()<=4*1024*1024&&file.open(QIODevice::ReadOnly))list=QJsonDocument::fromJson(file.readAll()).array();}
        list.prepend(record);while(list.size()>30)list.removeLast();
        QDir().mkpath(QFileInfo(path).absolutePath());install::atomicWrite(path,QJsonDocument(list).toJson());
        ui_->setCrashRecords(list.toVariantList());
    }catch(const std::exception &e){ui_->showError("Crash record",QString::fromUtf8(e.what()));}
}
// Folders that hold the owner's games: artwork kept beside a ROM or in its boxart/covers/snap subfolders is found
// there without being added as an artwork folder. Settings first (no disk reads), then each launcher's games.json
// (read on the scan's worker).
static QStringList romFolderSettings(const QVariantMap &values){
    QStringList folders;
    for(auto it=values.cbegin();it!=values.cend();++it)if(it.key().endsWith("GameFolder"))folders<<it.value().toString();
    folders<<values.value("libraryRoots").toStringList();
    return folders;
}
static QStringList launcherRomFolders(const QStringList &launchers){
    QStringList folders;
    for(const auto &launcher:launchers){
        if(launcher.isEmpty())continue;
        QFile file(QFileInfo(launcher).absoluteDir().filePath("games.json"));
        if(file.size()>8*1024*1024||!file.open(QIODevice::ReadOnly))continue;
        auto bytes=file.readAll();if(bytes.startsWith("\xEF\xBB\xBF"))bytes.remove(0,3);
        const auto all=QJsonDocument::fromJson(bytes).object();
        for(const auto &value:all){
            const auto entry=value.toObject();
            for(const auto *key:{"file","disc","rom"}){const auto path=entry.value(key).toString();if(QDir::isAbsolutePath(path))folders<<QFileInfo(path).absolutePath();}
            for(const auto &path:entry.value("RomPath").toString().split(';',Qt::SkipEmptyParts))if(QDir::isAbsolutePath(path.trimmed()))folders<<path.trimmed();
        }
    }
    return folders;
}
// Distinct folders that exist, none inside another, never a whole drive.
static QStringList artRoots(const QStringList &candidates){
    QStringList clean;
    for(const auto &candidate:candidates){
        if(candidate.trimmed().isEmpty())continue;
        const QFileInfo info(candidate);if(!info.isDir())continue;
        const auto path=QDir::cleanPath(info.absoluteFilePath());if(QDir(path).isRoot())continue;
        clean<<path;
    }
    std::sort(clean.begin(),clean.end(),[](const QString &a,const QString &b){return a.size()<b.size();});
    QStringList roots;
    for(const auto &path:clean){
        bool inside=false;
        for(const auto &root:roots)if(path.compare(root,Qt::CaseInsensitive)==0||path.startsWith(root+"/",Qt::CaseInsensitive)){inside=true;break;}
        if(!inside)roots<<path;
    }
    return roots;
}
void HubServices::scanLocalArt(){
    const auto chosen=settings_->get("artFolders",QVariantList{}).toStringList();
    QStringList launchers;
    for(const auto &key:{"ps1Launcher","n64Launcher","saturnLauncher","ps2Launcher"})launchers<<settings_->get(key,QString()).toString();
    for(const auto &system:scripted::systems())launchers<<ui_->launcherFor(system.id);
    const auto romFolders=romFolderSettings(settings_->values());
    const auto key=chosen+QStringList{"|"}+romFolders+QStringList{"|"}+launchers;
    if(localArtScanned_&&key==localArtFolders_)return;
    localArtScanned_=true;localArtFolders_=key;
    if(localArtCancel_)localArtCancel_->store(true);
    auto cancel=std::make_shared<std::atomic_bool>(false);localArtCancel_=cancel;
    QPointer<HubServices> self(this);
    // Read in place: only names and paths are kept, and nothing is copied. The owner's chosen artwork folders
    // come first, so a picture there wins over one beside a ROM.
    QThreadPool::globalInstance()->start([self,chosen,romFolders,launchers,cancel]{
        // ROM folders can be large; play comes first. The pool's thread gets its priority back for the next job.
        const auto priority=QThread::currentThread()->priority();QThread::currentThread()->setPriority(QThread::LowestPriority);
        const auto restore=qScopeGuard([priority]{QThread::currentThread()->setPriority(priority);});
        const auto folders=artRoots(chosen)+artRoots(romFolders+launcherRomFolders(launchers));
        auto index=std::make_shared<const art::LocalIndex>(art::LocalIndex::scan(folders,cancel.get()));
        QMetaObject::invokeMethod(qApp,[self,index,cancel]{if(self&&!cancel->load())self->ui_->setLocalArt(index);});
    });
}
bool HubServices::askModel2ToQuit(bool forceLater){
    if(!ui_->nowPlaying().value("gameId").toString().startsWith("model2-")||!launcher_.busy()||model2StopFile_.isEmpty())return false;
    QFile request(model2StopFile_);
    if(!request.open(QIODevice::WriteOnly)||request.write("stop\n")!=5){ui_->showError("Model 2","The stop request could not be written; the game is ended instead.");return false;}
    request.close();ui_->launchClosing();
    // The viewer waits up to 30 seconds for MAME to save and exit; only after that is the game forced.
    if(forceLater){const auto session=model2Session_;QTimer::singleShot(40000,this,[this,session]{if(session==model2Session_&&launcher_.busy())launcher_.stop();});}
    return true;
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
