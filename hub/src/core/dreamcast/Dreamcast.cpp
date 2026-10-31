// SPDX-License-Identifier: GPL-3.0-only
#include "Dreamcast.h"
#include "core/install/Install.h"
#include "core/scan/Scan.h"
#include "core/scan/DiscIndex.h"
#include "core/scan/DiscMemo.h"
#include <QDir>
#include <QFileInfo>
namespace ac::dreamcast {
QVariantList games(){return scan::DiscIndex::games("dreamcast");}
QVariantList views(){return scan::DiscIndex::metadata("dreamcast").value("views").toList();}
QString viewLabel(const QString &view){for(const auto &value:views())if(value.toMap().value("id")==view)return value.toMap().value("label").toString();return {};}
QVariantMap identify(const QString &file,std::atomic_bool &cancel,scan::DiscMemo *memo){
 const QFileInfo info(file);
 // Only a number that was found is kept: an empty answer may be a read stopped by its time limit.
 const auto readProduct=[&](bool &finished){const auto product=scan::Scanner::dreamcastProduct(info.absoluteFilePath(),cancel,true);finished=!product.isEmpty()&&!cancel;return product;};
 bool finished=false;const auto product=memo?memo->identity("dreamcast",info,readProduct):readProduct(finished);
 const auto row=scan::DiscIndex::match("dreamcast",product);
 return {{"file",QDir::fromNativeSeparators(info.absoluteFilePath())},{"product",product},{"game",row.value("game",QString())},{"title",row.value("title",QString())},{"region",row.value("region",QString())},{"verified",!row.isEmpty()},{"profile",row.value("profile",false)}};
}
QVariantList discs(const QString &folder,std::atomic_bool &cancel,scan::DiscMemo *memo){
 QVariantList found;
 if(folder.isEmpty())return found;
 for(const auto &entry:QDir(folder).entryInfoList({"*.chd","*.gdi","*.cdi","*.cue","*.m3u"},QDir::Files,QDir::Name)){
  if(cancel)break;
  if(entry.isSymLink()||entry.isJunction())continue;
  found<<identify(entry.absoluteFilePath(),cancel,memo);
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
         // and on the monitor the mouse aims only a port that holds a gun. UseReios: Flycast's built-in BIOS, so a
         // real BIOS and blank flash file beside the disc in the user's own folder never start the console's
         // Set Date/Time screen; the disc plays in place (Flycast still forces the real BIOS for games that need it).
         {"args",QStringList{"-config",QString("config:UseReios=yes,")+(view=="headset"?"config:vr.Xr=yes":"config:vr.Xr=no")+",input:device1=7,input:device2=7",QDir::toNativeSeparators(QFileInfo(file).absoluteFilePath())}}};
}
}
