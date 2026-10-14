// SPDX-License-Identifier: GPL-3.0-only
#include "Report.h"
#include "Setup.h"
#include "core/model3/Model3.h"
#include "core/pinball/Pinball.h"
#include "core/scan/Scan.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QXmlStreamReader>
namespace ac::setup {
namespace {
using Scanner=QVariantMap(*)(const QVariantMap &,const QVariantMap &,const Progress &,std::atomic_bool &);
QVariantMap item(const QString &id,const QString &title,const char *key,const QString &text){return {{"id",id},{"title",title},{QString::fromLatin1(key),text}};}
struct Groups {
 QVariantList ready,notRight,missing;
 QVariantMap result(const QString &id,const QString &note)const{
  QString title;for(const auto &value:systems())if(value.toMap().value("id")==id)title=value.toMap().value("title").toString();
  return {{"id",id},{"title",title},{"note",note},{"ready",ready},{"notRight",notRight},{"missing",missing}};
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
const QHash<QString,Scanner> &scanners(){static const QHash<QString,Scanner> table{{"model3",scanModel3},{"pinball",scanPinball}};return table;}
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
