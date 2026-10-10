// SPDX-License-Identifier: GPL-3.0-only
#include "Launch.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#ifdef Q_OS_WIN
// clang-format off: tlhelp32 requires the Windows SDK base declarations first.
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#endif
namespace ac::launch {
RuntimeInputs systemRuntimeInputs() {
  RuntimeInputs i;
  i.home = QDir::homePath();
#ifdef Q_OS_WIN
  // Only public application-location/runtime registry values are read.
  QSettings xr("HKEY_LOCAL_MACHINE\\SOFTWARE\\Khronos\\OpenXR\\1",
               QSettings::NativeFormat);
  i.registryRuntime = xr.value("ActiveRuntime").toString();
  QSettings steam("HKEY_CURRENT_USER\\Software\\Valve\\Steam",
                  QSettings::NativeFormat);
  i.steamPath = steam.value("SteamPath").toString();
#else
  for (const auto &p :
       QStringList{i.home + "/.local/share/Steam", i.home + "/.steam/steam"})
    if (QFileInfo(p + "/steamapps").isDir()) {
      i.steamPath = p;
      break;
    }
#endif
  return i;
}
RuntimeInfo detectRuntime(const RuntimeInputs &i) {
  RuntimeInfo r;
  if (i.environment.contains("XR_RUNTIME_JSON")) {
    r.active = i.environment.value("XR_RUNTIME_JSON");
    r.source = "environment";
  } else if (!i.registryRuntime.isEmpty()) {
    r.active = i.registryRuntime;
    r.source = "registry";
  } else {
    QStringList roots;
    roots << i.environment.value("XDG_CONFIG_HOME", i.home + "/.config");
    roots << i.environment.value("XDG_CONFIG_DIRS", "/etc/xdg")
                 .split(':', Qt::SkipEmptyParts);
    roots << "/etc";
    for (const auto &p : roots) {
      const auto candidate = p + "/openxr/1/active_runtime.json";
      if (QFileInfo(candidate).isFile()) {
        r.active = candidate;
        r.source = "xdg";
        break;
      }
    }
  }
  if (i.steamPath.isEmpty())
    return r;
  QStringList libraries{i.steamPath};
  QFile file(i.steamPath + "/steamapps/libraryfolders.vdf");
  if (file.size() <= 4 * 1024 * 1024 && file.open(QIODevice::ReadOnly)) {
    const QRegularExpression paths(
        "\"(?:path|[0-9]+)\"\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
    auto matches = paths.globalMatch(QString::fromUtf8(file.readAll()));
    while (matches.hasNext()) {
      auto p = matches.next().captured(1);
      p.replace("\\\\", "\\");
      p.replace("\\\"", "\"");
      if (QDir::isAbsolutePath(p))
        libraries << p;
    }
  }
  libraries.removeDuplicates();
  for (const auto &p : libraries) {
#ifdef Q_OS_WIN
    const auto manifest = p + "/steamapps/common/SteamVR/steamxr_win64.json";
    const auto monitor =
        p + "/steamapps/common/SteamVR/bin/win64/vrmonitor.exe";
#else
    const auto manifest = p + "/steamapps/common/SteamVR/steamxr_linux64.json";
    const auto monitor = p + "/steamapps/common/SteamVR/bin/vrmonitor.sh";
#endif
    if (QFileInfo(manifest).isFile()) {
      r.steamVrManifest = manifest;
      if (QFileInfo(monitor).isFile())
        r.steamVrMonitor = monitor;
      break;
    }
  }
  return r;
}
bool processRunning(const QStringList &names) {
#ifdef Q_OS_WIN
  const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    return true;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  bool found = false;
  if (Process32FirstW(snapshot, &entry))
    do {
      if (names.contains(QString::fromWCharArray(entry.szExeFile),
                         Qt::CaseInsensitive)) {
        found = true;
        break;
      }
    } while (Process32NextW(snapshot, &entry));
  CloseHandle(snapshot);
  return found;
#else
  QDir proc("/proc");
  if (!proc.exists())
    return true;
  for (const auto &id : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    bool numeric = false;
    id.toULongLong(&numeric);
    if (!numeric)
      continue;
    QFile comm(proc.filePath(id + "/comm"));
    if (comm.open(QIODevice::ReadOnly) &&
        names.contains(QString::fromUtf8(comm.readAll()).trimmed(),
                       Qt::CaseInsensitive))
      return true;
  }
  return false;
#endif
}
} // namespace ac::launch
