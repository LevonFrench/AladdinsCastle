// SPDX-License-Identifier: GPL-3.0-only
#include "Gamepad.h"
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMap>
#include <QRegularExpression>
#include <cstring>
#include <iterator>
#include <QStringList>
#include <QThread>
#include <QVersionNumber>
namespace ac::pinball {
namespace {
// The layout, as VPX mapping parts. A button is "Name"; an axis past a threshold is
// "Name;o;<threshold>", as VPX writes it; '&' joins buttons pressed together.
struct Binding { const char *action; QStringList parts; };
const QList<Binding> &bindings(){
 static const QList<Binding> list{
  {"LeftFlipper",{"LeftShoulder","LeftTrigger;o;-0.300000"}},
  {"RightFlipper",{"RightShoulder","RightTrigger;o;-0.300000"}},
  {"LeftStagedFlipper",{"LeftShoulder","LeftTrigger;o;0.300000"}},
  {"RightStagedFlipper",{"RightShoulder","RightTrigger;o;0.300000"}},
  {"LaunchBall",{"A"}},
  {"Start",{"Start"}},
  {"Credit1",{"Back"}},
  {"ExitGame",{"Start&Back"}},
  {"LeftMagna",{"LeftStick"}},
  {"RightMagna",{"RightStick"}},
  {"UIUp",{"DPadUp"}},{"UIDown",{"DPadDown"}},{"UILeft",{"DPadLeft"}},{"UIRight",{"DPadRight"}}};
 return list;
}
// Analog parts: dead zone, scale, limit as VPX writes them. Stick down is positive in SDL,
// which VPX's own gamepad layout reads as the plunger pulled back.
constexpr auto kSensor=";P;0.100000;1.000000;1.000000";
QByteArray eolOf(const QByteArray &text){return text.contains("\r\n")?"\r\n":"\n";}
QString value(const QString &line){const auto at=line.indexOf('=');return at<0?QString():line.mid(at+1).trimmed();}
QString keyOf(const QString &line){const auto at=line.indexOf('=');return at<0?QString():line.left(at).trimmed();}
QString part(const QString &uid,const QVariantMap &elements,const QString &spec){
 QStringList together;
 for(const auto &piece:spec.split('&')){
  const auto name=piece.section(';',0,0),rest=piece.section(';',1);
  if(!elements.contains(name))return {};
  together<<uid+";"+QString::number(elements.value(name).toInt())+(rest.isEmpty()?QString():";"+rest);
 }
 return together.join(" & ");
}
// The user's bindings for an action without any binding of this gamepad, then ours.
QString merged(const QString &current,const QString &uid,const QStringList &ours){
 QStringList kept;
 for(const auto &alternative:current.split('|',Qt::SkipEmptyParts)){const auto text=alternative.trimmed();if(!text.isEmpty()&&!text.contains(uid+";"))kept<<text;}
 return (kept+ours).join(" | ");
}
}
QVariantList gamepadLayout(){
 return {
  QVariantMap{{"control","LB / RB, or LT / RT"},{"action","Left / right flipper"}},
  QVariantMap{{"control","Right stick, pull down and let go"},{"action","Plunger"}},
  QVariantMap{{"control","A (hold)"},{"action","Plunger button"}},
  QVariantMap{{"control","Start"},{"action","Start game"}},
  QVariantMap{{"control","Back / View"},{"action","Add coin"}},
  QVariantMap{{"control","Left stick left, right, up"},{"action","Nudge"}},
  QVariantMap{{"control","Left / right stick click"},{"action","Magna-save, where the table has it"}},
  QVariantMap{{"control","D-pad"},{"action","Visual Pinball's menus"}},
  QVariantMap{{"control","Start and Back together"},{"action","Exit the table"}}};
}
QByteArray gamepadSettings(const QByteArray &settings,const QVariantList &pads){
 const auto eol=eolOf(settings);
 QStringList lines=QString::fromUtf8(settings).split('\n');
 for(auto &line:lines)if(line.endsWith('\r'))line.chop(1);
 if(!lines.isEmpty()&&lines.last().isEmpty())lines.removeLast();
 // The [Input] section: its first line and the line after its last key.
 qsizetype start=-1,end=-1;
 for(qsizetype i=0;i<lines.size();++i){
  const auto text=lines[i].trimmed();
  if(text.startsWith('[')){if(start>=0){end=i;break;}if(text.compare("[Input]",Qt::CaseInsensitive)==0)start=i;}
 }
 if(start<0){if(!lines.isEmpty()&&!lines.last().trimmed().isEmpty())lines<<QString();lines<<"[Input]";start=lines.size()-1;end=lines.size();}
 else if(end<0)end=lines.size();
 while(end-1>start&&lines[end-1].trimmed().isEmpty())--end; // New keys go before the blank lines ending the section.
 const auto find=[&](const QString &name)->qsizetype{for(qsizetype i=start+1;i<end;++i)if(keyOf(lines[i]).compare(name,Qt::CaseInsensitive)==0)return i;return -1;};
 const auto get=[&](const QString &name){const auto at=find(name);return at<0?QString():value(lines[at]);};
 const auto set=[&](const QString &name,const QString &text){
  const auto line=name+" = "+text;const auto at=find(name);
  if(at>=0)lines[at]=line;else{lines.insert(end,line);++end;}
 };
 for(const auto &value:pads){
  const auto pad=value.toMap();const auto uid=pad.value("uid").toString();const auto elements=pad.value("elements").toMap();
  if(uid.isEmpty()||uid.contains(QRegularExpression("[;|&=\\s]")))continue;
  auto devices=get("Devices").split(';',Qt::SkipEmptyParts);for(auto &d:devices)d=d.trimmed();
  if(!devices.contains(uid))devices<<uid;
  set("Devices",devices.join(';'));
  set("Device."+uid+".Type","2");
  set("Device."+uid+".Name",pad.value("name").toString().remove(QRegularExpression("[^A-Za-z0-9 #()_.-]")).trimmed().left(80));
  // VPX would otherwise offer its own gamepad layout over this one.
  set("Device."+uid+".NoAutoLayout","1");
  for(const auto &binding:bindings()){
   QStringList ours;for(const auto &spec:binding.parts){const auto text=part(uid,elements,spec);if(!text.isEmpty())ours<<text;}
   if(ours.isEmpty())continue;
   const QString name=QString("Mapping.")+binding.action;
   set(name,merged(get(name),uid,ours));
  }
  // One plunger and one nudge sensor for this gamepad, after the user's own.
  const auto sensor=[&](const QString &kind,const QString &countKey,const QStringList &fields,const QMap<QString,QString> &extra){
   const int count=qMax(0,get(countKey).toInt());int index=count;
   for(int i=0;i<count;++i)if(get(QString("Mapping.%1%2.%3").arg(kind).arg(i).arg(fields.first().section('=',0,0))).startsWith(uid+";"))index=i;
   for(const auto &field:fields)set(QString("Mapping.%1%2.%3").arg(kind).arg(index).arg(field.section('=',0,0)),field.section('=',1));
   for(auto it=extra.cbegin();it!=extra.cend();++it)set(QString("Mapping.%1%2.%3").arg(kind).arg(index).arg(it.key()),it.value());
   if(index==count)set(countKey,QString::number(count+1));
  };
  if(elements.contains("RightY"))
   sensor("Plunger","PlungerSensorCount",{"Position="+uid+";"+QString::number(elements.value("RightY").toInt())+kSensor},{{"Velocity",QString()}});
  if(elements.contains("LeftX")&&elements.contains("LeftY"))
   sensor("Nudge","NudgeSensorCount",{"X="+uid+";"+QString::number(elements.value("LeftX").toInt())+kSensor,"Y="+uid+";"+QString::number(elements.value("LeftY").toInt())+kSensor},
          {{"Type","0"},{"Strength","1.0"}});
 }
 return lines.join(QString::fromLatin1(eol)).toUtf8()+eol;
}
QByteArray exitOnChord(const QByteArray &settings){
 const auto eol=eolOf(settings);
 QStringList lines=QString::fromUtf8(settings).split('\n');
 for(auto &line:lines)if(line.endsWith('\r'))line.chop(1);
 if(!lines.isEmpty()&&lines.last().isEmpty())lines.removeLast();
 qsizetype start=-1,end=lines.size();
 for(qsizetype i=0;i<lines.size();++i){const auto text=lines[i].trimmed();if(text.startsWith('[')){if(start>=0){end=i;break;}if(text.compare("[Input]",Qt::CaseInsensitive)==0)start=i;}}
 if(start<0)return settings;
 // The VR controller's own numbers for Y and the left stick click, as VPX listed them.
 QString y="9",stick="12";bool openXr=false;
 for(qsizetype i=start+1;i<end;++i){
  const auto key=keyOf(lines[i]),text=value(lines[i]);
  if(key.compare("Devices",Qt::CaseInsensitive)==0)openXr=text.split(';').contains("OpenXR");
  if(!key.startsWith("Device.OpenXR.Element",Qt::CaseInsensitive))continue;
  if(text.endsWith("_left_input_y_click"))y=text.section(';',0,0).trimmed();
  if(text.endsWith("_left_input_thumbstick_click"))stick=text.section(';',0,0).trimmed();
 }
 const QString single="OpenXR;"+y,chord=single+" & OpenXR;"+stick;
 for(qsizetype i=start+1;i<end;++i){
  if(keyOf(lines[i]).compare("Mapping.ExitGame",Qt::CaseInsensitive)!=0)continue;
  QStringList alternatives;for(const auto &part:value(lines[i]).split('|',Qt::SkipEmptyParts))alternatives<<(part.trimmed()==single?chord:part.trimmed());
  lines[i]="Mapping.ExitGame = "+alternatives.join(" | ");
  return lines.join(QString::fromLatin1(eol)).toUtf8()+eol;
 }
 // No mapping of the user's own: VPX's default would exit on a single Y, so the chord is written with Escape.
 if(!openXr)return settings;
 while(end-1>start&&lines[end-1].trimmed().isEmpty())--end;
 lines.insert(end,"Mapping.ExitGame = Key;41 | "+chord);
 return lines.join(QString::fromLatin1(eol)).toUtf8()+eol;
}
QString userSettingsFile(const QString &executable){
 // %APPDATA%\VPinballX\<major.minor>\VPinballX.ini, the newest version folder, else beside the program.
 const QDir base(QDir(qEnvironmentVariable("APPDATA")).filePath("VPinballX"));
 QVersionNumber best;QString found;
 if(!qEnvironmentVariable("APPDATA").isEmpty())
  for(const auto &folder:base.entryList(QDir::Dirs|QDir::NoDotAndDotDot)){
   const auto version=QVersionNumber::fromString(folder);
   if(version.segmentCount()==2&&QFileInfo(base.filePath(folder+"/VPinballX.ini")).isFile()&&version>best){best=version;found=base.filePath(folder+"/VPinballX.ini");}
  }
 if(!found.isEmpty())return found;
 const QFileInfo beside(QFileInfo(executable).absoluteDir().filePath("VPinballX.ini"));
 return beside.isFile()?beside.absoluteFilePath():QString();
}
#ifdef Q_OS_WIN
namespace {
// The parts of SDL 3's public interface used here.
struct Guid { unsigned char data[16]; };
struct Binding3 { int inputType; union { int button; struct { int axis,min,max; } axis; struct { int hat,mask; } hat; } input;
                  int outputType; union { int button; struct { int axis,min,max; } axis; } output; };
constexpr unsigned kJoystick=0x200u,kGamepad=0x2000u;
enum { BindButton=1,BindAxis=2,BindHat=3 };
}
QVariantList probeGamepads(const QString &sdlLibrary){
 QVariantList result;
 if(!QFileInfo(sdlLibrary).isFile())return result;
 // Kept loaded: SDL's helper threads must not outlive the library.
 static QLibrary *library=nullptr;
 if(!library){library=new QLibrary(QFileInfo(sdlLibrary).absoluteFilePath());if(!library->load())return result;}
 using InitFn=bool(*)(unsigned);using QuitFn=void(*)(unsigned);using ListFn=unsigned*(*)(int*);using GuidFn=Guid(*)(unsigned);
 using GuidTextFn=void(*)(Guid,char*,int);using NameFn=const char*(*)(unsigned);using IsFn=bool(*)(unsigned);using OpenFn=void*(*)(unsigned);
 using BindsFn=Binding3**(*)(void*,int*);using CloseFn=void(*)(void*);using FreeFn=void(*)(void*);using UpdateFn=void(*)();
 const auto init=reinterpret_cast<InitFn>(library->resolve("SDL_InitSubSystem"));const auto quit=reinterpret_cast<QuitFn>(library->resolve("SDL_QuitSubSystem"));
 const auto list=reinterpret_cast<ListFn>(library->resolve("SDL_GetJoysticks"));const auto guidOf=reinterpret_cast<GuidFn>(library->resolve("SDL_GetJoystickGUIDForID"));
 const auto guidText=reinterpret_cast<GuidTextFn>(library->resolve("SDL_GUIDToString"));const auto nameOf=reinterpret_cast<NameFn>(library->resolve("SDL_GetJoystickNameForID"));
 const auto isPad=reinterpret_cast<IsFn>(library->resolve("SDL_IsGamepad"));const auto open=reinterpret_cast<OpenFn>(library->resolve("SDL_OpenGamepad"));
 const auto binds=reinterpret_cast<BindsFn>(library->resolve("SDL_GetGamepadBindings"));const auto close=reinterpret_cast<CloseFn>(library->resolve("SDL_CloseGamepad"));
 const auto release=reinterpret_cast<FreeFn>(library->resolve("SDL_free"));const auto update=reinterpret_cast<UpdateFn>(library->resolve("SDL_UpdateJoysticks"));
 if(!init||!quit||!list||!guidOf||!guidText||!nameOf||!isPad||!open||!binds||!close||!release||!update)return result;
 if(!init(kJoystick|kGamepad))return result;
 for(int i=0;i<5;++i){update();QThread::msleep(40);} // Devices that arrive a moment after start.
 int count=0;unsigned *ids=list(&count);
 // SDL's names for buttons and axes, by its own numbers, in the Hub's words.
 static const char *buttons[]={"A","B","X","Y","Back","Guide","Start","LeftStick","RightStick","LeftShoulder","RightShoulder","DPadUp","DPadDown","DPadLeft","DPadRight"};
 static const char *axes[]={"LeftX","LeftY","RightX","RightY","LeftTrigger","RightTrigger"};
 for(int i=0;ids&&i<count;++i){
  // The same number VPX gives: one more than the earlier devices of the same kind.
  const Guid guid=guidOf(ids[i]);int index=1;
  for(int j=0;j<i;++j){const Guid other=guidOf(ids[j]);if(memcmp(&guid,&other,sizeof guid)==0)++index;}
  char text[33]={};guidText(guid,text,33);
  if(!isPad(ids[i]))continue;
  void *pad=open(ids[i]);if(!pad)continue;
  QVariantMap elements;int n=0;
  if(Binding3 **list3=binds(pad,&n)){
   for(int k=0;k<n;++k){const auto *b=list3[k];
    if(b->outputType==BindButton&&b->output.button>=0&&b->output.button<int(std::size(buttons))){
     const QString name=QString::fromLatin1(buttons[b->output.button]);
     if(b->inputType==BindButton)elements[name]=b->input.button;
     else if(b->inputType==BindHat){const int dir=b->input.hat.mask==8?0:b->input.hat.mask==2?1:b->input.hat.mask==1?2:b->input.hat.mask==4?3:-1;if(dir>=0)elements[name]=0x0100|(b->input.hat.hat*4+dir);}
    }else if(b->outputType==BindAxis&&b->inputType==BindAxis&&b->output.axis.axis>=0&&b->output.axis.axis<int(std::size(axes)))
     elements[QString::fromLatin1(axes[b->output.axis.axis])]=0x0200|b->input.axis.axis;
   }
   release(list3);
  }
  close(pad);
  result<<QVariantMap{{"uid",QString("SDLJoy_%1_%2").arg(QString::fromLatin1(text)).arg(index)},{"name",QString::fromUtf8(nameOf(ids[i])?nameOf(ids[i]):"Gamepad")},{"elements",elements}};
 }
 if(ids)release(ids);
 quit(kJoystick|kGamepad);
 return result;
}
#else
QVariantList probeGamepads(const QString &){return {};}
#endif
}
