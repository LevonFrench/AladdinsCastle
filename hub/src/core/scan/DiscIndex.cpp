// SPDX-License-Identifier: GPL-3.0-only
#include "DiscIndex.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
namespace ac::scan {
namespace {
QVariantMap read(const QString &name){QFile file(":/data/"+name+".json");if(!file.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();}
const QVariantMap &profiles(){static const auto data=read("disc-profiles");return data;}
const QVariantMap &index(const QString &system){static const auto serial=read("serial-index"),wii=read("wii-disc-index"),dreamcast=read("dreamcast-disc-index");return system=="dreamcast"?dreamcast:(system=="wii"||system=="gc")?wii:serial;}
}
QVariantMap DiscIndex::metadata(const QString &system){return profiles().value(system).toMap();}
QString DiscIndex::fingerprint(){
 static const auto stamp=[](){QByteArray all;for(const auto *name:{"serial-index","wii-disc-index","dreamcast-disc-index","disc-profiles"}){QFile file(QString(":/data/%1.json").arg(name));if(file.open(QIODevice::ReadOnly))all+=file.readAll();}return QString::fromLatin1(QCryptographicHash::hash(all,QCryptographicHash::Sha256).toHex());}();
 return stamp;
}
QVariantList DiscIndex::games(const QString &system){return metadata(system).value("games").toList();}
QVariantMap DiscIndex::match(const QString &system,const QString &identity){
 if(!QStringList{"ps1","ps2","dreamcast","wii","gc"}.contains(system)||identity.trimmed().isEmpty())return {};
 auto key=identity.trimmed().toUpper();if(system=="ps1"||system=="ps2")key.replace('_','-').remove('.');
 auto row=index(system).value(key).toMap();if(row.isEmpty()||row.value("gameId").toString().isEmpty()||row.value("system").toString()!=system)return {};
 const auto region=row.value("region").toString();
 if(region=="US"||region=="NTSC-U"||region=="NTSC-U/C")row["region"]="USA";
 else if(region=="PAL")row["region"]="Europe";
 else if(region=="NTSC-J")row["region"]="Japan";
 else if(region=="NTSC-K")row["region"]="Korea";
 row["catalogId"]=row.value("gameId");row["game"]=row.value("gameId");
 for(const auto &entry:games(system)){const auto game=entry.toMap();if(game.value("catalogId",game.value("id"))==row.value("gameId")){row["game"]=game.value("id");row["title"]=game.value("title");break;}}
 return row;
}
}
