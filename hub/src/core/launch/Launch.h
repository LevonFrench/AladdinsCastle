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
#include "core/launch/ProcessTree.h"
#include <memory>
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
  // A second program some setups need running first, such as an emulator that
  // feeds a separate viewer. It is started before the main program with the
  // same environment, "${companion_pid}" in the main arguments becomes its
  // process id, and it is stopped when the main program ends. Empty = none.
  install::LaunchPlan companion;
  QString runtime = "none";
  RuntimeInputs runtimeInputs;
  bool prepareProfile = true, materializeAliases = false;
  QMap<QString,QString> discIdentities;
  // Helper programs Windows may start for the game outside its process tree (a
  // backglass server, for one): those started during the session end with it.
  QStringList helperPrograms;
};
Json normalizeBindings(const Json &bindings, const QString &gameId);
Request flatRequest(const CatalogData &catalog, const QString &gameId,
                    const QString &root, const Json &bindings,
                    const QString &variantId = {});
Request gameRequest(const CatalogData &catalog, const QString &gameId,
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
  bool busy() const { return playing() || completing_ || !persistence_.isEmpty() || helper_.state() != QProcess::NotRunning; }
  bool start(const Request &request);
  Q_INVOKABLE void stop();
  // Ends the running game's whole process tree at once, and anything left over from
  // earlier sessions the Hub started: the "Stop everything" button.
  Q_INVOKABLE void endEverything();
  // Processes still alive from games the Hub started, the running one included.
  QList<qint64> ownedProcesses() const;
  // The running game's own processes only: its tree, the program and its companion.
  QList<qint64> sessionProcesses() const;
  // What the Hub started, for diagnostics and the launch proof.
  qint64 processId() const { return process_.processId(); }
  QString processProgram() const { return process_.program(); }
  qint64 companionProcessId() const { return helper_.processId(); }
  bool childRunning() const { return process_.state() == QProcess::Running; }
  bool companionRunning() const { return helper_.state() == QProcess::Running; }
  Q_INVOKABLE void requestClose();
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
  QProcess helper_;
  QTimer runtimeTimer_;
  QTimer closeTimer_;
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
  bool closeRequested_ = false;
  // The running session's process tree, trees left over after their game ended, and when the session began.
  std::unique_ptr<ProcessTree> tree_;
  QList<std::shared_ptr<ProcessTree>> leftovers_;
  qint64 sessionStartMs_ = 0;
  void endTree();
  void retireTree();
};
} // namespace ac::launch
