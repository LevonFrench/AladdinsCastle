// SPDX-License-Identifier: GPL-3.0-only
#include "Watchdog.h"
#include "ProcessTree.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSet>
#include <QVariantMap>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif
namespace ac::launch {
namespace {
QString clean(const QString &path) { return QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower(); }
bool under(const QString &path, const QString &folder) {
  const auto f = clean(folder);
  return !f.isEmpty() && path.startsWith(f + "/");
}
bool named(const QString &name, const QStringList &patterns) {
  auto base = name.toLower();
  if (base.endsWith(".exe"))
    base.chop(4);
  for (auto pattern : patterns) {
    pattern = pattern.toLower();
    if (pattern.endsWith('*') ? base.startsWith(pattern.chopped(1)) : base == pattern)
      return true;
  }
  return false;
}
#ifdef Q_OS_WIN
qint64 msSinceEpoch(const FILETIME &time) {
  ULARGE_INTEGER value;
  value.LowPart = time.dwLowDateTime;
  value.HighPart = time.dwHighDateTime;
  return qint64(value.QuadPart / 10000ULL) - 11644473600000LL;
}
struct WindowSearch {
  QSet<qint64> pids;
  WindowState state;
  bool allMinimized = true;
  HWND first = nullptr;
};
BOOL CALLBACK inspect(HWND window, LPARAM parameter) {
  auto *search = reinterpret_cast<WindowSearch *>(parameter);
  DWORD pid = 0;
  GetWindowThreadProcessId(window, &pid);
  if (!search->pids.contains(qint64(pid)) || GetWindow(window, GW_OWNER) != nullptr)
    return TRUE;
  search->state.any = true;
  if (!search->first)
    search->first = window;
  if (IsWindowVisible(window)) {
    if (!search->state.visible)
      search->first = window;
    search->state.visible = true;
    search->allMinimized = search->allMinimized && IsIconic(window);
  }
  if (IsHungAppWindow(window))
    search->state.hung = true;
  return TRUE;
}
#endif
} // namespace

QList<ProcessInfo> listProcesses() {
  QList<ProcessInfo> result;
#ifdef Q_OS_WIN
  const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    return result;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof entry;
  for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry)) {
    ProcessInfo info;
    info.pid = qint64(entry.th32ProcessID);
    info.parent = qint64(entry.th32ParentProcessID);
    info.name = QString::fromWCharArray(entry.szExeFile);
    if (const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID)) {
      wchar_t path[2048];
      DWORD size = 2048;
      if (QueryFullProcessImageNameW(process, 0, path, &size))
        info.path = QString::fromWCharArray(path, int(size));
      FILETIME created, exited, kernel, user;
      if (GetProcessTimes(process, &created, &exited, &kernel, &user))
        info.createdMs = msSinceEpoch(created);
      CloseHandle(process);
    }
    result << info;
  }
  CloseHandle(snapshot);
