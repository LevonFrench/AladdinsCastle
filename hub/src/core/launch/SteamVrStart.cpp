// SPDX-License-Identifier: GPL-3.0-only
#include "SteamVrStart.h"
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
namespace ac::launch {
namespace {
QString defaultPathsFile() {
#ifdef Q_OS_WIN
  return qEnvironmentVariable("LOCALAPPDATA") + "/openvr/openvrpaths.vrpath";
#else
  return QDir::homePath() + "/.config/openvr/openvrpaths.vrpath";
#endif
}
} // namespace
QString steamVrStarter(const QString &pathsFile) {
  QFile file(pathsFile.isEmpty() ? defaultPathsFile() : pathsFile);
  if (file.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly))
    return {};
  for (const auto &value : QJsonDocument::fromJson(file.readAll()).object().value("runtime").toArray()) {
#ifdef Q_OS_WIN
    const QFileInfo starter(value.toString() + "/bin/win64/vrstartup.exe");
#else
    const QFileInfo starter(value.toString() + "/bin/vrstartup.sh");
#endif
    if (starter.isFile())
      return starter.absoluteFilePath();
  }
  return {};
}
bool startSteamVr(QString *error) {
  if (const auto starter = steamVrStarter(); !starter.isEmpty()) {
    if (QProcess::startDetached(starter, {}, QFileInfo(starter).absolutePath()))
      return true;
  }
  // Steam's own way to start SteamVR (its app 250820), when SteamVR's starter was not found.
  if (QDesktopServices::openUrl(QUrl("steam://rungameid/250820")))
    return true;
  if (error)
    *error = "SteamVR was not found on this PC, and Steam did not answer.";
  return false;
}
} // namespace ac::launch
