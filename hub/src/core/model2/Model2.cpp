// SPDX-License-Identifier: GPL-3.0-only
#include "Model2.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFileInfo>
namespace ac::model2 {
namespace {
struct Game {const char *id,*title;int year;const char *controls;};
const char *const viewOnly="View only for now: no controls yet.";
const Game kGames[]={
 {"vcop2","Virtua Cop 2",1995,"Controls new, untested: aim with the right controller, the trigger shoots, shoot off the screen to reload, A = Start, B = Coin."},
 {"gunblade","Gunblade NY",1995,viewOnly},{"vf2","Virtua Fighter 2",1994,viewOnly}};
const Game *findGame(const QString &id){for(const auto &game:kGames)if(id==QLatin1String(game.id))return &game;return nullptr;}
QString beside(const QString &program,const QString &path){return QDir::cleanPath(QFileInfo(program).absoluteDir().filePath("../"+path));}
// Stereo and scale settings of the build. The scale was measured in one game and is used for all three.
const QList<QPair<QString,QString>> &settings(){
 static const QList<QPair<QString,QString>> values{{"ACVR_M2_STEREO","1"},{"ACVR_M2_STREAM","1"},{"ACVR_M2_INPUT_TEST","0"},{"ACVR_M2_IPD_M","0.064"},
  {"ACVR_M2_UNITS_PER_M","0.949387005"},{"ACVR_M2_ZERO_M","4"},{"ACVR_XR_SIZE","1"},{"ACVR_XR_HEIGHT","0"}};
 return values;
}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:kGames)result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year},{"controls",QString::fromUtf8(game.controls)}};
 return result;
}
QString problem(const QString &game,const QString &program,const QString &gameFolder){
 const auto *found=findGame(game);
 if(!found)return "This game is not in the Model 2 list.";
 if(program.isEmpty())return "Emulator not chosen. Choose the Model 2 MAME build in Set-up.";
 if(!QFileInfo(program).isFile())return "Emulator program not found. Choose it again in Set-up.";
 if(!QFileInfo(beside(program,"presenter/model2-viewer.exe")).isFile())return "Viewer not found. It belongs in the presenter folder beside the emulator's folder.";
 if(!QFileInfo(beside(program,"runtime")).isDir())return "Emulator working folder not found. The runtime folder belongs beside the emulator's folder.";
 if(gameFolder.isEmpty())return "Game folder not chosen. Choose it in Set-up.";
 if(!QFileInfo(gameFolder).isDir())return "Game folder not found. Choose it again in Set-up.";
 if(!QFileInfo(QDir(gameFolder).filePath(QString::fromLatin1(found->id)+".zip")).isFile())return QString("%1.zip is missing. Put your own copy in the game folder.").arg(QLatin1String(found->id));
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &program,const QString &gameFolder){
 const auto issue=problem(game,program,gameFolder);
 if(!issue.isEmpty())throw install::Error("E_MODEL2_SETUP",issue);
 const auto native=[](const QString &path){return QDir::toNativeSeparators(path);};
 const auto viewer=beside(program,"presenter/model2-viewer.exe");
 // The emulator draws no window and plays no sound itself; the viewer shows its picture.
 const QStringList emulator{game,"-rompath",native(QDir(gameFolder).absolutePath()),"-video","none","-sound","none","-numprocessors","2","-skip_gameinfo","-noreadconfig","-nowriteconfig",
  "-seconds_to_run","3600","-snapview","Screen 1","-snapshot_directory",native(beside(program,"captures/headset-"+game)),"-snapname",game+"-%i",
  "-nvram_directory","nvram","-cfg_directory","cfg","-state_directory","sta","-diff_directory","diff","-background_input"};
 QVariantMap env;for(const auto &setting:settings())env.insert(setting.first,setting.second);
 return {{"executable",native(viewer)},{"cwd",native(QFileInfo(viewer).absolutePath())},{"args",QStringList{"${companion_pid}"}},
         {"companion",QVariantMap{{"executable",native(QFileInfo(program).absoluteFilePath())},{"cwd",native(beside(program,"runtime"))},{"args",emulator}}},
         {"env",env}};
}
}
