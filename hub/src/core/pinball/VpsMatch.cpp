// SPDX-License-Identifier: GPL-3.0-only
// Ported from Visual Pinball Infinite's matcher.rs, itself ported from VPin Studio's matcher.
// Copyright (c) 2022 syd711, MIT licence: hub/packaging/licenses/VPinStudio-MIT.txt. See VpsMatch.h.
#include "VpsMatch.h"
#include <QHash>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
namespace ac::pinball::vps {
namespace {
constexpr double ThresholdNotFound=10.0,ThresholdConfirm=2.0,VersionNotFound=3.5;
const QStringList ExcludedWords{"mod","vpx","vr","dt","fss","fs","4k","alt","alt2","(1)","(2)","edition","version","vrroom"};
// Table info names that say nothing of the table: VPX's example table's, kept by tables made from it.
const QStringList PlaceholderNames{"visual pinball demo table","table1"};
bool asciiAlnum(QChar c){return c.unicode()<128&&(c.isLetterOrNumber()||c=='_');}
bool asciiAlpha(QChar c){return c.unicode()<128&&c.isLetter();}
bool asciiDigit(QChar c){return c>='0'&&c<='9';}
QString letters(const QString &s){QString out;for(const auto c:s)if(asciiAlpha(c))out+=c;return out.toLower();}
std::optional<QString> nonEmpty(const std::optional<QString> &s){return s&&!s->isEmpty()?s:std::nullopt;}
// Capture n when it took part in the match (Rust's captures.get(n)).
std::optional<QString> group(const QRegularExpressionMatch &m,int n){return m.capturedStart(n)<0?std::nullopt:std::optional<QString>(m.captured(n));}
QString removeCi(QString text,const QString &what){if(what.isEmpty())return text;text.remove(what,Qt::CaseInsensitive);return text;}
// VPin Studio's cleanTable.
QString cleanTable(const QString &name){return cleanChars(name).replace("the ","").replace(", the","");}
QStringList alternateMakers(const QString &maker){
 const auto m=maker.toLower();
 if(m=="bally"||m=="midway")return {"Bally","Midway"};
 if(m=="original"||m=="tba")return {"Original","TBA"};
 if(m=="mylstar"||m=="gottlieb")return {"Gottlieb","Mylstar"};
 if(m.startsWith("taito"))return {"Taito do Brasil","Taito"};
 if(m.startsWith("spooky"))return {"Spooky Pinball","Spooky"};
 return {maker};
}
double combined(double table,double maker,double year){return (1.0+table*9.0)*(1.0+maker*3.0)*(1.0+year*1.5)-1.0;}
// The Hub's rule, as Identity.cpp's: a table file with no format is a VPX one.
bool vpxFile(const TableFile &t){return t.format.isEmpty()||t.format=="VPX";}
// A game VPS lists a VPX table for (VPin Studio's isEmulatorApplicable).
bool applies(const Game &game){
 if(game.manufacturer.compare("Zen Studios",Qt::CaseInsensitive)==0)return false;
 return std::any_of(game.tableFiles.cbegin(),game.tableFiles.cend(),vpxFile);
}
// How far a game is from the parsed name, maker and year (VPin Studio's getTableDistance).
double gameDistance(const Game &game,const QString &displayName,const std::optional<QString> &tableName,const std::optional<QString> &maker,int year){
 const auto theirs=cleanTable(game.name.toLower());const int theirYear=game.year;
 if(tableName){
  const auto dTable=nameDistance(theirs,*tableName);double dMaker=0.2;
  if(maker){dMaker=10000.0;for(const auto &alt:alternateMakers(game.manufacturer))dMaker=std::min(dMaker,nameDistance(*maker,alt));}
  const double dYear=year>1900?std::min(double(std::abs(year-theirYear)),5.0)/5.0:0.2;
  return combined(dTable,dMaker,dYear);
 }
 auto clean=displayName;double dMaker=0.2,dYear=0.2;
 for(const auto &alt:alternateMakers(game.manufacturer))if(clean.contains(alt,Qt::CaseInsensitive)){clean=removeCi(clean,alt);dMaker=0.0;}
 const auto y=QString::number(theirYear);
 if(clean.contains(y)){clean=removeCi(clean,y);dYear=0.0;}
 return combined(nameDistance(cleanTable(clean),theirs),dMaker,dYear);
}
std::optional<QString> cleanedTableName(const NameParts &p){return nonEmpty(p.tableName)?std::optional<QString>(cleanTable(*p.tableName)):std::nullopt;}
// VPin Studio's cleanVersion: a leading "VPX..." taken off, lower case.
QString cleanVersion(const QString &version){
 qsizetype p=0;
 if(version.size()>=2&&version.left(2).compare("VP",Qt::CaseInsensitive)==0){p=2;while(p<version.size()&&QString("XS0123456789.").contains(version[p]))++p;}
 return version.mid(p).toLower().trimmed();
}
// VPin Studio's VersionTokenizer: the numbers of a version, one at a time.
struct VersionTokens {
 QString text;qsizetype at=0;qint64 number=0;
 explicit VersionTokens(QString s):text(std::move(s)){while(at<text.size()&&!asciiDigit(text[at]))++at;}
 bool next(){
  number=0;bool has=false;
  while(at<text.size()){const auto c=text[at];if(c=='.'){++at;break;}if(!asciiDigit(c))break;
   number=number>(std::numeric_limits<qint64>::max()-9)/10?std::numeric_limits<qint64>::max():number*10+(c.unicode()-'0');++at;has=true;}
  return has;
 }
 QString suffix()const{return text.mid(at);}
};
// VPin Studio's authorDistance: each of the table's authors found among VPS's (taken out once
// found), costing how far down VPS's list it is, or the whole list when not there.
double authorDistance(const QStringList &ours,qsizetype i,QStringList &theirs,qsizetype max){
 if(i>=ours.size())return 0.0;
 for(qsizetype j=0;j<theirs.size();++j)if(sameAuthor(ours[i],theirs[j])){theirs.removeAt(j);return double(j)+authorDistance(ours,i+1,theirs,max);}
 return double(max)+authorDistance(ours,i+1,theirs,max);
}
QStringList splitAuthors(const QString &authors){
 static const QRegularExpression separators("[,/\\-&]");QStringList out;
 for(const auto &a:authors.toLower().split(separators)){const auto t=a.trimmed();if(!t.isEmpty())out<<t;}
 return out;
}
QList<const TableFile*> tableFilesOf(const Game &game){QList<const TableFile*> out;for(const auto &t:game.tableFiles)if(vpxFile(t))out<<&t;return out;}
// The game's table files of the file name's version: the one to take when it's alone.
QList<const TableFile*> ofNamedVersion(const Game &game,const FileQuery &q){
 if(!q.versionFromName||!q.version)return {};
 const auto v=plainVersion(*q.version);if(v.isEmpty())return {};
 QList<const TableFile*> out;for(const auto *t:tableFilesOf(game))if(!t->version.isEmpty()&&plainVersion(t->version)==v)out<<t;
 return out;
}
QString romKey(const QString &rom){auto r=rom.trimmed().toLower();if(r.endsWith(".zip"))r.chop(4);return r;}
bool tiedFirst(const QList<Scored> &list){return list.size()>1&&std::abs(list[1].distance-list[0].distance)<1e-9;}
}

std::vector<Game> games(const QJsonArray &db){
 std::vector<Game> out;out.reserve(size_t(db.size()));
 for(qsizetype i=0;i<db.size();++i){
  const auto o=db.at(i).toObject();Game g;g.index=i;g.id=o.value("id").toString();g.name=o.value("name").toString();
  g.manufacturer=o.value("manufacturer").toString();g.year=o.value("year").toInt();
  for(const auto &value:o.value("tableFiles").toArray()){const auto f=value.toObject();TableFile t;
   t.id=f.value("id").toString();t.version=f.value("version").toString();t.comment=f.value("comment").toString();t.format=f.value("tableFormat").toString();
   t.createdAt=qint64(f.value("createdAt").toDouble());
   for(const auto &a:f.value("authors").toArray())t.authors<<a.toString();
   for(const auto &a:f.value("features").toArray())t.features<<a.toString();
   g.tableFiles<<t;}
  for(const auto &value:o.value("romFiles").toArray()){const auto v=value.toObject().value("version").toString();if(!v.isEmpty())g.roms<<v;}
  out.push_back(std::move(g));
 }
 return out;
}

// VPin Studio's TableNameSplitter.parseFilename.
NameParts parseFilename(const QString &fileName){
 static const QRegularExpression vpReference("VPX?[0-9.]+"),features("\\[(\\+?[A-Z][A-Z&][A-Z]?){1,2}\\]");
 // "(Maker Year) extra" or "(Year Maker) extra".
 static const QRegularExpression makerYear("\\(((([A-Za-z0-9_ ]+)[() ]*([0-9]{4}))|(([0-9]{4})[() ]*)([A-Za-z0-9_ ]+))\\)(.*)");
 // "Name Year extra".
 static const QRegularExpression nameYear("(.+)([0-9]{4})(.+)");
 auto display=fileName;display.remove(vpReference);display.remove(features);
 NameParts parts;parts.displayName=display;
 if(const auto c=makerYear.match(display);c.hasMatch()){
  parts.tableName=display.left(c.capturedStart(0));
  parts.manufacturer=cleanChars(group(c,3).value_or(group(c,7).value_or(QString())));
  bool ok=false;const int y=group(c,4).value_or(group(c,6).value_or(QString())).toInt(&ok);parts.year=ok?y:-1;
  parts.extra=group(c,8).value_or(QString()).trimmed();
 }else if(const auto n=nameYear.match(display);n.hasMatch()){
  parts.tableName=n.captured(1).trimmed();bool ok=false;const int y=n.captured(2).toInt(&ok);parts.year=ok?y:-1;parts.extra=n.captured(3).trimmed();
 }
 if(!parts.tableName&&!parts.extra)parts.tableName=display;
 if(const auto extra=nonEmpty(parts.extra)){
  parts.version=extractVersion(*extra);
  if(nonEmpty(parts.version))parts.extra=removeCi(*extra,*parts.version).trimmed();
 }
 if(!parts.version){
  const auto name=parts.tableName.value_or(QString());parts.version=extractVersion(name);
  if(nonEmpty(parts.version))parts.tableName=removeCi(name,*parts.version);
 }
 return parts;
}

std::optional<QString> extractVersion(const QString &text){
 static const QRegularExpression version("([rv]?[_.]?[0-9]+([_.][0-9]+)+([r-][0-9]+([_.][0-9]+)*)?[abcde]?)(.*)",QRegularExpression::CaseInsensitiveOption);
 const auto m=version.match(text);if(!m.hasMatch())return std::nullopt;return m.captured(1);
}

// VPin Studio's cleanChars: punctuation to spaces, spaces single.
QString cleanChars(const QString &text){
 QString replaced=text;for(auto &c:replaced)if(QString("_.,+-'[]()").contains(c))c=' ';
 return replaced.split(' ',Qt::SkipEmptyParts).join(' ');
}

// Apache Commons Text's cosine distance: 1 less the cosine of the two texts' word counts, a word
// being a run of letters, digits and underscores.
double cosineDistance(const QString &a,const QString &b){
 const auto count=[](const QString &s){QHash<QString,double> m;QString word;
  for(const auto c:s+' '){if(asciiAlnum(c)){word+=c;continue;}if(!word.isEmpty()){m[word]+=1.0;word.clear();}}
  return m;};
 const auto l=count(a),r=count(b);double dot=0,d1=0,d2=0;
 for(auto it=l.cbegin();it!=l.cend();++it){if(r.contains(it.key()))dot+=it.value()*r.value(it.key());d1+=it.value()*it.value();}
 for(const auto v:r)d2+=v*v;
 const double similarity=d1<=0.0||d2<=0.0?0.0:dot/(std::sqrt(d1)*std::sqrt(d2));
 return 1.0-similarity;
}

// Apache Commons Text's Jaccard distance: 1 less the share of the two texts' characters, as sets,
// that both have.
double jaccardDistance(const QString &a,const QString &b){
 if(a.isEmpty()&&b.isEmpty())return 0.0;
 if(a.isEmpty()||b.isEmpty())return 1.0;
 QSet<QChar> l,r;for(const auto c:a)l.insert(c);for(const auto c:b)r.insert(c);
 const auto both=QSet<QChar>(l).intersect(r).size(),all=l.size()+r.size()-both;
 return 1.0-double(both)/double(all);
}

// VPin Studio's TableMatcher.distance: 0 for the same name, a part's share for one inside the
// other, else the product of the cosine and Jaccard distances.
double nameDistance(const QString &first,const QString &second){
 if(first.isEmpty()&&second.isEmpty())return 0.0;
 if(first.isEmpty()||second.isEmpty())return 1.0;
 const auto a=first.toLower(),b=second.toLower();
 if(a==b)return 0.0;
 double distance=10000.0;const double la=double(a.size()),lb=double(b.size());
 if(a.contains(b))distance=std::min(distance,1.0-lb/la)/2.0;
 else if(b.contains(a))distance=std::min(distance,1.0-la/lb)/2.0;
 return std::min(distance,cosineDistance(a,b)*jaccardDistance(a,b));
}

// VPin Studio's versionDistance: VPS's version against the table's, a table older than VPS's file
// costing more than a newer one.
double versionDistance(const QString &theirs,const QString &ours){
 if(theirs.isEmpty()&&ours.isEmpty())return 0.0;
 VersionTokens t1(cleanVersion(theirs)),t2(cleanVersion(ours));int depth=0;
 while(t1.next()){
  if(!t2.next())return 0.3/std::pow(10.0,depth);
  const auto n1=t1.number,n2=t2.number;
  if(n1<n2)return double(std::max(3-depth,1));
  if(n1>n2)return std::min(double(n1-n2)/std::pow(10.0,depth),0.3);
  ++depth;
 }
 if(t2.next())return 0.3/std::pow(10.0,depth);
 const auto s1=t1.suffix(),s2=t2.suffix();
 const auto marked=[](const QString &s){const auto l=s.toLower();return l.startsWith('-')||l.startsWith('r')||l.startsWith('/')||l.startsWith(':');};
 if(marked(s1)&&marked(s2))return versionDistance(s1.mid(1),s2.mid(1))/4.0;
 if(s1.isEmpty()!=s2.isEmpty())return 0.2;
 const auto order=s1.toLower().compare(s2.toLower());
 return order<0?1.0:order==0?0.0:0.1;
}

// VPI's: lower case, a leading "v" or "r" off, trailing ".0"s off and the dots out, so "v1.1.3"
// is "1.13" and "1.0.2" is "1.02".
QString plainVersion(const QString &version){
 auto v=cleanVersion(version).replace('_','.');
 qsizetype start=0;while(start<v.size()&&(v[start]=='v'||v[start]=='r'||v[start]=='.'))++start;
 auto parts=v.mid(start).split('.');
 const auto zeros=[](const QString &p){return std::all_of(p.cbegin(),p.cend(),[](QChar c){return c=='0';});};
 while(parts.size()>1&&zeros(parts.last()))parts.removeLast();
 return parts.join(QString());
}

// VPin Studio's test, close by the cosine distance; VPI's besides, the same letters with spaces,
// digits and marks aside ("Clark Kent" and "ClarkKent", "Javier" and "Javier15").
bool sameAuthor(const QString &ours,const QString &theirs){
 const auto a=letters(ours),b=letters(theirs);
 return cosineDistance(ours,theirs.toLower())<0.3||(a.size()>=3&&a==b);
}

bool relates(const QString &fileName,const Game &game){
 static const QRegularExpression notWord("[^\\p{L}\\p{N}]+");
 const auto file=letters(fileName);QStringList words;
 for(const auto &w:game.name.split(notWord,Qt::SkipEmptyParts)){const auto l=letters(w);if(!l.isEmpty())words<<l;}
 QString first,initials;
 for(const auto &w:words){initials+=w.front();if(first.isEmpty()&&w.size()>=3&&w!="the"&&w!="and")first=w;}
 return (!first.isEmpty()&&file.contains(first))||(initials.size()>=3&&file.contains(initials))||(initials.size()==2&&file.startsWith(initials));
}

QString residue(const QString &text,const Game &game){
 static const QRegularExpression notWord("[^\\p{L}\\p{N}]+");
 const auto words=[](const QString &s){return s.toLower().split(notWord,Qt::SkipEmptyParts);};
 QSet<QString> known;for(const auto &w:words(game.name))known.insert(w);for(const auto &w:words(game.manufacturer))known.insert(w);
 if(game.year)known.insert(QString::number(game.year));
 QStringList left;for(const auto &w:words(text))if(!known.contains(w))left<<w;
 return left.join(' ');
}

QList<Scored> closestGames(const NameParts &parts,const std::vector<Game> &all){
 const auto cleanName=cleanedTableName(parts);QList<Scored> found;double minDist=10000.0;
 for(const auto &game:all){
  if(!applies(game))continue;
  const auto dist=gameDistance(game,parts.displayName,cleanName,parts.manufacturer,parts.year);
  if(dist>=minDist)continue;
  if(dist<=ThresholdConfirm){if(ThresholdConfirm<minDist){found.clear();minDist=ThresholdConfirm;}}
  else if(dist<=ThresholdNotFound){if(ThresholdNotFound<minDist){found.clear();minDist=ThresholdNotFound;}}
  else{found.clear();minDist=dist;}
  const auto at=std::find_if(found.cbegin(),found.cend(),[dist](const Scored &s){return s.distance>=dist;});
  found.insert(at,Scored{&game,dist});
 }
 return found;
}

// Every one of the game's table files scored against the table (VPin Studio's
// TableVersionMatcher.findVersion, each file's distances kept; VPI's where VpsMatch.h says).
QList<FileScore> tableFileScores(const Game &game,const FileQuery &q){
 const auto version=cleanVersion(q.version.value_or(QString()));const auto name=residue(q.name,game);
 std::optional<QStringList> fromInfo;if(q.infoAuthors){const auto a=splitAuthors(*q.infoAuthors);if(!a.isEmpty())fromInfo=a;}
 auto fromNameText=q.nameAuthors.toLower();
 for(const auto &w:ExcludedWords){QStringList kept;for(const auto &x:fromNameText.split(' '))if(x!=w)kept<<x;fromNameText=kept.join(' ');}
 std::optional<QStringList> fromName;{const auto a=splitAuthors(cleanChars(fromNameText));if(!a.isEmpty())fromName=a;}
 QList<FileScore> scores;
 for(const auto *t:tableFilesOf(game)){
  auto comment=t->comment;const auto reupload=comment.indexOf("reupload",0,Qt::CaseInsensitive);if(reupload>=0)comment.truncate(reupload);
  comment=residue(comment,game);
  const double dName=!comment.isEmpty()&&!name.isEmpty()?cosineDistance(comment,name):1.0;
  const double dMade=q.made?double(std::llabs(t->createdAt-*q.made))/1000.0/60.0/60.0/24.0:10000.0;
  double dVersion=1.0,dAuthor=0.5;
  if(dMade<1.0){dVersion=0.0;dAuthor=0.0;}
  else{
   if(!t->version.isEmpty()&&!version.isEmpty())dVersion=plainVersion(t->version)==plainVersion(version)?0.0:versionDistance(t->version,version);
   if(!t->authors.isEmpty()){
    // The table info's authors as VPin Studio weighs them, the file name's at half (they may not
    // be authors at all), the nearer counting.
    const auto weigh=[t](const QStringList &ours){const double whole=double(ours.size()*t->authors.size());auto theirs=t->authors;return authorDistance(ours,0,theirs,t->authors.size())/whole;};
    std::optional<double> d;
    if(fromInfo)d=weigh(*fromInfo);
    if(fromName){const auto n=weigh(*fromName)/2.0;d=d?std::min(*d,n):n;}
    if(d){dAuthor=*d;if(dAuthor==0.0)dVersion/=3.0;}
   }
  }
  const double total=(1.0+dName)*(1.0+dAuthor)*(1.0+dVersion/3.0)-1.0;
  scores<<FileScore{t,dName,dAuthor,dVersion,total};
 }
 return scores;
}

std::optional<FileScore> closestTableFile(const Game &game,const FileQuery &q){
 const auto files=tableFilesOf(game);const auto scores=tableFileScores(game,q);
 const auto scoreOf=[&scores](const TableFile *t)->std::optional<FileScore>{for(const auto &s:scores)if(s.file==t)return s;return std::nullopt;};
 if(files.size()==1)return scoreOf(files.first());
 const auto named=ofNamedVersion(game,q);
 if(named.size()==1)return scoreOf(named.first());
 if(q.name.isEmpty()&&(!q.version||cleanVersion(*q.version).isEmpty())&&!q.infoAuthors&&q.nameAuthors.isEmpty()&&!q.made)return std::nullopt;
 std::optional<FileScore> best;
 for(const auto &s:scores){if(!named.isEmpty()&&!named.contains(s.file))continue;if(!best||s.total<best->total)best=s;}
 if(best&&best->total<VersionNotFound)return best;
 return std::nullopt;
}

bool sure(Found found){return found==Found::ByFileName||found==Found::ByTableInfo||found==Found::ByRom;}
QString describe(Found found){
 switch(found){
 case Found::ByFileName:return "by its file name";
 case Found::ByTableInfo:return "by the name in its table info";
 case Found::ByRom:return "by its ROM";
 case Found::Guess:return "a guess, to confirm";
 case Found::None:break;
 }
 return {};
}

FileQuery fileQuery(const Game &game,const Clues &clues,NameParts &parts){
 auto version=parts.version;const bool versionFromName=version.has_value();
 const auto extra=parts.extra?cleanChars(*parts.extra):removeCi(parts.tableName.value_or(QString()),game.name).trimmed();
 parts.extra=extra;
 const auto name=clues.infoName.value_or(extra);
 if(!version&&clues.infoVersion&&!clues.infoVersion->isEmpty()&&!clues.infoVersion->contains("VP",Qt::CaseInsensitive))version=extractVersion(*clues.infoVersion);
 if(version){auto v=version->replace('_','.').toLower().trimmed();for(const auto *start:{"v","r","."})if(v.startsWith(QLatin1String(start)))v=v.mid(1);version=v;}
 FileQuery q;q.name=name;q.nameAuthors=extra;q.version=version;q.versionFromName=versionFromName;q.made=clues.made;
 if(clues.infoAuthors&&!clues.infoAuthors->trimmed().isEmpty())q.infoAuthors=clues.infoAuthors;
 return q;
}

Match matchTable(const std::vector<Game> &all,const Clues &clues){
 Match m;m.parts=parseFilename(clues.fileName);
 const auto byName=closestGames(m.parts,all);
 if(!byName.isEmpty()&&byName.first().distance<=ThresholdConfirm){m.game=byName.first().game;m.found=Found::ByFileName;m.distance=byName.first().distance;m.tied=tiedFirst(byName);}
 else{
  // The table info's name, with the file name's maker and year when it gives none; a game the
  // file name says nothing of is left out (a table built on another's keeps its name).
  std::optional<Scored> byInfo;bool infoTied=false;
  if(clues.infoName&&!PlaceholderNames.contains(clues.infoName->trimmed().toLower())){
   auto p=parseFilename(*clues.infoName);
   if(!p.manufacturer&&p.year<0){p.manufacturer=m.parts.manufacturer;p.year=m.parts.year;}
   const auto list=closestGames(p,all);
   if(!list.isEmpty()&&relates(clues.fileName,*list.first().game)){byInfo=list.first();infoTied=tiedFirst(list);}
  }
  // The games VPS lists the script's ROM for that the file name speaks of, the nearest by name.
  std::optional<Scored> byRom;bool romTied=false;
  if(clues.rom&&!clues.rom->isEmpty()){
   const auto rom=romKey(*clues.rom);const auto cleanName=cleanedTableName(m.parts);
   for(const auto &g:all){
    if(!applies(g)||!std::any_of(g.roms.cbegin(),g.roms.cend(),[&rom](const QString &r){return romKey(r)==rom;})||!relates(clues.fileName,g))continue;
    const auto d=gameDistance(g,m.parts.displayName,cleanName,m.parts.manufacturer,m.parts.year);
    if(!byRom||d<byRom->distance){byRom=Scored{&g,d};romTied=false;}else if(std::abs(d-byRom->distance)<1e-9)romTied=true;
   }
  }
  const std::optional<Scored> nearest=byName.isEmpty()?std::nullopt:std::optional<Scored>(byName.first());
  if(byInfo&&byInfo->distance<=ThresholdConfirm){m.game=byInfo->game;m.found=Found::ByTableInfo;m.distance=byInfo->distance;m.tied=infoTied;}
  else if(byRom){m.game=byRom->game;m.found=Found::ByRom;m.distance=byRom->distance;m.tied=romTied;}
  else if(nearest&&byInfo&&byInfo->distance<nearest->distance){m.game=byInfo->game;m.found=Found::Guess;m.distance=byInfo->distance;}
  else if(nearest){m.game=nearest->game;m.found=Found::Guess;m.distance=nearest->distance;}
  else if(byInfo){m.game=byInfo->game;m.found=Found::Guess;m.distance=byInfo->distance;}
 }
 if(m.game){m.asked=fileQuery(*m.game,clues,m.parts);m.tableFile=closestTableFile(*m.game,m.asked);}
 return m;
}
}
