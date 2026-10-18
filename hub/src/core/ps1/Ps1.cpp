// SPDX-License-Identifier: GPL-3.0-only
#include "Ps1.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
namespace ac::ps1 {
namespace {
struct Game {const char *id,*title;int year;const char *gun;bool sprites;};
const QList<Game> &table(){
 static const QList<Game> games{
  {"point-blank","Point Blank",1997,"GunCon",true},
  {"point-blank-2","Point Blank 2",1999,"GunCon",true},
  {"point-blank-3","Point Blank 3",2000,"GunCon",true},
  {"arcade-point-blank-2","Point Blank 2 (arcade, MAME)",1999,"arcade gun",true},
  {"time-crisis","Time Crisis",1997,"GunCon",false},
  {"time-crisis-project-titan","Time Crisis: Project Titan",2001,"GunCon",false},
  {"ghoul-panic","Ghoul Panic",2000,"GunCon",true},
  {"mighty-hits-special","Mighty Hits Special",2000,"GunCon",true},
  {"gunfighter-the-legend-of-jesse-james","Gunfighter: The Legend of Jesse James",2001,"GunCon",false},
  {"judge-dredd","Judge Dredd",1998,"GunCon",false},
  {"extreme-ghostbusters","Extreme Ghostbusters: The Ultimate Invasion",2004,"GunCon",false},
  {"die-hard-trilogy-2-viva-las-vegas","Die Hard Trilogy 2: Viva Las Vegas",2000,"GunCon",false},
  {"die-hard-trilogy","Die Hard Trilogy",1996,"Justifier",false},
  {"maximum-force","Maximum Force",1997,"GunCon",false},
  {"resident-evil-survivor","Resident Evil Survivor",2000,"",false},
  {"elemental-gearbolt","Elemental Gearbolt",1998,"GunCon",false},
  {"rescue-shot","Rescue Shot",2000,"GunCon",false},
  {"rescue-shot-bubibo","Rescue Shot Bubibo (Japan)",1999,"GunCon",false},
  {"mighty-hits-special-japan","Mighty Hits Special (Japan)",2000,"GunCon",true},
  {"ghoul-panic-japan","Oh! Bakyuuun (Ghoul Panic, Japan)",2000,"GunCon",false},
  {"project-horned-owl","Project: Horned Owl",1996,"Justifier",false},
  {"horned-owl-japan","Horned Owl (Japan)",1995,"Justifier",false},
 };
 return games;
}
QString controls(const Game &game){
 if(!*game.gun)return "This release has no light-gun support: the gun buttons only.";
 QString text=QString("Your right controller is the %1: trigger fires, point off the screen to reload, A and B are its buttons").arg(QString::fromLatin1(game.gun));
 text+=QString::fromLatin1(game.gun)=="GunCon"?QString("; the grip is the pedal (hold it to step out and shoot where a game has cover)."):QString(".");
 if(QString::fromLatin1(game.gun)=="arcade gun")text="Your right controller is the gun: trigger fires, B inserts a coin, A starts.";
 const QString id=QString::fromLatin1(game.id);
 if(game.sprites)return text+" On a big theatre screen.";
 // Real 3D runs through the stereo core, whose light gun works on North American and Japanese discs. Die Hard Trilogy
 // and Horned Owl read the Justifier (not in that core); Maximum Force has no 3D depth; the European discs keep the proven theatre core.
 static const QStringList theatreOnly{"die-hard-trilogy","project-horned-owl","horned-owl-japan","maximum-force","extreme-ghostbusters","rescue-shot"};
 if(theatreOnly.contains(id))return text+" On a big theatre screen.";
 return text+" In real 3D on a big screen.";
}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:table())result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year},{"controls",controls(game)},{"view","theatre"}};
 return result;
}
QStringList launcherKeys(const QString &launcher){
 QFile file(QFileInfo(launcher).absoluteDir().filePath("games.json"));
 if(launcher.isEmpty()||file.size()>4*1024*1024||!file.open(QIODevice::ReadOnly))return {};
 return QJsonDocument::fromJson(file.readAll()).object().keys();
}
QString problem(const QString &game,const QString &launcher){
 bool listed=false;for(const auto &entry:table())listed=listed||game==QLatin1String(entry.id);
 if(!listed)return "This game is not in the PS1 list.";
 if(launcher.isEmpty()||!QFileInfo(launcher).isFile())return "PS1 launcher not found. Choose it in Set-up.";
 if(!launcherKeys(launcher).contains(game))return "Your PS1 launcher has no entry for this game.";
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &launcher){
 const auto issue=problem(game,launcher);
 if(!issue.isEmpty())throw install::Error("E_PS1_SETUP",issue);
 const QFileInfo script(launcher);
 // Windows PowerShell 5.1 from the system folder, never one found on PATH.
 const auto windows=QProcessEnvironment::systemEnvironment().value("SystemRoot","C:\\Windows");
 const auto shell=QDir(windows).filePath("System32/WindowsPowerShell/v1.0/powershell.exe");
 return {{"executable",QDir::toNativeSeparators(shell)},{"cwd",QDir::toNativeSeparators(script.absolutePath())},
         {"args",QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(script.absoluteFilePath()),game}}};
}
}
