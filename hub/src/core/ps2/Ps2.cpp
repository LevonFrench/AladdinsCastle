// SPDX-License-Identifier: GPL-3.0-only
#include "Ps2.h"
#include "core/install/Install.h"
#include "core/scan/Scan.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
namespace ac::ps2 {
namespace {
// Serials are the ones read from real discs; a disc whose serial cannot be read
// is matched by words of its file name and reported as not checked.
struct Game {const char *id,*title;int year;bool gun,measured;QStringList serials,words;};
const QList<Game> &table(){
 static const QList<Game> games{
  {"virtua-cop","Virtua Cop Elite Edition",2002,true,true,{"SLES-51229"},{"virtuacop"}},
  {"outrun-2006","OutRun 2006",2006,false,true,{"SLUS-21274"},{"outrun2006"}},
  {"wipeout-fusion","Wipeout Fusion",2002,false,false,{},{"wipeoutfusion"}},
  {"xgra","XGRA",2003,false,false,{},{"xgra"}},
  {"time-crisis-3","Time Crisis 3",2003,true,false,{"SCES-51844"},{"timecrisis3","timecrisisiii"}},
  {"crisis-zone","Crisis Zone",2004,true,false,{"SCES-52530"},{"crisiszone"}},
  {"time-crisis-2","Time Crisis II",2001,true,false,{"SLUS-20219"},{"timecrisisii","timecrisis2"}},
  {"vampire-night","Vampire Night",2001,true,false,{},{"vampirenight"}},
  {"ninja-assault","Ninja Assault",2002,true,false,{"SLUS-20492"},{"ninjaassault"}},
  {"outrun-2","OutRun 2",2006,false,false,{"SLPM-66628"},{"outrun2"}},
  {"wipeout-pulse","Wipeout Pulse",2009,false,false,{"SCES-54748"},{"wipeoutpulse"}},
  {"xgiii","XGIII Extreme G Racing",2001,false,false,{},{"xgiii","xg3"}},
 };
 return games;
}
QString squeezed(QString text){static const QRegularExpression other("[^a-z0-9]");return text.toLower().remove(other);}
QString normalSerial(QString serial){return serial.toUpper().replace('_','-').remove('.');}
QString controls(const Game &game){
 return QString(game.gun?"Gamepad only for now (an Xbox-style controller); aiming with the VR controllers is still to come.":"Played with a gamepad (an Xbox-style controller).")
  +(game.measured?" Depth: measured.":" Depth: experimental, a general profile not yet tuned for this game.");
}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:table())result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year},{"controls",controls(game)}};
 return result;
}
QString gameForSerial(const QString &serial){
 const auto wanted=normalSerial(serial);
 for(const auto &game:table())for(const auto &known:game.serials)if(normalSerial(known)==wanted)return QString::fromLatin1(game.id);
 return {};
}
QVariantMap identify(const QString &file,std::atomic_bool &cancel){
 const QFileInfo info(file);
 // The serial is in the disc's own SYSTEM.CNF; compressed .gz images are not opened.
 QString serial;
 if(QStringList{"iso","bin","chd"}.contains(info.suffix().toLower())){
  const auto read=scan::Scanner::inspect(info.absoluteFilePath(),cancel);
  if(read.error.isEmpty()&&(read.kind=="disc-serial"||read.kind=="chd-disc-serial"))serial=read.identity;
 }
 QVariantMap disc{{"file",QDir::fromNativeSeparators(info.absoluteFilePath())},{"serial",serial},{"game",QString()},{"title",QString()},{"verified",false}};
 auto id=serial.isEmpty()?QString():gameForSerial(serial);
 if(!id.isEmpty())disc["verified"]=true;
 else if(serial.isEmpty()){
  // Longer words first, so "OutRun 2006" is not taken for "OutRun 2".
  const auto name=squeezed(info.completeBaseName());qsizetype length=0;
  for(const auto &game:table())for(const auto &word:game.words)if(name.contains(word)&&word.size()>length){id=QString::fromLatin1(game.id);length=word.size();}
 }
 for(const auto &game:table())if(id==QLatin1String(game.id)){disc["game"]=id;disc["title"]=QString::fromUtf8(game.title);}
 return disc;
}
QVariantList discs(const QString &folder,std::atomic_bool &cancel){
 QVariantList found;
 if(folder.isEmpty())return found;
 for(const auto &entry:QDir(folder).entryInfoList({"*.iso","*.bin","*.chd","*.gz","*.cso"},QDir::Files,QDir::Name)){
  if(cancel)break;
  if(entry.isSymLink()||entry.isJunction())continue;
  found<<identify(entry.absoluteFilePath(),cancel);
 }
 return found;
}
namespace {
QJsonObject launcherGames(const QString &launcher){
 QFile file(QFileInfo(launcher).absoluteDir().filePath("games.json"));
 if(launcher.isEmpty()||file.size()>4*1024*1024||!file.open(QIODevice::ReadOnly))return {};
 return QJsonDocument::fromJson(file.readAll()).object();
}
// The Inside the game view, in one place each, to be confirmed against the user's launcher:
// the games with an inside profile (none until confirmed), the launcher's flag, and where it goes.
const QStringList kInsideGames{};
const QString kInsideFlag("--inside");
constexpr bool kInsideFlagBeforeKey=false; // false: launcher, key, flag. true: launcher, flag, key.
}
QStringList launcherKeys(const QString &launcher){return launcherGames(launcher).keys();}
QStringList insideGames(){return kInsideGames;}
QString viewLabel(const QString &view){
 if(view=="inside")return "Inside the game";
 if(view=="screen")return "3D screen";
 if(view=="depth")return "Depth trial";
 if(view=="check")return "Headset check";
 return {};
}
namespace {
// Inside the game: confirmed in the list, or the launcher's own entry has an inside profile.
bool hasInside(const QString &game,const QString &launcher,const QStringList &inside){
 const auto entry=launcherGames(launcher).value(game);
 return entry.isObject()&&(inside.contains(game)||entry.toObject().value("inside").isObject());
}
QString depthKey(const QString &game){return game+"-depth-trial";}
const QString kCheckGame("virtua-cop");
}
QVariantList views(const QString &game,const QString &launcher,const QStringList &inside){
 QVariantList list{QVariantMap{{"id","screen"},{"label",viewLabel("screen")},{"help","A framed 3D screen straight ahead of you in the headset."}}};
 if(hasInside(game,launcher,inside))
  list<<QVariantMap{{"id","inside"},{"label",viewLabel("inside")},{"help","No frame: the game opens around you, with the depth your launcher's inside profile sets."}};
 const auto keys=launcherKeys(launcher);
 if(keys.contains(depthKey(game)))
  list<<QVariantMap{{"id","depth"},{"label",viewLabel("depth")},{"help","The 3D screen with the depth of the overnight measurement (convergence 2.5 m instead of 1000 m), to compare with 3D screen."}};
 if(game==kCheckGame&&keys.contains(game))
  list<<QVariantMap{{"id","check"},{"label",viewLabel("check")},{"help","The launcher's short check of the PS2 picture in the headset; it ends by itself and writes its result beside the launcher."}};
 return list;
}
QString problem(const QString &game,const QString &launcher,const QString &python,const QString &stopFile){
 bool listed=false;for(const auto &entry:table())listed=listed||game==QLatin1String(entry.id);
 if(!listed)return "This game is not in the PS2 list.";
 if(launcher.isEmpty()||!QFileInfo(launcher).isFile())return "PS2 launcher not found. Choose it in Set-up.";
 if(python.isEmpty()||!QFileInfo(python).isFile())return "Python for the PS2 launcher not found. Set ps2Python in the Hub's settings.";
 // Without its stop file the Hub would have no gentle way to end the game.
 if(stopFile.isEmpty()||!QFileInfo(QFileInfo(stopFile).absolutePath()).isDir())return "The PS2 stop file is not set. Set ps2StopFile in the Hub's settings.";
 if(!launcherKeys(launcher).contains(game))return "Your PS2 launcher has no entry for this game.";
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &launcher,const QString &python,const QString &stopFile,const QString &view,const QStringList &inside){
 const auto issue=problem(game,launcher,python,stopFile);
 if(!issue.isEmpty())throw install::Error("E_PS2_SETUP",issue);
 if(view!="screen"&&view!="inside"&&view!="depth"&&view!="check")throw install::Error("E_PS2_VIEW","Choose 3D screen or Inside the game.");
 if(view=="inside"&&!hasInside(game,launcher,inside))throw install::Error("E_PS2_VIEW","Inside the game is not set up for this PS2 game yet. Choose 3D screen.");
 if(view=="depth"&&!launcherKeys(launcher).contains(depthKey(game)))throw install::Error("E_PS2_VIEW","Your PS2 launcher has no depth trial for this game. Choose 3D screen.");
 if(view=="check"&&game!=kCheckGame)throw install::Error("E_PS2_VIEW","The headset check runs with Virtua Cop only. Choose 3D screen.");
 const QFileInfo script(launcher);
 QStringList args{QDir::toNativeSeparators(script.absoluteFilePath())};
 if(view=="inside"&&kInsideFlagBeforeKey)args<<kInsideFlag;
 args<<(view=="depth"?depthKey(game):game);
 if(view=="inside"&&!kInsideFlagBeforeKey)args<<kInsideFlag;
 if(view=="check")args<<"--headset-check";
 return {{"executable",QDir::toNativeSeparators(QFileInfo(python).absoluteFilePath())},{"cwd",QDir::toNativeSeparators(script.absolutePath())},
         {"args",args}};
}
}
