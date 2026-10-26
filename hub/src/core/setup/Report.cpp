// SPDX-License-Identifier: GPL-3.0-only
#include "core/library/Library.h"
#include "core/wii/Wii.h"
#include "Report.h"
#include "Setup.h"
#include "core/model3/Model3.h"
#include "core/model2/Model2.h"
#include "core/ps2/Ps2.h"
#include "core/ps1/Ps1.h"
#include "core/n64/N64.h"
#include "core/saturn/Saturn.h"
#include "core/libretro/Libretro.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/pinball/Pinball.h"
#include "core/scan/Scan.h"
#include "core/scan/DiscIndex.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QXmlStreamReader>
#include <algorithm>
#include <QProcess>
#include <QProcessEnvironment>
namespace ac::setup {
namespace {
using Scanner=QVariantMap(*)(const QVariantMap &,const QVariantMap &,const Progress &,std::atomic_bool &);
QVariantMap item(const QString &id,const QString &title,const char *key,const QString &text){return {{"id",id},{"title",title},{QString::fromLatin1(key),text}};}
bool listedGame(const QVariantList &games,const QString &id){for(const auto &value:games)if(value.toMap().value("id").toString()==id)return true;return false;}
// The Game library (context "library" and the user's "libraryChoice"): every copy it found of a
// system's games, the folder of a game's chosen zip, and whether a disc is the chosen copy.
QVariantList libraryDiscs(const QVariantMap &context,const QString &system){
 QVariantList out;const auto byGame=context.value("library").toMap().value("systems").toMap().value(system).toMap().value("copies").toMap();
 for(auto it=byGame.cbegin();it!=byGame.cend();++it)out<<it.value().toList();
 return out;
}
QString libraryFolder(const QVariantMap &context,const QString &system,const QString &game){
 const auto copy=library::chosen(context.value("library").toMap(),system,game,context.value("libraryChoice").toMap());
 return copy.isEmpty()?QString():QFileInfo(copy.value("file").toString()).absolutePath();
}
bool choice(const QVariantMap &context,const QString &system,const QString &game,const QVariantMap &disc){
 const auto wanted=context.value("libraryChoice").toMap().value(system+"/"+game).toString();
 return !wanted.isEmpty()&&QDir::fromNativeSeparators(disc.value("file").toString())==QDir::fromNativeSeparators(wanted);
}
// A system with no folder of its own may still have its games in the library.
QString noFolder(const QString &folder,const QVariantMap &context,const QString &system){
 if(!folder.isEmpty())return {};
 return context.value("library").toMap().value("systems").toMap().contains(system)?QString("Found through your game library."):QString("No folder chosen for this system.");
}
struct Groups {
 QVariantList ready,notRight,missing,other;
 QVariantMap result(const QString &id,const QString &note)const{
  QString title;for(const auto &value:systems())if(value.toMap().value("id")==id)title=value.toMap().value("title").toString();
  return {{"id",id},{"title",title},{"note",note},{"ready",ready},{"notRight",notRight},{"missing",missing},{"other",other}};
 }
};
// The emulator's own list: per game, its parent and the files it must have, keyed
// by where they load (region/offset). Regions marked required="false" are left out.
struct Set {QString parent;QMap<QString,QPair<quint32,QString>> files;};
using Sets=QHash<QString,Set>;
Sets supermodelSets(const QString &gamesXml){
 QFile file(gamesXml);
 if(file.size()>32*1024*1024||!file.open(QIODevice::ReadOnly))return {};
 Sets sets;QXmlStreamReader xml(&file);QString current,region;bool required=true;
 while(!xml.atEnd()){
  xml.readNext();if(!xml.isStartElement())continue;
  const auto attributes=xml.attributes();
  if(xml.name()==u"game"){current=attributes.value("name").toString();sets[current].parent=attributes.value("parent").toString();}
  else if(xml.name()==u"region"){region=attributes.value("name").toString();required=attributes.value("required")!=u"false";}
  else if(xml.name()==u"file"&&!current.isEmpty()&&required){
   auto text=attributes.value("crc32").toString();if(text.startsWith("0x",Qt::CaseInsensitive))text=text.mid(2);
   bool ok=false;const auto crc=text.toUInt(&ok,16);
   if(ok)sets[current].files[region+"/"+attributes.value("offset").toString()]={crc,attributes.value("name").toString()};
  }
 }
 return xml.hasError()?Sets{}:sets;
}
// A game replaces its parent's file at the same place and needs the rest of the parent's.
QMap<quint32,QString> needed(const Sets &sets,const QString &game){
 const auto own=sets.value(game);auto merged=sets.value(own.parent).files;
 for(auto it=own.files.begin();it!=own.files.end();++it)merged[it.key()]=it.value();
 QMap<quint32,QString> files;for(const auto &entry:merged)files.insert(entry.first,entry.second);
 return files;
}
// The game first, then the other versions of the same machine. The emulator goes by
// what is inside the zip, so a zip named for one version may hold another.
QStringList family(const Sets &sets,const QString &game){
 QStringList names{game};const auto root=sets.value(game).parent.isEmpty()?game:sets.value(game).parent;
 if(root!=game)names<<root;
 QStringList others;for(auto it=sets.begin();it!=sets.end();++it)if(it.value().parent==root&&it.key()!=game)others<<it.key();
 others.sort();return names+others;
}
// Supermodel finds a game's files by checksum, in the game's zip and its parent's.
QSet<quint32> checksums(const QString &zip,std::atomic_bool &cancel,bool *readable){
 QSet<quint32> found;const auto file=scan::Scanner::inspect(zip,cancel);
 *readable=file.error.isEmpty()&&file.kind=="archive-crc";
 for(const auto &entry:file.entries)if(entry.hasCrc)found<<entry.crc;
 return found;
}
QVariantMap scanModel3(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 const auto systemFolder=settings.value("model3GameFolder").toString(),program=settings.value("model3Executable").toString();
 const auto work=model3::workingFolder(program);
 const bool programReady=QFileInfo(program).isFile()&&!work.isEmpty();
 const auto sets=programReady?supermodelSets(QDir(work).filePath("Config/Games.xml")):Sets{};
 Groups groups;
 for(const auto &value:model3::games()){
  if(cancel)break;
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  auto folder=systemFolder;
  if(folder.isEmpty()||!QFileInfo(model3::gameFile(id,folder)).isFile())folder=libraryFolder(context,"model3",id);
  const auto zip=folder.isEmpty()?QString():model3::gameFile(id,folder),set=model3::setName(id);
  progress(title);
  if(folder.isEmpty()||!QFileInfo(zip).isFile()){groups.missing<<item(id,title,"expected",id+".zip");continue;}
  if(!programReady){groups.notRight<<item(id,title,"reason","The game file was found by its name, but it could not be checked: the emulator build for this system is not set up on this PC.");continue;}
  if(!sets.contains(set)){groups.notRight<<item(id,title,"reason","This emulator build does not list this game.");continue;}
  bool readable=false;auto have=checksums(zip,cancel,&readable);
  if(!readable){groups.notRight<<item(id,title,"reason","The file is not a zip the Hub can read.");continue;}
  const auto parent=sets.value(set).parent;bool parentFound=true;
  if(!parent.isEmpty()){const auto parentZip=QFileInfo(zip).absoluteDir().filePath(parent+".zip");parentFound=QFileInfo(parentZip).isFile();if(parentFound){bool ok=false;have.unite(checksums(parentZip,cancel,&ok));}}
  // Right when the zip holds every needed file of this game or of another version of it.
  QString match;int closest=-1,closestTotal=0;
  for(const auto &version:family(sets,set)){
   const auto files=needed(sets,version);int absent=0;
   for(auto it=files.begin();it!=files.end();++it)if(!have.contains(it.key()))++absent;
   if(files.isEmpty())continue;
   if(absent==0){match=version;break;}
   if(closest<0||absent<closest){closest=absent;closestTotal=static_cast<int>(files.size());}
  }
  if(!match.isEmpty()){groups.ready<<item(id,title,"detail",match==set?QString("Checked against the emulator's own list."):QString("Checked against the emulator's own list. The file holds the version it calls %1.").arg(match));
   if(folder!=systemFolder){auto entry=groups.ready.last().toMap();entry["folder"]=folder;groups.ready.last()=entry;}}
  else if(!parentFound)groups.notRight<<item(id,title,"reason",QString("It also needs %1.zip beside it, which was not found.").arg(parent));
  else groups.notRight<<item(id,title,"reason",QString("%1 of the %2 files this emulator expects inside are missing or different. This is usually an older or newer version of the set.").arg(closest).arg(closestTotal));
 }
 return groups.result("model3",noFolder(systemFolder,context,"model3"));
}
QVariantMap scanPinball(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &){
 const auto folder=settings.value("pinballFolder").toString(),program=settings.value("pinballExecutable").toString();
 Groups groups;
 if(folder.isEmpty())return groups.result("pinball","No folder chosen for this system.");
 auto tables=context.value("pinballScan").toMap();
 if(!tables.contains("tables")){progress("Reading your tables. A large collection takes a few minutes the first time.");tables=pinball::scan(folder,settings.value("pinballDatabase").toString(),settings.value("pinballRoms").toString(),settings.value("pinballMedia").toString());}
 const auto marks=settings.value("pinballOverrides").toMap();const bool programReady=QFileInfo(program).isFile();
 int leftOut=0;
 for(const auto &value:tables.value("tables").toList()){
  const auto row=value.toMap();const auto id=row.value("id").toString(),title=row.value("title").toString();
  if(!(marks.contains(id)?marks.value(id).toBool():row.value("vrReady").toBool())){++leftOut;continue;}
  const auto error=row.value("error").toString();
  if(!error.isEmpty())groups.notRight<<item(id,title,"reason","The table file could not be read: "+error);
  else if(row.value("romState")=="ROM missing")groups.notRight<<item(id,title,"reason",QString("It needs the ROM %1.zip, which is not in your ROM folder.").arg(row.value("rom").toString()));
  else if(!programReady)groups.notRight<<item(id,title,"reason","The table is fine, but Visual Pinball is not set up on this PC.");
  else groups.ready<<item(id,title,"detail",row.value("romState")=="unknown"?"VR-ready. Whether it needs a ROM could not be told.":"VR-ready.");
 }
 auto result=groups.result("pinball",leftOut?QString("%1 other tables have no VR room and are left out.").arg(leftOut):QString());
 result["pinballScan"]=tables; // Handed back so the caller can keep it for the Pinball tab.
 return result;
}
quint32 crc32(const QByteArray &data){
 quint32 crc=0xffffffffU;
 for(const auto byte:data){crc^=quint8(byte);for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1U)));}
 return ~crc;
}
// The emulator's own description of one game, printed to its output. No window, nothing written.
QByteArray mameListXml(const QString &program,const QString &workingFolder,const QString &libraries,const QString &game){
 QProcess emulator;auto environment=QProcessEnvironment::systemEnvironment();
 if(!libraries.isEmpty())environment.insert("PATH",QDir::toNativeSeparators(libraries)+QDir::listSeparator()+environment.value("PATH"));
 emulator.setProcessEnvironment(environment);emulator.setWorkingDirectory(workingFolder);emulator.setStandardErrorFile(QProcess::nullDevice());
 emulator.start(program,{"-noreadconfig","-listxml",game});
 if(!emulator.waitForFinished(30000)){emulator.kill();emulator.waitForFinished(2000);return {};}
 return emulator.exitStatus()==QProcess::NormalExit&&emulator.exitCode()==0?emulator.readAllStandardOutput():QByteArray();
}
QVariantMap scanModel2(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 const auto systemFolder=settings.value("model2GameFolder").toString(),program=settings.value("model2Executable").toString();
 Groups groups;QStringList standIns;bool unchecked=false;
 for(const auto &value:model2::games()){
  if(cancel)break;
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  progress(title);
  auto folder=systemFolder;const auto file=model2::fileName(id)+".zip";
  if(folder.isEmpty()||!QFileInfo(QDir(folder).filePath(file)).isFile())folder=libraryFolder(context,"model2",id);
  if(folder.isEmpty()||!QFileInfo(QDir(folder).filePath(file)).isFile()){groups.missing<<item(id,title,"expected",file);continue;}
  const auto issue=model2::problem(id,program,folder);
  if(!issue.isEmpty()){groups.notRight<<item(id,title,"reason","The game file was found by its name, but the emulator build for this system is not set up on this PC. "+issue);continue;}
  // What this game needs, files and device sets, is asked of the emulator build itself, when the user allows it.
  if(!settings.value("queryEmulators").toBool()){unchecked=true;groups.ready<<item(id,title,"detail","Found by its set name. The files inside were not checked: the Hub runs the emulator for its list only when you allow it.");continue;}
  const auto xml=mameListXml(program,QDir::cleanPath(QFileInfo(program).absoluteDir().filePath("../runtime")),settings.value("model2RuntimePath").toString(),id);
  if(xml.isEmpty()){unchecked=true;groups.ready<<item(id,title,"detail","Found by its set name. The emulator did not give its list, so the files inside were not checked.");continue;}
  // The billboard set only drives the cabinet's sign, so a stand-in for it is accepted and said.
  const auto check=mameCheck(xml,id,folder,{"segabill"},cancel);
  for(const auto &name:check.value("standIns").toStringList())if(!standIns.contains(name))standIns<<name;
  if(check.value("ok").toBool()){groups.ready<<item(id,title,"detail","Checked against the emulator's own list. "+game.value("controls").toString());
   if(folder!=systemFolder){auto entry=groups.ready.last().toMap();entry["folder"]=folder;groups.ready.last()=entry;}}
  else groups.notRight<<item(id,title,"reason",check.value("reason").toString());
 }
 QString note="Controls new, untested: Virtua Cop 2, Virtua Cop, The House of the Dead, Gunblade NY, Behind Enemy Lines, Rail Chase 2, Sky Target, Daytona USA, Sega Rally Championship, INDY 500, Motor Raid, Super GT 24h and Desert Tank. Virtua Fighter 2: view only for now.";
 if(!standIns.isEmpty())note+=QString(" The %1 set here is a stand-in, which only leaves the cabinet's sign blank.").arg(standIns.join(", "));
 if(unchecked)note+=settings.value("queryEmulators").toBool()?QString(" Some games could not be checked against the emulator."):QString(" Allow the Hub to ask found emulators for their game lists, and the files inside each game are checked too.");
 return groups.result("model2",systemFolder.isEmpty()&&!context.value("library").toMap().value("systems").toMap().contains("model2")?QString("No folder chosen for this system."):note);
}
QVariantMap scanPs2(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 const auto folder=settings.value("ps2GameFolder").toString(),launcher=settings.value("ps2Launcher").toString();
 Groups groups;progress("Reading the disc images");
 const auto found=ps2::discs(folder,cancel)+libraryDiscs(context,"ps2");const auto keys=ps2::launcherKeys(launcher);
 for(const auto &value:ps2::games()){
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  // The best identified disc of this game, with the owner's choice first.
  QVariantMap disc;int rank=-1;
  for(const auto &candidate:found){const auto c=candidate.toMap();if(c.value("game")!=id)continue;const int r=choice(context,"ps2",id,c)?2:c.value("verified").toBool()?1:0;if(r>rank){rank=r;disc=c;}}
  if(rank<0){groups.missing<<item(id,title,"expected",title+" (disc image)");continue;}
  const auto checked=disc.value("verified").toBool()?QString("Disc %1.").arg(disc.value("serial").toString()):QString("Found by its file name; the disc's serial was not read.");
  const auto issue=ps2::problem(id,launcher,settings.value("ps2Python").toString(),settings.value("ps2StopFile").toString());
  if(!issue.isEmpty())groups.notRight<<item(id,title,"reason","The disc was found, but the game cannot be started from the Hub yet. "+issue);
  else groups.ready<<item(id,title,"detail",checked+" "+game.value("controls").toString());
 }
 for(const auto &candidate:found){const auto c=candidate.toMap();if(listedGame(ps2::games(),c.value("game").toString()))continue;
  const auto serial=c.value("serial").toString();
  groups.other<<QVariantMap{{"title",c.value("title").toString().isEmpty()?QFileInfo(c.value("file").toString()).completeBaseName():c.value("title").toString()},{"detail",serial.isEmpty()?QString("serial not read"):"disc "+serial}};}
 Q_UNUSED(keys);
 return groups.result("ps2",folder.isEmpty()&&noFolder(folder,context,"ps2").startsWith("No")?QString("No folder chosen for this system."):QString("The Hub starts these through your PS2 launcher, which plays its own copies of the discs."));
}
QVariantMap scanPs1(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 // The launcher holds each game's disc, gun and core; the Hub only checks that it knows the game.
 Q_UNUSED(context);Q_UNUSED(cancel);progress("Reading the PS1 launcher");
 const auto launcher=settings.value("ps1Launcher").toString();
 Groups groups;
 for(const auto &value:ps1::games()){
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  const auto issue=ps1::problem(id,launcher);
  if(issue.isEmpty())groups.ready<<item(id,title,"detail",game.value("controls").toString());
  else if(issue.contains("no entry"))groups.missing<<item(id,title,"expected",title+" (an entry in your PS1 launcher)");
  else groups.notRight<<item(id,title,"reason",issue);
 }
 return groups.result("ps1",launcher.isEmpty()?QString("No PS1 launcher chosen."):QString("The Hub starts these through your PS1 launcher, which knows each disc and gun."));
}
QVariantMap scanN64(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 // The launcher holds each game's cartridge dump, controls and views; the Hub only checks that it knows the game.
 Q_UNUSED(context);Q_UNUSED(cancel);progress("Reading the N64 launcher");
 const auto launcher=settings.value("n64Launcher").toString();
 Groups groups;
 for(const auto &value:n64::games()){
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  const auto issue=n64::problem(id,launcher);
  if(issue.isEmpty())groups.ready<<item(id,title,"detail",game.value("controls").toString());
  else if(issue.contains("no entry"))groups.missing<<item(id,title,"expected",title+" (an entry in your N64 launcher)");
  else groups.notRight<<item(id,title,"reason",issue);
 }
 return groups.result("n64",launcher.isEmpty()?QString("No N64 launcher chosen."):QString("The Hub starts these through your N64 launcher, which knows each cartridge dump and its views."));
}
QVariantMap scanSaturn(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 // The launcher holds each game's disc, aim trim and saved calibration; the Hub only checks that it knows the game.
 Q_UNUSED(context);Q_UNUSED(cancel);progress("Reading the Saturn launcher");
 const auto launcher=settings.value("saturnLauncher").toString();
 Groups groups;
 for(const auto &value:saturn::games()){
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  const auto issue=saturn::problem(id,launcher);
  if(issue.isEmpty())groups.ready<<item(id,title,"detail",game.value("controls").toString());
  else if(issue.contains("no entry"))groups.missing<<item(id,title,"expected",title+" (an entry in your Saturn launcher)");
  else groups.notRight<<item(id,title,"reason",issue);
 }
 return groups.result("saturn",launcher.isEmpty()?QString("No Saturn launcher chosen."):QString("The Hub starts these through your Saturn launcher, which knows each disc and its gun calibration."));
}
QVariantMap scan3ds(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 // The program, the core and the profile folder are Hub settings; each game's dump is found in the folder by its title.
 Q_UNUSED(context);Q_UNUSED(cancel);progress("Reading the 3DS folder");
 const auto host=settings.value("libretroHost").toString(),core=settings.value("threedsCore").toString(),folder=settings.value("threedsGameFolder").toString();
 const auto profiles=libretro::profilesFolder(settings.value("libretroProfiles").toString(),QCoreApplication::applicationDirPath());
 Groups groups;
 for(const auto &value:libretro::games()){
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  const auto issue=libretro::problem(id,host,core,folder,profiles);
  if(issue.isEmpty())groups.ready<<item(id,title,"detail",game.value("controls").toString());
  else if(issue.contains("game file was not found"))groups.missing<<item(id,title,"expected",title+" (your own decrypted dump, named after the game)");
  else groups.notRight<<item(id,title,"reason",issue);
 }
 return groups.result("3ds",folder.isEmpty()?QString("No 3DS folder chosen."):QString("The Hub starts these through acvr-ps1 and the Azahar core, with a profile for each game; the dumps stay where they are."));
}
QVariantMap scanDreamcast(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 const auto folder=settings.value("dreamcastGameFolder").toString();
 const bool programReady=QFileInfo(settings.value("dreamcastExecutable").toString()).isFile();
 Groups groups;
 progress("Reading the disc images");
 const auto found=dreamcast::discs(folder,cancel)+libraryDiscs(context,"dreamcast");
 for(const auto &value:dreamcast::games()){
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  // The best disc of this game: one with a VR profile, then one whose number was read.
  QVariantMap disc;int rank=-1;
  for(const auto &candidate:found){const auto c=candidate.toMap();if(c.value("game")!=id)continue;const int r=choice(context,"dreamcast",id,c)?3:c.value("profile").toBool()?2:c.value("verified").toBool()?1:0;if(r>rank){rank=r;disc=c;}}
  if(rank<0){groups.missing<<item(id,title,"expected",id=="hotd2"?title+" (US or PAL disc image)":title+" (disc image)");continue;}
  QVariantMap entry{{"id",id},{"title",title},{"file",disc.value("file")},{"startable",programReady}};
  const auto product=disc.value("product").toString();
  if(!programReady){entry["reason"]="The disc was found, but the Dreamcast VR build is not set up on this PC.";groups.notRight<<entry;}
  else if(disc.value("profile").toBool()){entry["detail"]=QString("Full VR profile (%1 release).").arg(disc.value("region").toString());groups.ready<<entry;}
  else if(id=="hotd2"&&product.isEmpty()){entry["detail"]="Found by its file name. The release could not be read from the disc, so its VR profile is not confirmed.";groups.ready<<entry;}
  else if(id=="hotd2"){entry["reason"]=QString("This release (disc number %1) has no VR profile; only the US and PAL releases do. It still starts, with the wrong view and no hands.").arg(product);groups.notRight<<entry;}
  else{entry["reason"]=QString("It starts, but there is no VR profile for it yet: the view, the HUD and the hands will be wrong.")+(disc.value("verified").toBool()?QString():QString(" Found by its file name, not checked."));groups.notRight<<entry;}
 }
 // Every other disc is named with its own number where that could be read, so a game that is there is never passed over in silence.
 for(const auto &candidate:found){const auto c=candidate.toMap();if(listedGame(dreamcast::games(),c.value("game").toString()))continue;
  const auto product=c.value("product").toString();
  groups.other<<QVariantMap{{"title",c.value("title").toString().isEmpty()?QFileInfo(c.value("file").toString()).completeBaseName():c.value("title").toString()},{"detail",product.isEmpty()?QString("number not read"):"disc number "+product}};}
 return groups.result("dreamcast",noFolder(folder,context,"dreamcast"));
}
QVariantMap scanWii(const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 const auto folder=settings.value("wiiGameFolder").toString(),program=settings.value("wiiExecutable").toString();
 const bool programReady=QFileInfo(program).isFile();
 Groups groups;
 progress("Reading the disc images");
 // Each disc is known by the game id in its own header, whatever the file is called.
 const auto found=wii::discs(folder,cancel)+libraryDiscs(context,"wii");
 for(const auto &value:wii::games()){
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString(),route=game.value("route").toString();
  // The user's chosen copy first, then the library's own order (USA first), then the system folder's.
  QVariantMap disc;for(const auto &candidate:found)if(candidate.toMap().value("game")==id&&(disc.isEmpty()||choice(context,"wii",id,candidate.toMap())))disc=candidate.toMap();
  if(disc.isEmpty()){groups.missing<<item(id,title,"expected",title+" (Wii disc image: "+game.value("ids").toStringList().join(", ")+")");continue;}
  const auto discId=disc.value("id").toString();
  QVariantMap entry{{"id",id},{"title",title},{"file",disc.value("file")},{"discId",discId},{"startable",programReady}};
  if(!programReady){entry["reason"]="The disc was found (game id "+discId+"), but the DolphinXR build is not set up on this PC.";groups.notRight<<entry;continue;}
  const auto lacking=wii::missing(id,discId,program);
  const QString routeNote=route=="working"?QString():route=="imperfect"?QString(" Dolphin plays it with known faults."):QString(" Not yet tried in DolphinXR.");
  if(route=="working"){
   // The game the owner has played this way: ready, with anything the recipe adds said plainly.
   entry["detail"]="Disc "+discId+". Played this way in the headset."+(lacking.isEmpty()?QString():" Not in this build yet: "+lacking.join("; ")+".");
   groups.ready<<entry;
  }else if(lacking.isEmpty()){entry["detail"]="Disc "+discId+"."+routeNote;groups.ready<<entry;}
  else{entry["reason"]="It starts in the headset, but this build has no "+lacking.join(", and no ")+"."+routeNote;groups.notRight<<entry;}
 }
 // Every other disc is named with its own game id, so a game that is there is never passed over in silence.
 for(const auto &candidate:found){const auto c=candidate.toMap();if(listedGame(wii::games(),c.value("game").toString()))continue;
  const auto discId=c.value("id").toString();
  groups.other<<QVariantMap{{"title",c.value("title").toString().isEmpty()?QFileInfo(c.value("file").toString()).completeBaseName():c.value("title").toString()},{"detail",discId.isEmpty()?QString("not a Wii disc header"):"game id "+discId}};}
 return groups.result("wii",folder.isEmpty()&&noFolder(folder,context,"wii").startsWith("No")?QString("No folder chosen for this system."):QString("Started through your DolphinXR build with its own settings; the discs stay where they are."));
}
QVariantMap scanSystem22(const QVariantMap &settings,const QVariantMap &,const Progress &,std::atomic_bool &){
 // The native build carries its game files in a "roms" folder beside the program.
 const auto program=settings.value("system22Executable").toString();const QFileInfo info(program);
 Groups groups;const QString id="timecris",title="Time Crisis";
 if(program.isEmpty()||!info.isFile())groups.notRight<<item(id,title,"reason","The Time Crisis VR build is not set up on this PC.");
 else if(QDir(info.absoluteDir().filePath("roms")).entryList(QDir::AllEntries|QDir::NoDotAndDotDot).isEmpty())groups.notRight<<item(id,title,"reason","The program was found, but the roms folder beside it is empty: the game files it plays are not there.");
 else groups.ready<<item(id,title,"detail","The program and the game files beside it were found.");
 return groups.result("system22",QString());
}
const QHash<QString,Scanner> &scanners(){static const QHash<QString,Scanner> table{{"model3",scanModel3},{"pinball",scanPinball},{"model2",scanModel2},{"ps2",scanPs2},{"ps1",scanPs1},{"n64",scanN64},{"saturn",scanSaturn},{"3ds",scan3ds},{"dreamcast",scanDreamcast},{"wii",scanWii},{"system22",scanSystem22}};return table;}
}
QVariantMap mameCheck(const QByteArray &listXml,const QString &game,const QString &folder,const QStringList &cosmetic,std::atomic_bool &cancel){
 // Every machine in the output with the files it loads; the game itself names its parent and its devices.
 struct Machine {QString parent;QStringList devices;QList<quint32> files;};
 QHash<QString,Machine> machines;QXmlStreamReader xml(listXml);QString current;
 while(!xml.atEnd()){
  xml.readNext();if(!xml.isStartElement())continue;
  const auto attributes=xml.attributes();
  if(xml.name()==u"machine"){current=attributes.value("name").toString();machines[current].parent=attributes.value("romof").toString();}
  else if(xml.name()==u"device_ref"&&!current.isEmpty())machines[current].devices<<attributes.value("name").toString();
  else if(xml.name()==u"rom"&&!current.isEmpty()&&attributes.value("status")!=u"nodump"&&attributes.hasAttribute("crc")){
   bool ok=false;const auto crc=attributes.value("crc").toUInt(&ok,16);if(ok)machines[current].files<<crc;
  }
 }
 const auto fail=[](const QString &reason){return QVariantMap{{"ok",false},{"reason",reason},{"standIns",QStringList{}}};};
 if(xml.hasError()||!machines.contains(game))return fail("This emulator build does not list this game.");
 const auto &wanted=machines[game];
 bool readable=false;auto have=checksums(QDir(folder).filePath(game+".zip"),cancel,&readable);
 if(!readable)return fail("The file is not a zip the Hub can read.");
 bool parentFound=true;
 if(!wanted.parent.isEmpty()){const auto parentZip=QDir(folder).filePath(wanted.parent+".zip");parentFound=QFileInfo(parentZip).isFile();if(parentFound){bool ok=false;have.unite(checksums(parentZip,cancel,&ok));}}
 int absent=0;for(const auto crc:wanted.files)if(!have.contains(crc))++absent;
 if(absent&&!parentFound)return fail(QString("It also needs %1.zip beside it, which was not found.").arg(wanted.parent));
 if(absent)return fail(QString("%1 of the %2 files this emulator expects inside are missing or different. This is usually an older or newer version of the set.").arg(absent).arg(wanted.files.size()));
 QStringList standIns,devices=wanted.devices;devices.removeDuplicates();devices.sort();
 for(const auto &device:devices){
  const auto files=machines.value(device).files;
  if(files.isEmpty())continue; // Most devices are chips with nothing to load.
  auto found=have;bool present=false;
  const auto zip=QDir(folder).filePath(device+".zip");const QDir loose(QDir(folder).filePath(device));
  if(QFileInfo(zip).isFile()){present=true;bool ok=false;found.unite(checksums(zip,cancel,&ok));}
  if(loose.exists()){present=true;for(const auto &entry:loose.entryInfoList(QDir::Files)){QFile file(entry.absoluteFilePath());if(entry.size()<=64*1024*1024&&file.open(QIODevice::ReadOnly))found<<crc32(file.readAll());}}
  bool complete=true;for(const auto crc:files)complete=complete&&found.contains(crc);
  if(complete)continue;
  if(!present)return fail(QString("The game file is right, but it needs the device set %1 beside it, which was not found.").arg(device));
  if(cosmetic.contains(device))standIns<<device;
  else return fail(QString("The game file is right, but the device set %1 beside it is not the one this emulator expects.").arg(device));
 }
 return {{"ok",true},{"reason",QString()},{"standIns",standIns}};
}
QVariantMap supermodelFiles(const QString &gamesXml,const QString &game){
 const auto sets=supermodelSets(gamesXml);
 if(!sets.contains(game))return {};
 QVariantMap files;const auto wanted=needed(sets,game);
 for(auto it=wanted.begin();it!=wanted.end();++it)files.insert(QString::number(it.key()),it.value());
 return {{"parent",sets.value(game).parent},{"files",files}};
}
namespace {
QString folderOf(const QString &system,const QVariantMap &settings){
 for(const auto &value:systems()){const auto row=value.toMap();if(row.value("id").toString()==system)return row.value("folderKey").toString().isEmpty()?QString():settings.value(row.value("folderKey").toString()).toString();}
 return {};
}
QString titleOf(const QString &system){
 for(const auto &value:systems())if(value.toMap().value("id").toString()==system)return value.toMap().value("title").toString();
 return system;
}
// The Wii folder also holds GameCube discs.
bool sameFamily(const QString &system,const QString &disc){return disc==system||(system=="wii"&&disc=="gamecube");}
constexpr int kMostListed=50;
// What the library already knows about this system's folder: the files it matched to nothing, and the games found twice.
QVariantMap withFinds(QVariantMap result,const QString &system,const QVariantMap &settings,const QVariantMap &context){
 const auto found=context.value("library").toMap();
 if(found.isEmpty())return result;
 const auto folder=folderOf(system,settings);
 // The pinball folder's own report names its tables and ROMs; a zip is not an unknown there.
 if(!folder.isEmpty()&&system!="pinball"){
  const bool arcade=system=="model2"||system=="model3";
  QVariantList files;
  for(const auto &value:library::unrecognisedIn(found,folder)){
   const auto entry=value.toMap();const auto kind=entry.value("kind").toString();
   if(arcade?kind!="archive":kind=="archive")continue;
   if(kind=="other"&&sameFamily(system,entry.value("system").toString()))continue; // Already in "other".
   files<<value;
  }
  result["unrecognisedChecked"]=library::covers(found,folder);
  result["unrecognisedMore"]=qMax(0,int(files.size())-kMostListed);
  result["unrecognised"]=files.size()>kMostListed?files.mid(0,kMostListed):files;
 }
 QVariantList twice;
 for(const auto &value:found.value("duplicates").toList())if(value.toMap().value("system").toString()==system)twice<<value;
 if(!twice.isEmpty()){result["duplicates"]=twice.size()>kMostListed?twice.mid(0,kMostListed):twice;result["duplicateRule"]=library::duplicateRule();}
 return result;
}
}
QVariantMap scanSystem(const QString &system,const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 const auto scanner=scanners().value(system);
 if(scanner)return withFinds(scanner(settings,context,progress?progress:[](const QString &){},cancel),system,settings,context);
 Groups none;return none.result(system,"The Hub cannot check this system yet.");
}
QVariantMap reportStamp(const QVariantMap &settings,const QStringList &systems){
 // Raised whenever a system is read differently, so every saved report is read again.
 static constexpr int kVersion=3;
 static const QStringList folders{"model3GameFolder","model2GameFolder","model2RuntimePath","pinballFolder","pinballRoms","ps2GameFolder","wiiGameFolder","dreamcastGameFolder","threedsGameFolder"};
 static const QStringList programs{"model3Executable","model2Executable","system22Executable","pinballExecutable","pinballDatabase","pinballMedia","pinballOverrides",
                                   "ps2Python","ps2StopFile","wiiExecutable","dreamcastExecutable","libretroHost","threedsCore","libretroProfiles","queryEmulators"};
 static const QStringList launchers{"ps1Launcher","ps2Launcher","n64Launcher","saturnLauncher"};
 // Times as text: a saved number comes back as a double.
 const auto time=[](const QString &path){const QFileInfo info(path);return path.isEmpty()||!info.exists()?QString():QString::number(info.lastModified().toMSecsSinceEpoch());};
 QVariantMap values,times;
 for(const auto &key:folders){const auto value=settings.value(key).toString();values[key]=value;times[key]=time(value);}
 for(const auto &key:programs)values[key]=settings.value(key).toString();
 for(const auto &key:launchers){const auto value=settings.value(key).toString();values[key]=value;
  times[key]=value.isEmpty()?QString():time(QFileInfo(value).absoluteDir().filePath("games.json"));}
 auto read=systems;read.sort();
 return {{"version",kVersion},{"discData",scan::DiscIndex::fingerprint()},{"systems",read},{"settings",values},{"times",times}};
}
namespace {
// The Hub's names for arcade sets, to tell a set it knows from any other zip; a disc, table or cartridge needs none.
struct Probe{QString system;QSet<QString> sets;};
Probe probeFor(const QString &system){
 Probe probe{system,{}};
 if(system=="model3")for(const auto &value:model3::games())probe.sets<<value.toMap().value("id").toString();
 else if(system=="model2")for(const auto &value:model2::games()){const auto game=value.toMap();probe.sets<<game.value("id").toString();if(!game.value("file").toString().isEmpty())probe.sets<<game.value("file").toString();}
 return probe;
}
bool belongs(const Probe &probe,const QString &path,std::atomic_bool &cancel){
 // Arcade sets are known by their set name, as the library knows them; the zip must also open.
 if((probe.system=="model3"||probe.system=="model2")&&!probe.sets.contains(QFileInfo(path).completeBaseName().toLower()))return false;
 return scan::looksLike(probe.system,path,cancel);
}
// The files of the system's kinds in these folders and a few levels below. A track file of a cue or gdi sheet is left out:
// it is read through its sheet, never on its own.
QStringList kindFiles(const QString &system,const QStringList &roots,int depth,std::atomic_bool &cancel){
 const auto all=scan::walk(roots,scan::systemExtensions(system),depth,cancel);
 QSet<QString> sheets;
 for(const auto &file:all){const QFileInfo info(file);const auto ext=info.suffix().toLower();if(ext=="cue"||ext=="gdi")sheets<<info.absolutePath();}
 QStringList kept;
 for(const auto &file:all){const QFileInfo info(file);if(info.suffix().compare("bin",Qt::CaseInsensitive)==0&&sheets.contains(info.absolutePath()))continue;kept<<QDir::fromNativeSeparators(info.absoluteFilePath());}
 kept.removeDuplicates();kept.sort();
 return kept;
}
struct Tally{int checked=0,matched=0;};
// A few files spread evenly through the list are read, each at most once however many folders hold it, and the whole
// check stops when the budget of reads is spent.
Tally tally(const Probe &probe,const QStringList &files,int count,QHash<QString,bool> &seen,int &budget,std::atomic_bool &cancel){
 Tally result;const int n=int(files.size()),take=qMin(n,count);
 for(int k=0;k<take&&!cancel;++k){
  const auto file=files.at(k*n/take);bool yes=false;
  const auto known=seen.constFind(file);
  if(known!=seen.cend())yes=known.value();
  else{if(budget<=0)break;--budget;yes=belongs(probe,file,cancel);if(cancel)break;seen.insert(file,yes);}
  ++result.checked;if(yes)++result.matched;
 }
 return result;
}
QString describe(const QString &system,qsizetype files,const Tally &t){
 if(files==0)return "No files of the kinds this system uses were found here.";
 if(t.matched==0)return QString("None of the %1 files checked look like %2 games.").arg(t.checked).arg(titleOf(system));
 return QString("%1 of the %2 files checked look like %3 games.").arg(t.matched).arg(t.checked).arg(titleOf(system));
}
bool inside(const QString &path,const QString &folder){return path==folder||path.startsWith(folder+"/");}
struct Candidate{QString folder;int files,checked,matched;bool sure;};
}
QVariantMap folderCheck(const QString &system,const QString &folder,std::atomic_bool &cancel){
 const auto where=QDir::cleanPath(QDir::fromNativeSeparators(folder));
 const auto files=folder.isEmpty()||!QFileInfo(folder).isDir()?QStringList{}:kindFiles(system,QStringList{folder},2,cancel);
 QHash<QString,bool> seen;int budget=8;
 const auto t=tally(probeFor(system),files,8,seen,budget,cancel);
 return {{"folder",where},{"files",int(files.size())},{"checked",t.checked},{"matched",t.matched},{"detail",describe(system,files.size(),t)}};
}
QVariantList suggestFolders(const QString &system,const QString &configured,const QStringList &knownFolders,const QStringList &libraryRoots,std::atomic_bool &cancel){
 if(scan::systemExtensions(system).isEmpty())return {};
 // Where to look: beside and above each chosen folder (and a roms folder at either place), the library's roots, and below
 // the configured folder itself. A drive's own root is never searched.
 QStringList roots;
 const auto add=[&roots](const QString &path){
  if(path.isEmpty())return;
  const QDir dir(path);
  if(!dir.exists()||dir.isRoot())return;
  const auto clean=QDir::cleanPath(dir.absolutePath());
  if(!roots.contains(clean))roots<<clean;
 };
 for(const auto &known:knownFolders+QStringList{configured}){
  if(known.isEmpty()||!QFileInfo(known).isDir())continue;
  QDir above(known);
  for(int level=0;level<2&&above.cdUp();++level){add(above.absolutePath());add(above.absoluteFilePath("roms"));}
 }
 add(configured);
 for(const auto &root:libraryRoots)add(root);
 const auto files=kindFiles(system,roots,3,cancel);
 // Every folder holding some of those files, itself or below it.
 QHash<QString,QStringList> held;
 for(const auto &file:files)
  for(const auto &root:roots){
   if(!inside(file,root))continue;
   auto dir=QFileInfo(file).absolutePath();
   while(inside(dir,root)){
    held[dir]<<file;
    const auto up=QFileInfo(dir).absolutePath();
    if(up==dir)break;
    dir=up;
   }
  }
 for(auto it=held.begin();it!=held.end();++it)it.value().removeDuplicates();
 const auto skip=QDir::cleanPath(QDir::fromNativeSeparators(configured));
 QStringList order;
 for(auto it=held.cbegin();it!=held.cend();++it)if(it.key()!=skip)order<<it.key();
 // The fullest folders are read first, up to a fixed number, so a big collection is never read through.
 std::sort(order.begin(),order.end(),[&held](const QString &a,const QString &b){const auto x=held.value(a).size(),y=held.value(b).size();return x!=y?x>y:a<b;});
 if(order.size()>60)order=order.mid(0,60);
 QList<Candidate> found;
 const auto probe=probeFor(system);
 QHash<QString,bool> seen;
 int budget=200;
 for(const auto &folder:order){
  if(cancel)break;
  auto inFolder=held.value(folder);
  inFolder.sort();
  const auto t=tally(probe,inFolder,5,seen,budget,cancel);
  if(t.matched>0)found.push_back(Candidate{folder,int(inFolder.size()),t.checked,t.matched,t.matched*5>=t.checked*4});
 }
 // A folder most of whose files are the system's comes first, then the fuller one, then the more specific.
 std::sort(found.begin(),found.end(),[](const Candidate &a,const Candidate &b){return a.sure!=b.sure?a.sure:a.files!=b.files?a.files>b.files:a.folder.size()!=b.folder.size()?a.folder.size()>b.folder.size():a.folder<b.folder;});
 // When one folder fits well, the ones that only half fit are not offered.
 const bool anySure=!found.isEmpty()&&found.first().sure;
 QVariantList suggestions;
 QList<Candidate> chosen;
 for(const auto &candidate:found){
  if(anySure&&!candidate.sure)break;
  // Not a folder inside one already offered, nor one that holds no more than a more specific one already offered.
  bool nested=false;
  for(const auto &other:chosen)nested=nested||inside(candidate.folder,other.folder)||(inside(other.folder,candidate.folder)&&candidate.files==other.files);
  if(nested)continue;
  chosen.push_back(candidate);
  Tally seenOf;seenOf.checked=candidate.checked;seenOf.matched=candidate.matched;
  suggestions<<QVariantMap{{"folder",candidate.folder},{"files",candidate.files},{"checked",candidate.checked},{"matched",candidate.matched},
   {"detail",describe(system,candidate.files,seenOf)}};
  if(suggestions.size()==3)break;
 }
 return suggestions;
}
}
