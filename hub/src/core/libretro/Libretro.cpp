// SPDX-License-Identifier: GPL-3.0-only
#include "Libretro.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFileInfo>
#include "core/launch/UniversalControls.h"
namespace ac::libretro {
namespace {
struct Game {const char *id,*title;int year;const char *system,*controls;};
const QList<Game> &table(){
 static const QList<Game> games{
  {"3ds-star-fox-64-3d","Star Fox 64 3D",2011,"3ds",
   "Steer with the left stick; the right trigger fires (hold to charge a lock-on shot), right A is the 3DS Y button (bombs), the right grip is X (boost) and the left grip is B (brake). "
   "The left trigger and right B are L and R: bank, twice for a barrel roll. Answer no when the tutorial asks about gyro aiming. In stereo on a big theatre screen."},
  {"3ds-mario-kart-7","Mario Kart 7",2011,"3ds",
   "Steer with the left stick; the right trigger accelerates (A), the left trigger brakes and reverses (B), the right grip hops and drifts (R), the left grip or right A uses an item (L), "
   "right B looks behind (X). Steering is the stick only; there is no tilt steering. In stereo on a big theatre screen. Not yet checked in a headset."},
 };
 return games;
}
const Game *find(const QString &id){for(const auto &game:table())if(id==QLatin1String(game.id))return &game;return nullptr;}
// A name for comparing file names with titles: letters and digits only, lower case.
QString plain(const QString &text){QString out;for(const auto ch:text.toLower())if(ch.isLetterOrNumber())out+=ch;return out;}
}
QString profileName(const QString &game){return find(game)?game+".toml":QString();}
QVariantList games(){
 QVariantList result;
 for(const auto &game:table())result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year},{"system",QString::fromLatin1(game.system)},
  {"profile",profileName(QString::fromLatin1(game.id))},{"controls",universal::with(QString::fromUtf8(game.controls))}};
 return result;
}
QString profilesFolder(const QString &chosen,const QString &startFrom){
 if(!chosen.isEmpty())return QFileInfo(chosen).isDir()?QFileInfo(chosen).absoluteFilePath():QString();
 if(startFrom.isEmpty())return {};
 QDir dir(startFrom);
 for(int up=0;up<8;++up){
  const QFileInfo candidate(dir.filePath("setups/retro-ps1-vr/profiles"));
  if(candidate.isDir())return candidate.absoluteFilePath();
  if(dir.isRoot()||!dir.cdUp())break;
 }
 return {};
}
QString gameFile(const QString &game,const QString &folder){
 const auto *entry=find(game);if(!entry||folder.isEmpty())return {};
 const auto want=plain(QString::fromUtf8(entry->title));
 QString found;
 for(const auto &file:QDir(folder).entryInfoList({"*.3ds","*.cci","*.cxi","*.3dsx"},QDir::Files,QDir::Name)){
  if(file.isSymLink()||file.isJunction())continue;
  const auto name=plain(file.completeBaseName());
  if(name==want)return file.absoluteFilePath(); // The exact name wins over a longer one.
  if(found.isEmpty()&&name.contains(want))found=file.absoluteFilePath();
 }
 return found;
}
QString viewLabel(const QString &view){
 if(view=="theatre")return "Theatre screen";
 if(view=="swap-eyes")return "Eyes swapped (diagnostic)";
 return {};
}
QVariantList views(){
 return {QVariantMap{{"id","theatre"},{"label",viewLabel("theatre")},{"help","Both eyes of the game's top screen in stereo on a big screen in front of you."}},
         QVariantMap{{"id","swap-eyes"},{"label",viewLabel("swap-eyes")},{"help","The same with the eyes exchanged: for checking depth, if distant scenery comes towards you."}}};
}
QString problem(const QString &game,const QString &host,const QString &core,const QString &gameFolder,const QString &profiles){
 if(!find(game))return "This game is not in the list of games the Hub starts through acvr-ps1.";
 if(host.isEmpty()||!QFileInfo(host).isFile())return "The acvr-ps1 program is not set up. Choose it in Set-up.";
 if(core.isEmpty()||!QFileInfo(core).isFile())return "The Nintendo 3DS core is not set up. Choose Azahar's libretro dll in Set-up.";
 if(profiles.isEmpty()||!QFileInfo(QDir(profiles).filePath(profileName(game))).isFile())return "The game's profile file was not found. Choose the profile folder (setups/retro-ps1-vr/profiles) in Set-up.";
 if(gameFile(game,gameFolder).isEmpty())return "The game file was not found. Put your own decrypted dump, named after the game, in the 3DS folder you chose in Set-up.";
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &host,const QString &core,const QString &file,const QString &dataFolder,const QString &profiles,const QString &view){
 if(!find(game))throw install::Error("E_LIBRETRO_GAME","This game is not in the list of games the Hub starts through acvr-ps1.");
 if(host.isEmpty()||!QFileInfo(host).isFile())throw install::Error("E_LIBRETRO_SETUP","The acvr-ps1 program is not set up. Choose it in Set-up.");
 if(core.isEmpty()||!QFileInfo(core).isFile())throw install::Error("E_LIBRETRO_CORE","The Nintendo 3DS core is not set up. Choose Azahar's libretro dll in Set-up.");
 if(file.isEmpty()||!QFileInfo(file).isFile())throw install::Error("E_LIBRETRO_GAME_FILE","The game file is no longer where it was. Use Rescan in Set-up.");
 const QFileInfo profile(QDir(profiles).filePath(profileName(game)));
 if(profiles.isEmpty()||!profile.isFile())throw install::Error("E_LIBRETRO_PROFILE","The game's profile file was not found. Check the profile folder in Set-up.");
 if(view!="theatre"&&view!="swap-eyes")throw install::Error("E_LIBRETRO_VIEW","This game has no such view. Choose another view.");
 const auto native=[](const QString &path){return QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());};
 QStringList args{"--core",native(core),"--game",native(file),"--system",native(QDir(dataFolder).filePath("system")),"--saves",native(QDir(dataFolder).filePath("saves")),
                  "--profile",native(profile.absoluteFilePath())};
 if(view!="theatre")args<<"--view"<<view; // The profile's own default view needs no --view.
 return {{"executable",native(host)},{"cwd",QDir::toNativeSeparators(QFileInfo(host).absolutePath())},{"args",args}};
}
}