#endif
  return result;
}
WindowState windowState(const QList<qint64> &pids) {
#ifdef Q_OS_WIN
  WindowSearch search;
  for (const auto pid : pids)
    search.pids.insert(pid);
  if (!search.pids.isEmpty())
    EnumWindows(inspect, reinterpret_cast<LPARAM>(&search));
  search.state.minimized = search.state.visible && search.allMinimized;
  return search.state;
#else
  Q_UNUSED(pids);
  return {};
#endif
}
bool bringToFront(const QList<qint64> &pids) {
#ifdef Q_OS_WIN
  WindowSearch search;
  for (const auto pid : pids)
    search.pids.insert(pid);
  if (search.pids.isEmpty())
    return false;
  EnumWindows(inspect, reinterpret_cast<LPARAM>(&search));
  if (!search.first)
    return false;
  ShowWindow(search.first, IsIconic(search.first) ? SW_RESTORE : SW_SHOW);
  SetForegroundWindow(search.first);
  return true;
#else
  Q_UNUSED(pids);
  return false;
#endif
}
bool isCrashCode(quint32 code) {
  // Errors in Windows' own status codes, a breakpoint, or an uncaught C++ exception. Not a
  // console closed with Ctrl+C, and not the 1 the Hub ends a game with.
  if (code == 0xC000013A)
    return false;
  return (code & 0xF0000000) == 0xC0000000 || code == 0x80000003 || code == 0x80000002 || code == 0xE06D7363;
}
QString exitCodeMeaning(quint32 code) {
  switch (code) {
  case 0xC0000005: return "an access violation (it read or wrote memory it may not)";
  case 0xC00000FD: return "a stack overflow";
  case 0xC0000409: return "a stack buffer overrun, or the program ended itself on a fatal error";
  case 0xC0000374: return "heap corruption";
  case 0xC000001D: return "an illegal instruction (the processor cannot run this build)";
  case 0xC0000096: return "a privileged instruction";
  case 0xC0000094: return "a division by zero";
  case 0xC0000135: return "a missing DLL";
  case 0xC0000142: return "a DLL that failed to start";
  case 0xC0000417: return "an invalid parameter in the C runtime";
  case 0xC0000017: return "out of memory";
  case 0xC0000022: return "access denied";
  case 0xC0000008: return "an invalid handle";
  case 0xC0000602: return "a fail-fast error";
  case 0xC000041D: return "an error inside a Windows callback";
  case 0x80000003: return "a breakpoint (a debug check failed)";
  case 0x80000002: return "misaligned data";
  case 0xE06D7363: return "an uncaught C++ exception";
  default: break;
  }
  return isCrashCode(code) ? QString("a Windows error") : QString();
}
QString exitCodeText(int code) {
  const auto value = quint32(code);
  const auto hex = QString("0x%1").arg(value, 8, 16, QLatin1Char('0')).toUpper().replace("0X", "0x");
  const auto meaning = exitCodeMeaning(value);
  return meaning.isEmpty() ? QString("exit code %1").arg(code) : meaning + " (" + hex + ")";
}
bool neverTouch(const ProcessInfo &process) {
  static const QStringList steamVr{"vrserver", "vrcompositor", "vrmonitor", "vrdashboard", "vrstartup",
                                   "vrwebhelper", "vrprismhost", "vrcmd", "vrpathreg", "vrservice"};
  const auto path = clean(process.path);
  return process.pid <= 4 || named(process.name, steamVr) || named(process.name, {"steam*", "gameoverlayui*"}) ||
         path.contains("/steamvr/") || path.contains("/steamapps/common/steamvr") ||
         process.pid == QCoreApplication::applicationPid();
}
QString attribute(const ProcessInfo &process, const QList<SystemRule> &rules) {
  if (process.path.isEmpty() || neverTouch(process))
    return {};
  const auto path = clean(process.path);
  const auto folder = path.section('/', 0, -2);
  for (const auto &rule : rules) {
    for (const auto &own : rule.ownFolders)
      if (!own.isEmpty() && folder == clean(own))
        return rule.system;
    for (const auto &other : QStringList(rule.folders) + rule.ownFolders)
      if (under(path, other) && named(process.name, rule.names))
        return rule.system;
  }
  return {};
}

