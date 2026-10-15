// SPDX-License-Identifier: GPL-3.0-only
#include "Model3.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFileInfo>
namespace ac::model3 {
namespace {
struct Game {const char *id,*title;int year;const char *script;};
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
 };
 return views;
}
const Game *findGame(const QString &id){for(const auto &game:kGames)if(id==QLatin1String(game.id))return &game;return nullptr;}
const View *findView(const QString &id){for(const auto &view:table())if(id==QLatin1String(view.id))return &view;return nullptr;}
QString gameFile(const Game &game,const QString &folder){return QDir(folder).absoluteFilePath(QString::fromLatin1(game.id)+".zip");}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:kGames)result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year}};
 return result;
}
QVariantList views(){
 QVariantList result;
 for(const auto &view:table())result<<QVariantMap{{"id",QString::fromLatin1(view.id)},{"label",QString::fromUtf8(view.label)},{"help",QString::fromUtf8(view.help)}};
 return result;
}
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
 const auto *found=findGame(game);
 QVariantMap env;
 for(const auto &setting:choice->env)env.insert(QString::fromLatin1(setting.name),QString::fromLatin1(setting.value));
 if(*found->script)env.insert("ACVR_INPUT_SCRIPT",QString::fromLatin1(found->script));
 return {{"executable",QDir::toNativeSeparators(QFileInfo(executable).absoluteFilePath())},
         {"cwd",QDir::toNativeSeparators(workingFolder(executable))},
         {"args",QStringList{QDir::toNativeSeparators(gameFile(*found,gameFolder))}+choice->args},
         {"env",env}};
}
}
