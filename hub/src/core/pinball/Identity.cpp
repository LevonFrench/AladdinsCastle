// SPDX-License-Identifier: GPL-3.0-only
#include "Identity.h"
#include "core/match/TitleMatch.h"
#include <QFileInfo>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
namespace ac::pinball {
namespace {
QString shown(QString s){s.remove(QChar(0));return s.simplified().left(512);}
QString key(QString s){static const QRegularExpression punctuation("[^\\p{L}\\p{N}]+");return s.toCaseFolded().remove(punctuation);}
QString makerKey(QString s){
 auto k=key(s);
 if(k=="willams"||k=="williamsmanufacturing")return "williams";
 if(k=="gottliebpremier"||k=="premier"||k=="mylstar"||k=="gottileb")return "gottlieb";
 if(k=="ballymidway"||k=="midway")return "bally";
 if(k=="sternpinball"||k=="sternelectronics")return "stern";
 if(k=="segaofamerica")return "sega";
 if(k=="dataeastpinball")return "dataeast";
 if(k=="spooky")return "spookypinball";
 return k;
}
QString knownMaker(QString s){
 static const QStringList makers{"Bally Midway","Stern Electronics","Williams Manufacturing","Data East Pinball","Stern Pinball","Spooky Pinball","Chicago Coin","Game Plan","Data East","Gottlieb","Gottileb","Premier","Mylstar","Williams","Willams","Midway","Bally","Stern","Spooky","Sega","Capcom","Original","Atari","Taito","Inder","Zaccaria","Playmatic","Rowamet","Electromatic"};
 for(const auto &maker:makers)if(makerKey(s)==makerKey(maker))return shown(s);
 return {};
}
QString clean(QString s){
 s.replace('_',' ');s=shown(s);
 static const QRegularExpression suffix("(?:\\s*[-–]?[\\s]*(?:\\((?:VR|VR Room|VPW|Mod|FSS|PUP|PupPack|VPX6?)\\)|\\[(?:VR|VR Room|VPW|Mod|FSS|PUP|PupPack|VPX6?)\\]|\\b(?:VR Room|VR|VPW|Mod|FSS|PupPack|VPX6?)\\b|\\bv\\d+(?:\\.\\d+){0,3}\\b|\\b\\d+(?:\\.\\d+){1,3}\\b))+$",QRegularExpression::CaseInsensitiveOption);
 s.remove(suffix);return shown(s).replace(QRegularExpression("[\\s_-]+$"),QString());
}
struct Parts {QString name,maker;int year=0;bool marked=false;};
Parts parse(QString s){
 s=clean(s);Parts p;
 static const QRegularExpression both("^(.*?)\\s*\\(([^()]*)[,\\s]+((?:18|19|20)\\d{2})\\)(?:\\s|$|(?=v\\d+(?:\\.\\d+){0,3}$))",QRegularExpression::CaseInsensitiveOption);
 auto m=both.match(s);
 if(m.hasMatch()&&!m.captured(1).trimmed().isEmpty()){p.name=shown(m.captured(1));p.maker=shown(m.captured(2)).replace(QRegularExpression("[,\\s]+$"),QString());
  if(QStringList{"vpw","vr","mod","fss"}.contains(key(p.maker)))p.maker.clear();
  p.year=m.captured(3).toInt();p.marked=true;return p;}
 static const QRegularExpression year("^(.*?)\\s*\\(((?:18|19|20)\\d{2})\\)(?:\\s|$)");m=year.match(s);
 if(m.hasMatch()&&!m.captured(1).trimmed().isEmpty()){p.name=shown(m.captured(1));p.year=m.captured(2).toInt();p.marked=true;return p;}
 static const QRegularExpression bracket("^(.*?)\\s*\\(([^()]*)\\)(?:\\s|$)");m=bracket.match(s);
 if(m.hasMatch()&&!m.captured(1).trimmed().isEmpty()&&!knownMaker(m.captured(2)).isEmpty()){p.name=shown(m.captured(1));p.maker=knownMaker(m.captured(2));p.marked=true;return p;}
 static const QRegularExpression tailYear("^(.*?)\\s+((?:18|19|20)\\d{2})$");m=tailYear.match(s);
 if(m.hasMatch()){
  auto name=shown(m.captured(1));
  static const QStringList makers{"Bally Midway","Stern Electronics","Chicago Coin","Game Plan","Data East","Gottlieb","Premier","Mylstar","Williams","Willams","Midway","Bally","Stern","Sega","Capcom","Original","Atari","Taito","Inder","Zaccaria","Playmatic","Rowamet"};
  for(const auto &maker:makers)if(name.endsWith(" "+maker,Qt::CaseInsensitive)){p.maker=maker;name.chop(maker.size()+1);p.year=m.captured(2).toInt();break;}
  if(!name.isEmpty()&&!p.maker.isEmpty()){p.name=name;p.marked=true;return p;}
 }
 p.name=s;return p;
}
QString caption(const Parts &p){
 if(p.maker.isEmpty()&&p.year==0)return p.name;
 return p.name+" ("+p.maker+(p.maker.isEmpty()||!p.year?QString():QString(" "))+(p.year?QString::number(p.year):QString())+")";
}
QString safeVpsUrl(const QJsonObject &machine){
 const auto raw=machine.value("vpsUrl").toString(machine.value("url").toString());const QUrl url(raw);
 if(url.scheme()=="https"&&url.userInfo().isEmpty()&&url.port()==-1){const auto host=url.host().toCaseFolded();if(host=="virtualpinballspreadsheet.com"||host=="www.virtualpinballspreadsheet.com")return url.toString(QUrl::FullyEncoded);}
 const auto id=machine.value("id").toString();static const QRegularExpression safeId("^[A-Za-z0-9_-]{1,128}$");
 if(safeId.match(id).hasMatch())return "https://virtualpinballspreadsheet.com/?game="+id;
 return {};
}
QStringList alternateNames(const QString &name){
 QStringList names;
 static const QRegularExpression vr("^\\s*(?:VR\\s*Room|VRRoom|VR)\\b\\s*[-:]*\\s+",QRegularExpression::CaseInsensitiveOption);
 const auto m=vr.match(name);if(m.hasMatch())names<<name.mid(m.capturedEnd()).trimmed();
 static const QRegularExpression trailingThe("^(.*?)\\s*,?\\s+The$",QRegularExpression::CaseInsensitiveOption);
 for(const auto &candidate:QStringList{name}+names){const auto the=trailingThe.match(candidate);if(the.hasMatch()&&!the.captured(1).trimmed().isEmpty())names<<"The "+the.captured(1).trimmed();}
 names.removeDuplicates();return names;
}
bool corroboratedVersion(const QString &stem,const QString &name,const QString &version){
 if(name.isEmpty()||version.isEmpty())return false;
 auto file=shown(QString(stem).replace('_',' '));if(!file.startsWith(name,Qt::CaseInsensitive))return false;
 auto suffix=file.mid(name.size()).trimmed();while(!suffix.isEmpty()&&(suffix.front()=='-'||suffix.front()==QChar(0x2013)))suffix=suffix.mid(1).trimmed();
 return suffix.startsWith('v',Qt::CaseInsensitive)&&key(suffix)==key(QString("v")+version);
}
}
void identify(QVariantMap &row,const QString &fileStem){
 const auto file=parse(fileStem),embedded=parse(row.value("title").toString());Parts chosen=file;
 // TableInfo adds maker/year only when the filename's name agrees after known version/tag cleanup.
 const bool corroborated=key(embedded.name)==key(file.name)||(!file.marked&&corroboratedVersion(fileStem,embedded.name,row.value("version").toString()));
 if(embedded.marked&&corroborated&&(!file.year||!embedded.year||file.year==embedded.year)&&
    (file.maker.isEmpty()||embedded.maker.isEmpty()||makerKey(file.maker)==makerKey(embedded.maker))){
  if(!file.marked)chosen.name=embedded.name;
  if(chosen.maker.isEmpty())chosen.maker=embedded.maker;
  if(!chosen.year)chosen.year=embedded.year;
  chosen.marked=true;
 }
 if(chosen.name.isEmpty())chosen=embedded;
 if(chosen.name.isEmpty())return;
 row["machine"]=chosen.name;row["title"]=caption(chosen);
 if(!chosen.maker.isEmpty())row["maker"]=chosen.maker;else row.remove("maker");
 if(chosen.year)row["year"]=chosen.year;else row.remove("year");
}
void matchVps(QVariantMap &row,const QJsonArray &db){
 const auto name=key(row.value("machine").toString());auto maker=makerKey(row.value("maker").toString());const int year=row.value("year").toInt();
 if(maker=="vpw")maker.clear(); // VPW identifies a table team, not the original machine maker.
 if(name.isEmpty())return;
 QJsonObject match;int count=0;
 const auto stem=QFileInfo(row.value("path").toString()).completeBaseName().replace('_',' ');bool exact=false;
 for(const auto &value:db){const auto machine=value.toObject();if(key(machine.value("name").toString())!=name)continue;
  if(!maker.isEmpty()&&makerKey(machine.value("manufacturer").toString())!=maker)continue;
  if(year&&machine.value("year").toInt()!=year)continue;
  match=machine;exact=true;if(++count>1)return;
 }
 if(!exact){
  QStringList tried;for(const auto &alternate:alternateNames(row.value("machine").toString())){
   const auto alternateKey=key(alternate);if(alternateKey.isEmpty()||tried.contains(alternateKey))continue;
   tried<<alternateKey;
   for(const auto &value:db){const auto machine=value.toObject();if(key(machine.value("name").toString())!=alternateKey)continue;
    if(!maker.isEmpty()&&makerKey(machine.value("manufacturer").toString())!=maker)continue;
    if(year&&machine.value("year").toInt()!=year)continue;
    match=machine;if(++count>1)return;
   }
  }
  // Still nothing: the same title with a numeral, "&", accent or article written another way
  // ("Orbit 2" and "Orbit II"). Extra or missing words never match, and two machines are no match.
  if(count==0){
   for(const auto &value:db){const auto machine=value.toObject();
    if(match::compareTitles(row.value("machine").toString(),machine.value("name").toString())<match::Quality::Same)continue;
    if(!maker.isEmpty()&&makerKey(machine.value("manufacturer").toString())!=maker)continue;
    if(year&&machine.value("year").toInt()!=year)continue;
    match=machine;if(++count>1)break;
   }
  }
  if(count==1)exact=true;
 }
 // An arbitrary author suffix is discarded only when a unique VPS title is an exact prefix
 // and the remaining filename explicitly says Mod. The normal exact-title path wins.
 if(!exact&&row.value("maker").toString().isEmpty()&&!year){qsizetype longest=0;
   for(const auto &value:db){const auto machine=value.toObject();const auto candidate=shown(machine.value("name").toString());
   if(candidate.isEmpty()||!stem.startsWith(candidate+" ",Qt::CaseInsensitive))continue;
   const auto rest=stem.mid(candidate.size()).trimmed();
   static const QRegularExpression authorMod("^(?:[\\p{L}\\p{N} .'-]+\\s+)?(?:Mod|VR Room Mod)(?:\\s+v?\\d+(?:\\.\\d+)*)?$",QRegularExpression::CaseInsensitiveOption);
   if(!authorMod.match(rest).hasMatch())continue;
   if(candidate.size()>longest){longest=candidate.size();match=machine;count=1;}else if(candidate.size()==longest){++count;}
  }
 }
 if(count!=1)return;
 const auto id=match.value("id").toString(),canonical=shown(match.value("name").toString()),manufacturer=shown(match.value("manufacturer").toString());
 const int matchedYear=match.value("year").toInt();if(canonical.isEmpty()||manufacturer.isEmpty()||(matchedYear&&(matchedYear<1800||matchedYear>2100)))return;
 row["vpsMatched"]=true;if(!id.isEmpty())row["vpsId"]=id;
 row["vpsName"]=canonical;
 const auto url=safeVpsUrl(match);if(!url.isEmpty())row["vpsUrl"]=url;
 row["machine"]=canonical;row["maker"]=manufacturer;if(matchedYear)row["year"]=matchedYear;
 row["title"]=caption({canonical,manufacturer,matchedYear,true});
 row["type"]=shown(match.value("type").toString());row["players"]=match.value("players").toInt();row["theme"]=shown(match.value("theme").toString());
 QList<QJsonObject> files;const auto author=key(row.value("authors").toString()),version=row.value("version").toString().trimmed().toCaseFolded();
 if(author.isEmpty()||version.isEmpty())return;
 for(const auto &value:match.value("tableFiles").toArray()){
  const auto file=value.toObject();if(!file.value("tableFormat").toString().isEmpty()&&file.value("tableFormat").toString()!="VPX")continue;
  QStringList authors;for(const auto &authorValue:file.value("authors").toArray())authors<<authorValue.toString();
  if(key(authors.join(" "))==author&&file.value("version").toString().trimmed().toCaseFolded()==version)files<<file;
 }
 if(files.size()!=1)return;
 QStringList features;for(const auto &value:files.first().value("features").toArray())features<<shown(value.toString());
 row["features"]=features;
 if(features.contains("VR",Qt::CaseInsensitive)&&row.value("error").toString().isEmpty()){
  auto evidence=row.value("evidence").toStringList();evidence<<"VPS: VR";row["evidence"]=evidence;row["vrReady"]=true;
 }
}
}
