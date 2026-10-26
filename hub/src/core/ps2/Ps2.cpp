// SPDX-License-Identifier: GPL-3.0-only
#include "Ps2.h"
#include "core/install/Install.h"
#include "core/scan/Scan.h"
#include "core/scan/DiscIndex.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/launch/UniversalControls.h"
namespace ac::ps2 {
namespace {
QString controls(const QVariantMap &game){return QString(game.value("gun").toBool()?"Gamepad only for now (an Xbox-style controller); aiming with the VR controllers is still to come.":"Played with a gamepad (an Xbox-style controller).")+QString(" With the VR controllers, L3 is left Y and R3 is left X, for games that read them.")+(game.value("gun").toBool()?" In gun games either grip is the GunCon C button (cover).":"")+(game.value("measured").toBool()?" Depth: measured.":" Depth: experimental, a general profile not yet tuned for this game.");}
QVariantList table(){return scan::DiscIndex::games("ps2");}
}
QVariantList games(){QVariantList result;for(const auto &value:table()){const auto game=value.toMap();result<<QVariantMap{{"id",game.value("id")},{"title",game.value("title")},{"year",game.value("year")},{"controls",universal::with(controls(game))}};}return result;}
QString gameForSerial(const QString &serial){return scan::DiscIndex::match("ps2",serial).value("game").toString();}
QVariantMap identify(const QString &file,std::atomic_bool &cancel){
 const QFileInfo info(file);const auto read=scan::Scanner::inspect(info.absoluteFilePath(),cancel);
 const auto serial=read.error.isEmpty()&&read.system=="ps2"?read.identity:QString();
 const auto row=scan::DiscIndex::match("ps2",serial);
 return {{"file",QDir::fromNativeSeparators(info.absoluteFilePath())},{"serial",serial},{"game",row.value("game",QString())},{"title",row.value("title",QString())},{"verified",!row.isEmpty()}};
}
QVariantList discs(const QString &folder,std::atomic_bool &cancel){
 QVariantList found;
 if(folder.isEmpty())return found;
 for(const auto &entry:QDir(folder).entryInfoList({"*.iso","*.bin","*.chd","*.gz","*.cso","*.cue","*.m3u"},QDir::Files,QDir::Name)){
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
QStringList configuredInsideGames(){return scan::DiscIndex::metadata("ps2").value("insideGames").toStringList();}
QString insideFlag(){return scan::DiscIndex::metadata("ps2").value("insideFlag").toString();}
bool insideFlagBeforeKey(){return scan::DiscIndex::metadata("ps2").value("insideFlagBeforeKey").toBool();} // false: launcher, key, flag. true: launcher, flag, key.
}
QStringList launcherKeys(const QString &launcher){return launcherGames(launcher).keys();}
QStringList insideGames(){return configuredInsideGames();}
QString viewLabel(const QString &view){for(const auto &entry:scan::DiscIndex::metadata("ps2").value("views").toList()){const auto row=entry.toMap();if(row.value("id")==view)return row.value("label").toString();}return {};}
namespace {
// Inside the game: confirmed in the list, or the launcher's own entry has an inside profile.
bool hasInside(const QString &game,const QString &launcher,const QStringList &inside){
 const auto entry=launcherGames(launcher).value(game);
 return entry.isObject()&&(inside.contains(game)||entry.toObject().value("inside").isObject());
}
QString viewHelp(const QString &view){for(const auto &entry:scan::DiscIndex::metadata("ps2").value("views").toList()){const auto row=entry.toMap();if(row.value("id")==view)return row.value("help").toString();}return {};}
QString depthKey(const QString &game){return game+"-depth-trial";}
QString checkGame(){return scan::DiscIndex::metadata("ps2").value("checkGame").toString();}
}
QVariantList views(const QString &game,const QString &launcher,const QStringList &inside){
 QVariantList list{QVariantMap{{"id","screen"},{"label",viewLabel("screen")},{"help",viewHelp("screen")}}};
 if(hasInside(game,launcher,inside))
  list<<QVariantMap{{"id","inside"},{"label",viewLabel("inside")},{"help",viewHelp("inside")}};
 const auto keys=launcherKeys(launcher);
 if(keys.contains(depthKey(game)))
  list<<QVariantMap{{"id","depth"},{"label",viewLabel("depth")},{"help",viewHelp("depth")}};
 if(game==checkGame()&&keys.contains(game))
  list<<QVariantMap{{"id","check"},{"label",viewLabel("check")},{"help",viewHelp("check")}};
 return list;
}
QString problem(const QString &game,const QString &launcher,const QString &python,const QString &stopFile){
 bool listed=false;for(const auto &entry:table())listed=listed||game==entry.toMap().value("id").toString();
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
 if(view=="check"&&game!=checkGame())throw install::Error("E_PS2_VIEW","The headset check runs with Virtua Cop only. Choose 3D screen.");
 const QFileInfo script(launcher);
 QStringList args{QDir::toNativeSeparators(script.absoluteFilePath())};
 if(view=="inside"&&insideFlagBeforeKey())args<<insideFlag();
 args<<(view=="depth"?depthKey(game):game);
 if(view=="inside"&&!insideFlagBeforeKey())args<<insideFlag();
 if(view=="check")args<<"--headset-check";
 return {{"executable",QDir::toNativeSeparators(QFileInfo(python).absoluteFilePath())},{"cwd",QDir::toNativeSeparators(script.absolutePath())},
         {"args",args}};
}
}
