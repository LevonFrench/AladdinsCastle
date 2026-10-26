// SPDX-License-Identifier: GPL-3.0-only
#include "Model3.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFileInfo>
#include "core/launch/UniversalControls.h"
namespace ac::model3 {
namespace {
// args: extra emulator arguments the game needs. folder: where the set is kept when it is not in the game folder,
// relative to the game folder (Sega Rally 2 DX keeps its own folder beside it).
struct Game {const char *id,*title;int year;const char *script;const char *args="";const char *folder="";const char *set="";};
// The start-up script presses the test button twice so the cabinet leaves its
// first-boot service screen; only that game needs one.
const Game kGames[]={
 {"scud","Scud Race",1996,""},
 {"daytona2","Daytona USA 2",1998,""},
 {"srally2","Sega Rally 2",1998,""},
 {"swtrilgy","Star Wars Trilogy",1998,"300-312:TestA:1;700-712:TestA:1"},
 {"harley","Harley-Davidson LA Riders",1997,""},
 {"dirtdvls","Dirt Devils",1998,""},
 {"vf3","Virtua Fighter 3",1996,""},
 {"spikeout","Spikeout",1998,""},
 {"fvipers2","Fighting Vipers 2",1998,""},
 {"vs2","Virtua Striker 2",1997,""},
 {"srally2x","Sega Rally 2 DX",1998,"","-ppc-frequency=100","../games-dx","srally2dx"},
};
struct Setting {const char *name,*value;};
struct View {const char *id,*label,*help;QList<Setting> env;QStringList args;};
const QList<View> &table(){
 static const QStringList screenArgs{"-window","-res=1440,1080","-ss=2","-input-system=xinput"};
 static const QList<View> views{
  {"screen","Screen","A 3D screen straight ahead of you, the size of the arcade picture.",
   {{"ACVR_XR","1"},{"ACVR_EYE_SEP","0.065"},{"ACVR_WORLD_SCALE","1"},{"ACVR_XR_DISTANCE","2.5"},{"ACVR_XR_SIZE","1"}},screenArgs},
  {"wide","Wide screen","A screen one and a half times wider, so more of your view is filled. Experimental: scenery can be missing at an edge.",
   {{"ACVR_XR","1"},{"ACVR_EYE_SEP","0.065"},{"ACVR_WORLD_SCALE","1"},{"ACVR_XR_DISTANCE","2.5"},{"ACVR_XR_SIZE","1"},{"ACVR_FOV_SCALE","1.5"},{"ACVR_NO_CULL","1"}},screenArgs},
  {"inside","Inside the game","The camera follows your head and the scene surrounds you. Score and text float on a panel ahead of you.",
   {{"ACVR_XR","1"},{"ACVR_VR","1"},{"ACVR_WORLD_SCALE","1"},{"ACVR_HUD_DISTANCE","1.6"},{"ACVR_HUD_WIDTH","1.6"}},
   {"-window","-res=1344,1344","-stretch","-ss=2","-input-system=xinput"}},
  {"open-frame","Open frame (16:9)","A 16:9 3D screen: the game world is drawn past its 4:3 edges (Supermodel's wide screen).",
   {{"ACVR_XR","1"},{"ACVR_EYE_SEP","0.065"},{"ACVR_WORLD_SCALE","1"},{"ACVR_XR_DISTANCE","2.5"},{"ACVR_XR_SIZE","1"}},
   {"-window","-res=1920,1080","-ss=2","-wide-screen","-input-system=xinput"}},
  {"depth","Depth trial","The 3D screen with the depth from the overnight measurement, to compare with Screen.",
   {{"ACVR_XR","1"},{"ACVR_WORLD_SCALE","1"},{"ACVR_XR_DISTANCE","2.5"},{"ACVR_XR_SIZE","1"}},screenArgs},
  {"inside-wheel","Inside the game, with the wheel","Inside the game, with the cabinet's wheel (or handlebars) drawn in front of you; it turns with your steering.",
   {{"ACVR_CONTROLS","1"},{"ACVR_XR","1"},{"ACVR_VR","1"},{"ACVR_WORLD_SCALE","1"},{"ACVR_HUD_DISTANCE","1.6"},{"ACVR_HUD_WIDTH","1.6"}},
   {"-window","-res=1344,1344","-stretch","-ss=2","-input-system=xinput"}},
  {"inside-stick","Inside the game, grab the stick","Inside the game with 1:1 pointing by a laser; grabbing the stick is optional.",
   {{"ACVR_XR","1"},{"ACVR_VR","1"},{"ACVR_WORLD_SCALE","1"},{"ACVR_HUD_DISTANCE","1.6"},{"ACVR_HUD_WIDTH","1.6"},{"ACVR_STICK_GRAB","1"}},
   {"-window","-res=1344,1344","-stretch","-ss=2","-input-system=xinput"}},
  {"sbs-monitor","Side by side on the monitor","Both eye views side by side in a window on the PC monitor. Needs no headset.",
   {{"ACVR_STEREO","1"},{"ACVR_EYE_SEP","0.065"},{"ACVR_WORLD_SCALE","1"},{"ACVR_CONVERGENCE","2.5"}},
   {"-window","-res=1280,960","-input-system=xinput"}},
 };
 return views;
}
// Stereo measured per game in the headset, for the screen views only (those with an eye separation):
// Sega Rally 2 with the default puts the car about four times nearer than the screen, so it floats;
// at a 3.6 m zero-parallax plane and 2 cm separation the car sits on the road and the sky is comfortable.
struct Stereo {const char *game;QList<Setting> env;};
const QList<Stereo> &stereo(){
 static const QList<Stereo> games{{"srally2",{{"ACVR_EYE_SEP","0.02"},{"ACVR_CONVERGENCE","3.6"}}},{"srally2x",{{"ACVR_EYE_SEP","0.02"},{"ACVR_CONVERGENCE","3.6"}}}};
 return games;
}
// The depth trials, one per game the overnight measurement looked at (RACER-DEPTH-REVIEW).
const QList<Stereo> &depthTrials(){
 static const QList<Stereo> games{{"scud",{{"ACVR_CONVERGENCE","3.8"},{"ACVR_EYE_SEP","0.02"}}},{"daytona2",{{"ACVR_CONVERGENCE","2.6"},{"ACVR_EYE_SEP","0.02"}}},
  {"harley",{{"ACVR_CONVERGENCE","38"},{"ACVR_EYE_SEP","0.02"}}},{"dirtdvls",{{"ACVR_CONVERGENCE","130"},{"ACVR_EYE_SEP","0.065"}}}};
 return games;
}
// Which views each game has; the first ten keep Screen, Wide screen and Inside the game.
bool offers(const QString &game,const QString &view){
 const bool firstTen=game!="srally2x";
 if(view=="screen"||view=="open-frame")return true;
 if(view=="wide"||view=="inside")return firstTen;
 if(view=="depth"){for(const auto &trial:depthTrials())if(game==QLatin1String(trial.game))return true;return false;}
 if(view=="inside-wheel")return QStringList{"scud","daytona2","srally2","dirtdvls","harley","srally2x"}.contains(game);
 if(view=="inside-stick")return game=="swtrilgy";
 if(view=="sbs-monitor")return game=="srally2";
 return false;
}
// A headset view's own text, then U1-U4 (the keyboard keys are for the window on the monitor only).
QString vr(const QString &own){return universal::with(own);}
QString viewControls(const QString &game,const QString &view){
 const QString keys="Keys: 1 = start, 3 = coin, arrows = steer and pedals, Esc = quit.";
 const QString sit=view.startsWith("inside")?"Sit how you will play before you recentre.":"";
 if(view=="sbs-monitor")return "Needs no headset. "+keys;
 if(view=="inside-wheel")return QString("The drawn %1 turns as you steer with the left stick. Triggers = pedals. ").arg(game=="harley"?"handlebars":"wheel")+vr(sit);
 if(view=="inside-stick")return "Point the right controller: the game's crosshair goes where the red laser ends. Right trigger = fire, A = event button. Holding the right grip still takes the old tilt stick. "+vr(sit);
 if(game=="swtrilgy")return vr("Point the right controller to aim, right trigger = fire.");
 if(QStringList{"vf3","fvipers2","spikeout","vs2"}.contains(game))return vr("VR: the game's buttons are on the pad.");
 return vr("VR: left stick = steering, right trigger = accelerator, left trigger = brake.");
}
const Game *findGame(const QString &id){for(const auto &game:kGames)if(id==QLatin1String(game.id))return &game;return nullptr;}
const View *findView(const QString &id){for(const auto &view:table())if(id==QLatin1String(view.id))return &view;return nullptr;}
QString gameFile(const Game &game,const QString &folder){
 const auto own=QDir(folder).absoluteFilePath(QString::fromLatin1(game.id)+".zip");
 if(!*game.folder||QFileInfo(own).isFile())return own;
 return QDir::cleanPath(QDir(folder).absoluteFilePath(QString::fromLatin1(game.folder)+"/"+QString::fromLatin1(game.id)+".zip"));
}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:kGames)result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year}};
 return result;
}
QVariantList views(){
 QVariantList result;
 for(const auto &view:table())if(QStringList{"screen","wide","inside"}.contains(QString::fromLatin1(view.id)))
  result<<QVariantMap{{"id",QString::fromLatin1(view.id)},{"label",QString::fromUtf8(view.label)},{"help",QString::fromUtf8(view.help)}};
 return result;
}
bool isMonitorView(const QString &view){return view=="sbs-monitor";}
QString gameFile(const QString &game,const QString &folder){const auto *found=findGame(game);return found?gameFile(*found,folder):QString();}
QString setName(const QString &game){const auto *found=findGame(game);return found&&*found->set?QString::fromLatin1(found->set):game;}
QVariantList views(const QString &game){
 QVariantList result;if(!findGame(game))return result;
 // The usual order: the screen in 3D, open frame, wide, the depth trial, inside the game, then the monitor.
 for(const auto *id:{"screen","open-frame","wide","depth","inside","inside-wheel","inside-stick","sbs-monitor"}){
  const QString key=QString::fromLatin1(id);
  const auto *view=findView(key);if(!view||!offers(game,key))continue;
  QString label=QString::fromUtf8(view->label);if(key=="inside-wheel"&&game=="harley")label="Inside the game, with the handlebars";
  result<<QVariantMap{{"id",key},{"label",label},{"help",QString::fromUtf8(view->help)},{"controls",viewControls(game,key)},{"monitor",isMonitorView(key)}};
 }
 return result;
}
bool hasCabinetControl(const QString &game){return QStringList{"scud","daytona2","srally2","dirtdvls","harley"}.contains(game);}
QString cabinetControlName(const QString &game){return game=="harley"?QString("handlebars"):hasCabinetControl(game)?QString("wheel"):QString();}
QString viewLabel(const QString &view){const auto *found=findView(view);return found?QString::fromUtf8(found->label):QString();}
QString workingFolder(const QString &executable){
 if(executable.isEmpty())return {};
 QDir folder(QFileInfo(executable).absolutePath());
 for(int depth=0;depth<6;++depth){
  if(QFileInfo(folder.filePath("Config/Games.xml")).isFile())return folder.absolutePath();
  if(!folder.cdUp())break;
 }
 return {};
}
QString problem(const QString &game,const QString &executable,const QString &gameFolder){
 const auto *found=findGame(game);
 if(!found)return "This game is not in the Model 3 list.";
 if(executable.isEmpty())return "Emulator not chosen. Choose it in Set-up.";
 if(!QFileInfo(executable).isFile())return "Emulator program not found. Choose it again in Set-up.";
 if(workingFolder(executable).isEmpty())return "Emulator Config folder not found. Config/Games.xml must be beside or above the program.";
 if(gameFolder.isEmpty())return "Game folder not chosen. Choose it in Set-up.";
 if(!QFileInfo(gameFolder).isDir())return "Game folder not found. Choose it again in Set-up.";
 if(!QFileInfo(gameFile(*found,gameFolder)).isFile())return QString("%1.zip is missing. Put your own copy in the game folder.").arg(QLatin1String(found->id));
 return {};
}
QVariantMap launchPlan(const QString &game,const QString &view,const QString &executable,const QString &gameFolder){
 const auto *choice=findView(view);
 if(!choice)throw install::Error("E_MODEL3_VIEW","Choose Screen, Wide screen or Inside the game.");
 const auto issue=problem(game,executable,gameFolder);
 if(!issue.isEmpty())throw install::Error("E_MODEL3_SETUP",issue);
 if(!offers(game,view))throw install::Error("E_MODEL3_VIEW","This game has no such view. Choose Screen.");
 const auto *found=findGame(game);
 QVariantMap env;
 for(const auto &setting:choice->env)env.insert(QString::fromLatin1(setting.name),QString::fromLatin1(setting.value));
 // The measured stereo of a game belongs to its headset screens (not inside the game, not the monitor).
 if(QStringList{"screen","wide","open-frame"}.contains(view))
  for(const auto &tuned:stereo())if(game==QLatin1String(tuned.game))
   for(const auto &setting:tuned.env)env.insert(QString::fromLatin1(setting.name),QString::fromLatin1(setting.value));
 if(view=="depth")
  for(const auto &trial:depthTrials())if(game==QLatin1String(trial.game))
   for(const auto &setting:trial.env)env.insert(QString::fromLatin1(setting.name),QString::fromLatin1(setting.value));
 if(*found->script)env.insert("ACVR_INPUT_SCRIPT",QString::fromLatin1(found->script));
 QStringList args=choice->args;
 if(*found->args)args.insert(args.size()-1,QString::fromLatin1(found->args)); // before -input-system, as its launcher had it
 return {{"executable",QDir::toNativeSeparators(QFileInfo(executable).absoluteFilePath())},
         {"cwd",QDir::toNativeSeparators(workingFolder(executable))},
         {"args",QStringList{QDir::toNativeSeparators(gameFile(*found,gameFolder))}+args},
         {"env",env}};
}
}
