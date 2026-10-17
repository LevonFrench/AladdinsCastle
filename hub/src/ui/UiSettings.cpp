// SPDX-License-Identifier: GPL-3.0-only
#include "UiSettings.h"
#include "core/catalog/CatalogLoader.h"
#include <QDir>
#include <QSaveFile>
#include <QFileInfo>
#include <toml++/toml.hpp>
#include <sstream>
namespace ac {
namespace {
toml::table table(const QVariantMap &values);
toml::array array(const QVariantList &values){
 toml::array result;for(const auto &v:values){
  if(v.metaType().id()==QMetaType::QVariantMap)result.push_back(table(v.toMap()));
  else if(v.metaType().id()==QMetaType::QVariantList||v.metaType().id()==QMetaType::QStringList)result.push_back(array(v.toList()));
  else if(v.metaType().id()==QMetaType::Bool)result.push_back(v.toBool());
  else if(v.metaType().id()==QMetaType::Int||v.metaType().id()==QMetaType::LongLong)result.push_back(static_cast<int64_t>(v.toLongLong()));
  else if(v.metaType().id()==QMetaType::Double)result.push_back(v.toDouble());
  else result.push_back(v.toString().toStdString());
 }return result;
}
toml::table table(const QVariantMap &values) {
 toml::table t; for(auto i=values.begin();i!=values.end();++i) {
 auto key=i.key().toStdString(); const auto &v=i.value();
 if(v.metaType().id()==QMetaType::QVariantMap)t.insert(key,table(v.toMap()));
 else if(v.metaType().id()==QMetaType::QVariantList||v.metaType().id()==QMetaType::QStringList)t.insert(key,array(v.toList()));
 else if(v.metaType().id()==QMetaType::Bool)t.insert(key,v.toBool());
 else if(v.metaType().id()==QMetaType::Int||v.metaType().id()==QMetaType::LongLong)t.insert(key,static_cast<int64_t>(v.toLongLong()));
 else if(v.metaType().id()==QMetaType::Double)t.insert(key,v.toDouble());
 else t.insert(key,v.toString().toStdString());
 } return t;
}
}
QString UiSettings::userDir() const{return QFileInfo(m_path).absolutePath();}
UiSettings::UiSettings(QString userDir,QObject *parent):QObject(parent),m_path(QDir(userDir).filePath("hub-settings.toml")) {
 if(QFile::exists(acceptedPath()))try{m_accepted=jsonVariant(CatalogLoader::parseToml(acceptedPath()).value("accepted",Json::object())).toMap();}catch(...){}
 if(QFile::exists(m_path))try{auto data=CatalogLoader::parseToml(m_path);m_values=jsonVariant(data.value("ui",Json::object())).toMap();m_games=jsonVariant(data.value("game",Json::object())).toMap();}catch(...) {emit error("Settings could not be read; defaults are in use.");}
}
QString UiSettings::acceptedPath()const{return QFileInfo(m_path).absoluteDir().filePath("accepted.toml");}
void UiSettings::reloadAccepted(){
 m_accepted.clear();
 if(QFile::exists(acceptedPath()))try{m_accepted=jsonVariant(CatalogLoader::parseToml(acceptedPath()).value("accepted",Json::object())).toMap();}catch(...){emit error("The accepted-games list could not be read.");}
 emit changed();
}
QString UiSettings::acceptedOn(const QString &id,const QString &title)const{
 for(auto it=m_accepted.begin();it!=m_accepted.end();++it)
  if(it.key()==id||(!title.isEmpty()&&it.key().compare(title,Qt::CaseInsensitive)==0)){const auto when=it.value().toString();return when.isEmpty()?QString("yes"):when;}
 return {};
}
bool UiSettings::createAcceptedList()const{
 if(QFile::exists(acceptedPath()))return true;
 QDir().mkpath(QFileInfo(acceptedPath()).absolutePath());QSaveFile file(acceptedPath());
 if(!file.open(QIODevice::WriteOnly))return false;
 file.write("# Games you have tried in the headset and are happy with.\n"
            "# The Hub shows a game as accepted only when it is listed here; it never adds to this list itself.\n"
            "# One line per game: its id or its title, then the date, for example\n"
            "#   \"model3-scud\" = \"2026-10-14\"\n"
            "#   \"The House of the Dead 2\" = \"2026-10-14\"\n"
            "[accepted]\n");
 return file.commit();
}
bool UiSettings::set(const QString &key,const QVariant &value){m_values[key]=value;const bool ok=save();emit changed();return ok;}
bool UiSettings::remove(const QString &key){if(!m_values.contains(key))return true;m_values.remove(key);const bool ok=save();emit changed();return ok;}
bool UiSettings::saveGame(const QString &id,const QVariantMap &value){auto merged=m_games.value(id).toMap();for(auto it=value.begin();it!=value.end();++it)merged[it.key()]=it.value();m_games[id]=merged;const bool ok=save();if(ok)emit gameSaved(id,merged);emit changed();return ok;}
bool UiSettings::save(){QDir().mkpath(QFileInfo(m_path).absolutePath());QSaveFile f(m_path);if(!f.open(QIODevice::WriteOnly)){emit error("Cannot save portable settings.");return false;}toml::table t;t.insert("ui",table(m_values));t.insert("game",table(m_games));std::ostringstream out;out<<t;const auto bytes=QByteArray::fromStdString(out.str());if(f.write(bytes)!=bytes.size()||!f.commit()){emit error("Cannot commit portable settings.");return false;}return true;}
}
