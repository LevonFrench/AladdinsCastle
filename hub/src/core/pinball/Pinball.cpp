// SPDX-License-Identifier: GPL-3.0-only
#include "Pinball.h"
#include "core/install/Support.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QSet>
#include <QMap>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QtEndian>
#include <stdexcept>
namespace ac::pinball {
namespace {
constexpr quint32 End=0xfffffffeU, Free=0xffffffffU;
void require(bool ok){if(!ok)throw std::runtime_error("Invalid or unsupported bounded VPX metadata");}
quint32 word(const QByteArray &b,qsizetype p){require(p>=0&&p+4<=b.size());return qFromLittleEndian<quint32>(b.constData()+p);}
QString display(QString s){s.remove(QChar(0));s.remove(QRegularExpression("https?://[^\\s]+",QRegularExpression::CaseInsensitiveOption));return s.trimmed().left(512);}
QString key(QString s){static const QRegularExpression punctuation("[^\\p{L}\\p{N}]+");return s.toCaseFolded().remove(punctuation);}
bool link(const QFileInfo &f){return f.isSymLink()||f.isJunction();}
// Independently implemented CFB reader. Only named metadata streams are read;
// geometry, textures, sounds and embedded assets are never extracted.
class Compound {
 QFile file;quint32 sectorSize=512;quint64 budget=0;
 QVector<quint32> fat,miniFat;QByteArray dirs,mini;
 QMap<QString,QByteArray> entries;
 QByteArray bytes(quint64 pos,quint64 n){require(n<=8*1024*1024&&budget+n<=32*1024*1024&&pos<=quint64(file.size())&&n<=quint64(file.size())-pos);budget+=n;require(file.seek(qint64(pos)));auto b=file.read(qint64(n));require(quint64(b.size())==n);return b;}
 QByteArray sector(quint32 id){require(id<quint64(file.size())/sectorSize-1);return bytes((quint64(id)+1)*sectorSize,sectorSize);}
 QByteArray chain(quint32 id,const QVector<quint32> &map,bool small,quint64 size=0){
  require(size<=8*1024*1024);QByteArray out;QSet<quint32> seen;
  while(id!=End){require(id<quint32(map.size())&&!seen.contains(id)&&seen.size()<16384);seen.insert(id);
   if(small){require(quint64(id)*64+64<=quint64(mini.size()));out+=mini.mid(qsizetype(id)*64,64);}else out+=sector(id);
   require(out.size()<=8*1024*1024);id=map[id];
  }
  if(size){require(quint64(out.size())>=size);out.truncate(qsizetype(size));}return out;
 }
 void tree(quint32 id,const QString &prefix,QSet<quint32> &seen,int depth){
  if(id==Free) return;
  require(depth<40&&id<quint32(dirs.size()/128)&&!seen.contains(id));seen.insert(id);
  const auto e=dirs.mid(qsizetype(id)*128,128);const auto type=quint8(e[66]);
  require(type==1||type==2);const auto len=qFromLittleEndian<quint16>(e.constData()+64);require(len>=2&&len<=64&&len%2==0);
  const auto name=QString::fromUtf16(reinterpret_cast<const char16_t*>(e.constData()),len/2-1);
  require(!name.contains('/')&&!name.contains('\\'));
  tree(word(e,68),prefix,seen,depth+1);const auto path=prefix+name;
  if(type==1)tree(word(e,76),path+"/",seen,depth+1);else {require(!entries.contains(path));entries[path]=e;}
  tree(word(e,72),prefix,seen,depth+1);
 }
public:
 explicit Compound(const QString &path):file(path){
  require(file.open(QIODevice::ReadOnly)&&file.size()>=512);auto h=bytes(0,512);
  require(h.left(8)==QByteArray::fromHex("d0cf11e0a1b11ae1")&&qFromLittleEndian<quint16>(h.constData()+28)==0xfffe);
  const auto shift=qFromLittleEndian<quint16>(h.constData()+30),version=qFromLittleEndian<quint16>(h.constData()+26);
  require((version==3&&shift==9)||(version==4&&shift==12));sectorSize=1U<<shift;
  require(qFromLittleEndian<quint16>(h.constData()+32)==6&&word(h,56)==4096);
  const auto nfat=word(h,44);require(nfat>0&&nfat<=4096);
  QVector<quint32> ids;for(int i=0;i<109;++i){auto id=word(h,76+i*4);if(id!=Free)ids<<id;}
  auto next=word(h,68);const auto ndifat=word(h,72);require(ndifat<=256);QSet<quint32> seen;
  for(quint32 i=0;i<ndifat;++i){require(!seen.contains(next));seen.insert(next);auto d=sector(next);for(quint32 j=0;j<sectorSize/4-1;++j){auto id=word(d,j*4);if(id!=Free)ids<<id;}next=word(d,sectorSize-4);}
  require(quint64(ids.size())==nfat);seen.clear();for(auto id:ids){require(!seen.contains(id));seen.insert(id);auto b=sector(id);for(qsizetype j=0;j<b.size();j+=4)fat<<word(b,j);}
  dirs=chain(word(h,48),fat,false);require(dirs.size()>=128&&quint8(dirs[66])==5);
  const auto root=dirs.left(128);const auto miniSize=qFromLittleEndian<quint64>(root.constData()+120);
  if(miniSize)mini=chain(word(root,116),fat,false,miniSize);
  const auto nm=word(h,64);require(nm<=1024);
  if(nm){auto b=chain(word(h,60),fat,false);require(quint64(b.size())==quint64(nm)*sectorSize);for(qsizetype j=0;j<b.size();j+=4)miniFat<<word(b,j);}
  seen.clear();tree(word(root,76),QString(),seen,0);
 }
 QByteArray stream(const QString &name){if(!entries.contains(name))return {};const auto e=entries[name];auto size=qFromLittleEndian<quint64>(e.constData()+120);if(!size)return {};return chain(word(e,116),size<4096?miniFat:fat,size<4096,size);}
};
QString scriptFrom(const QByteArray &data){
 qsizetype p=0;while(p+8<=data.size()){
  const auto len=word(data,p);require(len>=4&&len<=quint64(data.size()-p-4));auto tag=data.mid(p+4,4);
  if(tag=="SECB")throw std::runtime_error("Protected script metadata is not classified");
  if(tag=="CODE"){require(len==4&&p+12<=data.size());const auto n=word(data,p+8);require(n<=2*1024*1024&&n<=quint64(data.size()-p-12));return QString::fromUtf8(data.mid(p+12,n));}
  if(tag=="ENDB") return {};
  p+=4+len;
 }throw std::runtime_error("No bounded script record was found");
}
}
QVariantMap inspect(const QString &path) {
 QVariantMap row{{"path",QFileInfo(path).absoluteFilePath()},{"title",display(QFileInfo(path).completeBaseName())},{"vrReady",false},{"evidence",QStringList{}},{"romState","unknown"}};
 try{
  require(!link(QFileInfo(path)));Compound c(path);
  for(const auto &item:{qMakePair(QString("title"),QString("TableName")),qMakePair(QString("authors"),QString("AuthorName")),qMakePair(QString("version"),QString("TableVersion"))}){
   const auto b=c.stream("TableInfo/"+item.second);if(!b.isEmpty()){require(b.size()%2==0);row[item.first]=display(QString::fromUtf16(reinterpret_cast<const char16_t*>(b.constData()),b.size()/2));}
  }
  const auto script=scriptFrom(c.stream("GameStg/GameData"));
  // Comments do not supply evidence. Conservative switch + visible room object.
  QString code,roomCode;
  for(const auto &line:script.split('\n')){
   if(line.trimmed().startsWith("Rem ",Qt::CaseInsensitive))continue;
   for(qsizetype i=0;i<line.size();++i){
    if(line[i]=='\'')break;
    if(line[i]!='"'){code+=line[i];roomCode+=line[i];continue;}
    const auto start=i++;
    while(i<line.size()){
     if(line[i]=='"'){if(i+1<line.size()&&line[i+1]=='"'){i+=2;continue;}break;}++i;
    }
    require(i<line.size());const auto literal=line.mid(start,i-start+1);
    code+=literal;roomCode+=literal.compare("\"VR Room\"",Qt::CaseInsensitive)==0?literal:QString("\"\"");
   }code+='\n';roomCode+='\n';
  }
  static const QRegularExpression switchAssignment("(?:^|:|\\bThen\\s+|\\bElse\\s+)\\s*(?:Const\\s+)?VR_?Room\\s*=\\s*(?:[0-9]+|True\\b|False\\b|VR_?RoomChoice\\b|[A-Za-z0-9_]+\\.Option\\s*\\(\\s*\"VR Room\")",QRegularExpression::CaseInsensitiveOption|QRegularExpression::MultilineOption);
  static const QRegularExpression roomObject("\\bVR(?:Room|_Room|_)[A-Za-z0-9_]*\\.Visible\\s*=",QRegularExpression::CaseInsensitiveOption);
  const bool room=switchAssignment.match(roomCode).hasMatch()&&roomObject.match(roomCode).hasMatch();
  if(room){row["vrReady"]=true;row["evidence"]=QStringList{"script: VR room"};}
  static const QRegularExpression romAssignment("(?:^|:|\\bThen\\s+|\\bElse\\s+)\\s*(?:Const\\s+)?(?:cGameName|GameName)\\s*=\\s*",QRegularExpression::CaseInsensitiveOption|QRegularExpression::MultilineOption);
  static const QRegularExpression literalRom("^\"([A-Za-z0-9_]+)\"[ \\t\\r]*(?:$|:)",QRegularExpression::MultilineOption);
  auto matches=romAssignment.globalMatch(code);QSet<QString> names;int assignments=0,literals=0;
  while(matches.hasNext()){const auto assignment=matches.next();++assignments;const auto literal=literalRom.match(code.mid(assignment.capturedEnd()));if(literal.hasMatch()&&literal.capturedStart()==0){names.insert(literal.captured(1));++literals;}}
  if(names.size()==1&&assignments==literals){row["rom"]=*names.begin();row["romState"]="ROM missing";}
  else if(names.isEmpty()&&!QRegularExpression("\\b(cGameName|GameName|VPinMAME|LoadVPM|Controller\\.Run|CreateObject|Execute)\\b",QRegularExpression::CaseInsensitiveOption).match(code).hasMatch())row["romState"]="needs no ROM";
  const QRegularExpression machineName("^(.*?)\\s*\\(([^()]*)\\s+(\\d{4})\\)");
  auto identity=machineName.match(row["title"].toString());
  if(!identity.hasMatch())identity=machineName.match(QFileInfo(path).completeBaseName());
  if(identity.hasMatch()){row["machine"]=identity.captured(1).trimmed();row["maker"]=display(identity.captured(2));row["year"]=identity.captured(3).toInt();}
 }catch(const std::exception &e){row["error"]=QString::fromUtf8(e.what());row["vrReady"]=false;row["evidence"]=QStringList{};}
 return row;
}
QVariantMap scan(const QString &folder,const QString &database,const QString &romFolder,const QString &mediaFolder) {
 QVariantList rows;QJsonArray db;
 if(!database.isEmpty()){QFile f(database);require(!link(QFileInfo(database))&&f.open(QIODevice::ReadOnly)&&f.size()<=64*1024*1024);QJsonParseError error;const auto doc=QJsonDocument::fromJson(f.readAll(),&error);require(error.error==QJsonParseError::NoError&&doc.isArray());db=doc.array();}
 QMultiMap<QString,QJsonObject> machineIndex;
 for(const auto &value:db){auto m=value.toObject();machineIndex.insert(key(m["name"].toString())+"|"+key(m["manufacturer"].toString())+"|"+QString::number(m["year"].toInt()),m);}
 QDir dir(folder);require(dir.exists());int ready=0;
 for(const auto &file:dir.entryInfoList({"*.vpx","*.VPX"},QDir::Files|QDir::NoDotAndDotDot,QDir::Name)){
  if(link(file)) continue;
  auto row=inspect(file.absoluteFilePath());
  row["size"]=file.size();row["mtime"]=file.lastModified().toMSecsSinceEpoch();
  // Explicit roots, exact stems, metadata only. No recursion or remote artwork.
  QStringList artRoots{folder};if(!mediaFolder.isEmpty())artRoots.prepend(mediaFolder);
  for(const auto &root:artRoots){
   for(const auto &ext:{"png","jpg","webp"}){
    const QFileInfo image(QDir(root).filePath(file.completeBaseName()+"."+ext));
    if(!row.contains("wheelPath")&&image.isFile()&&!link(image)&&image.size()<=32*1024*1024){row["wheelPath"]=image.absoluteFilePath();row["wheelUrl"]=QUrl::fromLocalFile(image.absoluteFilePath()).toString();}
   }
   const QFileInfo backglass(QDir(root).filePath(file.completeBaseName()+".directb2s"));
   if(!row.contains("backglassPath")&&backglass.isFile()&&!link(backglass))row["backglassPath"]=backglass.absoluteFilePath();
  }
  const auto rom=row.value("rom").toString();if(!rom.isEmpty()&&!romFolder.isEmpty()){
   QFileInfo zip(QDir(romFolder).filePath(rom+".zip"));if(zip.isFile()&&!link(zip))row["romState"]="ready";
  }
  const auto matchKey=key(row.value("machine").toString())+"|"+key(row.value("maker").toString())+"|"+QString::number(row.value("year").toInt());
  const auto machines=row.value("machine").toString().isEmpty()?QList<QJsonObject>{}:machineIndex.values(matchKey);
  if(machines.size()==1){
   const auto machine=machines.first();QList<QJsonObject> files;
   for(const auto &v:machine["tableFiles"].toArray()){auto f=v.toObject();if(!f["tableFormat"].toString().isEmpty()&&f["tableFormat"].toString()!="VPX")continue;QStringList authors;for(const auto &a:f["authors"].toArray())authors<<a.toString();if(key(authors.join(" "))==key(row.value("authors").toString())&&f["version"].toString().trimmed().toCaseFolded()==row.value("version").toString().trimmed().toCaseFolded()&&!key(row.value("authors").toString()).isEmpty())files<<f;}
   if(files.size()==1){QStringList features;for(const auto &f:files.first()["features"].toArray())features<<display(f.toString());row["features"]=features;row["type"]=display(machine["type"].toString());row["players"]=machine["players"].toInt();row["theme"]=display(machine["theme"].toString());if(features.contains("VR")&&row.value("error").toString().isEmpty()){auto evidence=row["evidence"].toStringList();evidence<<"VPS: VR";row["evidence"]=evidence;row["vrReady"]=true;}}
  }
  row["id"]=QString::fromLatin1(QCryptographicHash::hash(file.absoluteFilePath().toUtf8(),QCryptographicHash::Sha256).toHex());
  if(row["vrReady"].toBool()) ++ready;
  rows<<row;
 }
 return {{"tables",rows},{"total",rows.size()},{"vrReady",ready}};
}
QVariantMap launchGuidance() {
 return {{"launchAllowed",true},{"help","Play asks Visual Pinball to start the table in the headset. Hub does this with a small settings file of its own for that table, layered over your normal settings, which it never changes. VPX saves its own scores and settings. Hub scans read-only and never edits or copies your tables."}};
}
QVariantMap launchPlan(const QString &table,const QString &executable,const QString &ownedRoot,bool vr) {
 require(QFileInfo(table).isFile()&&QFileInfo(table).suffix().compare("vpx",Qt::CaseInsensitive)==0&&!link(QFileInfo(table))&&QFileInfo(executable).isFile()&&!link(QFileInfo(executable)));
 require(!inspect(table).contains("error"));
 auto plan=launchGuidance();
 plan.insert("executable",QFileInfo(executable).absoluteFilePath());
 plan.insert("cwd",QFileInfo(executable).absolutePath());
 QStringList args{"-Play",QFileInfo(table).absoluteFilePath()};
 if(vr&&!ownedRoot.isEmpty()){
  // The id is the scan's table id, so no table name reaches a Hub file name.
  const auto id=QString::fromLatin1(QCryptographicHash::hash(QFileInfo(table).absoluteFilePath().toUtf8(),QCryptographicHash::Sha256).toHex());
  const auto settings=QDir::toNativeSeparators(install::scopedPath("user/pinball/"+id+".ini",ownedRoot));
  args<<"-TableIni"<<settings;plan.insert("tableSettings",settings);
  // VPX's own choice when no file is given: beside the table, named after it or after its folder.
  const QFileInfo file(table);QString seed;
  for(const auto &name:{file.completeBaseName()+".ini",file.absoluteDir().dirName()+".ini"}){
   const QFileInfo candidate(file.absoluteDir().filePath(name));
   if(seed.isEmpty()&&candidate.isFile()&&!link(candidate))seed=candidate.absoluteFilePath();
  }
  plan.insert("tableSettingsSeed",seed);
 }
 plan.insert("args",args);
 return plan;
}
QByteArray vrOnSettings(const QByteArray &settings) {
 // Line edit only: comments, order, other keys and the file's line endings stay as they are.
 const QByteArray eol=settings.contains("\r\n")?"\r\n":"\n",wanted="AskToTurnOn = 0";
 auto lines=settings.split('\n');
 for(auto &line:lines)if(line.endsWith('\r'))line.chop(1);
 if(!lines.isEmpty()&&lines.last().isEmpty())lines.removeLast();
 qsizetype section=-1;bool done=false;
 for(qsizetype i=0;i<lines.size()&&!done;++i){
  const auto text=lines[i].trimmed();
  if(text.startsWith('[')){if(section>=0)break;if(text.toLower()=="[playervr]")section=i;continue;}
  if(section<0)continue;
  const auto equals=text.indexOf('=');
  if(equals>0&&text.left(equals).trimmed().toLower()=="asktoturnon"){lines[i]=wanted;done=true;}
 }
 if(!done){
  if(section>=0)lines.insert(section+1,wanted);
  else{if(!lines.isEmpty()&&!lines.last().trimmed().isEmpty())lines<<QByteArray();lines<<"[PlayerVR]"<<wanted;}
 }
 return lines.join(eol)+eol;
}
void writeVrSettings(const QVariantMap &plan) {
 const auto path=plan.value("tableSettings").toString();
 if(path.isEmpty())return;
 const auto seed=plan.value("tableSettingsSeed").toString();
 const bool exists=QFileInfo(path).isFile();
 QByteArray current;
 if(exists)current=install::readBytes(path);
 else if(!seed.isEmpty()&&QFileInfo(seed).size()<=1024*1024)current=install::readBytes(seed);
 const auto updated=vrOnSettings(current);
 if(exists&&updated==current)return;
 if(!QDir().mkpath(QFileInfo(path).absolutePath()))throw install::Error("E_WRITE_DENIED","Cannot create the Hub's Pinball settings folder");
 install::atomicWrite(path,updated);
}
}
