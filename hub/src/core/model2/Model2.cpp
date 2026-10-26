// SPDX-License-Identifier: GPL-3.0-only
#include "Model2.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFileInfo>
#include "core/launch/UniversalControls.h"
namespace ac::model2 {
namespace {
// file: the set's zip name when it differs from the set (a clone kept in its parent's zip).
struct Game {const char *id,*title;int year;const char *controls;const char *file=nullptr;};
const char *const viewOnly="View only for now: no controls yet.";
// A game with controls gets U1-U4 after its own text; a view-only game has no controls to describe.
QString shown(const char *text){return text==viewOnly?QString::fromUtf8(text):universal::with(QString::fromUtf8(text)+" A and B also start and insert a coin.");}
const char *const lightGun="Controls new, untested: aim with the right controller, the trigger shoots, shoot off the screen to reload.";
const char *const racing="Controls new, untested: the left stick steers, right trigger = gas, left trigger = brake, grips = shift or the game's buttons.";
const Game kGames[]={
 {"vcop2","Virtua Cop 2",1995,lightGun},
 {"vcop","Virtua Cop",1994,lightGun},
 // The usual hotd.zip holds the original revision, which MAME calls hotdo.
 {"hotdo","The House of the Dead",1997,lightGun,"hotd"},
 {"gunblade","Gunblade NY",1995,"Controls new, untested: aim the mounted gun with the right controller, the trigger fires."},
 {"bel","Behind Enemy Lines",1997,"Controls new, untested: aim the mounted gun with the right controller, the trigger fires, right grip = missile."},
 {"skytargt","Sky Target",1995,"Controls new, untested: point the right controller to fly, the trigger fires the gun, right grip = missile."},
 {"daytona","Daytona USA",1994,racing},{"srallyc","Sega Rally Championship",1995,racing},
 {"indy500","INDY 500",1995,racing},{"motoraid","Motor Raid",1997,racing},
 {"sgt24h","Super GT 24h",1996,racing},
 {"desert","Desert Tank",1994,"Controls new, untested: the left stick steers the tank, right trigger = gas, left trigger = brake, right grip = machine gun, left grip = cannon."},
 {"rchase2","Rail Chase 2",1994,"Controls new, untested: aim the mounted gun with the right controller, the trigger fires."},
 {"vf2","Virtua Fighter 2",1994,viewOnly}};
// The extra views, with the controls the old launchers said for them. Open frame adds 93 pixels a side (16:9).
struct Extra {const char *game,*view,*controls;};
const Extra kExtras[]={
 {"vcop2","open-frame","Open frame 16:9 (3D world drawn past 4:3 where the game sends it). Gun as in 3D; pointing at the extra sides counts as off the screen (reload)."},
 {"vcop","open-frame","Open frame 16:9 (3D world drawn past 4:3). Gun as in 3D; pointing at the extra sides counts as off the screen (reload)."},
 {"hotdo","open-frame","Open frame 16:9 (3D ground and trees fill the sides; the 2D sky stays 4:3). Right controller = gun (aim, trigger shoots, point off the screen to reload)."},
 {"gunblade","open-frame","Open frame 16:9. Right controller aims the mounted gun (stops at the screen edge), trigger fires."},
 {"bel","open-frame","Open frame 16:9 (world beside the 4:3 cockpit frame). Right controller aims the mounted gun, trigger fires, right grip = missile."},
 {"daytona","open-frame","Open frame 16:9. Left stick steers, right trigger = gas, left trigger = brake, grips = gears."},
 {"srallyc","open-frame","Open frame 16:9. Left stick steers, right trigger = gas, left trigger = brake, grips = gears."},
 {"indy500","open-frame","Open frame 16:9. Left stick steers, right trigger = gas, left trigger = brake, grips = shift. The game takes 2 coins per credit."},
 {"motoraid","open-frame","Open frame 16:9 (track fills the sides; the 2D sky stays 4:3). Left stick steers, right trigger = gas, left trigger = brake, grips = punch and kick."},
 {"daytona","roll-steer","Roll the right controller like a wheel (60 degrees = full lock; a pushed left stick still wins), right trigger = gas, left trigger = brake, grips = gears."},
 {"srallyc","roll-steer","Roll the right controller like a wheel (60 degrees = full lock; a pushed left stick still wins), right trigger = gas, left trigger = brake, grips = gears."},
};
const Game *findGame(const QString &id){for(const auto &game:kGames)if(id==QLatin1String(game.id))return &game;return nullptr;}
QString beside(const QString &program,const QString &path){return QDir::cleanPath(QFileInfo(program).absoluteDir().filePath("../"+path));}
// Stereo and scale settings of the build. The scale was measured in one game and is used for all.
const QList<QPair<QString,QString>> &settings(){
 static const QList<QPair<QString,QString>> values{{"ACVR_M2_STEREO","1"},{"ACVR_M2_STREAM","1"},{"ACVR_M2_INPUT_TEST","0"},{"ACVR_M2_IPD_M","0.064"},
  {"ACVR_M2_UNITS_PER_M","0.949387005"},{"ACVR_M2_ZERO_M","4"},{"ACVR_XR_SIZE","1"},{"ACVR_XR_HEIGHT","0"}};
 return values;
}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:kGames)result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"file",QString::fromLatin1(game.file?game.file:game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year},{"controls",shown(game.controls)}};
 return result;
}
QString viewLabel(const QString &view){
 if(view=="3d")return "3D screen";
 if(view=="open-frame")return "Open frame (3D, 16:9)";
 if(view=="roll-steer")return "Roll steer (opt-in)";
 return {};
}
QVariantList views(const QString &game){
 const auto *found=findGame(game);if(!found)return {};
 QVariantList list{QVariantMap{{"id","3d"},{"label",viewLabel("3d")},{"help","The game's 4:3 picture in stereo depth on a screen in the headset."},{"controls",shown(found->controls)}}};
 for(const auto *view:{"open-frame","roll-steer"})for(const auto &extra:kExtras)if(game==QLatin1String(extra.game)&&QLatin1String(view)==QLatin1String(extra.view))
  list<<QVariantMap{{"id",QString::fromLatin1(view)},{"label",viewLabel(QString::fromLatin1(view))},
   {"help",QLatin1String(view)==QLatin1String("open-frame")?QString("The game's 3D world drawn to 16:9 where the game sends it, in stereo."):QString("Steer by rolling your right controller like a wheel; the left stick still steers too.")},
   {"controls",shown(extra.controls)}};
 return list;
}
QString fileName(const QString &game){const auto *found=findGame(game);return found&&found->file?QString::fromLatin1(found->file):game;}
QString problem(const QString &game,const QString &program,const QString &gameFolder){
 const auto *found=findGame(game);
 if(!found)return "This game is not in the Model 2 list.";
 if(program.isEmpty())return "Emulator not chosen. Choose the Model 2 MAME build in Set-up.";
 if(!QFileInfo(program).isFile())return "Emulator program not found. Choose it again in Set-up.";
 if(!QFileInfo(beside(program,"presenter/model2-viewer.exe")).isFile())return "Viewer not found. It belongs in the presenter folder beside the emulator's folder.";
 if(!QFileInfo(beside(program,"runtime")).isDir())return "Emulator working folder not found. The runtime folder belongs beside the emulator's folder.";
 if(gameFolder.isEmpty())return "Game folder not chosen. Choose it in Set-up.";
 if(!QFileInfo(gameFolder).isDir())return "Game folder not found. Choose it again in Set-up.";
 if(!QFileInfo(QDir(gameFolder).filePath(fileName(game)+".zip")).isFile())return QString("%1.zip is missing. Put your own copy in the game folder.").arg(fileName(game));
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &program,const QString &gameFolder,const QString &chosenView){
 // "screen" was the one view's id before the views of the old launchers came in; it is real 3D.
 const QString view=chosenView=="screen"?QString("3d"):chosenView;
 const auto issue=problem(game,program,gameFolder);
 if(!issue.isEmpty())throw install::Error("E_MODEL2_SETUP",issue);
 bool offered=false;for(const auto &value:views(game))offered=offered||value.toMap().value("id")==view;
 if(!offered)throw install::Error("E_MODEL2_VIEW","This Model 2 game has no such view. Choose Real 3D.");
 const auto native=[](const QString &path){return QDir::toNativeSeparators(path);};
 const auto viewer=beside(program,"presenter/model2-viewer.exe");
 // The emulator draws no window; the viewer shows its picture. The emulator plays the game's sound itself through the Windows default output ("-sound none" made every Model 2 game silent until 2026-10-21).
 const QStringList emulator{game,"-rompath",native(QDir(gameFolder).absolutePath()),"-video","none","-sound","auto","-numprocessors","2","-skip_gameinfo","-noreadconfig","-nowriteconfig",
  "-snapview","Screen 1","-snapshot_directory",native(beside(program,"captures/headset-"+game)),"-snapname",game+"-%i",
  "-nvram_directory","nvram","-cfg_directory","cfg","-state_directory","sta","-diff_directory","diff","-background_input"};
 QVariantMap env;for(const auto &setting:settings())env.insert(setting.first,setting.second);
 // Open frame: extra pixels drawn on each side of the 4:3 picture (93 = 16:9); 0 = the original frame.
 env.insert("ACVR_M2_WIDE",view=="open-frame"?"93":"0");
 // Racing steering is the left stick; rolling the right controller like a wheel is opt-in.
 env.insert("ACVR_M2_ROLL_STEER",view=="roll-steer"?"1":"0");
 return {{"executable",native(viewer)},{"cwd",native(QFileInfo(viewer).absolutePath())},{"args",QStringList{"${companion_pid}"}},
         {"companion",QVariantMap{{"executable",native(QFileInfo(program).absoluteFilePath())},{"cwd",native(beside(program,"runtime"))},{"args",emulator}}},
         {"env",env}};
}
}
