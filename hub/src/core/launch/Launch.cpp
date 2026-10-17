// SPDX-License-Identifier: GPL-3.0-only
#include "Launch.h"
#include "ProductIdentity.h"
#include "core/install/Support.h"
#include "core/scan/Scan.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace ac::launch {
using install::Error;
using install::string;
namespace {
#ifdef Q_OS_WIN
struct CloseWindowSearch { DWORD pid; bool found=false; };
BOOL CALLBACK findCloseWindow(HWND window,LPARAM parameter){
  auto &search=*reinterpret_cast<CloseWindowSearch *>(parameter);DWORD pid=0;
  GetWindowThreadProcessId(window,&pid);
  if(pid==search.pid&&(IsWindowVisible(window)||(GetWindowLongPtrW(window,GWL_STYLE)&WS_CAPTION)!=0)){
    search.found=true;return FALSE;
  }
  return TRUE;
}
bool hasCloseWindow(qint64 pid){
  CloseWindowSearch search{static_cast<DWORD>(pid)};
  EnumWindows(findCloseWindow,reinterpret_cast<LPARAM>(&search));return search.found;
}
#endif
void verifyDisc(const QString &path,const QString &expected){
  std::atomic_bool cancel=false;const auto current=scan::Scanner::inspect(path,cancel);
  if(QFileInfo(path).isSymLink()||QFileInfo(path).isJunction()||!current.error.isEmpty()||current.kind!="disc-id"||current.identity!=expected||expected.isEmpty())
    throw Error("E_MEDIA_CHANGED","Disc header identity changed or is unreadable; scan again");
}
QMap<QString,QString> discChecks(const Json &bindings){QMap<QString,QString> result;for(const auto &b:bindings.value("media",Json::object()))if(b.value("verified",false)&&string(b,"proof")=="disc-id")result[string(b,"path")]=string(b,"identity");return result;}
}
Json normalizeBindings(const Json &source, const QString &gameId) {
  Json result{{"media", Json::object()}, {"tools", Json::object()}};
  if (source.contains("media"))
    result["media"] = source["media"];
  if (source.contains("bindings"))
    for (const auto &b : source["bindings"])
      if (string(b, "gameId") == gameId)
        result["media"][string(b, "requirementId").toStdString()] = b;
  if (source.contains("tools") && source["tools"].is_object())
    result["tools"] = source["tools"];
  else
    for (const auto &t : source.value("tools", Json::array()))
      if (t.value("verified", false))
        result["tools"][string(t, "id").toStdString()] = t;
  for (auto &t : result["tools"]) {
    const QFileInfo current(string(t, "path"));
    if (!t.value("verified", false) || !current.isFile()) {
      t["verified"] = false;
      continue;
    }
    if ((t.contains("size") && t.value("size", qint64(-1)) != current.size()) ||
        (t.contains("mtime") && t.value("mtime", qint64(-1)) !=
                                    current.lastModified().toMSecsSinceEpoch()))
      t["verified"] = false;
  }
  for (auto &b : result["media"]) {
    if (!b.value("verified", false))
      continue;
    if (!scan::hasValidatedChdBounds(b, source.value("files", Json::array()))) {
      b["verified"] = false;
      b["verificationError"] = "CHD bounds verification receipt is missing or stale; scan again";
      continue;
    }
    const auto check = [&](const QString &path) {
      const QFileInfo current(path);
      if (!current.isFile())
        throw Error("E_MEDIA_MISSING",
                    "Scanned media or support file is missing; scan again");
      for (const auto &f : source.value("files", Json::array()))
        if (string(f, "path") == current.filePath() &&
            (f.value("size", qint64(-1)) != current.size() ||
             f.value("mtime", qint64(-1)) !=
                 current.lastModified().toMSecsSinceEpoch()))
          throw Error("E_MEDIA_CHANGED",
                      "Scanned media or support file changed; scan again");
    };
    check(string(b, "path"));
    if(string(b,"proof")=="disc-id")verifyDisc(string(b,"path"),string(b,"identity"));
    for (const auto &support : b.value("supportPaths", Json::array()))
      check(support.is_string()
                ? QString::fromStdString(support.get<std::string>())
                : string(support, "path"));
    for (const auto &support : b.value("supportRequirements", Json::array()))
      check(string(support, "sourcePath"));
  }
  return result;
}
Request flatRequest(const CatalogData &catalog, const QString &gameId,
                    const QString &root, const Json &source,
                    const QString &variantId) {
  const auto *game = catalog.find(gameId);
  if (!game || !game->errors.isEmpty())
    throw Error("E_GAME_INVALID", "Game is missing or invalid");
  const auto bindings = normalizeBindings(source, gameId);
  for (const auto &v : game->variants) {
    if (v.quality != "flat" || (!variantId.isEmpty() && v.id != variantId))
      continue;
    if (v.tools.size() != 1 ||
        !catalog.emulators.contains(v.tools.first().toStdString()))
      continue;
    const auto tool =
        bindings["tools"].value(v.tools.first().toStdString(), Json::object());
    if (!tool.value("verified", false))
      continue;
    try {
      Request r;
      r.root = root;
      r.gameId = gameId;
      r.variantId = v.id;
      r.discIdentities=discChecks(bindings);
      r.plan = install::makeFlatLaunchPlan(
          *game, catalog.emulators[v.tools.first().toStdString()], root,
          bindings);
      return r;
    } catch (const Error &) {
      if (!variantId.isEmpty())
        throw;
    }
  }
  throw Error("E_FLAT_NOT_READY", "No flat variant has verified media and an "
                                  "emulator; scan or locate files first");
}
Request gameRequest(const CatalogData &catalog,const QString &gameId,const QString &root,const Json &source,const QString &variantId) {
  const auto *game=catalog.find(gameId);
  if(!game||!game->errors.isEmpty())throw Error("E_GAME_INVALID","Game is missing or invalid");
  if(variantId.isEmpty()) {
    QStringList choices;
    const auto preferred=install::string(game->install,"default");
    if(!preferred.isEmpty())choices<<preferred;
    for(const auto &v:game->variants)
      if(authoredLaunchSupported(v)&&!choices.contains(v.id))choices<<v.id;
    for(const auto &id:choices) {
      bool supported=false;
      for(const auto &v:game->variants)if(v.id==id)supported=authoredLaunchSupported(v);
      if(!supported)continue;
      install::Request saved;saved.root=root;saved.gameId=gameId;saved.variantId=id;
      const auto state=install::readEnvelope(install::stateBase(saved)+".toml");
      if(install::string(state,"state").startsWith("installed"))
        return gameRequest(catalog,gameId,root,source,id);
    }
    return flatRequest(catalog,gameId,root,source);
  }
  const Variant *variant=nullptr;
  for(const auto &v:game->variants)if(v.id==variantId&&!v.generated){variant=&v;break;}
  if(!variant)return flatRequest(catalog,gameId,root,source,variantId);
  if(!authoredLaunchSupported(*variant))throw Error("E_SETUP_UNSUPPORTED","This authored setup has no pinned launch contract");
  install::Request installed;installed.root=root;installed.catalogRoot=catalog.root;installed.gameId=gameId;installed.variantId=variantId;installed.recipe=game->install;
  // Scope before normalization: stale files from another route must not affect
  // this variant's admission, including media metadata validation.
  Json scoped=source;
  scoped["media"]=Json::object();
  scoped["bindings"]=Json::array();
  scoped["tools"]=Json::object();
  for(const auto &id:variant->media) {
    const auto key=id.toStdString();
    if(source.contains("media")&&source["media"].is_object()&&source["media"].contains(key))
      scoped["media"][key]=source["media"][key];
  }
  for(const auto &binding:source.value("bindings",Json::array()))
    if(string(binding,"gameId")==gameId&&variant->media.contains(string(binding,"requirementId")))
      scoped["bindings"].push_back(binding);
  const auto tools=source.value("tools",Json::object());
  if(tools.is_object()) {
    for(const auto &id:variant->tools)
      if(tools.contains(id.toStdString()))scoped["tools"][id.toStdString()]=tools[id.toStdString()];
  } else if(tools.is_array()) {
    for(const auto &tool:tools)
      if(variant->tools.contains(string(tool,"id")))scoped["tools"][string(tool,"id").toStdString()]=tool;
  }
  installed.bindings=normalizeBindings(scoped,gameId);
  for(const auto *space:{"media","tools"}) {
    const auto &required=QString(space)=="media"?variant->media:variant->tools;
    for(const auto &id:required) {
      const auto binding=installed.bindings[space].value(id.toStdString(),Json::object());
      if(!binding.value("verified",false)||!QFileInfo(string(binding,"path")).isFile())
        throw Error("E_MEDIA_MISSING","Locate verified files required by this setup first");
    }
  }
  const auto plan=install::Engine().plan(installed);
  const auto base=install::stateBase(installed);
  const auto state=install::readEnvelope(base+".toml"),manifest=install::readEnvelope(base+".manifest.toml");
  const auto recipeHash=install::sha256(QByteArray::fromStdString(game->install.dump()));
  if(!install::string(state,"state").startsWith("installed")||install::string(state,"installed_version")!=plan.version||
     install::string(manifest,"recipe_sha256")!=recipeHash||install::string(state,"recipe_sha256")!=recipeHash||
     install::string(manifest,"game")!=gameId||install::string(manifest,"variant")!=variantId||
     install::string(state,"game")!=gameId||install::string(state,"variant")!=variantId||
     install::string(manifest,"install_dir")!=QDir(root).relativeFilePath(plan.installDir))
    throw Error("E_SETUP_NOT_INSTALLED","Install or repair this exact authored setup before launching");
  const auto runtime=install::Engine().runtimeState(installed);
  bool ready=false;for(const auto &v:runtime.variants)if(v.id==variantId)ready=v.verified&&v.manifestExists&&v.installedWhenExists;
  if(!ready)throw Error("E_SETUP_NOT_INSTALLED","Owned setup files are missing; repair first");
  QMap<QString,QString> vars{{"install_dir",plan.installDir},{"install_root",QDir(root).absolutePath()},{"game.id",gameId}};
  for(const auto *space:{"media","tools"}) {
    const auto group=installed.bindings.value(space,Json::object());
    for(const auto &entry:group.items()){
    const auto path=install::string(entry.value(),"path");
    if(!entry.value().value("verified",false)||!QFileInfo(path).isFile())throw Error("E_MEDIA_MISSING","Locate verified launch files first");
    const auto key=QString(space)+"."+QString::fromStdString(entry.key());vars[key+".path"]=path;vars[key+".dir"]=QFileInfo(path).absolutePath();
  }
  }
  const auto authored=install::expandJson(plan.variant["launch"],vars);
  Request request;request.root=root;request.gameId=gameId;request.variantId=variantId;request.runtime=install::string(authored,"runtime");request.prepareProfile=false;
  // An unpinned OpenXR route uses the current selection; only SteamVR overrides
  // the child's runtime environment.
  if(request.runtime=="openxr")request.runtime="any";
  request.discIdentities=discChecks(installed.bindings);
  auto &launch=request.plan;launch.executable=install::scopedPath(install::string(authored,"exe"),plan.installDir);
  launch.cwd=install::scopedPath(install::string(authored,"cwd",plan.installDir),plan.installDir);
  auto executableHash=install::string(authored,"exe_sha256");
  if(install::string(authored,"exe_identity")=="user-local-build") {
    const auto local=manifest.value("local_build",Json::object()); int matches=0;
    for(const auto &step:plan.steps) if(install::string(step,"do")=="locate-package" && install::string(step,"kind")=="local-build")
      if(install::string(local,"port_commit")==install::string(step,"port_commit") &&
         install::string(local,"engine_commit")==install::string(step,"engine_commit")) ++matches;
    executableHash=install::string(local,"sha256");
    if(matches!=1 || !QRegularExpression("\\A[0-9a-f]{64}\\z").match(executableHash).hasMatch())
      throw Error("E_SETUP_IDENTITY","The private local build has no matching source/ownership receipt; install it first");
  }
  if(!QFileInfo(launch.executable).isFile()||install::hashFile(launch.executable)!=executableHash)
    throw Error("E_SETUP_IDENTITY","Authored executable identity changed; repair first");
  bool owned=false;for(const auto &file:manifest.value("file",Json::array()))
    if(install::scopedPath(install::string(file,"path"),plan.installDir)==launch.executable&&install::string(file,"sha256")==executableHash)owned=true;
  if(!owned)throw Error("E_SETUP_IDENTITY","The pinned executable has no matching ownership receipt");
  for(const auto &arg:authored["args"]){if(!arg.is_string())throw Error("E_PLAN_INVALID","Launch arguments must be strings");launch.args<<QString::fromStdString(arg.get<std::string>());}
  const auto user=install::scopedPath("User",plan.installDir);
  const auto userMode=install::string(authored,"user_mode","argument");
  if(userMode=="executable-directory") {
    const auto nativeMedia=install::scopedPath("roms",user);
    if(launch.cwd!=user || QFileInfo(launch.executable).absolutePath()!=user || !QFileInfo(nativeMedia).isDir() ||
       launch.args!=QStringList{"--no-dialog",nativeMedia})
      throw Error("E_PLAN_INVALID","Native host must run from its owned User executable directory with explicit media");
    for(const auto &file:manifest.value("file",Json::array())) if(install::string(file,"origin")=="extracted") {
      const auto path=install::scopedPath(install::string(file,"path"),plan.installDir);
      if(install::hashFile(path)!=install::string(file,"sha256"))
        throw Error("E_SETUP_IDENTITY","Owned native runtime support changed; repair this setup first");
    }
    for(const auto &step:plan.steps) if(install::string(step,"do")=="prepare-romset") {
      if(install::string(install::expandJson(step,vars),"to")!=nativeMedia)
        throw Error("E_PLAN_INVALID","Native prepared media must belong to the owned User directory");
      for(const auto &file:step["files"]) {
        const auto path=install::scopedPath(install::string(file,"name"),nativeMedia); QString expected;
        for(const auto &receipt:manifest.value("file",Json::array()))
          if(install::scopedPath(install::string(receipt,"path"),plan.installDir)==path) expected=install::string(receipt,"sha256");
        if(expected.isEmpty() || install::hashFile(path)!=expected)
          throw Error("E_MEDIA_CHANGED","Owned native media changed or has no installation receipt; repair first");
      }
    }
  } else if(userMode=="argument") {
    int userOptions=0;for(qsizetype i=0;i<launch.args.size();++i){const auto &arg=launch.args[i];
      if(arg=="--user"){++userOptions;if(i+1>=launch.args.size()||QDir::cleanPath(launch.args[i+1])!=user)throw Error("E_PLAN_INVALID","Authored User directory must be isolated");}
      else if(arg.startsWith("--user=")||arg.startsWith("-u"))throw Error("E_PLAN_INVALID","Use one explicit owned --user directory");
    }
    if(userOptions!=1||!QFileInfo(user).isDir())throw Error("E_PLAN_INVALID","Authored User directory is missing or ambiguous");
  } else throw Error("E_PLAN_INVALID","Unknown authored User directory contract");
  launch.payloadRoots<<plan.installDir;launch.writableDirs<<user;
  launch.data={{"game",gameId.toStdString()},{"variant",variantId.toStdString()},{"quality",variant->quality.toStdString()},{"runtime",request.runtime.toStdString()}};
  launch.data["user_dir"]=user.toStdString();
  if(userMode=="executable-directory")launch.data["media_dir"]=install::scopedPath("roms",user).toStdString();
  return request;
}
QString lastLines(const QString &path, int count) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  if (f.size() > 256 * 1024)
    f.seek(f.size() - 256 * 1024);
  auto lines = QString::fromUtf8(f.readAll()).split('\n');
  if (!lines.isEmpty() && lines.last().isEmpty())
    lines.removeLast();
  return lines.mid(qMax(qsizetype(0), lines.size() - qMax(0, count)))
      .join('\n');
}
QStringList rotateLaunchLogs(const QString &root, const QString &folder,
                            const std::function<bool(const QString &)> &remove) {
  QStringList warnings;
  const auto oldLogs = QDir(folder).entryInfoList({"*.log"}, QDir::Files, QDir::Time);
  for (qsizetype i = 19; i < oldLogs.size(); ++i) {
    try {
      const auto old = install::scopedPath(oldLogs[i].absoluteFilePath(), root);
      if (!(remove ? remove(old) : QFile::remove(old)))
        warnings << "Could not rotate an old launch log; launch will continue";
    } catch (const std::exception &) {
      warnings << "Could not rotate an old launch log safely; launch will continue";
    }
  }
  return warnings;
}
LaunchService::LaunchService(QObject *parent) : QObject(parent) {
#ifdef Q_OS_WIN
  // Started suspended, so the program is in its process tree before it can start anything of its own.
  const auto suspended=[](QProcess::CreateProcessArguments *arguments){arguments->flags|=CREATE_SUSPENDED;};
  process_.setCreateProcessArgumentsModifier(suspended);helper_.setCreateProcessArgumentsModifier(suspended);
#endif
  // Completion metadata must remain available while scans/installations occupy
  // Qt's global pool. This owned pool has at most one pending session task.
  persistencePool_.setMaxThreadCount(1);
  persistencePool_.setExpiryTimeout(0);
  connect(&preparation_, &QFutureWatcher<QString>::finished, this, [this] {
    if (preparationCancelled_) {
      complete(1, "Launch cancelled");
      return;
    }
    const auto error = preparation_.result();
    if (!error.isEmpty()) {
      complete(1, error);
      return;
    }
    launchReady();
  });
  runtimeTimer_.setInterval(250);
  connect(&runtimeTimer_, &QTimer::timeout, this, [this] {
    if (processRunning({"vrserver.exe", "vrserver"})) {
      runtimeTimer_.stop();
      beginChild();
    } else if ((waitedMs_ += 250) >= 60000)
      complete(1, "SteamVR did not start within 60 seconds");
  });
  // The companion ending first means the session is over: close the main
  // program the ordinary way, and insist only if it stays.
  connect(&helper_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this] {
    if (busy_ && process_.state() == QProcess::Running) {
      process_.terminate();
      const auto generation = generation_;
      QTimer::singleShot(10000, &process_, [this, generation] {
        if (generation_ == generation && process_.state() != QProcess::NotRunning) endTree();
      });
    }
    emit playingChanged();
  });
  closeTimer_.setInterval(100);
  connect(&closeTimer_,&QTimer::timeout,this,[this]{if(closeRequested_)requestClose();else closeTimer_.stop();});
  connect(&process_, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
    if(state==QProcess::Starting)emit starting(request_.gameId,"Creating game process");
  });
  connect(&process_, &QProcess::started, this, [this] {
    childStarted_ = true;
    try {if(payload_) payload_->trackChild(process_.processId());}
    catch(const std::exception &) {
      emit warning(request_.gameId,"Could not preserve child usage evidence; stopping this launch for safety.");
      stop(); return;
    }
    if(closeRequested_){requestClose();return;}
    auto state =
        runtimeSource_ ? runtimeSource_(request_.gameId) : RuntimeState{};
    state.gameId = request_.gameId;
    state.playing = true;
    emit runtimeStateReady(state);
    emit started(request_.gameId);
  });
  connect(&process_, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
              complete(1, "Could not start game: " + process_.errorString());
          });
  connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int code, QProcess::ExitStatus status) {
            // A crash keeps its own code (0xC0000005 and the like) so it can be said in words.
            complete(status == QProcess::CrashExit && code == 0 ? 1 : code,
                     status == QProcess::CrashExit || code != 0
                         ? "The game stopped unexpectedly"
                         : QString());
          });
}
LaunchService::~LaunchService() {
  runtimeTimer_.stop();
  closeTimer_.stop();
  preparation_.disconnect(this);
  process_.disconnect(this);
  helper_.disconnect(this);
  if (helper_.state() != QProcess::NotRunning) { helper_.kill(); helper_.waitForFinished(1000); }
  // Whatever is left of every tree ends with the Hub.
  if (tree_) tree_->endWithHub();
  for (const auto &tree : leftovers_) tree->endWithHub();
  preparation_.waitForFinished();
  for (auto *watcher : persistence_) {
    watcher->disconnect(this);
    watcher->waitForFinished();
  }
  persistencePool_.waitForDone();
  if (process_.state() != QProcess::NotRunning) {
    process_.terminate();
    if (!process_.waitForFinished(1000)) {
      process_.kill();
      process_.waitForFinished(1000);
    }
  }
  if(payload_ && process_.state()==QProcess::NotRunning) payload_->clearPendingChildLaunch();
}
bool LaunchService::playing() const { return busy_; }
void LaunchService::setRuntimeStateSource(
    std::function<RuntimeState(const QString &)> source) {
  runtimeSource_ = std::move(source);
}
bool LaunchService::start(const Request &request) {
  if (busy())
    return false;
  const auto generation = ++generation_;
  closeTimer_.stop();closeRequested_=false;
  preparationCancelled_ = false;
  request_ = request;
  logPath_.clear();
  childStarted_ = false;
  try {
    const QRegularExpression id("^[a-z0-9]+(?:-[a-z0-9]+)*$");
    if (!id.match(request.gameId).hasMatch() ||
        !id.match(request.variantId).hasMatch())
      throw Error("E_ID", "Invalid launch identifier");
    if (!QStringList{"none", "any", "steamvr"}.contains(request.runtime))
      throw Error("E_RUNTIME", "Unsupported runtime");
    payload_=std::make_unique<install::ResourceLocks>(request.plan.payloadRoots,install::ResourceAccess::Use);
    const auto folder = install::scopedPath("user/locks", request.root);
    if (!QDir().mkpath(folder))
      throw Error("E_WRITE_DENIED", "Cannot create launch lock folder");
    lock_ = std::make_unique<QLockFile>(install::scopedPath(
        folder + "/launch-" + request.gameId + ".lock", request.root));
    lock_->setStaleLockTime(0);
    if (!lock_->tryLock(0))
      throw Error("E_ALREADY_RUNNING", "This game is already running");
    const auto logs =
        install::scopedPath("user/logs/" + request.gameId, request.root);
    if (!QDir().mkpath(logs))
      throw Error("E_WRITE_DENIED", "Cannot create launch log folder");
    const auto rotationWarnings = rotateLaunchLogs(request.root, logs);
    logPath_ = install::scopedPath(
        logs + "/" +
            QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") +
            "-" + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".log",
        request.root);
    install::atomicWrite(logPath_, QByteArray(product::display_name)+" launch\nruntime=" +
                                       request.runtime.toUtf8() + "\n");
    for (const auto &message : rotationWarnings)
      emit warning(request.gameId, message);
    busy_ = true;
    emit playingChanged();
    if (!busy_ || preparationCancelled_ || generation_ != generation)
      return false;
    if (request.prepareProfile) {
      emit starting(request.gameId, "Preparing emulator profile");
      if (!busy_ || preparationCancelled_ || generation_ != generation)
        return false;
      preparation_.setFuture(QtConcurrent::run([request] {
        try {
          install::prepareFlatLaunch(request.plan, request.root,
                                     request.materializeAliases);
          return QString();
        } catch (const std::exception &e) {
          return QString::fromUtf8(e.what());
        }
      }));
    } else
      launchReady();
    return true;
  } catch (const std::exception &e) {
    complete(1, QString::fromUtf8(e.what()));
    return false;
  }
}
void LaunchService::launchReady() {
  try {
    const auto &request = request_;
    if (!QFileInfo(request.plan.executable).isFile() ||
        !QFileInfo(request.plan.cwd).isDir() ||
        (!request.companion.executable.isEmpty() &&
         (!QFileInfo(request.companion.executable).isFile() || !QFileInfo(request.companion.cwd).isDir())))
      throw Error("E_LAUNCH_PATH",
                  "Launch executable or working directory missing");
    // Flat launches never inspect or start VR processes.
    if (request.runtime == "steamvr") {
      auto inputs = request.runtimeInputs;
      if (inputs.steamPath.isEmpty()) {
        const auto system = systemRuntimeInputs();
        inputs.steamPath = system.steamPath;
        inputs.registryRuntime = system.registryRuntime;
        inputs.home = system.home;
      }
      runtime_ = detectRuntime(inputs);
      if (runtime_.steamVrManifest.isEmpty())
        throw Error("E_STEAMVR_MISSING", "SteamVR was not found");
      // One place starts SteamVR, the Hub's Play in headset, and only when it is pressed; here it is only checked.
      if (!(vrServerProbe_ ? vrServerProbe_() : processRunning({"vrserver.exe", "vrserver"})))
        throw Error("E_STEAMVR_NOT_RUNNING", "SteamVR is not running. Use Play in headset, which starts it, or start SteamVR first.");
    } else if (request.pinSteamVrOpenXr) {
      // Only read where SteamVR's OpenXR runtime is; a PC without SteamVR keeps its own default.
      runtime_ = detectRuntime(systemRuntimeInputs());
    }
    beginChild();
  } catch (const std::exception &e) {
    complete(1, QString::fromUtf8(e.what()));
  }
}
void LaunchService::beginChild() {
  if (!busy_ || preparationCancelled_)
    return;
  try{for(auto i=request_.discIdentities.cbegin();i!=request_.discIdentities.cend();++i)verifyDisc(i.key(),i.value());}
  catch(const std::exception &e){complete(1,QString::fromUtf8(e.what()));return;}
  auto environment = request_.runtimeInputs.environment;
  if ((request_.runtime == "steamvr" || request_.pinSteamVrOpenXr) && !runtime_.steamVrManifest.isEmpty())
    environment.insert("XR_RUNTIME_JSON", runtime_.steamVrManifest);
  process_.setProcessEnvironment(environment);
  process_.setWorkingDirectory(request_.plan.cwd);
  process_.setProcessChannelMode(QProcess::MergedChannels);
  process_.setStandardOutputFile(logPath_, QIODevice::Append);
  try {if(payload_) payload_->prepareChildLaunch();}
  catch(const std::exception &error) {
    // No OS spawn has been attempted. Only this live holder can clear a
    // partially written reservation before reporting the failed launch.
    if(payload_) payload_->clearPendingChildLaunch();
    complete(1,QString::fromUtf8(error.what()));return;
  }
  tree_ = std::make_unique<ProcessTree>();
  sessionStartMs_ = QDateTime::currentMSecsSinceEpoch();
  auto arguments = request_.plan.args;
  if (!request_.companion.executable.isEmpty()) {
    helper_.setProcessEnvironment(environment);
    helper_.setWorkingDirectory(request_.companion.cwd);
    helper_.setProcessChannelMode(QProcess::MergedChannels);
    auto helperLog = logPath_;
    helperLog.replace(QRegularExpression("\\.log$"), "-companion.log");
    helper_.setStandardOutputFile(helperLog, QIODevice::Append);
    helper_.start(request_.companion.executable, request_.companion.args);
    if (helper_.processId()) tree_->adopt(helper_.processId());
    // Never waited for on the GUI thread: the process exists, with its id, once start() has returned,
    // or it could not be made at all.
    if (helper_.state() == QProcess::NotRunning || !helper_.processId()) {
      if (payload_) payload_->clearPendingChildLaunch();
      complete(1, "Could not start the companion program: " + helper_.errorString());
      return;
    }
    for (auto &argument : arguments) argument.replace("${companion_pid}", QString::number(helper_.processId()));
  }
  process_.start(request_.plan.executable, arguments);
  if (process_.processId()) tree_->adopt(process_.processId());
}
void LaunchService::requestClose() {
  if(!busy_)return;
  closeRequested_=true;
  if(process_.state()==QProcess::NotRunning) {
    if(childStarted_)return; // Let the queued child-finished signal own completion.
    preparationCancelled_=true;
    runtimeTimer_.stop();
    // Preparation still owns the payload until its finished callback unwinds.
    if(!preparation_.isRunning())complete(1,"Launch cancelled");
    return;
  }
  // Windows QProcess::terminate posts WM_CLOSE to this child's windows.
  // No force timer: scores/settings must get their ordinary shutdown path.
#ifdef Q_OS_WIN
  if(childStarted_&&process_.state()==QProcess::Running){
    // Process creation can precede its first window. Retain the close intent
    // and usage lease until that owned window can receive the normal request.
    if(hasCloseWindow(process_.processId())){closeTimer_.stop();process_.terminate();}
    else if(!closeTimer_.isActive())closeTimer_.start();
  }
#endif
}
void LaunchService::stop() {
  if (!busy_)
    return;
  preparationCancelled_ = true;
  if (preparation_.isRunning())
    return;
  else if (runtimeTimer_.isActive())
    complete(1, "Launch cancelled");
  else if (process_.state() == QProcess::NotRunning)
    complete(1, "Launch cancelled");
  else {
    process_.terminate();
    const auto generation = generation_;
    // Its windows are asked to close; whatever is left after a few seconds is ended, tree and all.
    QTimer::singleShot(5000, &process_, [this, generation] {
      if (generation_ == generation && process_.state() != QProcess::NotRunning)
        endTree();
    });
  }
}
void LaunchService::endTree() {
  if (tree_) tree_->end();
  if (process_.state() != QProcess::NotRunning) process_.kill();
  if (helper_.state() != QProcess::NotRunning) helper_.kill();
}
// The game has ended: anything it left running is asked to close, then ended.
void LaunchService::retireTree() {
  QList<qint64> strays = strayHelpers(request_.helperPrograms, sessionStartMs_);
  std::shared_ptr<ProcessTree> tree(tree_.release());
  if (tree && tree->members().isEmpty()) tree.reset();
  if (!tree && strays.isEmpty()) return;
  if (tree) { tree->endWithHub(); tree->closeWindows(); leftovers_ << tree; }
  closeProcessWindows(strays);
  QTimer::singleShot(3000, this, [this, tree, strays] {
    if (tree) { tree->end(); leftovers_.removeOne(tree); }
    endProcesses(strays);
  });
}
void LaunchService::endEverything() {
  if (busy_) { preparationCancelled_ = true; closeRequested_ = false; endTree(); }
  for (const auto &tree : leftovers_) tree->end();
  leftovers_.clear();
  endProcesses(strayHelpers(request_.helperPrograms, sessionStartMs_));
}
QList<qint64> LaunchService::sessionProcesses() const {
  QList<qint64> result;
  if (tree_) result << tree_->members();
  if (process_.state() != QProcess::NotRunning && !result.contains(process_.processId())) result << process_.processId();
  if (helper_.state() != QProcess::NotRunning && !result.contains(helper_.processId())) result << helper_.processId();
  return result;
}
QList<qint64> LaunchService::ownedProcesses() const {
  QList<qint64> result;
  if (tree_) result << tree_->members();
  for (const auto &tree : leftovers_) result << tree->members();
  return result;
}
void LaunchService::complete(int code, const QString &error) {
  if (completing_)
    return;
  // Reserve this session through all advisory persistence and completion signals.
  // playing() can be false while busy() still prevents a reentrant new launch.
  completing_ = true;
  const auto generation = generation_;
  runtimeTimer_.stop();
  closeTimer_.stop();closeRequested_=false;
  lock_.reset();
  // A companion has no window to close, so it is given a moment and then ended.
  if (helper_.state() != QProcess::NotRunning) {
    helper_.terminate();
    QTimer::singleShot(4000, &helper_, [this] { if (helper_.state() != QProcess::NotRunning) helper_.kill(); });
  }
  retireTree();
  if(payload_ && process_.state()==QProcess::NotRunning) payload_->clearPendingChildLaunch();
  payload_.reset();
  const auto root = request_.root, gameId = request_.gameId,
             variantId = request_.variantId, logPath = logPath_;
  QString message = error;
  if (!error.isEmpty() && !logPath.isEmpty())
    message += "\n" + lastLines(logPath);
  const bool wasBusy = busy_, ranChild = childStarted_;
  busy_ = false;
  childStarted_ = false;
  const auto now = QDateTime::currentSecsSinceEpoch();
  if (ranChild) {
    auto state = runtimeSource_ ? runtimeSource_(gameId) : RuntimeState{};
    state.gameId = gameId;
    state.lastPlayed = now;
    state.selectedVariantId = variantId;
    state.playing = false;
    emit runtimeStateReady(state);
  }
  if (wasBusy)
    emit playingChanged();
  const auto finish = [this, gameId, code, message, logPath, ranChild, generation] {
    completing_ = false;
    emit finished(gameId, code, message, logPath);
    // A finished handler may legitimately launch another game. Never raise the
    // Hub for this completed session over its newly started replacement.
    if (ranChild && generation_ == generation)
      emit raiseHubRequested();
  };
  if (!ranChild) {
    finish();
    return;
  }
  // State I/O and the bounded contention wait happen on the dedicated worker.
  // Deliver completion after the advisory so CLI callers can log it before exit.
  auto *watcher = new QFutureWatcher<QString>(this);
  persistence_ << watcher;
  connect(watcher, &QFutureWatcher<QString>::finished, this,
          [this, watcher, gameId, finish] {
    const auto advisory = watcher->result();
    persistence_.removeOne(watcher);
    watcher->deleteLater();
    if (!advisory.isEmpty())
      emit warning(gameId, advisory);
    finish();
  });
  watcher->setFuture(QtConcurrent::run(&persistencePool_, [root, gameId, now] {
    try {
      const auto path = install::scopedPath("user/last-played.json", root);
      QLockFile stateLock(install::scopedPath("user/locks/last-played.lock", root));
      stateLock.setStaleLockTime(0);
      if (!stateLock.tryLock(1500))
        throw Error("E_LOCKED", "Last-played state is locked by another Hub");
      Json times = Json::object();
      if (QFileInfo::exists(path))
        times = Json::parse(install::readBytes(path).toStdString());
      times[gameId.toStdString()] = now;
      install::atomicWrite(path, QByteArray::fromStdString(times.dump(2)));
      return QString();
    } catch (const std::exception &e) {
      return "Could not record last played: " + QString::fromUtf8(e.what());
    }
  }));
}
} // namespace ac::launch
