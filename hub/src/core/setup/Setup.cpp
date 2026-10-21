// SPDX-License-Identifier: GPL-3.0-only
#include "Setup.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
namespace ac::setup {
namespace {
struct System {const char *id,*kind,*title,*detail,*folderKey,*programKey,*programLabel;QStringList names;};
const QList<System> &table(){
 static const QList<System> systems{
  {"model3","arcade","Sega Model 3","Ten games in real 3D: on a screen, a wide screen or inside the game.","model3GameFolder","model3Executable","Supermodel VR build",{"model3","Model 3","Sega Model 3","supermodel"}},
  {"model2","arcade","Sega Model 2","Three games on a 3D screen in the headset. Virtua Cop 2 has new, untested controls; the others are view only for now.","model2GameFolder","model2Executable","MAME VR build",{"model2","Model 2","Sega Model 2"}},
  {"system22","arcade","Namco System 22","Time Crisis in true VR through its own native build, which keeps its game files beside it.","","system22Executable","Time Crisis VR build",{}},
  {"pinball","pinball","Visual Pinball","VR-ready tables from your own collection.","pinballFolder","pinballExecutable","Visual Pinball (OpenXR build)",{"tables","Tables","pinball/tables","Visual Pinball/Tables"}},
  {"ps2","consoles","Sony PlayStation 2","Twelve games on a 3D screen in the headset through your PS2 launcher. Played with a gamepad for now.","ps2GameFolder","ps2Launcher","PS2 VR launcher",{"ps2","PS2","PlayStation 2","Sony PlayStation 2"}},
  {"ps1","consoles","Sony PlayStation","Light-gun games on a big theatre screen in the headset, your controller as the GunCon, through acvr-ps1 and your PS1 launcher.","ps1GameFolder","ps1Launcher","PS1 VR launcher (launch-ps1.ps1)",{"psx","ps1","PS1","PlayStation","Sony PlayStation"}},
  {"n64","consoles","Nintendo 64","Twenty-two N64 games in the headset through acvr-ps1 and your N64 launcher: real 3D, the HUD on its own panel, the world past the old frame where the game draws it, racers on the left stick and triggers, aiming by pointing.","n64GameFolder","n64Launcher","N64 VR launcher (launch-n64.ps1)",{"n64","N64","Nintendo 64","Nintendo - Nintendo 64"}},
  {"saturn","consoles","Sega Saturn","Five Saturn light-gun games on a big theatre screen in the headset, your controller as the Stunner, through acvr-ps1 and your Saturn launcher.","saturnGameFolder","saturnLauncher","Saturn VR launcher (launch-saturn.ps1)",{"saturn","Saturn","Sega Saturn","Sega - Saturn"}},
  {"wii","consoles","Nintendo Wii","Seven Wii gun games in the headset through your DolphinXR build, the way Umbrella Chronicles was played.","wiiGameFolder","wiiExecutable","DolphinXR build (its Dolphin.exe)",{"wii","Wii","Nintendo Wii"}},
  {"dreamcast","consoles","Sega Dreamcast","The House of the Dead 2 with a full VR profile; other gun games start without one.","dreamcastGameFolder","dreamcastExecutable","Dreamcast VR build",{"dreamcast","Dreamcast","Sega Dreamcast","dc"}},
 };
 return systems;
}
QString clean(const QString &path){return QDir::cleanPath(QDir::fromNativeSeparators(path));}
}
QVariantList systems(){
 QVariantList result;
 for(const auto &s:table())result<<QVariantMap{{"id",QString::fromLatin1(s.id)},{"kind",QString::fromLatin1(s.kind)},{"title",QString::fromUtf8(s.title)},{"detail",QString::fromUtf8(s.detail)},
  {"folderKey",QString::fromLatin1(s.folderKey)},{"programKey",QString::fromLatin1(s.programKey)},{"programLabel",QString::fromUtf8(s.programLabel)}};
 return result;
}
QString guessFolder(const QString &system,const QStringList &knownFolders){
 const System *found=nullptr;for(const auto &s:table())if(system==QLatin1String(s.id))found=&s;
 if(!found)return {};
 // Beside a chosen folder, one level above it, or in a "roms" folder at either place.
 QStringList roots;
 for(const auto &known:knownFolders){
  if(known.isEmpty()||!QFileInfo(known).isDir())continue;
  QDir folder(known);
  for(int level=0;level<2&&folder.cdUp();++level)for(const auto &root:{folder.absolutePath(),folder.absoluteFilePath("roms")})if(!roots.contains(root))roots<<root;
 }
 for(const auto &root:roots)for(const auto &name:found->names){
  // Follow the names as they are spelled on disk, whatever case was tried.
  QString path=root;bool exists=QFileInfo(root).isDir();
  for(const auto &part:name.split('/')){
   if(!exists)break;
   QString actual;for(const auto &entry:QDir(path).entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(entry.compare(part,Qt::CaseInsensitive)==0){actual=entry;break;}
   exists=!actual.isEmpty();path=QDir(path).absoluteFilePath(actual);
  }
  if(exists)return clean(path);
 }
 return {};
}
bool steamVrLists(const QString &appConfig,const QString &manifest){
 QFile file(appConfig);
 if(manifest.isEmpty()||file.size()>4*1024*1024||!file.open(QIODevice::ReadOnly))return false;
 const auto wanted=clean(manifest);
 for(const auto &value:QJsonDocument::fromJson(file.readAll()).object().value("manifest_paths").toArray())
  if(clean(value.toString()).compare(wanted,Qt::CaseInsensitive)==0)return true;
 return false;
}
}
