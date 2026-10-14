// SPDX-License-Identifier: GPL-3.0-only
#include "core/setup/Setup.h"
#include "core/setup/Report.h"
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
namespace {
void write(const QString &path,const QByteArray &bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);}
QByteArray list(const QStringList &paths){return QJsonDocument(QJsonObject{{"manifest_paths",QJsonArray::fromStringList(paths)}}).toJson();}
quint32 crc32(const QByteArray &data){
  quint32 crc=0xffffffffU;
  for(const auto byte:data){crc^=quint8(byte);for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1U)));}
  return ~crc;
}
// A stored (uncompressed) zip of synthetic members: enough for a directory read.
QByteArray zip(const QList<QPair<QByteArray,QByteArray>> &members){
  QByteArray out,directory;const auto u16=[](QByteArray &to,quint16 v){char b[2];qToLittleEndian(v,b);to.append(b,2);};const auto u32=[](QByteArray &to,quint32 v){char b[4];qToLittleEndian(v,b);to.append(b,4);};
  for(const auto &[name,data]:members){
    const auto offset=quint32(out.size()),crc=crc32(data),size=quint32(data.size());
    u32(out,0x04034b50);u16(out,20);u16(out,0);u16(out,0);u16(out,0);u16(out,0);u32(out,crc);u32(out,size);u32(out,size);u16(out,quint16(name.size()));u16(out,0);out+=name+data;
    u32(directory,0x02014b50);u16(directory,20);u16(directory,20);u16(directory,0);u16(directory,0);u16(directory,0);u16(directory,0);u32(directory,crc);u32(directory,size);u32(directory,size);
    u16(directory,quint16(name.size()));u16(directory,0);u16(directory,0);u16(directory,0);u16(directory,0);u32(directory,0);u32(directory,offset);directory+=name;
  }
  const auto start=quint32(out.size());out+=directory;
  u32(out,0x06054b50);u16(out,0);u16(out,0);u16(out,quint16(members.size()));u16(out,quint16(members.size()));u32(out,quint32(directory.size()));u32(out,start);u16(out,0);
  return out;
}
QString hex(const QByteArray &data){return "0x"+QString::number(crc32(data),16).toUpper();}
QVariantMap entry(const QVariantMap &result,const char *group,const QString &id){for(const auto &value:result.value(group).toList())if(value.toMap().value("id")==id)return value.toMap();return {};}
// A stand-in emulator folder whose list names two files for one game and a parent for another.
struct Model3 {
  QTemporaryDir t;QString games=t.filePath("my games"),program=t.filePath("emu/program.exe");QVariantMap settings{{"model3GameFolder",games},{"model3Executable",program}};
  std::atomic_bool cancel=false;
  Model3(){
    write(program,"synthetic");QVERIFY(QDir().mkpath(games));
    write(t.filePath("emu/Config/Games.xml"),("<games>"
      "<game name=\"base\"><roms><region name=\"a\"><file offset=\"0\" name=\"shared.bin\" crc32=\""+hex("shared")+"\"/><file offset=\"2\" name=\"old.bin\" crc32=\""+hex("old")+"\"/></region></roms></game>"
      "<game name=\"scud\"><roms><region name=\"a\"><file offset=\"0\" name=\"one.bin\" crc32=\""+hex("one")+"\"/><file offset=\"2\" name=\"two.bin\" crc32=\""+hex("two")+"\"/></region></roms></game>"
      "<game name=\"scudx\" parent=\"scud\"><roms><region name=\"a\"><file offset=\"0\" name=\"other.bin\" crc32=\""+hex("other")+"\"/></region>"
        "<region name=\"board\" required=\"false\"><file offset=\"0\" name=\"board.bin\" crc32=\""+hex("board")+"\"/></region></roms></game>"
      "<game name=\"daytona2\" parent=\"base\"><roms><region name=\"a\"><file offset=\"2\" name=\"new.bin\" crc32=\""+hex("new")+"\"/></region></roms></game>"
      "</games>").toUtf8());
  }
  QVariantMap scan(){return ac::setup::scanSystem("model3",settings,{},{},cancel);}
};
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
    write(file,list({"/other/app.vrmanifest"}));QVERIFY(!ac::setup::steamVrLists(file,manifest));
    // SteamVR may store the path with other separators or letter case.
    write(file,list({"/other/app.vrmanifest",QDir::toNativeSeparators(manifest).toUpper()}));
    QVERIFY(ac::setup::steamVrLists(file,manifest));
    write(file,"not json");QVERIFY(!ac::setup::steamVrLists(file,manifest));
    QVERIFY(!ac::setup::steamVrLists(file,QString()));
  }
  void emulatorListGivesAGamesFilesWithItsParents() {
    Model3 f;const auto xml=f.t.filePath("emu/Config/Games.xml");
    QCOMPARE(ac::setup::supermodelFiles(xml,"scud").value("files").toMap().size(),2);
    const auto clone=ac::setup::supermodelFiles(xml,"daytona2");
    QCOMPARE(clone.value("parent").toString(),QString("base"));
    // The parent's file at the same place is replaced; its other file is still needed.
    QStringList names;for(const auto &name:clone.value("files").toMap().values())names<<name.toString();names.sort();
    QCOMPARE(names,(QStringList{"new.bin","shared.bin"}));
    QVERIFY(ac::setup::supermodelFiles(xml,"unlisted").isEmpty());QVERIFY(ac::setup::supermodelFiles(f.t.filePath("none.xml"),"scud").isEmpty());
  }
  void model3GamesFallIntoThreeGroups() {
    Model3 f;
    // Right files under other names: the emulator goes by checksum, so this is ready.
    write(f.games+"/scud.zip",zip({{"renamed-1","one"},{"renamed-2","two"},{"extra","unused"}}));
    auto result=f.scan();
    QCOMPARE(result["title"].toString(),QString("Sega Model 3"));
    QVERIFY(!entry(result,"ready","scud").isEmpty());
    QCOMPARE(entry(result,"missing","srally2")["expected"].toString(),QString("srally2.zip"));
    QCOMPARE(result["ready"].toList().size()+result["notRight"].toList().size()+result["missing"].toList().size(),10);
    // One file differs: found, not right, and the count is stated.
    write(f.games+"/scud.zip",zip({{"one.bin","one"},{"two.bin","changed"}}));
    result=f.scan();QVERIFY(entry(result,"ready","scud").isEmpty());
    QVERIFY(entry(result,"notRight","scud")["reason"].toString().startsWith("1 of the 2 files"));
    // The zip is named for the game but holds another version of it, without that version's optional board file.
    write(f.games+"/scud.zip",zip({{"other.bin","other"},{"two.bin","two"}}));
    result=f.scan();QVERIFY(entry(result,"ready","scud")["detail"].toString().contains("scudx"));
    // Not a zip at all.
    write(f.games+"/scud.zip","plain text");QVERIFY(entry(f.scan(),"notRight","scud")["reason"].toString().contains("not a zip"));
    // A game the emulator build does not list.
    write(f.games+"/vf3.zip",zip({{"a","a"}}));QVERIFY(entry(f.scan(),"notRight","vf3")["reason"].toString().contains("does not list"));
  }
  void model3ParentSetIsNeededAndUsed() {
    Model3 f;write(f.games+"/daytona2.zip",zip({{"new.bin","new"}}));
    QVERIFY(entry(f.scan(),"notRight","daytona2")["reason"].toString().contains("base.zip"));
    write(f.games+"/base.zip",zip({{"shared.bin","shared"},{"old.bin","old"}}));
    QVERIFY(!entry(f.scan(),"ready","daytona2").isEmpty());
  }
  void model3WithoutItsProgramIsFoundButNotChecked() {
    Model3 f;write(f.games+"/scud.zip",zip({{"one.bin","one"},{"two.bin","two"}}));
    f.settings["model3Executable"]=QString();
    const auto result=f.scan();QVERIFY(entry(result,"notRight","scud")["reason"].toString().contains("not set up"));
    QVERIFY(!entry(result,"missing","srally2").isEmpty());
    // Nothing is written beside the user's files or the emulator.
    QCOMPARE(QDir(f.games).entryList(QDir::AllEntries|QDir::NoDotAndDotDot),QStringList{"scud.zip"});
    QCOMPARE(QDir(f.t.filePath("emu")).entryList(QDir::AllEntries|QDir::NoDotAndDotDot,QDir::Name),(QStringList{"Config","program.exe"}));
  }
  void pinballTablesAreGroupedFromTheTableScan() {
    QTemporaryDir t;write(t.filePath("vpx.exe"),"synthetic");std::atomic_bool cancel=false;
    const auto row=[](const QString &id,bool vr,const QString &roms,const QString &error={}){return QVariantMap{{"id",id},{"title","Table "+id},{"vrReady",vr},{"romState",roms},{"rom","rom"+id},{"error",error}};};
    const QVariantMap tables{{"tables",QVariantList{row("a",true,"ready"),row("b",true,"ROM missing"),row("c",false,"ready"),row("d",true,"unknown","unreadable"),row("e",false,"ready")}}};
    QVariantMap settings{{"pinballFolder",t.path()},{"pinballExecutable",t.filePath("vpx.exe")},{"pinballOverrides",QVariantMap{{"e",true}}}};
    auto result=ac::setup::scanSystem("pinball",settings,{{"pinballScan",tables}},{},cancel);
    QVERIFY(!entry(result,"ready","a").isEmpty());QVERIFY(!entry(result,"ready","e").isEmpty()); // "e" is marked VR-ready by the user.
    QVERIFY(entry(result,"notRight","b")["reason"].toString().contains("romb.zip"));
    QVERIFY(entry(result,"notRight","d")["reason"].toString().contains("unreadable"));
    QVERIFY(result["missing"].toList().isEmpty());QVERIFY(result["note"].toString().startsWith("1 other"));
    settings["pinballExecutable"]=QString();
    result=ac::setup::scanSystem("pinball",settings,{{"pinballScan",tables}},{},cancel);
    QVERIFY(result["ready"].toList().isEmpty());QVERIFY(entry(result,"notRight","a")["reason"].toString().contains("not set up"));
  }
  void anUnknownSystemSaysSo() {
    std::atomic_bool cancel=false;
    QVERIFY(ac::setup::scanSystem("dreamcast",{},{},{},cancel)["note"].toString().contains("cannot check"));
  }
};
QTEST_APPLESS_MAIN(SetupTest)
#include "SetupTest.moc"
