// SPDX-License-Identifier: GPL-3.0-only
#include "Scores.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
#include <QVariantList>
#include <algorithm>
#include <cmath>
#include <optional>
namespace ac::pinball {
namespace {
constexpr qint64 maxNvram=1024*1024;
QString setName(QString name){name=name.trimmed().toCaseFolded();static const QRegularExpression valid("^[a-z0-9_]{1,64}$");return valid.match(name).hasMatch()?name:QString();}
const QJsonObject &ownMaps(){
 static const QJsonObject maps=[]{QFile file(":/pinball/pinball-score-maps.json");return file.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(file.readAll()).object():QJsonObject();}();
 return maps;
}
// One byte of a descriptor and the nibble of it that counts.
struct Unit {quint8 byte;QString nibble;};
std::optional<QList<Unit>> units(const QByteArray &nvram,const QJsonObject &platform,const QJsonObject &d){
 QList<qint64> addresses;
 if(d.contains("addresses")){for(const auto &a:d.value("addresses").toArray())addresses<<qint64(a.toDouble());}
 else{const auto start=qint64(d.value("start").toDouble()),count=qint64(d.value("count").toDouble());if(count<1||count>64){return std::nullopt;}for(qint64 i=0;i<count;++i)addresses<<start+i;}
 if(addresses.isEmpty()||addresses.size()>64)return std::nullopt;
 const auto regions=platform.value("nvram").toArray();QList<Unit> out;
 for(const auto address:addresses){
  qint64 at=0;std::optional<Unit> found;
  // The .nv file holds the platform's NVRAM regions one after another.
  for(const auto &value:regions){
   const auto r=value.toObject();const auto base=qint64(r.value("address").toDouble()),size=qint64(r.value("size").toDouble());
   if(address>=base&&address<base+size){const auto offset=at+address-base;if(offset<nvram.size()){found=Unit{quint8(nvram[offset]),d.value("nibble").toString(r.value("nibble").toString("both"))};}break;}
   at+=size;
  }
  if(!found)return std::nullopt;
  if(d.contains("mask"))found->byte&=quint8(d.value("mask").toInt());
  out<<*found;
 }
 return out;
}
QList<int> digits(const QList<Unit> &units){
 QList<int> out;
 for(const auto &u:units){if(u.nibble=="low")out<<(u.byte&15);else if(u.nibble=="high")out<<(u.byte>>4);else out<<(u.byte>>4)<<(u.byte&15);}
 return out;
}
std::optional<qint64> number(const QByteArray &nvram,const QJsonObject &platform,const QJsonObject &d){
 auto found=units(nvram,platform,d);if(!found)return std::nullopt;
 if(d.value("endian").toString(platform.value("endian").toString("big"))=="little")std::reverse(found->begin(),found->end());
 quint64 value=0;const bool bytes=std::all_of(found->cbegin(),found->cend(),[](const Unit &u){return u.nibble=="both";});
 if(d.value("encoding").toString()=="bcd"){for(const auto digit:digits(*found))value=value*10+quint64(digit<10?digit:0);} // A to F count as 0
 else if(bytes){if(found->size()>7){return std::nullopt;}for(const auto &u:*found)value=value*256+u.byte;}
 else{for(const auto digit:digits(*found))value=value*16+quint64(digit);}
 if(value>quint64(1e15))return std::nullopt;
 return qint64(std::llround(double(value)*d.value("scale").toDouble(1)));
}
QString text(const QByteArray &nvram,const QJsonObject &platform,const QJsonObject &d,const QString &charMap){
 const auto found=units(nvram,platform,d);if(!found)return {};
 QList<int> bytes;
 if(std::all_of(found->cbegin(),found->cend(),[](const Unit &u){return u.nibble=="both";})){for(const auto &u:*found)bytes<<u.byte;}
 else{const auto ds=digits(*found);for(qsizetype i=0;i+1<ds.size();i+=2)bytes<<(ds[i]*16+ds[i+1]);} // high nibble first
 QString out;const auto null=d.value("null").toString("ignore");
 for(const auto b:bytes){
  if(!charMap.isEmpty()){out+=b<charMap.size()?charMap[b]:QChar('?');continue;}
  if(b==0){if(null=="ignore"){continue;}break;}
  out+=b>=32&&b<127?QChar(b):QChar('?');
 }
 return out;
}
QString shown(qint64 score,const QString &suffix){return QLocale(QLocale::English).toString(score)+suffix;}
}

QString nvramFolder(const QString &pinmameFolder,const QString &romFolder){
 if(const auto fixture=qEnvironmentVariable("AC_TEST_PINBALL_NVRAM");!fixture.isEmpty())return fixture;
#ifdef Q_OS_WIN
 const QSettings globals(R"(HKEY_CURRENT_USER\Software\Freeware\Visual PinMame\globals)",QSettings::NativeFormat);
 if(const auto own=globals.value("nvram_directory").toString();!own.isEmpty()&&QFileInfo(own).isDir())return QDir::cleanPath(own);
#endif
 if(!pinmameFolder.isEmpty()&&QFileInfo(QDir(pinmameFolder).filePath("nvram")).isDir())return QDir::cleanPath(QDir(pinmameFolder).filePath("nvram"));
 if(!romFolder.isEmpty()){const QFileInfo beside(QDir(romFolder).filePath("../nvram"));if(beside.isDir())return QDir::cleanPath(beside.absoluteFilePath());}
 return {};
}

QVariantMap nvramScores(const QString &set,const QByteArray &nvram,const QJsonObject &given){
 const auto &maps=given.isEmpty()?ownMaps():given;
 const auto index=maps.value("roms").toObject().value(set);
 if(index.isUndefined())return {{"set",set},{"note","PinMAME saves scores for "+set+", but the Pinball Memory Maps do not cover it yet."}};
 const auto map=maps.value("maps").toArray().at(index.toInt()).toObject();
 const auto platform=maps.value("platforms").toObject().value(map.value("platform").toString()).toObject();
 const auto charMap=map.value("charMap").toString();QVariantList scores;
 for(const auto &value:map.value("scores").toArray()){
  const auto e=value.toObject();const auto score=number(nvram,platform,e.value("score").toObject());if(!score)continue;
  const auto initials=e.contains("initials")?text(nvram,platform,e.value("initials").toObject(),charMap).trimmed():QString();
  if(!*score&&initials.isEmpty())continue;
  scores<<QVariantMap{{"kind",e.value("kind").toString()},{"label",e.value("label").toString()},{"short",e.value("short").toString()},{"initials",initials},
   {"score",double(*score)},{"text",shown(*score,e.value("score").toObject().value("suffix").toString())}};
 }
 return {{"set",set},{"scores",scores}};
}

QVariantMap tableScores(const QVariantMap &row,const QString &folder,const QJsonObject &maps){
 if(folder.isEmpty()||row.value("romState")=="needs no ROM")return {};
 QStringList sets=row.value("romCandidates").toStringList();if(sets.isEmpty())sets<<row.value("rom").toString();
 QFileInfo newest;QString chosen;
 for(const auto &raw:sets){
  const auto set=setName(raw);if(set.isEmpty())continue;
  const QFileInfo file(QDir(folder).filePath(set+".nv"));
  if(file.isFile()&&file.size()<=maxNvram&&(chosen.isEmpty()||file.lastModified()>newest.lastModified())){newest=file;chosen=set;}
 }
 if(chosen.isEmpty())return {};
 QFile file(newest.absoluteFilePath());if(!file.open(QIODevice::ReadOnly))return {};
 auto out=nvramScores(chosen,file.read(maxNvram),maps);
 out["source"]="PinMAME NVRAM";out["saved"]=newest.lastModified().toString(Qt::ISODate);
 return out;
}
}
