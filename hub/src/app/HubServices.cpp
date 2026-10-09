// SPDX-License-Identifier: GPL-3.0-only
#include "HubServices.h"
#include "core/install/Support.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
namespace ac {
namespace {
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
    installer_.setRuntimeStateSource([this](const QString &id){return current(id);});
    launcher_.setRuntimeStateSource([this](const QString &id){return current(id);});
    connect(ui_,&UiController::writeConfigRequested,this,[this](const QString &id,const QVariantMap &){ui_->settingsSaved(id);});
    connect(ui_,&UiController::scanRequested,this,&HubServices::scan);
    connect(ui_,&UiController::cancelScanRequested,&scanner_,&scan::ScanController::cancel);
    connect(&scanner_,&scan::ScanController::scanStarted,ui_,&UiController::scanStarted);
    connect(&scanner_,&scan::ScanController::progress,ui_,&UiController::scanProgress);
    connect(&scanner_,&scan::ScanController::scanFinished,this,[this](bool cancelled){ui_->scanFinished(!cancelled);if(rescanPending_){rescanPending_=false;QTimer::singleShot(0,this,[this]{scan({});});}});
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
            if(game)for(const auto &v:game->variants)if(v.id==variant&&v.generated)available=true;
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
        try{launcher_.start(launch::flatRequest(games_->catalog(),id,root_,bindings_,variant));}catch(const std::exception &e){ui_->showError("Launch",QString::fromUtf8(e.what()));}
    });
    connect(ui_,&UiController::stopLaunchRequested,&launcher_,&launch::LaunchService::stop);
    connect(&launcher_,&launch::LaunchService::starting,ui_,&UiController::launchPreparing);
    connect(&launcher_,&launch::LaunchService::started,this,[this](const QString &id){childPlaying_=true;emit playingChanged();ui_->launchStarted(id);});
    connect(&launcher_,&launch::LaunchService::finished,this,[this](const QString &id,int,const QString &error,const QString &){childPlaying_=false;emit playingChanged();ui_->launchFinished(id,error);});
    connect(&launcher_,&launch::LaunchService::runtimeStateReady,this,[this](const RuntimeState &state){games_->applyRuntimeStates({state});});
    connect(&launcher_,&launch::LaunchService::playingChanged,this,&HubServices::playingChanged);
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
            bool supported=false;for(const auto &v:game->variants)if(v.id==variantId&&v.generated)supported=true;
            if(!supported)throw install::Error("E_VARIANT_INVALID","Select an available M1 flat variant for this game");
            if(QDir(root_).absolutePath()!=QDir(QCoreApplication::applicationDirPath()).absolutePath())throw install::Error("E_PORTABLE_ROOT","Create Steam entries from the portable Hub folder");
            launch::flatRequest(games_->catalog(),id,root_,bindings_,variantId);
        }
        steamRequest_.steamRoot=steamRootSource_();
        steamRequest_.userRoot=install::scopedPath("user",root_);
        steamRequest_.shortcut={id,game->roles.value("title").toString(),QCoreApplication::applicationFilePath(),root_,false,0,game->runtime.lastPlayed,variantId};
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
    request.gameId=id;request.variantId=variant;request.recipe=game->install;request.runtime=current(id);
    request.bindings=launch::normalizeBindings(bindings_,id);request.settings=Json::parse(QJsonDocument(QJsonObject::fromVariantMap(settings_->game(id))).toJson(QJsonDocument::Compact).toStdString());return request;
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
                scan::Binding binding;binding.gameId=game.id;binding.requirementId=text(b,"requirementId");binding.path=text(b,"path");binding.identity=text(b,"identity");binding.proof=text(b,"proof");binding.verified=b.value("verified",false);binding.supportPaths=strings(b,"supportPaths");
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
            states<<state;
        }
        games_->applyRuntimeStates(states);art_->setBindings(artBindings);
        art_->setRoots(scanner_.options().artRoots+scanner_.options().mediaRoots);
        filter_->setScanComplete(bindings_.contains("bindings"));
    }catch(const std::exception &e){ui_->scanProgress({{"text",QString("Saved scan needs refresh: ")+QString::fromUtf8(e.what())}});}
}
}
