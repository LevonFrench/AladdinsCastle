// SPDX-License-Identifier: GPL-3.0-only
#include "Launch.h"
#include "core/install/Support.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>
namespace ac::launch {
using install::Error;
using install::string;
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
  connect(&process_, &QProcess::started, this, [this] {
    childStarted_ = true;
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
            complete(status == QProcess::CrashExit ? qMax(1, code) : code,
                     status == QProcess::CrashExit || code != 0
                         ? "The game stopped unexpectedly"
                         : QString());
          });
}
LaunchService::~LaunchService() {
  runtimeTimer_.stop();
  preparation_.disconnect(this);
  preparation_.waitForFinished();
  for (auto *watcher : persistence_)
    watcher->waitForFinished();
  if (process_.state() != QProcess::NotRunning) {
    process_.terminate();
    if (!process_.waitForFinished(1000)) {
      process_.kill();
      process_.waitForFinished(1000);
    }
  }
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
    install::atomicWrite(logPath_, "AladdinsCastle launch\nruntime=" +
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
        !QFileInfo(request.plan.cwd).isDir())
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
      if (!processRunning({"vrserver.exe", "vrserver"})) {
        if (runtime_.steamVrMonitor.isEmpty())
          throw Error("E_STEAMVR_MISSING", "SteamVR launcher was not found");
        if (!QProcess::startDetached(runtime_.steamVrMonitor, {}))
          throw Error("E_STEAMVR_START", "Could not start SteamVR");
        waitedMs_ = 0;
        emit starting(request.gameId, "Starting SteamVR");
        if (busy_ && !preparationCancelled_)
          runtimeTimer_.start();
        return;
      }
    }
    beginChild();
  } catch (const std::exception &e) {
    complete(1, QString::fromUtf8(e.what()));
  }
}
void LaunchService::beginChild() {
  if (!busy_ || preparationCancelled_)
    return;
  auto environment = request_.runtimeInputs.environment;
  if (request_.runtime == "steamvr")
    environment.insert("XR_RUNTIME_JSON", runtime_.steamVrManifest);
  process_.setProcessEnvironment(environment);
  process_.setWorkingDirectory(request_.plan.cwd);
  process_.setProcessChannelMode(QProcess::MergedChannels);
  process_.setStandardOutputFile(logPath_, QIODevice::Append);
  process_.start(request_.plan.executable, request_.plan.args);
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
    QTimer::singleShot(3000, &process_, [this, generation] {
      if (generation_ == generation && process_.state() != QProcess::NotRunning)
        process_.kill();
    });
  }
}
void LaunchService::complete(int code, const QString &error) {
  if (completing_)
    return;
  // Reserve this session through all advisory persistence and completion signals.
  // playing() can be false while busy() still prevents a reentrant new launch.
  completing_ = true;
  const auto generation = generation_;
  runtimeTimer_.stop();
  lock_.reset();
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
  // State I/O is advisory and happens off the GUI thread; contention skips it.
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
  watcher->setFuture(QtConcurrent::run([root, gameId, now] {
    try {
      const auto path = install::scopedPath("user/last-played.json", root);
      QLockFile stateLock(install::scopedPath("user/locks/last-played.lock", root));
      stateLock.setStaleLockTime(0);
      if (!stateLock.tryLock(0))
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
