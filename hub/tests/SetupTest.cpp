// SPDX-License-Identifier: GPL-3.0-only
#include "core/setup/Setup.h"
#include "core/setup/Report.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/model2/Model2.h"
#include "core/ps2/Ps2.h"
#include "core/ps1/Ps1.h"
#include "core/install/Install.h"
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
      // A program that keeps its game files beside itself has no folder setting.
      for(const auto &field:{"id","kind","title","detail","programKey","programLabel"})QVERIFY2(!system.value(field).toString().isEmpty(),field);
      QVERIFY(!ids.contains(system["id"].toString()));ids<<system["id"].toString();
      for(const auto &key:{system["folderKey"].toString(),system["programKey"].toString()}){if(key.isEmpty())continue;QVERIFY(!keys.contains(key));keys<<key;}
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
  void aSystemWithoutACheckSaysSo() {
    std::atomic_bool cancel=false;
    QVERIFY(ac::setup::scanSystem("not-a-system",{},{},{},cancel)["note"].toString().contains("cannot check"));
  }
  // A stand-in Model 2 build: emulator, viewer and working folder side by side, as the real build keeps them.
  void model2StartsTheEmulatorThenTheViewer() {
    QTemporaryDir t;const auto program=t.filePath("build/mame/emulator.exe"),sets=t.filePath("my sets");
    write(program,"synthetic");write(t.filePath("build/presenter/model2-viewer.exe"),"synthetic");QVERIFY(QDir().mkpath(t.filePath("build/runtime")));
    write(sets+"/vcop2.zip","synthetic");
    QVERIFY(ac::model2::problem("vcop2",program,sets).isEmpty());QVERIFY(ac::model2::problem("vf2",program,sets).contains("vf2.zip"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::model2::launchPlan("vf2",program,sets));
    const auto plan=ac::model2::launchPlan("vcop2",program,sets);const auto emulator=plan["companion"].toMap();
    const auto native=[](const QString &path){return QDir::toNativeSeparators(path);};
    // The viewer is the program the Hub watches; its one argument is the emulator's process id.
    QCOMPARE(plan["executable"].toString(),native(t.filePath("build/presenter/model2-viewer.exe")));QCOMPARE(plan["cwd"].toString(),native(t.filePath("build/presenter")));
    QCOMPARE(plan["args"].toStringList(),QStringList{"${companion_pid}"});
    QCOMPARE(emulator["executable"].toString(),native(program));QCOMPARE(emulator["cwd"].toString(),native(t.filePath("build/runtime")));
    QCOMPARE(emulator["args"].toStringList(),(QStringList{"vcop2","-rompath",native(sets),"-video","none","-sound","none","-numprocessors","2","-skip_gameinfo","-noreadconfig","-nowriteconfig",
      "-snapview","Screen 1","-snapshot_directory",native(t.filePath("build/captures/headset-vcop2")),"-snapname","vcop2-%i",
      "-nvram_directory","nvram","-cfg_directory","cfg","-state_directory","sta","-diff_directory","diff","-background_input"}));
    QCOMPARE(plan["env"].toMap(),(QVariantMap{{"ACVR_M2_STEREO","1"},{"ACVR_M2_STREAM","1"},{"ACVR_M2_INPUT_TEST","0"},{"ACVR_M2_IPD_M","0.064"},{"ACVR_M2_UNITS_PER_M","0.949387005"},{"ACVR_M2_ZERO_M","4"},{"ACVR_XR_SIZE","1"},{"ACVR_XR_HEIGHT","0"}}));
    QVERIFY(!QFileInfo::exists(t.filePath("build/captures"))); // Planning creates nothing.
  }
  // The emulator's own list decides what a game needs: its files, and device sets that have files of their own.
  void model2IsCheckedAgainstTheEmulatorsOwnList() {
    QTemporaryDir t;std::atomic_bool cancel=false;const auto sets=t.filePath("my sets");
    const auto rom=[](const QByteArray &name,const QByteArray &data){return "<rom name=\""+name+"\" crc=\""+QByteArray::number(crc32(data),16)+"\"/>";};
    const QByteArray xml="<mame><machine name=\"vcop2\" romof=\"base\"><rom name=\"none\" status=\"nodump\"/>"+rom("a","one")+rom("b","shared")
      +"<device_ref name=\"chip\"/><device_ref name=\"sign\"/><device_ref name=\"board\"/></machine>"
      "<machine name=\"chip\" isdevice=\"yes\"/><machine name=\"sign\" isdevice=\"yes\">"+rom("s","sign data")+"</machine>"
      "<machine name=\"board\" isdevice=\"yes\">"+rom("io","board data")+"</machine></mame>";
    const auto check=[&]{return ac::setup::mameCheck(xml,"vcop2",sets,{"sign"},cancel);};
    write(sets+"/vcop2.zip",zip({{"a","one"}}));
    QVERIFY(check()["reason"].toString().contains("base.zip")); // Its parent holds the shared file.
    write(sets+"/base.zip",zip({{"b","shared"}}));
    QVERIFY(check()["reason"].toString().contains("device set board")); // Devices are asked for only when they have files.
    write(sets+"/board.zip",zip({{"io","wrong"}}));QVERIFY(check()["reason"].toString().contains("not the one"));
    write(sets+"/board.zip",zip({{"io","board data"}}));
    QVERIFY(check()["reason"].toString().contains("device set sign"));
    // A stand-in for the sign's set, as loose files in a folder: accepted and named.
    write(sets+"/sign/s","blank");
    QVERIFY(check()["ok"].toBool());QCOMPARE(check()["standIns"].toStringList(),QStringList{"sign"});
    // The real thing, loose or zipped, is no stand-in.
    write(sets+"/sign/s","sign data");QVERIFY(check()["ok"].toBool());QVERIFY(check()["standIns"].toStringList().isEmpty());
    write(sets+"/vcop2.zip",zip({{"a","changed"}}));QVERIFY(check()["reason"].toString().startsWith("1 of the 2 files"));
    QVERIFY(ac::setup::mameCheck(xml,"vf2",sets,{},cancel)["reason"].toString().contains("does not list"));
  }
  // With no emulator build the game is found by name and cannot start; with one that gives no list it is found, unchecked.
  void model2WithoutAUsableEmulatorIsFoundButNotChecked() {
    QTemporaryDir t;std::atomic_bool cancel=false;const auto program=t.filePath("build/mame/emulator.exe"),sets=t.filePath("my sets");
    write(program,"synthetic");write(t.filePath("build/presenter/model2-viewer.exe"),"synthetic");QVERIFY(QDir().mkpath(t.filePath("build/runtime")));
    write(sets+"/vcop2.zip","synthetic");
    auto result=ac::setup::scanSystem("model2",{{"model2GameFolder",sets}},{},{},cancel);
    QVERIFY(entry(result,"notRight","vcop2")["reason"].toString().contains("not set up"));QCOMPARE(entry(result,"missing","vf2")["expected"].toString(),QString("vf2.zip"));
    // The emulator is run for its list only when the user allowed it: until then, names only, and said.
    result=ac::setup::scanSystem("model2",{{"model2GameFolder",sets},{"model2Executable",program}},{},{},cancel);
    QVERIFY(entry(result,"ready","vcop2")["detail"].toString().contains("only when you allow it"));
    QVERIFY(result["note"].toString().contains("Allow the Hub to ask found emulators"));
    result=ac::setup::scanSystem("model2",{{"model2GameFolder",sets},{"model2Executable",program},{"queryEmulators",true}},{},{},cancel);
    QVERIFY(entry(result,"ready","vcop2")["detail"].toString().contains("not checked"));
    QVERIFY(result["note"].toString().contains("could not be checked"));
    // Every game with a controller map says its controls are new and untested; Virtua Fighter 2 stays view only.
    QVERIFY(result["note"].toString().contains("Controls new, untested: Virtua Cop 2"));QVERIFY(result["note"].toString().contains("Virtua Fighter 2: view only for now"));
    for(const auto &value:ac::model2::games()){const auto game=value.toMap();
      QCOMPARE(game["controls"].toString().startsWith("Controls new, untested"),game["id"].toString()!="vf2");}
    // A clone kept in its parent's zip is looked for under the parent's name.
    QCOMPARE(ac::model2::fileName("hotdo"),QString("hotd"));QCOMPARE(ac::model2::fileName("vcop2"),QString("vcop2"));
  }
  // PS2: serials read from real discs, file names when a serial cannot be read, and the user's launcher's own keys.
  void ps2GamesAreKnownBySerialAndStartedThroughTheLauncher() {
    QCOMPARE(ac::ps2::games().size(),12);
    QCOMPARE(ac::ps2::gameForSerial("SLES_512.29"),QString("virtua-cop"));QCOMPARE(ac::ps2::gameForSerial("SLUS-21274"),QString("outrun-2006"));
    QCOMPARE(ac::ps2::gameForSerial("SLUS-20219"),QString("time-crisis-2"));
    for(const auto &value:ac::ps2::games()){const auto game=value.toMap();QVERIFY(game["controls"].toString().contains("amepad"));}
    QTemporaryDir t;std::atomic_bool cancel=false;const auto launcher=t.filePath("play/launcher.py"),python=t.filePath("python/pythonw.exe"),stop=t.filePath("runtime/quit.request");
    write(launcher,"synthetic");write(python,"synthetic");QVERIFY(QDir().mkpath(t.filePath("runtime")));
    QVERIFY(ac::ps2::problem("virtua-cop",launcher,python,stop).contains("no entry"));
    write(t.filePath("play/games.json"),"{\"virtua-cop\":{},\"outrun-2006\":{}}");
    QVERIFY(ac::ps2::problem("virtua-cop",launcher,python,stop).isEmpty());
    QVERIFY(ac::ps2::problem("virtua-cop",launcher,python,QString()).contains("stop file"));
    QVERIFY(ac::ps2::problem("virtua-cop",launcher,QString(),stop).contains("Python"));
    const auto plan=ac::ps2::launchPlan("outrun-2006",launcher,python,stop);
    QCOMPARE(plan["executable"].toString(),QDir::toNativeSeparators(python));QCOMPARE(plan["cwd"].toString(),QDir::toNativeSeparators(t.filePath("play")));
    QCOMPARE(plan["args"].toStringList(),(QStringList{QDir::toNativeSeparators(launcher),"outrun-2006"}));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps2::launchPlan("xgra",launcher,python,stop));
    // Inside the game: off for every game until each is confirmed, so only 3D screen is offered and Inside is refused.
    QVERIFY(ac::ps2::insideGames().isEmpty());
    for(const auto &value:ac::ps2::games())QCOMPARE(ac::ps2::views(value.toMap()["id"].toString(),launcher).size(),1);
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"inside"));
    // With a game in the list (given here), Inside is offered for it when the launcher knows it, and passed on.
    const QStringList inside{"virtua-cop","xgra"};
    const auto views=ac::ps2::views("virtua-cop",launcher,inside);QCOMPARE(views.size(),2);
    QCOMPARE(views[0].toMap()["id"].toString(),QString("screen"));QCOMPARE(views[1].toMap()["label"].toString(),QString("Inside the game"));
    QCOMPARE(ac::ps2::views("outrun-2006",launcher,inside).size(),1);QCOMPARE(ac::ps2::views("xgra",launcher,inside).size(),1);
    QCOMPARE(ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"inside",inside)["args"].toStringList(),(QStringList{QDir::toNativeSeparators(launcher),"virtua-cop","--inside"}));
    QCOMPARE(ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"screen",inside)["args"].toStringList(),(QStringList{QDir::toNativeSeparators(launcher),"virtua-cop"}));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps2::launchPlan("outrun-2006",launcher,python,stop,"inside",inside));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"wide",inside));
    // A compressed image is matched by its name only; the longer name wins.
    const auto folder=t.filePath("discs");
    write(folder+"/OutRun 2006 (copy).gz","synthetic");write(folder+"/XGRA - Extreme G Racing Association (USA).gz","synthetic");write(folder+"/Something Else (USA).gz","synthetic");
    const QVariantMap settings{{"ps2GameFolder",folder},{"ps2Launcher",launcher},{"ps2Python",python},{"ps2StopFile",stop}};
    const auto result=ac::setup::scanSystem("ps2",settings,{},{},cancel);
    QVERIFY(entry(result,"ready","outrun-2006")["detail"].toString().contains("serial was not read"));
    QVERIFY(entry(result,"notRight","xgra")["reason"].toString().contains("no entry")); // Found, but the launcher does not know it.
    QCOMPARE(entry(result,"missing","virtua-cop")["expected"].toString(),QString("Virtua Cop Elite Edition (disc image)"));
    QCOMPARE(result["other"].toList().size(),1);QCOMPARE(result["other"].toList()[0].toMap()["title"].toString(),QString("Something Else (USA)"));
  }
  // PS1: the user's PowerShell launcher knows each disc and gun; the Hub checks its keys and starts it.
  void ps1GamesStartThroughTheLaunchersKeys() {
    QVERIFY(ac::ps1::games().size()>=14);
    for(const auto &value:ac::ps1::games()){const auto game=value.toMap();QCOMPARE(game["view"].toString(),QString("theatre"));QVERIFY(!game["controls"].toString().isEmpty());}
    QTemporaryDir t;std::atomic_bool cancel=false;const auto launcher=t.filePath("play/ps1/launch-ps1.ps1");
    QVERIFY(ac::ps1::problem("point-blank",QString()).contains("not found"));
    write(launcher,"synthetic");
    QVERIFY(ac::ps1::problem("point-blank",launcher).contains("no entry"));
    QVERIFY(ac::ps1::problem("not-a-game",launcher).contains("not in the PS1 list"));
    write(t.filePath("play/ps1/games.json"),"{\"point-blank\":{},\"time-crisis\":{}}");
    QVERIFY(ac::ps1::problem("point-blank",launcher).isEmpty());
    const auto plan=ac::ps1::launchPlan("time-crisis",launcher);
    QVERIFY(plan["executable"].toString().endsWith("powershell.exe",Qt::CaseInsensitive));
    QCOMPARE(plan["cwd"].toString(),QDir::toNativeSeparators(t.filePath("play/ps1")));
    QCOMPARE(plan["args"].toStringList(),(QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(launcher),"time-crisis"}));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps1::launchPlan("judge-dredd",launcher));
    const auto result=ac::setup::scanSystem("ps1",{{"ps1Launcher",launcher}},{},{},cancel);
    QVERIFY(!entry(result,"ready","point-blank").isEmpty());
    QCOMPARE(entry(result,"missing","judge-dredd")["expected"].toString(),QString("Judge Dredd (an entry in your PS1 launcher)"));
  }
  void timeCrisisNeedsItsProgramAndTheFilesBesideIt() {
    QTemporaryDir t;std::atomic_bool cancel=false;const auto program=t.filePath("build/game.exe");
    const auto scan=[&](const QString &path){return ac::setup::scanSystem("system22",{{"system22Executable",path}},{},{},cancel);};
    QVERIFY(entry(scan(QString()),"notRight","timecris")["reason"].toString().contains("not set up"));
    write(program,"synthetic");QVERIFY(entry(scan(program),"notRight","timecris")["reason"].toString().contains("roms folder"));
    write(t.filePath("build/roms/set.bin"),"synthetic");QVERIFY(!entry(scan(program),"ready","timecris").isEmpty());
  }
  void dreamcastStartsFromTheEmulatorsOwnFolder() {
    QTemporaryDir t;const auto program=t.filePath("emu/program.exe"),disc=t.filePath("my discs/game.chd");write(program,"synthetic");write(disc,"synthetic");
    const auto plan=ac::dreamcast::launchPlan(disc,"headset",program);
    QCOMPARE(plan["cwd"].toString(),QDir::toNativeSeparators(t.filePath("emu")));
    QCOMPARE(plan["args"].toStringList(),(QStringList{"-config","config:vr.Xr=yes,input:device1=7,input:device2=7",QDir::toNativeSeparators(disc)}));
    QCOMPARE(ac::dreamcast::launchPlan(disc,"monitor",program)["args"].toStringList()[1],QString("config:vr.Xr=no,input:device1=7,input:device2=7"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::dreamcast::launchPlan(disc,"sideways",program));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::dreamcast::launchPlan(disc,"headset",t.filePath("gone.exe")));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::dreamcast::launchPlan(t.filePath("gone.chd"),"headset",program));
  }
  // A raw track is enough for the disc's own number: the header marker, then the number 64 bytes on.
  void dreamcastDiscsAreToldApartByTheirOwnNumber() {
    QTemporaryDir t;std::atomic_bool cancel=false;const auto folder=t.filePath("discs"),program=t.filePath("emu/program.exe");write(program,"synthetic");
    const auto track=[](const QByteArray &product){QByteArray bytes(2352,'\0');bytes.replace(16,15,"SEGA SEGAKATANA");bytes.replace(16+64,10,product.leftJustified(10,' '));return bytes;};
    write(folder+"/first.gdi",("3\n1 0 4 2352 a.bin 0\n2 600 0 2352 b.raw 0\n3 45000 4 2352 \"game track.bin\" 0\n"));write(folder+"/game track.bin",track("MK-51002"));
    write(folder+"/Confidential Mission.cdi",QByteArray(3000,'x')+track("MK-51160"));
    write(folder+"/house of the dead 2 other.cdi",track("HDR-0000"));
    write(folder+"/Death Crimson 2.cue","FILE \"none.bin\" BINARY\n");
    write(folder+"/unrelated.cdi",track("T-99999"));
    write(folder+"/no header.cdi",QByteArray(4096,'x'));
    const QVariantMap settings{{"dreamcastGameFolder",folder},{"dreamcastExecutable",program}};
    auto result=ac::setup::scanSystem("dreamcast",settings,{},{},cancel);
    // The US release has the full profile even though a second, unprofiled disc of the same game is present.
    QVERIFY(entry(result,"ready","hotd2")["detail"].toString().contains("US release"));QVERIFY(entry(result,"ready","hotd2")["file"].toString().endsWith("first.gdi"));
    // Confidential Mission (US) has its own VR profile since 2026-10-20: found and ready.
    QVERIFY(!entry(result,"ready","confmiss").isEmpty());QVERIFY(entry(result,"notRight","confmiss").isEmpty());
    QVERIFY(entry(result,"notRight","dcrimson2")["reason"].toString().contains("not checked"));
    QVERIFY(!entry(result,"missing","vcop2").isEmpty());QVERIFY(!entry(result,"missing","dcox").isEmpty());
    QVERIFY(!result["note"].toString().contains("headset")); // Headset acceptance is the owner's list, not the scan's.
    // Discs the Hub does not list are still named, with their own number where it was read.
    QStringList others;for(const auto &value:result["other"].toList())others<<value.toMap()["title"].toString()+"|"+value.toMap()["detail"].toString();
    QCOMPARE(others,(QStringList{"no header|number not read","unrelated|disc number T-99999"}));
    // Only the unprofiled release of the main game: found, not right, still startable.
    QVERIFY(QFile::remove(folder+"/first.gdi"));
    result=ac::setup::scanSystem("dreamcast",settings,{},{},cancel);
    QVERIFY(entry(result,"notRight","hotd2")["reason"].toString().contains("HDR-0000"));QVERIFY(entry(result,"notRight","hotd2")["startable"].toBool());
    // Without the emulator build nothing is startable.
    result=ac::setup::scanSystem("dreamcast",{{"dreamcastGameFolder",folder}},{},{},cancel);
    QVERIFY(entry(result,"notRight","confmiss")["reason"].toString().contains("not set up"));QVERIFY(!entry(result,"notRight","confmiss")["startable"].toBool());
    QCOMPARE(QDir(folder).entryList(QDir::Files).size(),6);
  }
  // The numbers read from real Japanese discs: found whatever the file is called, never reported missing.
  void dreamcastJapaneseGunDiscsAreFoundByTheirNumbers() {
    QTemporaryDir t;std::atomic_bool cancel=false;const auto folder=t.filePath("discs"),program=t.filePath("emu/program.exe");write(program,"synthetic");
    const auto track=[](const QByteArray &product){QByteArray bytes(2352,'\0');bytes.replace(16,15,"SEGA SEGAKATANA");bytes.replace(16+64,10,product.leftJustified(10,' '));return bytes;};
    write(folder+"/disc a.cdi",track("T23201M"));write(folder+"/disc b.cdi",track("HDR-0061"));
    // A .cdi keeps the game's header after its first session: here past the quick read, in a file named for the game.
    write(folder+"/Confidential Mission (late header).cdi",QByteArray(17*1024*1024,'x')+track("MK-51160"));
    const auto result=ac::setup::scanSystem("dreamcast",{{"dreamcastGameFolder",folder},{"dreamcastExecutable",program}},{},{},cancel);
    // Confidential Mission now has a VR profile, so its late-header disc is found and ready.
    QVERIFY2(!entry(result,"ready","confmiss").isEmpty(),"confmiss");
    for(const auto &id:{"dcrimson2","vcop2"}){
      QVERIFY2(entry(result,"missing",id).isEmpty(),id);
      const auto reason=entry(result,"notRight",id)["reason"].toString();
      QVERIFY2(reason.contains("no VR profile"),id);QVERIFY2(!reason.contains("not checked"),id);
    }
    QVERIFY(result["other"].toList().isEmpty());
  }
};
QTEST_APPLESS_MAIN(SetupTest)
#include "SetupTest.moc"
