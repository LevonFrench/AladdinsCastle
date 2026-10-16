// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <functional>
namespace ac::launch {
// One running program, as the watchdog sees it.
struct ProcessInfo {
  qint64 pid = 0, parent = 0, createdMs = 0;
  QString path, name;
};
// Every running program with its full path (Windows); empty elsewhere.
QList<ProcessInfo> listProcesses();
// The top-level windows of these processes: any at all, any visible, all visible ones minimised,
// any not responding (Windows' own "not responding" test).
struct WindowState {
  bool any = false, visible = false, minimized = false, hung = false;
};
WindowState windowState(const QList<qint64> &pids);
// Restores and raises the first window of these processes. False when there is none.
bool bringToFront(const QList<qint64> &pids);
// Crash exit codes in words: an access violation, a stack overflow and the like. Empty for an
// ordinary exit, including the code the Hub itself ends a game with.
bool isCrashCode(quint32 code);
QString exitCodeMeaning(quint32 code);
QString exitCodeText(int code);
// SteamVR and Steam are never the watchdog's to end, wherever they live and whoever started them.
bool neverTouch(const ProcessInfo &process);

// What belongs to a system: every program in its own program folders, and programs with one of its
// known names under its other configured folders. Nothing else is ever the watchdog's.
struct SystemRule {
  QString system, label;
  QStringList ownFolders, folders, names; // names may end in '*'
};
QString attribute(const ProcessInfo &process, const QList<SystemRule> &rules);

class Watchdog : public QObject {
  Q_OBJECT
public:
  // A game the Hub started: what it is, when, whether it shows a window, and its processes now.
  struct Session {
    QString gameId, title, system;
    qint64 startMs = 0;
    bool expectWindow = true;
    std::function<QList<qint64>()> pids;
  };
  explicit Watchdog(QObject *parent = nullptr);
  void setRules(const QList<SystemRule> &rules);
  // Stand-ins for the tests; the real process list and window probe otherwise.
  void setLister(std::function<QList<ProcessInfo>()> lister) { lister_ = std::move(lister); }
  void setWindowProbe(std::function<WindowState(const QList<qint64> &)> probe) { probe_ = std::move(probe); }
  // How an ending is done: graceful first (a stop file, closing windows), then the whole tree.
  void setGraceful(std::function<void(const QString &system, const QList<qint64> &)> graceful) { graceful_ = std::move(graceful); }
  void setEnder(std::function<void(const QList<qint64> &)> ender) { ender_ = std::move(ender); }
  void setTimings(int scanMs, int troubleMs, int startGraceMs, int endGraceMs);
  void start();
  void scan();
  void beginSession(const Session &session);
  void endSession();
  // Programs of the configured systems running outside any game the Hub is running now:
  // left behind by a crash or a closed window, or started some other way.
  QVariantList strays() const { return strays_; }
  // The running game's state when something is wrong: "hung", "hidden", "minimized"; empty when fine.
  QString trouble() const { return trouble_; }
  // Windows Error Reporting opened for one of these programs: its process, and the program's.
  void end(qint64 pid);
  void endAll();
  // Ends this system's background programs started since sinceMs: what a crashed game left behind.
  void endSince(const QString &system, qint64 sinceMs);
  bool bringSessionToFront();
signals:
  void straysChanged();
  void troubleChanged();
  void errorReportSeen(qint64 reportPid, qint64 forPid, const QString &system);

private:
  void endTrees(const QList<qint64> &roots, const QString &system);
  QList<qint64> treeOf(qint64 root) const;
  QList<SystemRule> rules_;
  std::function<QList<ProcessInfo>()> lister_ = listProcesses;
  std::function<WindowState(const QList<qint64> &)> probe_ = windowState;
  std::function<void(const QString &, const QList<qint64> &)> graceful_;
  std::function<void(const QList<qint64> &)> ender_;
  QTimer timer_;
  QList<ProcessInfo> last_;
  QHash<qint64, QString> systemOf_;
  QVariantList strays_;
  QString trouble_;
  Session session_;
  bool inSession_ = false;
  qint64 troubleSince_ = 0;
  QString troubleSeen_;
  QList<qint64> reported_;
  int troubleMs_ = 5000, startGraceMs_ = 15000, endGraceMs_ = 4000;
};
} // namespace ac::launch
