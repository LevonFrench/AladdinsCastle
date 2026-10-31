// SPDX-License-Identifier: GPL-3.0-only
#include "Wii.h"
#include "core/install/Install.h"
#include "core/scan/Scan.h"
#include "core/scan/DiscIndex.h"
#include "core/scan/DiscMemo.h"
#include <QDir>
#include <QFileInfo>
namespace ac::wii {
QVariantList games(){return scan::DiscIndex::games("wii");}
QString discId(const QString &path,std::atomic_bool &cancel){const auto read=scan::Scanner::inspect(path,cancel);return read.error.isEmpty()&&read.system=="wii"?read.identity:QString();}
QString discId(const QString &path){std::atomic_bool cancel{false};return discId(path,cancel);}
QString gameFor(const QString &id){return scan::DiscIndex::match("wii",id).value("game").toString();}
QVariantMap identify(const QString &path,std::atomic_bool &cancel,scan::DiscMemo *memo){
 const QFileInfo info(path);
 // Only an id that was found is kept: an empty answer may be a read stopped by its time limit.
 const auto readId=[&](bool &finished){const auto id=discId(info.absoluteFilePath(),cancel);finished=!id.isEmpty()&&!cancel;return id;};
 bool finished=false;const auto id=memo?memo->identity("wii",info,readId):readId(finished);const auto row=scan::DiscIndex::match("wii",id);
 return {{"file",QDir::fromNativeSeparators(info.absoluteFilePath())},{"id",id},{"game",row.value("game",QString())},{"title",row.value("title",QString())},{"verified",!row.isEmpty()}};
}
QVariantMap identify(const QString &path){std::atomic_bool cancel{false};return identify(path,cancel);}
QVariantList discs(const QString &folder, std::atomic_bool &cancel, scan::DiscMemo *memo) {
  QVariantList found;
  if (folder.isEmpty())
    return found;
  for (const auto &entry : QDir(folder).entryInfoList({"*.iso", "*.wbfs", "*.rvz", "*.wia", "*.gcz", "*.wad", "*.nkit", "*.m3u"}, QDir::Files, QDir::Name)) {
    if (cancel)
      break;
    if (entry.isSymLink() || entry.isJunction())
      continue;
    found << identify(entry.absoluteFilePath(),cancel,memo);
  }
  return found;
}
QStringList missing(const QString &game, const QString &id, const QString &executable) {
  QStringList result;
  QVariantMap row;
  for(const auto &entry:games())if(entry.toMap().value("id")==game)row=entry.toMap();
  if(row.isEmpty())return result;
  const QDir user(QFileInfo(executable).absolutePath() + "/User");
  if (!id.isEmpty() && !QFileInfo(user.filePath("GameSettingsVR/" + id + ".ini")).isFile())
    result << QString("VR settings of its own (GameSettingsVR/%1.ini), so it starts with DolphinXR's default VR screen").arg(id);
  if (row.value("hold").toString() == "docked")
    result << "docked controls: it is meant to be held with both hands on one gun, and the build is set for a gun in one hand";
  return result;
}
QVariantMap launchPlan(const QString &file, const QString &executable) {
  if (executable.isEmpty() || !QFileInfo(executable).isFile())
    throw install::Error("E_WII_SETUP", "The DolphinXR build is not set up. Choose its Dolphin.exe in Set-up.");
  if (file.isEmpty() || !QFileInfo(file).isFile())
    throw install::Error("E_WII_DISC", "The disc image is no longer where it was. Use Rescan in Set-up.");
  const QFileInfo program(executable);
  const auto native = [](const QString &path) { return QDir::toNativeSeparators(path); };
  return {{"executable", native(program.absoluteFilePath())},
          {"cwd", native(program.absolutePath())},
          {"args", QStringList{"--user", native(program.absolutePath() + "/User"), "-C", "Graphics.VR.EnableOpenXR=True", "-C",
                               "Graphics.VR.ResolutionScale=1.0", "-C", "Graphics.VR.EagerHeartbeat=False", "-C",
                               "Graphics.Stereoscopy.StereoMode=0", "-C", "Dolphin.Interface.ConfirmStop=False", "--exec",
                               native(QFileInfo(file).absoluteFilePath())}}};
}
} // namespace ac::wii
