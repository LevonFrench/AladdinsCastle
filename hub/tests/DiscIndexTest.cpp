// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogLoader.h"
#include "core/scan/DiscIndex.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/ps2/Ps2.h"
#include "core/wii/Wii.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>
#include <nlohmann/json-schema.hpp>
namespace {
nlohmann::json jsonFile(const QString &path){QFile file(path);if(!file.open(QIODevice::ReadOnly))qFatal("Cannot read public metadata fixture");const auto bytes=file.readAll();return nlohmann::json::parse(bytes.constData(),bytes.constData()+bytes.size());}
QString root(){return QStringLiteral(AC_CATALOG_ROOT);}
}
class DiscIndexTest : public QObject {
 Q_OBJECT
private slots:
 void canonicalFilesValidateAndResourcesAgree(){
  const auto schema=jsonFile(root()+"/data/disc-index.schema.json");nlohmann::json_schema::json_validator validator;validator.set_root_schema(schema);
  for(const auto &name:QStringList{"serial-index","wii-disc-index","dreamcast-disc-index"}){
   const auto disk=jsonFile(root()+"/data/"+name+".json");QVERIFY(!disk.empty());QVERIFY_THROWS_NO_EXCEPTION(validator.validate(disk));
   QVERIFY(disk==jsonFile(":/data/"+name+".json"));
   for(const auto &[identity,row]:disk.items()){
    const auto system=QString::fromStdString(row.at("system").get<std::string>());const auto id=QString::fromStdString(identity);
    const auto matched=ac::scan::DiscIndex::match(system,id);QVERIFY2(!matched.isEmpty(),identity.c_str());
    QCOMPARE(matched.value("system").toString(),system);QCOMPARE(matched.value("catalogId").toString(),QString::fromStdString(row.at("gameId").get<std::string>()));
    QVERIFY(!row.at("source").get<std::string>().empty());
    if(system=="ps1"||system=="ps2")QVERIFY(QRegularExpression("^[A-Z0-9]+-[A-Z0-9]+$").match(id).hasMatch());
    else if(system=="wii")QVERIFY(QRegularExpression("^(?:[A-Z0-9]{6}|[A-F0-9]{16})$").match(id).hasMatch());
    else if(system=="gc")QVERIFY(QRegularExpression("^[A-Z0-9]{6}$").match(id).hasMatch());
   }
  }
  validator.set_root_schema(jsonFile(root()+"/data/disc-profiles.schema.json"));const auto profiles=jsonFile(root()+"/data/disc-profiles.json");QVERIFY_THROWS_NO_EXCEPTION(validator.validate(profiles));QVERIFY(profiles==jsonFile(":/data/disc-profiles.json"));
 }
 void everyCatalogDiscTitleHasAnExplicitIdentity(){
  const QMap<QString,QString> systems{{"sony-ps1","ps1"},{"sony-ps2","ps2"},{"sega-dreamcast","dreamcast"},{"nintendo-wii","wii"},{"nintendo-gamecube","gc"}};
  QMap<QString,QSet<QString>> indexed;
  for(const auto &name:QStringList{"serial-index","wii-disc-index","dreamcast-disc-index"})for(const auto &row:jsonFile(root()+"/data/"+name+".json"))indexed[QString::fromStdString(row.at("system").get<std::string>())].insert(QString::fromStdString(row.at("gameId").get<std::string>()));
  QMap<QString,int> covered;
  const QDir games(root()+"/games");
  for(const auto &entry:games.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot)){
   const auto game=ac::CatalogLoader::parseToml(entry.absoluteFilePath()+"/game.toml");const auto hardware=QString::fromStdString(game.value("hardware",std::string()));
   if(!systems.contains(hardware)){continue;}const auto system=systems.value(hardware);const auto id=QString::fromStdString(game.at("id").get<std::string>());
   QVERIFY2(indexed.value(system).contains(id),qPrintable(entry.absoluteFilePath()+"/game.toml has no exact public disc identity"));++covered[system];
  }
  QCOMPARE(covered.value("ps1"),28);QCOMPARE(covered.value("ps2"),26);QCOMPARE(covered.value("dreamcast"),15);QCOMPARE(covered.value("wii"),13);QCOMPARE(covered.value("gc"),5);
 }
 void everyFrontendProfileResolvesItsExplicitPublicIdentities(){
  using ac::scan::DiscIndex;
  for(const auto &system:QStringList{"ps2","dreamcast","wii"}){
   const auto name=system=="ps2"?"serial-index":system=="dreamcast"?"dreamcast-disc-index":"wii-disc-index";
   const auto index=jsonFile(root()+"/data/"+QString::fromLatin1(name)+".json");
   for(const auto &entry:DiscIndex::games(system)){
    const auto profile=entry.toMap();const auto canonical=profile.value("catalogId").toString();const auto alias=profile.value("id").toString();QVERIFY(!canonical.isEmpty());bool found=false;
    for(const auto &[identity,row]:index.items()){
     if(QString::fromStdString(row.at("system").get<std::string>())!=system||QString::fromStdString(row.at("gameId").get<std::string>())!=canonical)continue;
     found=true;const auto matched=DiscIndex::match(system,QString::fromStdString(identity));QCOMPARE(matched.value("game").toString(),alias);QCOMPARE(matched.value("catalogId").toString(),canonical);QCOMPARE(matched.value("title").toString(),profile.value("title").toString());
    }
    QVERIFY2(found,qPrintable(system+" frontend profile "+alias+" has no exact identity row for "+canonical));
   }
  }
  QCOMPARE(DiscIndex::match("ps2","SLUS-20632").value("game").toString(),QString("xgra"));
  QCOMPARE(DiscIndex::match("ps2","SLUS-20302").value("game").toString(),QString("xgiii"));
  QCOMPARE(DiscIndex::match("ps2","SLUS-21274").value("game").toString(),QString("outrun-2006"));
  QCOMPARE(DiscIndex::match("ps2","SLPM-66628").value("game").toString(),QString("outrun-2"));
 }
 void identitiesKeepSystemRegionAndMakerBoundaries(){
  using ac::scan::DiscIndex;
  QVERIFY(DiscIndex::match("","unindexed").isEmpty());QVERIFY(DiscIndex::match("","").isEmpty());QVERIFY(DiscIndex::match("unknown","SLUS-00481").isEmpty());
  QCOMPARE(DiscIndex::match("ps2","SLES_512.29").value("game").toString(),QString("virtua-cop"));
  QVERIFY(DiscIndex::match("ps1","SLES-51229").isEmpty());QVERIFY(DiscIndex::match("ps2","SLUS-00481").isEmpty());
  QCOMPARE(DiscIndex::match("ps1","SLUS-00481").value("catalogId").toString(),QString("ps1-point-blank"));
  QCOMPARE(DiscIndex::match("gc","GFZE01").value("catalogId").toString(),QString("gc-f-zero-gx"));QVERIFY(DiscIndex::match("wii","GFZE01").isEmpty());
  QCOMPARE(DiscIndex::match("wii","RBUP08").value("game").toString(),QString("umbrella"));QVERIFY(DiscIndex::match("wii","RBUX08").isEmpty());QVERIFY(DiscIndex::match("wii","RBUE01").isEmpty());
  QCOMPARE(DiscIndex::match("wii","RBUE08").value("region").toString(),QString("USA"));QCOMPARE(DiscIndex::match("wii","RBUP08").value("region").toString(),QString("Europe"));
  QCOMPARE(DiscIndex::match("dreamcast","MK-51002").value("catalogId").toString(),QString("dc-house-of-the-dead-2"));
  QVERIFY(DiscIndex::match("dreamcast","MK-51002-50").isEmpty());
 }
 void filenamesNeverSupplyDiscIdentity(){
  QTemporaryDir t;QVERIFY(t.isValid());const auto path=t.filePath("House of the Dead 2 Virtua Cop Umbrella.iso");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(QByteArray(4096,'x')),qint64(4096));file.close();std::atomic_bool cancel{false};
  QVERIFY(ac::ps2::identify(path,cancel).value("game").toString().isEmpty());QVERIFY(ac::dreamcast::identify(path,cancel).value("game").toString().isEmpty());QVERIFY(ac::wii::identify(path,cancel).value("game").toString().isEmpty());
 }
 void recognitionDoesNotCreateFrontendProfiles(){
  using ac::scan::DiscIndex;
  QCOMPARE(DiscIndex::games("wii").size(),qsizetype(7));QCOMPARE(DiscIndex::games("ps2").size(),qsizetype(12));QCOMPARE(DiscIndex::games("dreamcast").size(),qsizetype(5));
  const auto row=DiscIndex::match("wii","R3ME01");QCOMPARE(row.value("catalogId").toString(),QString("wii-metroid-prime-trilogy"));
  for(const auto &entry:DiscIndex::games("wii"))QVERIFY(entry.toMap().value("catalogId")!=row.value("catalogId"));
 }
};
QTEST_GUILESS_MAIN(DiscIndexTest)
#include "DiscIndexTest.moc"

