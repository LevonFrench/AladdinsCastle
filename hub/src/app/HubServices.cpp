// SPDX-License-Identifier: GPL-3.0-only
#include "HubServices.h"
#include "core/install/Support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
namespace ac {
namespace {
scan::ScanOptions scanOptions(const QString &root) {
    scan::ScanOptions options; options.userRoot=QDir(root).filePath("user");
    options.toolRoots={QDir(root).filePath("emulators")}; return options;
}
Json readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return Json::object();
    if (file.size()>128*1024*1024) throw install::Error("E_STATE_SIZE","Hub state is too large");
    return Json::parse(file.readAll().toStdString());
}
QString text(const Json &j,const char *key){return install::string(j,key);}
QStringList strings(const Json &j,const char *key){QStringList out;for(const auto &v:j.value(key,Json::array()))if(v.is_string())out<<QString::fromStdString(v.get<std::string>());return out;}
}
HubServices::HubServices(GameListModel *games,FilterSortModel *filter,UiController *ui,
                        UiSettings *settings,QString root,QObject *parent)
    :QObject(parent),games_(games),filter_(filter),ui_(ui),settings_(settings),root_(std::move(root)),
      scanner_(games,scanOptions(root_)),art_(std::make_shared<art::Resolver>(games->catalog(),art::ArtOptions{root_+"/user",{}})) {
    installer_.setRuntimeStateSource([this](const QString &id){return current(id);});
    launcher_.setRuntimeStateSource([this](const QString &id){return current(id);});
    connect(ui_,&UiController::scanRequested,this,&HubServices::scan);
    connect(ui_,&UiController::cancelScanRequested,&scanner_,&scan::ScanController::cancel);
    connect(&scanner_,&scan::ScanController::scanStarted,ui_,&UiController::scanStarted);
    connect(&scanner_,&scan::ScanController::progress,ui_,&UiController::scanProgress);
    connect(&scanner_,&scan::ScanController::scanFinished,this,[this](bool cancelled){ui_->scanFinished(!cancelled);});
    connect(&scanner_,&scan::ScanController::resultsReady,this,[this](const scan::ScanResult &result){
        bindings_=result.toJson(); art_->setBindings(result.bindings);
        auto roots=scanner_.options().artRoots+scanner_.options().mediaRoots;
        for(const auto &tool:result.tools)roots<<QFileInfo(tool.path).absolutePath();
        art_->setRoots(roots); filter_->setScanComplete(true);
    });
    connect(&scanner_,&scan::ScanController::artRevisionChanged,this,[this]{ui_->setArtRevision(static_cast<int>(scanner_.artRevision()%2147483647));});
    connect(&installer_,&install::InstallService::event,ui_,&UiController::installEvent);
    connect(&installer_,&install::InstallService::finished,this,[this](bool ok,const QString &message){ui_->installFinished(ok,message);if(ok)scan({});});
    connect(&installer_,&install::InstallService::runtimeStateReady,this,[this](const RuntimeState &state){if(games_->find(state.gameId))games_->applyRuntimeStates({state});});
    connect(ui_,&UiController::cancelInstallRequested,&installer_,&install::InstallService::cancel);
    connect(ui_,&UiController::installRequested,this,[this](const QString &id,const QString &variant){
        try{lastInstall_=gameRequest(id,variant);installer_.start(lastInstall_);}catch(const std::exception &e){report(e);}
    });
    connect(ui_,&UiController::retryInstallRequested,this,[this](bool,const QString &handover){
        if(lastInstall_.gameId.isEmpty())return;
        lastInstall_.handover=handover;installer_.start(lastInstall_);
    });
    connect(ui_,&UiController::uninstallPreviewRequested,this,[this](const QString &id,const QString &variant){
        try{auto request=gameRequest(id,variant);request.operation="uninstall";previewRemoval(request);}catch(const std::exception &e){report(e);}
    });
    connect(ui_,&UiController::playRequested,this,[this](const QString &id,const QString &variant){
        if(launcher_.playing())return;
        try{launcher_.start(launch::flatRequest(games_->catalog(),id,root_,bindings_,variant));}catch(const std::exception &e){ui_->launchFinished(id,QString::fromUtf8(e.what()));}
    });
    connect(&launcher_,&launch::LaunchService::started,ui_,&UiController::launchStarted);
    connect(&launcher_,&launch::LaunchService::finished,this,[this](const QString &id,int,const QString &error,const QString &){ui_->launchFinished(id,error);});
    connect(&launcher_,&launch::LaunchService::runtimeStateReady,this,[this](const RuntimeState &state){games_->applyRuntimeStates({state});});
    connect(&launcher_,&launch::LaunchService::playingChanged,this,&HubServices::playingChanged);
    connect(&launcher_,&launch::LaunchService::raiseHubRequested,this,&HubServices::raiseHubRequested);
    restore();
}
RuntimeState HubServices::current(const QString &id)const{auto game=games_->find(id);return game?game->runtime:RuntimeState{};}
void HubServices::report(const std::exception &e){ui_->installFinished(false,QString::fromUtf8(e.what()));}
void HubServices::scan(const QStringList &folders){
    QStringList roots=folders;
    if(roots.isEmpty())roots=scanner_.options().mediaRoots+scanner_.options().toolRoots+scanner_.options().artRoots;
    roots<<QDir(root_).filePath("emulators");roots.removeDuplicates();scanner_.scan(roots);
}
install::Request HubServices::gameRequest(const QString &id,const QString &variant){
    auto game=games_->find(id);if(!game)throw install::Error("E_GAME_INVALID","Unknown game");
    install::Request request;request.root=root_;request.catalogRoot=QFileInfo(game->folder).dir().absolutePath()+"/..";
    request.gameId=id;request.variantId=variant;request.recipe=game->install;request.runtime=current(id);
    request.bindings=launch::normalizeBindings(bindings_,id);return request;
}
QString HubServices::toolPlan()const{
    try{auto request=install::Engine::emulatorRequest(root_,games_->catalog().emulators.at("supermodel"));return install::Engine().plan(request).text;}
    catch(const std::exception &e){return QString::fromUtf8(e.what());}
}
void HubServices::installSupermodel(){
    try{lastInstall_=install::Engine::emulatorRequest(root_,games_->catalog().emulators.at("supermodel"));lastInstall_.catalogRoot=QFileInfo(games_->records().first().folder).dir().absolutePath()+"/..";installer_.start(lastInstall_);}catch(const std::exception &e){report(e);}
}
void HubServices::previewSupermodelRemoval(){
    install::Request request;request.root=root_;request.gameId="tool-supermodel";request.variantId="windows-x64";request.operation="uninstall";previewRemoval(request);
}
void HubServices::previewRemoval(const install::Request &request){
    try{pendingRemoval_=request;removalText_=QString::fromStdString(install::Engine().uninstallPreview(request).dump(2));emit removalPlanChanged();}catch(const std::exception &e){report(e);}
}
void HubServices::confirmRemoval(){if(!pendingRemoval_.gameId.isEmpty())installer_.start(pendingRemoval_);pendingRemoval_={};}
void HubServices::restore(){
    try{
        bindings_=readJson(root_+"/user/cache/scan-bindings.json");
        QVector<RuntimeState> states;QVector<scan::Binding> artBindings;
        const auto times=readJson(root_+"/user/last-played.json");
        for(const auto &game:games_->records()){
            auto state=game.runtime;state.gameId=game.id;
            state.lastPlayed=times.value(game.id.toStdString(),qint64(0));
            for(const auto &b:bindings_.value("bindings",Json::array()))if(text(b,"gameId")==game.id){
                scan::Binding binding;binding.gameId=game.id;binding.requirementId=text(b,"requirementId");binding.path=text(b,"path");binding.identity=text(b,"identity");binding.proof=text(b,"proof");binding.verified=b.value("verified",false);binding.supportPaths=strings(b,"supportPaths");
                const QFileInfo currentFile(binding.path);
                if(!currentFile.isFile())binding.verified=false;
                for(const auto &file:bindings_.value("files",Json::array()))if(text(file,"path")==binding.path&&(file.value("size",qint64(-1))!=currentFile.size()||file.value("mtime",qint64(-1))!=currentFile.lastModified().toMSecsSinceEpoch()))binding.verified=false;
                if(binding.verified)state.mediaFound<<binding.requirementId;
                artBindings<<binding;
            }
            for(const auto &tool:bindings_.value("tools",Json::array()))if(tool.value("verified",false)&&QFileInfo(text(tool,"path")).isFile())state.toolsOk<<text(tool,"id");
            states<<state;
        }
        games_->applyRuntimeStates(states);art_->setBindings(artBindings);
        art_->setRoots(scanner_.options().artRoots+scanner_.options().mediaRoots);
        filter_->setScanComplete(bindings_.contains("bindings"));
    }catch(const std::exception &e){ui_->scanProgress({{"text",QString("Saved scan needs refresh: ")+QString::fromUtf8(e.what())}});}
}
}
