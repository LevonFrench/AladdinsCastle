// SPDX-License-Identifier: GPL-3.0-only
#include "Ps1.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include "core/launch/UniversalControls.h"
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
bool inThreeD(const Game &game);
QString controls(const Game &game){
 if(!*game.gun)return "This release has no light-gun support: the gun buttons only.";
 const QString id=QString::fromLatin1(game.id);
 // These games need the whole pad as well, so the launcher gives them the owner's face-button layout.
 static const QStringList faceButtons{"extreme-ghostbusters","project-horned-owl","horned-owl-japan"};
 if(faceButtons.contains(id))
  return QString("Your right controller is the %1: trigger fires, point off the screen to reload; its own two buttons are on the grips. "
                 "Pad: right A Cross, right B Circle (cancel), left X Square, left Y Triangle. "
                 "On a big theatre screen.").arg(QString::fromLatin1(game.gun));
 QString text=QString("Your right controller is the %1: trigger fires, point off the screen to reload, A and B are its buttons").arg(QString::fromLatin1(game.gun));
 text+=QString::fromLatin1(game.gun)=="GunCon"?QString("; the grip is the pedal (hold it to step out and shoot where a game has cover)."):QString(".");
 if(QString::fromLatin1(game.gun)=="arcade gun")text="Your right controller is the gun: trigger fires, point off the screen to reload.";
 if(QString::fromLatin1(game.gun)=="Justifier")text="Your right controller is the Justifier: trigger fires, point off the screen to reload; Start begins each mission.";
 if(game.sprites)return text+" On a big theatre screen.";
 // Real 3D runs through the stereo core, whose light gun works on North American and Japanese discs. Die Hard Trilogy
 // and Horned Owl read the Justifier (not in that core); Oh! Bakyuuun reads the GunCon through the light-pen
 // interrupt, which that core lacks; Maximum Force has no 3D depth; the European discs keep the proven theatre core.
 return text+(inThreeD(game)?" In real 3D on a big screen.":" On a big theatre screen.");
}
bool inThreeD(const Game &game){
 static const QStringList theatreOnly{"die-hard-trilogy","project-horned-owl","horned-owl-japan","ghoul-panic-japan","maximum-force","extreme-ghostbusters","rescue-shot"};
 return *game.gun&&!game.sprites&&!theatreOnly.contains(QString::fromLatin1(game.id));
}
const Game *find(const QString &id){for(const auto &game:table())if(id==QLatin1String(game.id))return &game;return nullptr;}
struct View {const char *id,*suffix,*label,*help;};
// The usual view first; then the views the old numbered launchers offered beside it.
const QList<View> &viewTable(){
 static const QList<View> list{
  {"standard","","","" },
  {"open-frame","-open-frame","Open frame (3D, 16:9)","The game drawn 16:9 in real 3D, its aim corrected to the game's own 4:3 view: enemies in the extra side areas can be seen but not shot until they move into the middle. Untested in the headset."},
  {"diorama","-diorama","Diorama (prototype)","Sprites placed at depths from the order the game draws them. A prototype."},
  {"no-trim","-no-trim","No aim trim (compare)","The same game without its measured aim trim, to compare with the usual view."},
 };
 return list;
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
QString launcherKey(const QString &game,const QString &view){
 if(view=="theatre")return game; // the one view's id before the views of the old launchers came in
 for(const auto &entry:viewTable())if(view==QLatin1String(entry.id))return game+QString::fromLatin1(entry.suffix);
 return {};
}
QString viewLabel(const QString &game,const QString &view){
 const auto *entry=find(game);if(!entry)return {};
 if(view=="standard")return inThreeD(*entry)?QString("Real 3D screen"):QString("Theatre screen");
 for(const auto &row:viewTable())if(view==QLatin1String(row.id))return QString::fromUtf8(row.label);
 return {};
}
QVariantList views(const QString &game,const QString &launcher){
 const auto *entry=find(game);if(!entry)return {};
 const auto keys=launcherKeys(launcher);QVariantList list;
 for(const auto &row:viewTable()){
  const QString id=QString::fromLatin1(row.id);
  if(!keys.contains(launcherKey(game,id)))continue;
  const QString help=id=="standard"?(inThreeD(*entry)?QString("The game's own 3D world in stereo on a big screen, with your controller as the light gun."):QString("A big screen in the headset, with your controller as the light gun.")):QString::fromUtf8(row.help);
  list<<QVariantMap{{"id",id},{"label",viewLabel(game,id)},{"help",help}};
 }
 return list;
}
QString problem(const QString &game,const QString &launcher){
 bool listed=false;for(const auto &entry:table())listed=listed||game==QLatin1String(entry.id);
 if(!listed)return "This game is not in the PS1 list.";
 if(launcher.isEmpty()||!QFileInfo(launcher).isFile())return "PS1 launcher not found. Choose it in Set-up.";
 if(!launcherKeys(launcher).contains(game))return "Your PS1 launcher has no entry for this game.";
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &launcher,const QString &view){
 const auto issue=problem(game,launcher);
 if(!issue.isEmpty())throw install::Error("E_PS1_SETUP",issue);
 const auto key=launcherKey(game,view);
 if(key.isEmpty()||!launcherKeys(launcher).contains(key))throw install::Error("E_PS1_VIEW","Your PS1 launcher has no entry for this view. Choose another view.");
 const QFileInfo script(launcher);
 // Windows PowerShell 5.1 from the system folder, never one found on PATH.
 const auto windows=QProcessEnvironment::systemEnvironment().value("SystemRoot","C:\\Windows");
 const auto shell=QDir(windows).filePath("System32/WindowsPowerShell/v1.0/powershell.exe");
 return {{"executable",QDir::toNativeSeparators(shell)},{"cwd",QDir::toNativeSeparators(script.absolutePath())},
         {"args",QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(script.absoluteFilePath()),key}}};
}
}
