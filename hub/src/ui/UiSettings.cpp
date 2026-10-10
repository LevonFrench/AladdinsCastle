// SPDX-License-Identifier: GPL-3.0-only
#include "UiSettings.h"
#include "core/catalog/CatalogLoader.h"
#include <QDir>
#include <QSaveFile>
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
UiSettings::UiSettings(QString userDir,QObject *parent):QObject(parent),m_path(QDir(userDir).filePath("hub-settings.toml")) {
 if(QFile::exists(m_path))try{auto data=CatalogLoader::parseToml(m_path);m_values=jsonVariant(data.value("ui",Json::object())).toMap();m_games=jsonVariant(data.value("game",Json::object())).toMap();}catch(...) {emit error("Settings could not be read; defaults are in use.");}
}
bool UiSettings::set(const QString &key,const QVariant &value){m_values[key]=value;const bool ok=save();emit changed();return ok;}
bool UiSettings::saveGame(const QString &id,const QVariantMap &value){auto merged=m_games.value(id).toMap();for(auto it=value.begin();it!=value.end();++it)merged[it.key()]=it.value();m_games[id]=merged;const bool ok=save();if(ok)emit gameSaved(id,merged);emit changed();return ok;}
bool UiSettings::save(){QDir().mkpath(QFileInfo(m_path).absolutePath());QSaveFile f(m_path);if(!f.open(QIODevice::WriteOnly)){emit error("Cannot save portable settings.");return false;}toml::table t;t.insert("ui",table(m_values));t.insert("game",table(m_games));std::ostringstream out;out<<t;const auto bytes=QByteArray::fromStdString(out.str());if(f.write(bytes)!=bytes.size()||!f.commit()){emit error("Cannot commit portable settings.");return false;}return true;}
}
