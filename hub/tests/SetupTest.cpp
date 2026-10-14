// SPDX-License-Identifier: GPL-3.0-only
#include "core/setup/Setup.h"
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
namespace {
void write(const QString &path,const QByteArray &bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);}
QByteArray list(const QStringList &paths){return QJsonDocument(QJsonObject{{"manifest_paths",QJsonArray::fromStringList(paths)}}).toJson();}
}
class SetupTest : public QObject {
  Q_OBJECT
private slots:
  void everySystemNamesItsOwnSettings() {
    QSet<QString> ids,keys;QStringList kinds;
    for(const auto &value:ac::setup::systems()){
      const auto system=value.toMap();
      for(const auto &field:{"id","kind","title","detail","folderKey","programKey","programLabel"})QVERIFY2(!system.value(field).toString().isEmpty(),field);
      QVERIFY(!ids.contains(system["id"].toString()));ids<<system["id"].toString();
      for(const auto &key:{system["folderKey"].toString(),system["programKey"].toString()}){QVERIFY(!keys.contains(key));keys<<key;}
      if(!kinds.contains(system["kind"].toString()))kinds<<system["kind"].toString();
    }
    QCOMPARE(kinds,(QStringList{"arcade","pinball","consoles"}));
    // Folders chosen before this page existed keep their meaning.
    QVERIFY(keys.contains("model3GameFolder"));QVERIFY(keys.contains("pinballFolder"));QVERIFY(keys.contains("pinballExecutable"));
  }
  void guessLooksBesideAndAboveChosenFolders() {
    QTemporaryDir t;const auto root=QDir(t.path()).absolutePath();
    for(const auto &name:{"roms/model3","roms/model2","pinball/tables","Dreamcast"})QVERIFY(QDir(root).mkpath(name));
    // A sibling of the folder already chosen.
    QCOMPARE(ac::setup::guessFolder("model2",{root+"/roms/model3"}),root+"/roms/model2");
    // One level further up, and inside a "roms" folder found there.
    QCOMPARE(ac::setup::guessFolder("dreamcast",{root+"/roms/model3"}),root+"/Dreamcast");
    QCOMPARE(ac::setup::guessFolder("model3",{root+"/pinball/tables"}),root+"/roms/model3");
    QCOMPARE(ac::setup::guessFolder("pinball",{root+"/Dreamcast"}),root+"/pinball/tables");
  }
  void guessIsEmptyWhenNothingFits() {
    QTemporaryDir t;QVERIFY(QDir(t.path()).mkpath("roms/model3"));
    QVERIFY(ac::setup::guessFolder("dreamcast",{t.filePath("roms/model3")}).isEmpty());
    QVERIFY(ac::setup::guessFolder("model2",{}).isEmpty());
    QVERIFY(ac::setup::guessFolder("model2",{t.filePath("gone")}).isEmpty());
    QVERIFY(ac::setup::guessFolder("not-a-system",{t.filePath("roms/model3")}).isEmpty());
    // A file with a fitting name is not a folder.
    write(t.filePath("roms/model2"),"x");QVERIFY(ac::setup::guessFolder("model2",{t.filePath("roms/model3")}).isEmpty());
  }
  void steamVrListIsReadAsWritten() {
    QTemporaryDir t;const auto file=t.filePath("config/appconfig.json"),manifest=t.filePath("hub/resources/product.vrmanifest");
    QVERIFY(!ac::setup::steamVrLists(file,manifest)); // No file yet.
    write(file,list({"C:/other/app.vrmanifest"}));QVERIFY(!ac::setup::steamVrLists(file,manifest));
    // SteamVR may store the path with other separators or letter case.
    write(file,list({"C:/other/app.vrmanifest",QDir::toNativeSeparators(manifest).toUpper()}));
    QVERIFY(ac::setup::steamVrLists(file,manifest));
    write(file,"not json");QVERIFY(!ac::setup::steamVrLists(file,manifest));
    QVERIFY(!ac::setup::steamVrLists(file,QString()));
  }
};
QTEST_APPLESS_MAIN(SetupTest)
#include "SetupTest.moc"
