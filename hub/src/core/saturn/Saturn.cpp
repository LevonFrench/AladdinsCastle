// SPDX-License-Identifier: GPL-3.0-only
#include "Saturn.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include "core/launch/UniversalControls.h"
namespace ac::saturn {
namespace {
// padMenus: the game's menus also need the pad (the other port), not only the gun.
struct Game {const char *id,*title;int year;bool padMenus;const char *note;};
const QList<Game> &table(){
 static const QList<Game> games{
  {"virtua-cop","Virtua Cop",1995,false,""},
  {"virtua-cop-2","Virtua Cop 2",1996,false,""},
  {"house-of-the-dead","The House of the Dead",1998,false,"The Japanese release, with English menus."},
  {"area-51","Area 51",1996,false,"Point at a menu item and shoot it."},
  {"hakaider","Mechanical Violator Hakaider: Last Judgement",1996,true,"Japanese text; right A changes the weapon in play."},
 };
 return games;
}
const Game *find(const QString &id){for(const auto &game:table())if(id==QLatin1String(game.id))return &game;return nullptr;}
QString controls(const Game &game){
 QString text="Your right controller is the Saturn Stunner (Virtua Gun): trigger fires, point off the screen to reload.";
 if(game.padMenus)text+=" Menus also take the pad: the left stick moves, the left trigger chooses.";
 if(*game.note)text+=" "+QString::fromUtf8(game.note);
 return text+" The gun is already calibrated in the game. On a big theatre screen.";
}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:table())result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year},{"controls",universal::with(controls(game))},{"view","theatre"}};
 return result;
}
QStringList launcherKeys(const QString &launcher){
 QFile file(QFileInfo(launcher).absoluteDir().filePath("games.json"));
 if(launcher.isEmpty()||file.size()>4*1024*1024||!file.open(QIODevice::ReadOnly))return {};
 return QJsonDocument::fromJson(file.readAll()).object().keys();
}
QString problem(const QString &game,const QString &launcher){
 if(!find(game))return "This game is not in the Saturn list.";
 if(launcher.isEmpty()||!QFileInfo(launcher).isFile())return "Saturn launcher not found. Choose it in Set-up.";
 if(!launcherKeys(launcher).contains(game))return "Your Saturn launcher has no entry for this game.";
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &launcher){
 const auto issue=problem(game,launcher);
 if(!issue.isEmpty())throw install::Error("E_SATURN_SETUP",issue);
 const QFileInfo script(launcher);
 // Windows PowerShell 5.1 from the system folder, never one found on PATH.
 const auto windows=QProcessEnvironment::systemEnvironment().value("SystemRoot","C:\\Windows");
 const auto shell=QDir(windows).filePath("System32/WindowsPowerShell/v1.0/powershell.exe");
 return {{"executable",QDir::toNativeSeparators(shell)},{"cwd",QDir::toNativeSeparators(script.absolutePath())},
         {"args",QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(script.absoluteFilePath()),game}}};
}
}