Watchdog::Watchdog(QObject *parent) : QObject(parent) {
  timer_.setInterval(3000);
  connect(&timer_, &QTimer::timeout, this, &Watchdog::scan);
}
void Watchdog::setRules(const QList<SystemRule> &rules) { rules_ = rules; }
void Watchdog::setTimings(int scanMs, int troubleMs, int startGraceMs, int endGraceMs) {
  timer_.setInterval(scanMs);
  troubleMs_ = troubleMs;
  startGraceMs_ = startGraceMs;
  endGraceMs_ = endGraceMs;
}
void Watchdog::start() {
  scan();
  timer_.start();
}
void Watchdog::beginSession(const Session &session) {
  session_ = session;
  inSession_ = true;
  troubleSeen_.clear();
  troubleSince_ = 0;
  scan();
}
void Watchdog::endSession() {
  inSession_ = false;
  session_ = {};
  if (!trouble_.isEmpty()) {
    trouble_.clear();
    emit troubleChanged();
  }
  scan();
}
void Watchdog::scan() {
  last_ = lister_ ? lister_() : QList<ProcessInfo>{};
  QHash<qint64, ProcessInfo> byPid;
  for (const auto &p : last_)
    byPid.insert(p.pid, p);
  const auto sessionPids = inSession_ && session_.pids ? session_.pids() : QList<qint64>{};
  const auto inSessionTree = [&](const ProcessInfo &p) {
    // The running game's own processes, and whatever they started that is still alive.
    for (auto at = p; at.pid > 0;) {
      if (sessionPids.contains(at.pid))
        return true;
      const auto parent = byPid.value(at.parent);
      if (parent.pid <= 0 || parent.createdMs > at.createdMs)
        return false;
      at = parent;
    }
    return false;
  };
  QVariantList strays;
  systemOf_.clear();
  for (const auto &p : last_) {
    if (neverTouch(p))
      continue;
    auto system = attribute(p, rules_);
    QString label;
    if (system.isEmpty() && p.name.compare("WerFault.exe", Qt::CaseInsensitive) == 0) {
      // Windows Error Reporting, opened by a crashing program it reports on (its parent).
      const auto parent = byPid.value(p.parent);
      const bool forSession = parent.pid > 0 && inSessionTree(parent);
      const auto parentSystem = parent.pid > 0 ? attribute(parent, rules_) : QString();
      if (!forSession && parentSystem.isEmpty())
        continue;
      system = forSession ? session_.system : parentSystem;
      if (!reported_.contains(p.pid)) {
        reported_ << p.pid;
        emit errorReportSeen(p.pid, parent.pid, system);
      }
      if (forSession)
        continue;
      label = "Windows Error Reporting for " + parent.name;
    }
    if (system.isEmpty())
      continue;
    systemOf_.insert(p.pid, system);
    if (inSessionTree(p))
      continue;
    // Started for the running game outside its tree (a backglass server, a viewer): part of it.
    if (inSession_ && system == session_.system && p.createdMs >= session_.startMs - 2000)
      continue;
    const auto rule = std::find_if(rules_.cbegin(), rules_.cend(), [&](const SystemRule &r) { return r.system == system; });
    strays << QVariantMap{{"pid", p.pid}, {"name", p.name}, {"system", system},
                          {"label", label.isEmpty() ? (rule != rules_.cend() ? rule->label : system) + " · " + p.name : label},
                          {"since", p.createdMs}};
  }
  if (strays != strays_) {
    strays_ = strays;
    emit straysChanged();
  }
  // The running game: not responding, without a visible window, or minimised, for a while.
  QString now;
  const auto clock = QDateTime::currentMSecsSinceEpoch();
  if (inSession_ && !sessionPids.isEmpty() && clock - session_.startMs >= startGraceMs_) {
    const auto state = probe_ ? probe_(sessionPids) : WindowState{};
    if (state.hung)
      now = "hung";
    else if (session_.expectWindow && !state.visible)
      now = "hidden";
    else if (state.visible && state.minimized)
      now = "minimized";
  }
  if (now != troubleSeen_) {
    troubleSeen_ = now;
    troubleSince_ = clock;
  }
  // Said once it has lasted troubleMs; cleared as soon as it is over.
  const auto shown = now.isEmpty() ? QString() : clock - troubleSince_ >= troubleMs_ ? now : (trouble_ == now ? now : QString());
  if (shown != trouble_) {
    trouble_ = shown;
    emit troubleChanged();
  }
}
QList<qint64> Watchdog::treeOf(qint64 root) const {
  QList<qint64> tree{root};
  QHash<qint64, ProcessInfo> byPid;
  for (const auto &p : last_)
    byPid.insert(p.pid, p);
  for (int i = 0; i < tree.size(); ++i)
    for (const auto &p : last_)
      if (p.parent == tree[i] && !tree.contains(p.pid) && !neverTouch(p) &&
          p.createdMs >= byPid.value(tree[i]).createdMs)
        tree << p.pid;
  return tree;
}
void Watchdog::endTrees(const QList<qint64> &roots, const QString &system) {
  QList<qint64> all;
  QHash<qint64, qint64> created;
  for (const auto root : roots)
    for (const auto pid : treeOf(root))
      if (!all.contains(pid))
        all << pid;
  for (const auto &p : last_)
    if (all.contains(p.pid))
      created.insert(p.pid, p.createdMs);
  // Graceful first: the system's own way to quit, or closing the windows.
  if (graceful_)
    graceful_(system, all);
  else
    closeProcessWindows(all);
  QTimer::singleShot(endGraceMs_, this, [this, all, created] {
    // Only the very same processes: a number Windows has given to a new one is left alone.
    QList<qint64> left;
    for (const auto &p : lister_ ? lister_() : QList<ProcessInfo>{})
      if (all.contains(p.pid) && created.value(p.pid) == p.createdMs && !neverTouch(p))
        left << p.pid;
    if (!left.isEmpty())
      (ender_ ? ender_ : endProcesses)(left);
    scan();
  });
}
void Watchdog::end(qint64 pid) {
  for (const auto &value : strays_) {
    const auto row = value.toMap();
    if (row.value("pid").toLongLong() == pid) {
      endTrees({pid}, row.value("system").toString());
      return;
    }
  }
}
void Watchdog::endAll() {
  QHash<QString, QList<qint64>> bySystem;
  for (const auto &value : strays_) {
    const auto row = value.toMap();
    bySystem[row.value("system").toString()] << row.value("pid").toLongLong();
  }
  for (auto it = bySystem.cbegin(); it != bySystem.cend(); ++it)
    endTrees(it.value(), it.key());
}
void Watchdog::endSince(const QString &system, qint64 sinceMs) {
  QList<qint64> roots;
  for (const auto &value : strays_) {
    const auto row = value.toMap();
    if (row.value("system") == system && row.value("since").toLongLong() >= sinceMs)
      roots << row.value("pid").toLongLong();
  }
  if (!roots.isEmpty())
    endTrees(roots, system);
}
bool Watchdog::bringSessionToFront() {
  return inSession_ && session_.pids && bringToFront(session_.pids());
}
} // namespace ac::launch
