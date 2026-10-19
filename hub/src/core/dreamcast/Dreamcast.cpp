// SPDX-License-Identifier: GPL-3.0-only
#include "Dreamcast.h"
#include "core/install/Install.h"
#include "core/scan/Scan.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
namespace ac::dreamcast {
namespace {
struct Release {const char *product,*region;bool profile;};
// Words that name the game in a file name, for a disc whose own number cannot be read.
struct Game {const char *id,*title;int year;QList<Release> releases;QStringList words;};
const QList<Game> &table(){
 static const QList<Game> games{
  {"hotd2","The House of the Dead 2",1999,{{"MK-51002","US",true},{"MK-5100250","PAL",true}},{"houseofthedead2","hotd2"}},
  {"confmiss","Confidential Mission",2001,{{"MK-51160","US",false}},{"confidentialmission","confmission","confmiss"}},
  {"dcox","Death Crimson OX",2001,{{"T2401N","US",false}},{"deathcrimsonox","crimsonox"}},
  {"dcrimson2","Death Crimson 2",1999,{{"T23201M","Japan",false}},{"deathcrimson2","deathcrimsonii"}},
  {"vcop2","Virtua Cop 2",2000,{{"HDR-0061","Japan",false}},{"virtuacop2","vcop2"}},
 };
 return games;
}
QString squeezed(QString text){static const QRegularExpression other("[^a-z0-9]");return text.toLower().remove(other);}
}
QVariantList games(){
 QVariantList result;
 for(const auto &game:table())result<<QVariantMap{{"id",QString::fromLatin1(game.id)},{"title",QString::fromUtf8(game.title)},{"year",game.year}};
 return result;
}
QVariantList views(){
 return {QVariantMap{{"id","headset"},{"label","In the headset"},{"help","The game around you in VR."}},
         QVariantMap{{"id","monitor"},{"label","On the monitor"},{"help","A normal window with no headset. The mouse is the gun."}}};
}
QString viewLabel(const QString &view){for(const auto &value:views())if(value.toMap().value("id")==view)return value.toMap().value("label").toString();return {};}
QVariantMap identify(const QString &file,std::atomic_bool &cancel){
 const QFileInfo info(file);
 auto product=scan::Scanner::dreamcastProduct(info.absoluteFilePath(),cancel);
 // A file named like one of the listed games is worth the longer read for its number.
 if(product.isEmpty()){const auto name=squeezed(info.completeBaseName());bool named=false;for(const auto &game:table())for(const auto &word:game.words)named=named||name.contains(word);
  if(named)product=scan::Scanner::dreamcastProduct(info.absoluteFilePath(),cancel,true);}
 QVariantMap disc{{"file",QDir::fromNativeSeparators(info.absoluteFilePath())},{"product",product},{"game",QString()},{"title",QString()},{"region",QString()},{"verified",false},{"profile",false}};
 const auto set=[&](const Game &game){disc["game"]=QString::fromLatin1(game.id);disc["title"]=QString::fromUtf8(game.title);};
 // The disc's own number decides when it can be read; the file name only suggests.
 if(!product.isEmpty())for(const auto &game:table())for(const auto &release:game.releases)if(product==QLatin1String(release.product)){
  set(game);disc["region"]=QString::fromLatin1(release.region);disc["verified"]=true;disc["profile"]=release.profile;return disc;
 }
 const auto name=squeezed(info.completeBaseName());
 // Longer words first, so "Death Crimson OX" is not taken for "Death Crimson 2" or the reverse.
 const Game *best=nullptr;qsizetype length=0;
 for(const auto &game:table())for(const auto &word:game.words)if(name.contains(word)&&word.size()>length){best=&game;length=word.size();}
 if(best)set(*best);
 return disc;
}
QVariantList discs(const QString &folder,std::atomic_bool &cancel){
 QVariantList found;
 if(folder.isEmpty())return found;
 for(const auto &entry:QDir(folder).entryInfoList({"*.chd","*.gdi","*.cdi","*.cue"},QDir::Files,QDir::Name)){
  if(cancel)break;
  if(entry.isSymLink()||entry.isJunction())continue;
  found<<identify(entry.absoluteFilePath(),cancel);
 }
 return found;
}
QVariantMap launchPlan(const QString &file,const QString &view,const QString &executable){
 if(viewLabel(view).isEmpty())throw install::Error("E_DREAMCAST_VIEW","Choose In the headset or On the monitor.");
 if(executable.isEmpty()||!QFileInfo(executable).isFile())throw install::Error("E_DREAMCAST_SETUP","The Dreamcast VR build is not set up. Choose its program in Set-up.");
 if(file.isEmpty()||!QFileInfo(file).isFile())throw install::Error("E_DREAMCAST_DISC","The disc image is no longer where it was. Use Rescan in Set-up.");
 const QFileInfo program(executable);
 return {{"executable",QDir::toNativeSeparators(program.absoluteFilePath())},{"cwd",QDir::toNativeSeparators(program.absolutePath())},
         // Light guns in ports A and B: the build only plugs them in itself for games it has a profile for,
         // and on the monitor the mouse aims only a port that holds a gun.
         {"args",QStringList{"-config",QString(view=="headset"?"config:vr.Xr=yes":"config:vr.Xr=no")+",input:device1=7,input:device2=7",QDir::toNativeSeparators(QFileInfo(file).absoluteFilePath())}}};
}
}
