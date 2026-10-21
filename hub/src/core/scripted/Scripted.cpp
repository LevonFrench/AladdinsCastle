// SPDX-License-Identifier: GPL-3.0-only
#include "Scripted.h"
#include "core/install/Install.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <algorithm>
namespace ac::scripted {
namespace {
struct Known {const char *id,*label,*help;};
// The usual order of views: the game's own world first, opt-in controls and the monitor last.
const QList<Known> &known(){
 static const QList<Known> views{
  {"3d","Real 3D","The game's own world in stereo depth."},
  {"screen","3D screen","A 3D screen straight ahead of you."},
  {"open-frame","Open frame","The game drawn past its old 4:3 frame, filling more of your view."},
  {"inside","Inside the game","No frame: the game opens around you."},
  {"gun","With the gun","The game with the gun in your hand."},
  {"pointer","Pointer","Your right controller is the Wii Remote's pointer."},
  {"motion","Motion controls","Your controllers move like the game's own controllers."},
  {"pad","Pad","The game's pad on your controllers."},
  {"theatre","Theatre screen","The original picture on a big screen in front of you."},
  {"headset-rate","Headset refresh rate","The console's video rate raised to your headset's refresh rate."},
  {"no-60fps","Headset rate, no 60 fps code","As Headset refresh rate, without the 60 fps code: try it if the game hangs at start."},
  {"tilt-steer","Tilt steer (opt-in)","Steer by tilting your controller, like a wheel."},
  {"roll-steer","Roll steer (opt-in)","Steer by rolling your right controller, like a wheel."},
  {"body","Body motion (opt-in)","Your body and both controllers play the cabinet's own controls."},
  {"depth-trial","Depth trial","Measured depth settings to compare with the usual view."},
  {"monitor","On the monitor","A normal window on the PC monitor, with no headset."},
 };
 return views;
}
int rank(const QString &id){for(int i=0;i<known().size();++i)if(id==QLatin1String(known()[i].id))return i;return id.endsWith("monitor")?int(known().size())+1:int(known().size());}
const Known *findKnown(const QString &id){for(const auto &view:known())if(id==QLatin1String(view.id))return &view;return nullptr;}
QJsonObject read(const QString &launcher){
 if(launcher.isEmpty())return {};
 QFile file(QFileInfo(launcher).absoluteDir().filePath("games.json"));
 if(file.size()>8*1024*1024||!file.open(QIODevice::ReadOnly))return {};
 auto bytes=file.readAll();
 if(bytes.startsWith("\xEF\xBB\xBF"))bytes.remove(0,3); // Windows PowerShell writes a byte-order mark.
 return QJsonDocument::fromJson(bytes).object();
}
bool validKey(const QString &key){static const QRegularExpression pattern("^[a-z0-9][a-z0-9-]*$");return pattern.match(key).hasMatch();}
QVariantList viewsOf(const QJsonObject &entry){
 QVariantList list;
 const auto views=entry.value("views");
 QStringList ids;QJsonObject details;
 if(views.isObject()){details=views.toObject();ids=details.keys();}
 else for(const auto &value:views.toArray())ids<<value.toString();
 std::stable_sort(ids.begin(),ids.end(),[](const QString &a,const QString &b){return rank(a)<rank(b);});
 for(const auto &id:ids){
  if(!validKey(id))continue;
  const auto own=details.value(id).toObject();const auto *base=findKnown(id);
  QVariantMap view{{"id",id},{"label",own.value("label").toString(base?QString::fromUtf8(base->label):id)},
                   {"help",own.value("help").toString(base?QString::fromUtf8(base->help):QString())},
                   {"controls",own.value("controls").toString(entry.value("controls").toString())}};
  view["monitor"]=own.value("monitor").toBool(id=="monitor"||id.endsWith("-monitor"));
  list<<view;
 }
 return list;
}
} // namespace
const QList<System> &systems(){
 static const QList<System> list{
  {"mamegun","ARCADE","mamegunLauncher","mame-gun","launch-mame-gun-key.ps1"},
  {"dreamcast","DREAMCAST","dreamcastLauncher","dreamcast","launch-dreamcast.ps1"},
  {"wii","WII","wiiLauncher","wii","launch-wii.ps1"},
  {"gamecube","GAMECUBE","gamecubeLauncher","gamecube","launch-gamecube.ps1"},
 };
 return list;
}
const System *system(const QString &id){for(const auto &entry:systems())if(entry.id==id)return &entry;return nullptr;}
QString locate(const System &system,const QString &chosen,const QStringList &otherLaunchers){
 if(!chosen.isEmpty())return QFileInfo(chosen).isFile()?QFileInfo(chosen).absoluteFilePath():QString();
 for(const auto &other:otherLaunchers){
  if(other.isEmpty()||!QFileInfo(other).isFile())continue;
  const auto candidate=QFileInfo(QFileInfo(other).absoluteDir().filePath("../"+system.folder+"/"+system.script));
  if(candidate.isFile())return QDir::cleanPath(candidate.absoluteFilePath());
 }
 return {};
}
QVariantList games(const QString &launcher){
 const auto all=read(launcher);QVariantList result;
 for(auto it=all.begin();it!=all.end();++it){
  if(!validKey(it.key())||!it.value().isObject())continue;
  const auto entry=it.value().toObject();
  const auto views=viewsOf(entry);if(views.isEmpty())continue;
  result<<QVariantMap{{"id",it.key()},{"title",entry.value("title").toString(it.key())},{"year",entry.value("year").toInt()},
                      {"controls",entry.value("controls").toString()},{"group",entry.value("group").toString(entry.value("system").toString())},
                      {"status",entry.value("status").toString()},{"views",views}};
 }
 std::stable_sort(result.begin(),result.end(),[](const QVariant &a,const QVariant &b){return a.toMap().value("title").toString().compare(b.toMap().value("title").toString(),Qt::CaseInsensitive)<0;});
 return result;
}
QString viewLabel(const QString &view){const auto *found=findKnown(view);return found?QString::fromUtf8(found->label):view;}
bool isMonitor(const QVariantMap &view){return view.value("monitor").toBool();}
QString problem(const QString &launcher,const QString &key,const QString &view){
 if(launcher.isEmpty()||!QFileInfo(launcher).isFile())return "Launcher not found. Choose it in Set-up.";
 for(const auto &value:games(launcher)){
  const auto game=value.toMap();if(game.value("id")!=key)continue;
  if(view.isEmpty())return {};
  for(const auto &v:game.value("views").toList())if(v.toMap().value("id")==view)return {};
  return "Your launcher has no such view for this game. Choose another view.";
 }
 return "Your launcher has no entry for this game.";
}
QVariantMap launchPlan(const QString &launcher,const QString &key,const QString &view){
 const auto issue=problem(launcher,key,view);
 if(!issue.isEmpty()||view.isEmpty())throw install::Error("E_LAUNCHER_SETUP",issue.isEmpty()?QString("Choose a view."):issue);
 const QFileInfo script(launcher);
 // Windows PowerShell 5.1 from the system folder, never one found on PATH.
 const auto windows=QProcessEnvironment::systemEnvironment().value("SystemRoot","C:\\Windows");
 const auto shell=QDir(windows).filePath("System32/WindowsPowerShell/v1.0/powershell.exe");
 return {{"executable",QDir::toNativeSeparators(shell)},{"cwd",QDir::toNativeSeparators(script.absolutePath())},
         {"args",QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(script.absoluteFilePath()),key,"-View",view}}};
}
} // namespace ac::scripted
