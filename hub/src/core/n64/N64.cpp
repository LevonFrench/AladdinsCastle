// SPDX-License-Identifier: GPL-3.0-only
#include "N64.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
namespace ac::n64 {
namespace {
// How the VR controllers play the game (acvr-ps1 --controls and the launcher's map).
enum class Play { Wheel, Aim, StarFox, Turok, Pad };
// How far past its old frame the game is drawn: open (wider than 16:9, round you), wide (16:9), standard (4:3).
struct Game {const char *id,*title;int year;Play play;const char *frame;bool hudPanel;const char *note;};
const QList<Game> &table(){
 static const QList<Game> games{
  {"goldeneye","GoldenEye 007",1997,Play::Aim,"open",true,""},
  {"mario-kart-64","Mario Kart 64",1996,Play::Wheel,"open",true,""},
  {"f-zero-x","F-Zero X",1998,Play::Wheel,"open",true,"The left trigger brakes and right B boosts."},
  {"star-fox-64","Star Fox 64",1997,Play::StarFox,"open",true,""},
  {"wave-race-64","Wave Race 64",1996,Play::Wheel,"open",true,""},
  {"perfect-dark","Perfect Dark",2000,Play::Aim,"open",true,""},
  {"diddy-kong-racing","Diddy Kong Racing",1997,Play::Wheel,"open",true,""},
  {"beetle-adventure-racing","Beetle Adventure Racing!",1999,Play::Wheel,"standard",true,""},
  {"1080-snowboarding","1080\u00b0 Snowboarding",1998,Play::Wheel,"open",true,"The left stick carves; the right trigger crouches and jumps, right A and B do tricks, the right grip grabs."},
  {"cruisn-usa","Cruis'n USA",1996,Play::Wheel,"open",true,""},
  {"cruisn-world","Cruis'n World",1998,Play::Wheel,"open",true,""},
  {"cruisn-exotica","Cruis'n Exotica",2000,Play::Wheel,"open",true,""},
  {"hydro-thunder","Hydro Thunder",2000,Play::Wheel,"open",true,""},
  {"san-francisco-rush","San Francisco Rush: Extreme Racing",1997,Play::Wheel,"wide",true,""},
  {"rush-2","Rush 2: Extreme Racing USA",1998,Play::Wheel,"wide",true,""},
  {"rush-2049","San Francisco Rush 2049",2000,Play::Wheel,"wide",true,""},
  {"star-wars-racer","Star Wars Episode I: Racer",1999,Play::Wheel,"wide",true,""},
  {"super-mario-64","Super Mario 64",1996,Play::Pad,"wide",true,""},
  {"ocarina-of-time","The Legend of Zelda: Ocarina of Time",1998,Play::Pad,"wide",false,""},
  {"majoras-mask","The Legend of Zelda: Majora's Mask",2000,Play::Pad,"open",true,""},
  {"banjo-kazooie","Banjo-Kazooie",1998,Play::Pad,"wide",true,""},
  {"turok","Turok: Dinosaur Hunter",1997,Play::Turok,"open",true,""},
 };
 return games;
}
const Game *find(const QString &id){for(const auto &game:table())if(id==QLatin1String(game.id))return &game;return nullptr;}
QString controls(const Game &game){
 QString text;
 switch(game.play){
 case Play::Wheel:text="The left stick steers; the right trigger accelerates, the left trigger brakes; the grips are R and Z.";break;
 case Play::Aim:text="Hold the right grip and point: the game's own crosshair follows your right controller. The right trigger fires; the left stick walks (and strafes while you aim).";break;
 case Play::StarFox:text="Point your right controller where the targeting box should go and the Arwing flies it there. The right trigger fires (hold to charge a lock-on shot), right A drops a bomb, the right grip boosts, the left grip brakes, the left trigger banks left and right B banks right (twice for a barrel roll).";break;
 case Play::Turok:text="Point where you want to look: the view turns toward it, and the crosshair (the middle of the picture) comes onto what you point at. The right trigger fires; the left stick walks and strafes; the right grip jumps.";break;
 case Play::Pad:text="The N64 pad on your controllers: the left stick moves, the right stick is the C buttons, right A and B are A and B, the right trigger is Z, the right grip is R.";break;
 }
 if(*game.note)text+=" "+QString::fromUtf8(game.note);
 text+=" Left stick click is Start (pause); hold left Y for 3 seconds to quit and save.";
 if(game.hudPanel)text+=" The game's HUD floats on its own steady panel.";
 return text;
}
QString frameText(const Game &game){
 const QString frame=QString::fromLatin1(game.frame);
 return frame=="open"?QString("open frame"):frame=="wide"?QString("16:9"):QString("4:3");
}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:table())result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year},
  {"controls",controls(game)},{"frame",QString::fromLatin1(game.frame)}};
 return result;
}
QStringList launcherKeys(const QString &launcher){
 QFile file(QFileInfo(launcher).absoluteDir().filePath("games.json"));
 if(launcher.isEmpty()||file.size()>4*1024*1024||!file.open(QIODevice::ReadOnly))return {};
 return QJsonDocument::fromJson(file.readAll()).object().keys();
}
QString launcherKey(const QString &game,const QString &view){
 if(view=="3d")return game+"-3d";
 if(view=="flat")return game;
 if(view=="theatre")return game+"-theatre";
 return {};
}
QString viewLabel(const QString &game,const QString &view){
 const auto *entry=find(game);if(!entry)return {};
 const auto frame=frameText(*entry);
 if(view=="3d")return "Real 3D, "+frame;
 if(view=="flat")return frame.left(1).toUpper()+frame.mid(1);
 if(view=="theatre")return "Theatre screen";
 return {};
}
QVariantList views(const QString &game,const QString &launcher){
 const auto *entry=find(game);if(!entry)return {};
 const auto keys=launcherKeys(launcher);const auto frame=frameText(*entry);
 const QString around=QString::fromLatin1(entry->frame)=="open"?QString(" The world reaches round you past the old frame, with no border."):QString();
 QVariantList list;
 if(keys.contains(launcherKey(game,"3d")))list<<QVariantMap{{"id","3d"},{"label",viewLabel(game,"3d")},{"help","The game's own world in stereo depth."+around}};
 if(keys.contains(launcherKey(game,"flat")))list<<QVariantMap{{"id","flat"},{"label",viewLabel(game,"flat")},{"help","The game flat, "+frame+"."+around}};
 if(keys.contains(launcherKey(game,"theatre")))list<<QVariantMap{{"id","theatre"},{"label",viewLabel(game,"theatre")},{"help","The original 4:3 picture on a big screen in front of you."}};
 return list;
}
QString problem(const QString &game,const QString &launcher){
 if(!find(game))return "This game is not in the N64 list.";
 if(launcher.isEmpty()||!QFileInfo(launcher).isFile())return "N64 launcher not found. Choose it in Set-up.";
 if(views(game,launcher).isEmpty())return "Your N64 launcher has no entry for this game.";
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &launcher,const QString &view){
 const auto issue=problem(game,launcher);
 if(!issue.isEmpty())throw install::Error("E_N64_SETUP",issue);
 const auto key=launcherKey(game,view);
 if(key.isEmpty()||!launcherKeys(launcher).contains(key))throw install::Error("E_N64_VIEW","Your N64 launcher has no entry for this view. Choose another view.");
 const QFileInfo script(launcher);
 // Windows PowerShell 5.1 from the system folder, never one found on PATH.
 const auto windows=QProcessEnvironment::systemEnvironment().value("SystemRoot","C:\\Windows");
 const auto shell=QDir(windows).filePath("System32/WindowsPowerShell/v1.0/powershell.exe");
 return {{"executable",QDir::toNativeSeparators(shell)},{"cwd",QDir::toNativeSeparators(script.absolutePath())},
         {"args",QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(script.absoluteFilePath()),key}}};
}
}
