// SPDX-License-Identifier: GPL-3.0-only
#include "System22.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFileInfo>
namespace ac::system22 {
namespace {
struct View {const char *id,*label,*help,*controls,*env;};
const char *const headsetControls="A = credits, either trigger = start and fire, hold a grip to leave cover, X = recentre. Close its window on the PC to quit.";
const QList<View> &table(){
 static const QList<View> list{
  {"vr","In the headset","The whole game around you, with a tracked gun.",headsetControls,""},
  {"frame","With the screen frame","As In the headset, with a pale blue outline of the arcade screen where the game's 2D effects stop.",headsetControls,"TCVR_SCREEN_FRAME"},
  {"inbetween","Smooth in-between frames (test)","The game still runs at 60, but at 90 or 120 Hz the 3D scene is drawn part way between its last two frames. Up to one game frame behind; hits are unchanged. Set SteamVR to 90 or 120 Hz first.",headsetControls,"TCVR_INBETWEEN"},
  {"own-gun","Our own gun (untested)","The same game with our own pistol model and button labels. Never tried in a headset yet.",headsetControls,""},
  {"monitor","On the monitor","Flat-screen mode with the mouse. Needs no headset.","C = three credits, left mouse = fire and start, right mouse or Space = leave cover, Escape = pause.",""},
 };
 return list;
}
const View *find(const QString &id){for(const auto &view:table())if(id==QLatin1String(view.id))return &view;return nullptr;}
}
QString ownGunExecutable(const QString &executable){
 if(executable.isEmpty())return {};
 return QDir::cleanPath(QFileInfo(executable).absoluteDir().filePath("../../../own-gun85/build/pc/"+QFileInfo(executable).fileName()));
}
QVariantList views(const QString &executable){
 QVariantList list;
 for(const auto &view:table()){
  const QString id=QString::fromLatin1(view.id);
  if(id=="own-gun"&&!QFileInfo(ownGunExecutable(executable)).isFile())continue;
  list<<QVariantMap{{"id",id},{"label",QString::fromUtf8(view.label)},{"help",QString::fromUtf8(view.help)},{"controls",QString::fromUtf8(view.controls)},{"monitor",id=="monitor"}};
 }
 return list;
}
QString viewLabel(const QString &view){const auto *found=find(view);return found?QString::fromUtf8(found->label):QString();}
QVariantMap launchPlan(const QString &executable,const QString &view){
 const auto *choice=find(view);
 if(!choice)throw install::Error("E_SYSTEM22_VIEW","Choose In the headset or On the monitor.");
 QFileInfo program(view=="own-gun"?ownGunExecutable(executable):executable);
 if(executable.isEmpty()||!QFileInfo(executable).isFile())throw install::Error("E_SYSTEM22_SETUP","The Time Crisis VR build is not set up. Choose its program in Set-up.");
 if(!program.isFile())throw install::Error("E_SYSTEM22_SETUP","The build with our own gun is not on this PC. Choose In the headset.");
 // Only the view's own setting; the Hub clears every other TCVR_ setting first, as a fresh .cmd window had none.
 QVariantMap env;
 if(*choice->env)env.insert(QString::fromLatin1(choice->env),"1");
 return {{"executable",QDir::toNativeSeparators(program.absoluteFilePath())},{"cwd",QDir::toNativeSeparators(program.absolutePath())},
         {"args",view=="monitor"?QStringList{"--desktop"}:QStringList{}},{"env",env}};
}
} // namespace ac::system22
