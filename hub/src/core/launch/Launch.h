// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/catalog/CatalogLoader.h"
#include "core/install/Install.h"
#include "core/install/Support.h"
#include <QFutureWatcher>
#include <QLockFile>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QThreadPool>
#include <functional>
namespace ac::launch {
struct RuntimeInputs {
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  QString registryRuntime, steamPath, home;
};
struct RuntimeInfo {
  QString active, source, steamVrManifest, steamVrMonitor;
};
RuntimeInputs systemRuntimeInputs();
RuntimeInfo detectRuntime(const RuntimeInputs &inputs);
bool processRunning(const QStringList &names);
struct Request {
  QString root, gameId, variantId;
  install::LaunchPlan plan;
  QString runtime = "none";
  RuntimeInputs runtimeInputs;
  bool prepareProfile = true, materializeAliases = false;
};
Json normalizeBindings(const Json &bindings, const QString &gameId);
Request flatRequest(const CatalogData &catalog, const QString &gameId,
                    const QString &root, const Json &bindings,
                    const QString &variantId = {});
QString lastLines(const QString &log, int count = 40);
QStringList rotateLaunchLogs(const QString &root, const QString &folder,
                            const std::function<bool(const QString &)> &remove = {});
class LaunchService : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
public:
  explicit LaunchService(QObject *parent = nullptr);
  ~LaunchService() override;
  bool playing() const;
  bool busy() const { return playing() || completing_ || !persistence_.isEmpty(); }
  bool start(const Request &request);
  Q_INVOKABLE void stop();
  void
  setRuntimeStateSource(std::function<RuntimeState(const QString &)> source);
signals:
  void playingChanged();
  void starting(const QString &gameId, const QString &message);
  void started(const QString &gameId);
  void finished(const QString &gameId, int exitCode, const QString &error,
                const QString &logPath);
  void runtimeStateReady(const ac::RuntimeState &state);
  void raiseHubRequested();
  void warning(const QString &gameId, const QString &message);

private:
  void beginChild();
  void launchReady();
  void complete(int exitCode, const QString &error);
  // Member ordering retains use through QProcess destruction on teardown.
  std::unique_ptr<install::ResourceLocks> payload_;
  QProcess process_;
  QTimer runtimeTimer_;
  QFutureWatcher<QString> preparation_;
  QList<QFutureWatcher<QString> *> persistence_;
  QThreadPool persistencePool_;
  Request request_;
  RuntimeInfo runtime_;
  std::unique_ptr<QLockFile> lock_;
  std::function<RuntimeState(const QString &)> runtimeSource_;
  QString logPath_;
  int waitedMs_ = 0;
  bool busy_ = false, childStarted_ = false, completing_ = false;
  quint64 generation_ = 0;
  bool preparationCancelled_ = false;
};
} // namespace ac::launch
