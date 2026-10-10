// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/catalog/GameRecord.h"
#include <QFutureWatcher>
#include <QObject>
#include <atomic>
#include <functional>
#include <memory>
namespace ac::install {
struct Error : std::runtime_error {
  QString code;
  Error(QString c, QString message)
      : std::runtime_error(message.toStdString()), code(std::move(c)) {}
};
struct Request {
  QString root, catalogRoot, gameId, variantId, handover, operation = "install";
  Json recipe = Json::object(), bindings = Json::object(),
       settings = Json::object(), profile = Json::object();
  RuntimeState runtime;
  QStringList skipSteps;
  bool consentAccepted = false, allowMediaCopy = false;
};
struct Plan {
  QString gameId, variantId, installDir, version, text;
  Json variant = Json::object(), steps = Json::array();
};
struct Result {
  bool success = false;
  int exitCode = 1;
  QString state, code, message;
  Json manifest = Json::object();
};
using EventSink = std::function<void(const QVariantMap &)>;
using FaultHook = std::function<void(const QString &)>;
struct Options {
  EventSink event;
  FaultHook fault;
  std::atomic_bool *cancel = nullptr;
  int survivalMs = 3000;
  std::function<Json(const Json &)> shortcut;
  // Explicit dependency injection for synthetic cleanup-failure tests only.
  std::function<bool(const QString &)> mediaRemovalPurge;
};
QString sha256(const QByteArray &bytes);
QString hashFile(const QString &path);
QByteArray canonicalJson(const Json &data);
Json readEnvelope(const QString &path, QStringList *warnings = nullptr);
void writeEnvelope(const QString &path, const Json &data);
class Engine {
public:
  explicit Engine(Options options = {});
  Plan plan(const Request &request) const;
  Result install(const Request &request);
  Result uninstall(const Request &request);
  Result recover(const Request &request);
  // Purge only UUID quarantine runs proven terminal by strict local journals.
  // Interrupted, orphan, corrupt and unsafe folders are deliberately retained.
  QStringList sweepFinishedMediaRemovals(const QString &root) const;
  Json uninstallPreview(const Request &request) const;
  RuntimeState runtimeState(const Request &request) const;
  static Request emulatorRequest(const QString &root, const Json &manifest,
                                 bool consent = false);
  static Json locateTool(const QString &root, const Json &manifest,
                         const QString &path);

private:
  Options options_;
};
struct LaunchPlan {
  QString executable, cwd;
  QStringList args, writableDirs;
  QStringList payloadRoots; // Original emulator/setup resources held while in use.
  Json data = Json::object();
};
LaunchPlan makeFlatLaunchPlan(const GameRecord &game, const Json &emulator,
                              const QString &root, const Json &bindings);
// Applies only the explicit allowlisted preparation plan to Hub-owned user
// profiles. Existing profile config is never read: it may contain account data
// after a user launch.
Json prepareFlatLaunch(const LaunchPlan &plan, const QString &root,
                       bool materializeAliases = false);
class InstallService : public QObject {
  Q_OBJECT
public:
  explicit InstallService(QObject *parent = nullptr);
  ~InstallService() override;
  void start(const Request &request);
  Q_INVOKABLE void cancel();
  bool busy() const;
  void
  setRuntimeStateSource(std::function<RuntimeState(const QString &)> source);
signals:
  void event(const QVariantMap &value);
  void finished(bool success, const QString &message);
  void runtimeStateReady(const ac::RuntimeState &state);

private:
  std::atomic_bool cancelled_{false};
  QFutureWatcher<Result> watcher_;
  Request request_;
  QList<Request> pending_;
  std::function<RuntimeState(const QString &)> runtimeSource_;
};
} // namespace ac::install
