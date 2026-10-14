// SPDX-License-Identifier: GPL-3.0-only
#include "Report.h"
#include "Setup.h"
#include "core/model3/Model3.h"
#include "core/model2/Model2.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/pinball/Pinball.h"
#include "core/scan/Scan.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QXmlStreamReader>
#include <QProcess>
#include <QProcessEnvironment>
namespace ac::setup {
namespace {
using Scanner=QVariantMap(*)(const QVariantMap &,const QVariantMap &,const Progress &,std::atomic_bool &);
QVariantMap item(const QString &id,const QString &title,const char *key,const QString &text){return {{"id",id},{"title",title},{QString::fromLatin1(key),text}};}
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
QVariantMap scanModel3(const QVariantMap &settings,const QVariantMap &,const Progress &progress,std::atomic_bool &cancel){
 const auto folder=settings.value("model3GameFolder").toString(),program=settings.value("model3Executable").toString();
 const auto work=model3::workingFolder(program);
 const bool programReady=QFileInfo(program).isFile()&&!work.isEmpty();
 const auto sets=programReady?supermodelSets(QDir(work).filePath("Config/Games.xml")):Sets{};
 Groups groups;
 for(const auto &value:model3::games()){
  if(cancel)break;
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  const auto zip=QDir(folder).filePath(id+".zip");
  progress(title);
  if(folder.isEmpty()||!QFileInfo(zip).isFile()){groups.missing<<item(id,title,"expected",id+".zip");continue;}
  if(!programReady){groups.notRight<<item(id,title,"reason","The game file was found by its name, but it could not be checked: the emulator build for this system is not set up on this PC.");continue;}
  if(!sets.contains(id)){groups.notRight<<item(id,title,"reason","This emulator build does not list this game.");continue;}
  bool readable=false;auto have=checksums(zip,cancel,&readable);
  if(!readable){groups.notRight<<item(id,title,"reason","The file is not a zip the Hub can read.");continue;}
  const auto parent=sets.value(id).parent;bool parentFound=true;
  if(!parent.isEmpty()){const auto parentZip=QDir(folder).filePath(parent+".zip");parentFound=QFileInfo(parentZip).isFile();if(parentFound){bool ok=false;have.unite(checksums(parentZip,cancel,&ok));}}
  // Right when the zip holds every needed file of this game or of another version of it.
  QString match;int closest=-1,closestTotal=0;
  for(const auto &version:family(sets,id)){
   const auto files=needed(sets,version);int absent=0;
   for(auto it=files.begin();it!=files.end();++it)if(!have.contains(it.key()))++absent;
   if(files.isEmpty())continue;
   if(absent==0){match=version;break;}
   if(closest<0||absent<closest){closest=absent;closestTotal=static_cast<int>(files.size());}
  }
  if(!match.isEmpty())groups.ready<<item(id,title,"detail",match==id?QString("Checked against the emulator's own list."):QString("Checked against the emulator's own list. The file holds the version it calls %1.").arg(match));
  else if(!parentFound)groups.notRight<<item(id,title,"reason",QString("It also needs %1.zip beside it, which was not found.").arg(parent));
  else groups.notRight<<item(id,title,"reason",QString("%1 of the %2 files this emulator expects inside are missing or different. This is usually an older or newer version of the set.").arg(closest).arg(closestTotal));
 }
 return groups.result("model3",folder.isEmpty()?"No folder chosen for this system.":QString());
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
QVariantMap scanModel2(const QVariantMap &settings,const QVariantMap &,const Progress &progress,std::atomic_bool &cancel){
 const auto folder=settings.value("model2GameFolder").toString(),program=settings.value("model2Executable").toString();
 Groups groups;QStringList standIns;bool unchecked=false;
 for(const auto &value:model2::games()){
  if(cancel)break;
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  progress(title);
  if(folder.isEmpty()||!QFileInfo(QDir(folder).filePath(id+".zip")).isFile()){groups.missing<<item(id,title,"expected",id+".zip");continue;}
  const auto issue=model2::problem(id,program,folder);
  if(!issue.isEmpty()){groups.notRight<<item(id,title,"reason","The game file was found by its name, but the emulator build for this system is not set up on this PC. "+issue);continue;}
  // What this game needs, files and device sets, is asked of the emulator build itself.
  const auto xml=mameListXml(program,QDir::cleanPath(QFileInfo(program).absoluteDir().filePath("../runtime")),settings.value("model2RuntimePath").toString(),id);
  if(xml.isEmpty()){unchecked=true;groups.ready<<item(id,title,"detail","Found by its set name. The emulator did not give its list, so the files inside were not checked.");continue;}
  // The billboard set only drives the cabinet's sign, so a stand-in for it is accepted and said.
  const auto check=mameCheck(xml,id,folder,{"segabill"},cancel);
  for(const auto &name:check.value("standIns").toStringList())if(!standIns.contains(name))standIns<<name;
  if(check.value("ok").toBool())groups.ready<<item(id,title,"detail","Checked against the emulator's own list. "+game.value("controls").toString());
  else groups.notRight<<item(id,title,"reason",check.value("reason").toString());
 }
 QString note="Virtua Cop 2: controls new, untested. Gunblade NY and Virtua Fighter 2: view only for now.";
 if(!standIns.isEmpty())note+=QString(" The %1 set here is a stand-in, which only leaves the cabinet's sign blank.").arg(standIns.join(", "));
 if(unchecked)note+=" Some games could not be checked against the emulator.";
 return groups.result("model2",folder.isEmpty()?"No folder chosen for this system.":note);
}
QVariantMap scanDreamcast(const QVariantMap &settings,const QVariantMap &,const Progress &progress,std::atomic_bool &cancel){
 const auto folder=settings.value("dreamcastGameFolder").toString();
 const bool programReady=QFileInfo(settings.value("dreamcastExecutable").toString()).isFile();
 Groups groups;
 progress("Reading the disc images");
 const auto found=dreamcast::discs(folder,cancel);
 for(const auto &value:dreamcast::games()){
  const auto game=value.toMap();const auto id=game.value("id").toString(),title=game.value("title").toString();
  // The best disc of this game: one with a VR profile, then one whose number was read.
  QVariantMap disc;int rank=-1;
  for(const auto &candidate:found){const auto c=candidate.toMap();if(c.value("game")!=id)continue;const int r=c.value("profile").toBool()?2:c.value("verified").toBool()?1:0;if(r>rank){rank=r;disc=c;}}
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
 for(const auto &candidate:found){const auto c=candidate.toMap();if(!c.value("game").toString().isEmpty())continue;
  const auto product=c.value("product").toString();
  groups.other<<QVariantMap{{"title",QFileInfo(c.value("file").toString()).completeBaseName()},{"detail",product.isEmpty()?QString("number not read"):"disc number "+product}};}
 return groups.result("dreamcast",folder.isEmpty()?"No folder chosen for this system.":"New: not yet tried in a headset.");
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
const QHash<QString,Scanner> &scanners(){static const QHash<QString,Scanner> table{{"model3",scanModel3},{"pinball",scanPinball},{"model2",scanModel2},{"dreamcast",scanDreamcast},{"system22",scanSystem22}};return table;}
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
QVariantMap scanSystem(const QString &system,const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel){
 const auto scanner=scanners().value(system);
 if(scanner)return scanner(settings,context,progress?progress:[](const QString &){},cancel);
 Groups none;return none.result(system,"The Hub cannot check this system yet.");
}
}
