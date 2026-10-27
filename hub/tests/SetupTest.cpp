// SPDX-License-Identifier: GPL-3.0-only
#include "core/setup/Setup.h"
#include "core/setup/Report.h"
#include "core/library/Library.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/model2/Model2.h"
#include "core/ps2/Ps2.h"
#include "core/ps1/Ps1.h"
#include "core/n64/N64.h"
#include "core/saturn/Saturn.h"
#include "core/libretro/Libretro.h"
#include "core/scripted/Scripted.h"
#include "core/system22/System22.h"
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
QByteArray ps2Header(const QByteArray &serial){
  QByteArray b(24*2048,'\0');const int p=16*2048,d=20*2048;
  b[p]=1;b.replace(p+1,5,"CD001");b[p+6]=1;b[p+156]=34;
  qToLittleEndian<quint32>(20,b.data()+p+158);qToLittleEndian<quint32>(2048,b.data()+p+166);
  b[d]=46;qToLittleEndian<quint32>(21,b.data()+d+2);qToLittleEndian<quint32>(64,b.data()+d+10);
  b[d+32]=12;b.replace(d+33,12,"SYSTEM.CNF;1");const auto cnf=QByteArray("BOOT2 = cdrom0:\\")+serial+";1\r\n";b.replace(21*2048,cnf.size(),cnf);return b;
}
QByteArray wiiHeader(const QByteArray &id){QByteArray h(0x400,'\0');h.replace(0,6,id);qToBigEndian<quint32>(0x5D1C9EA3,h.data()+0x18);return h;}
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
    QCOMPARE(result["ready"].toList().size()+result["notRight"].toList().size()+result["missing"].toList().size(),11); // with Sega Rally 2 DX
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
    QCOMPARE(emulator["args"].toStringList(),(QStringList{"vcop2","-rompath",native(sets),"-video","none","-sound","auto","-numprocessors","2","-skip_gameinfo","-noreadconfig","-nowriteconfig",
      "-snapview","Screen 1","-snapshot_directory",native(t.filePath("build/captures/headset-vcop2")),"-snapname","vcop2-%i",
      "-nvram_directory","nvram","-cfg_directory","cfg","-state_directory","sta","-diff_directory","diff","-background_input"}));
    QCOMPARE(plan["env"].toMap(),(QVariantMap{{"ACVR_M2_STEREO","1"},{"ACVR_M2_STREAM","1"},{"ACVR_M2_INPUT_TEST","0"},{"ACVR_M2_IPD_M","0.064"},{"ACVR_M2_UNITS_PER_M","0.949387005"},{"ACVR_M2_ZERO_M","4"},{"ACVR_XR_SIZE","1"},{"ACVR_XR_HEIGHT","0"},
      {"ACVR_M2_WIDE","0"},{"ACVR_M2_ROLL_STEER","0"}}));
    QVERIFY(!QFileInfo::exists(t.filePath("build/captures"))); // Planning creates nothing.
    // The views the old numbered launchers offered: real 3D first, open frame (-Wide 93), roll steer opt-in.
    QStringList ids;for(const auto &v:ac::model2::views("daytona"))ids<<v.toMap()["id"].toString();
    QCOMPARE(ids,(QStringList{"3d","open-frame","roll-steer"}));
    ids.clear();for(const auto &v:ac::model2::views("vcop2"))ids<<v.toMap()["id"].toString();QCOMPARE(ids,(QStringList{"3d","open-frame"}));
    QCOMPARE(ac::model2::views("rchase2").size(),1);QCOMPARE(ac::model2::views("vf2").size(),1);
    int views=0;for(const auto &value:ac::model2::games())views+=ac::model2::views(value.toMap()["id"].toString()).size();
    QCOMPARE(views,25); // the 25 archived Model 2 launchers
    QCOMPARE(ac::model2::launchPlan("vcop2",program,sets,"open-frame")["env"].toMap()["ACVR_M2_WIDE"].toString(),QString("93"));
    QCOMPARE(ac::model2::launchPlan("vcop2",program,sets,"open-frame")["env"].toMap()["ACVR_M2_ROLL_STEER"].toString(),QString("0"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::model2::launchPlan("vcop2",program,sets,"roll-steer"));
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
  // PS2: exact disc serials and the user's launcher's own keys.
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
    for(const auto &value:ac::ps2::games())for(const auto &view:ac::ps2::views(value.toMap()["id"].toString(),launcher))QVERIFY(view.toMap()["id"].toString()!="inside");
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"inside"));
    // With a game in the list (given here), Inside is offered for it when the launcher knows it, and passed on.
    const QStringList inside{"virtua-cop","xgra"};
    const auto views=ac::ps2::views("virtua-cop",launcher,inside);QCOMPARE(views.size(),3);QCOMPARE(views[2].toMap()["id"].toString(),QString("check")); // and its headset check
    QCOMPARE(views[0].toMap()["id"].toString(),QString("screen"));QCOMPARE(views[1].toMap()["label"].toString(),QString("Inside the game"));
    QCOMPARE(ac::ps2::views("outrun-2006",launcher,inside).size(),1);QCOMPARE(ac::ps2::views("xgra",launcher,inside).size(),1);
    QCOMPARE(ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"inside",inside)["args"].toStringList(),(QStringList{QDir::toNativeSeparators(launcher),"virtua-cop","--inside"}));
    QCOMPARE(ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"screen",inside)["args"].toStringList(),(QStringList{QDir::toNativeSeparators(launcher),"virtua-cop"}));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps2::launchPlan("outrun-2006",launcher,python,stop,"inside",inside));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"wide",inside));
    // A launcher entry with its own inside profile offers Inside the game without the list; a depth-trial key
    // offers Depth trial; Virtua Cop also has the launcher's short headset check.
    write(t.filePath("play/games.json"),"{\"virtua-cop\":{\"inside\":{\"params\":{}}},\"time-crisis-3\":{\"inside\":{}},\"time-crisis-3-depth-trial\":{}}");
    QStringList ids;for(const auto &v:ac::ps2::views("time-crisis-3",launcher))ids<<v.toMap()["id"].toString();
    QCOMPARE(ids,(QStringList{"screen","inside","depth"}));
    ids.clear();for(const auto &v:ac::ps2::views("virtua-cop",launcher))ids<<v.toMap()["id"].toString();
    QCOMPARE(ids,(QStringList{"screen","inside","check"}));
    QCOMPARE(ac::ps2::launchPlan("time-crisis-3",launcher,python,stop,"inside")["args"].toStringList(),(QStringList{QDir::toNativeSeparators(launcher),"time-crisis-3","--inside"}));
    QCOMPARE(ac::ps2::launchPlan("time-crisis-3",launcher,python,stop,"depth")["args"].toStringList(),(QStringList{QDir::toNativeSeparators(launcher),"time-crisis-3-depth-trial"}));
    QCOMPARE(ac::ps2::launchPlan("virtua-cop",launcher,python,stop,"check")["args"].toStringList(),(QStringList{QDir::toNativeSeparators(launcher),"virtua-cop","--headset-check"}));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps2::launchPlan("time-crisis-3",launcher,python,stop,"check"));
    write(t.filePath("play/games.json"),"{\"virtua-cop\":{},\"outrun-2006\":{}}");
    // Disc headers prove the game independently of the file name.
    const auto folder=t.filePath("discs");
    write(folder+"/unrelated-a.iso",ps2Header("SLUS_212.74"));write(folder+"/unrelated-b.iso",ps2Header("SLUS_206.32"));write(folder+"/Something Else (USA).gz","synthetic");
    const QVariantMap settings{{"ps2GameFolder",folder},{"ps2Launcher",launcher},{"ps2Python",python},{"ps2StopFile",stop}};
    const auto result=ac::setup::scanSystem("ps2",settings,{},{},cancel);
    QVERIFY(entry(result,"ready","outrun-2006")["detail"].toString().contains("SLUS-21274"));
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
    // Each view is a key of its own: <id>, <id>-open-frame, <id>-diorama, <id>-no-trim; the usual one first.
    write(t.filePath("play/ps1/games.json"),"{\"point-blank\":{},\"point-blank-diorama\":{},\"time-crisis\":{},\"time-crisis-open-frame\":{},\"judge-dredd-no-trim\":{}}");
    QStringList ids;for(const auto &v:ac::ps1::views("time-crisis",launcher))ids<<v.toMap()["id"].toString();
    QCOMPARE(ids,(QStringList{"standard","open-frame"}));
    QCOMPARE(ac::ps1::viewLabel("time-crisis","standard"),QString("Real 3D screen"));QCOMPARE(ac::ps1::viewLabel("point-blank","standard"),QString("Theatre screen"));
    ids.clear();for(const auto &v:ac::ps1::views("point-blank",launcher))ids<<v.toMap()["id"].toString();QCOMPARE(ids,(QStringList{"standard","diorama"}));
    QCOMPARE(ac::ps1::launchPlan("time-crisis",launcher,"open-frame")["args"].toStringList().last(),QString("time-crisis-open-frame"));
    QCOMPARE(ac::ps1::launchPlan("point-blank",launcher,"diorama")["args"].toStringList().last(),QString("point-blank-diorama"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::ps1::launchPlan("point-blank",launcher,"open-frame"));
    write(t.filePath("play/ps1/games.json"),"{\"point-blank\":{},\"time-crisis\":{}}");
    const auto result=ac::setup::scanSystem("ps1",{{"ps1Launcher",launcher}},{},{},cancel);
    QVERIFY(!entry(result,"ready","point-blank").isEmpty());
    QCOMPARE(entry(result,"missing","judge-dredd")["expected"].toString(),QString("Judge Dredd (an entry in your PS1 launcher)"));
  }
  // N64: the user's launcher has one key per view (<id>-3d, <id>, <id>-theatre); the Hub offers the ones it lists.
  void n64GamesStartThroughTheLaunchersViewKeys() {
    QCOMPARE(ac::n64::games().size(),22);
    for(const auto &value:ac::n64::games()){const auto game=value.toMap();QVERIFY(!game["controls"].toString().isEmpty());
     QVERIFY(QStringList({"open","wide","standard"}).contains(game["frame"].toString()));}
    QTemporaryDir t;std::atomic_bool cancel=false;const auto launcher=t.filePath("play/n64/launch-n64.ps1");
    QVERIFY(ac::n64::problem("mario-kart-64",QString()).contains("not found"));
    write(launcher,"synthetic");
    QVERIFY(ac::n64::problem("mario-kart-64",launcher).contains("no entry"));
    QVERIFY(ac::n64::problem("not-a-game",launcher).contains("not in the N64 list"));
    write(t.filePath("play/n64/games.json"),"{\"mario-kart-64\":{},\"mario-kart-64-3d\":{},\"mario-kart-64-theatre\":{},\"turok-3d\":{}}");
    QVERIFY(ac::n64::problem("mario-kart-64",launcher).isEmpty());
    QVERIFY(ac::n64::problem("turok",launcher).isEmpty());
    QStringList ids;for(const auto &v:ac::n64::views("mario-kart-64",launcher))ids<<v.toMap()["id"].toString();
    QCOMPARE(ids,(QStringList{"3d","flat","theatre"}));
    QCOMPARE(ac::n64::views("turok",launcher).size(),1);
    QCOMPARE(ac::n64::viewLabel("mario-kart-64","3d"),QString("Real 3D, open frame"));
    QCOMPARE(ac::n64::viewLabel("super-mario-64","flat"),QString("16:9"));
    const auto plan=ac::n64::launchPlan("mario-kart-64",launcher,"3d");
    QVERIFY(plan["executable"].toString().endsWith("powershell.exe",Qt::CaseInsensitive));
    QCOMPARE(plan["cwd"].toString(),QDir::toNativeSeparators(t.filePath("play/n64")));
    QCOMPARE(plan["args"].toStringList(),(QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(launcher),"mario-kart-64-3d"}));
    QCOMPARE(ac::n64::launchPlan("mario-kart-64",launcher,"flat")["args"].toStringList().last(),QString("mario-kart-64"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::n64::launchPlan("turok",launcher,"theatre"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::n64::launchPlan("goldeneye",launcher,"3d"));
    const auto result=ac::setup::scanSystem("n64",{{"n64Launcher",launcher}},{},{},cancel);
    QVERIFY(!entry(result,"ready","mario-kart-64").isEmpty());
    QCOMPARE(entry(result,"missing","goldeneye")["expected"].toString(),QString("GoldenEye 007 (an entry in your N64 launcher)"));
  }
  // Saturn: the five gun games, each started by its key in the user's launcher, on the theatre screen.
  void saturnGamesStartThroughTheLaunchersKeys() {
    QCOMPARE(ac::saturn::games().size(),5);
    for(const auto &value:ac::saturn::games()){const auto game=value.toMap();QCOMPARE(game["view"].toString(),QString("theatre"));QVERIFY(game["controls"].toString().contains("Stunner"));}
    QTemporaryDir t;std::atomic_bool cancel=false;const auto launcher=t.filePath("play/saturn/launch-saturn.ps1");
    QVERIFY(ac::saturn::problem("virtua-cop-2",QString()).contains("not found"));
    write(launcher,"synthetic");
    QVERIFY(ac::saturn::problem("virtua-cop-2",launcher).contains("no entry"));
    QVERIFY(ac::saturn::problem("not-a-game",launcher).contains("not in the Saturn list"));
    write(t.filePath("play/saturn/games.json"),"{\"virtua-cop-2\":{},\"house-of-the-dead\":{}}");
    QVERIFY(ac::saturn::problem("virtua-cop-2",launcher).isEmpty());
    const auto plan=ac::saturn::launchPlan("house-of-the-dead",launcher);
    QVERIFY(plan["executable"].toString().endsWith("powershell.exe",Qt::CaseInsensitive));
    QCOMPARE(plan["cwd"].toString(),QDir::toNativeSeparators(t.filePath("play/saturn")));
    QCOMPARE(plan["args"].toStringList(),(QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(launcher),"house-of-the-dead"}));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::saturn::launchPlan("area-51",launcher));
    const auto result=ac::setup::scanSystem("saturn",{{"saturnLauncher",launcher}},{},{},cancel);
    QVERIFY(!entry(result,"ready","virtua-cop-2").isEmpty());
    QCOMPARE(entry(result,"missing","hakaider")["expected"].toString(),QString("Mechanical Violator Hakaider: Last Judgement (an entry in your Saturn launcher)"));
  }
  // 3DS: program/core/profiles and explicit per-game files are Hub settings; no title inference.
  void threeDsGamesNeedTheirProgramCoreProfilesAndDumps() {
    QVariantMap system;for(const auto &value:ac::setup::systems())if(value.toMap()["id"]=="3ds")system=value.toMap();
    QCOMPARE(system["kind"].toString(),QString("consoles"));QVERIFY(system["folderKey"].toString().isEmpty());QCOMPARE(system["programKey"].toString(),QString("libretroHost"));
    QTemporaryDir t;std::atomic_bool cancel=false;
    const auto host=t.filePath("bin/acvr-ps1.exe"),core=t.filePath("cores/azahar_libretro.dll");
    QVariantMap settings{{"libretroHost",host},{"threedsCore",core},{"libretroProfiles",t.filePath("profiles")},{"threedsGameFolder",t.filePath("dumps")}};
    const auto scan=[&]{return ac::setup::scanSystem("3ds",settings,{},{},cancel);};
    // Nothing on disk: each game says what to choose first.
    QVERIFY(entry(scan(),"notRight","3ds-star-fox-64-3d")["reason"].toString().contains("acvr-ps1 program"));
    write(host,"synthetic");QVERIFY(entry(scan(),"notRight","3ds-mario-kart-7")["reason"].toString().contains("3DS core"));
    write(core,"synthetic");QVERIFY(entry(scan(),"notRight","3ds-mario-kart-7")["reason"].toString().contains("profile"));
    for(const auto &id:{"3ds-star-fox-64-3d","3ds-mario-kart-7"})write(t.filePath(QString("profiles/%1.toml").arg(id)),"schema = 1\n");
    // The program, core and profiles are right: a game without a dump is "not found", with the name to give it.
    QVERIFY(entry(scan(),"missing","3ds-star-fox-64-3d")["expected"].toString().contains("choose this game's file"));
    write(t.filePath("dumps/Mario Kart 7.3ds"),"synthetic");
    QVERIFY(entry(scan(),"ready","3ds-mario-kart-7").isEmpty()); // No inference from a title-shaped name.
    settings["threedsMarioKartFile"]=t.filePath("dumps/Mario Kart 7.3ds");
    auto result=scan();
    QVERIFY(!entry(result,"ready","3ds-mario-kart-7").isEmpty());QVERIFY(entry(result,"ready","3ds-mario-kart-7")["detail"].toString().contains("left stick"));
    QVERIFY(!entry(result,"missing","3ds-star-fox-64-3d").isEmpty());
    write(t.filePath("dumps/Star Fox 64 3D.cci"),"synthetic");
    settings["threedsStarFoxFile"]=t.filePath("dumps/Star Fox 64 3D.cci");
    result=scan();QVERIFY(!entry(result,"ready","3ds-star-fox-64-3d").isEmpty());QVERIFY(result["missing"].toList().isEmpty()&&result["notRight"].toList().isEmpty());
    QVERIFY(entry(result,"ready","3ds-star-fox-64-3d")["detail"].toString().contains("identity is not verified"));
    // The saved report is read again when any of the four settings changes.
    const auto json=[](const QVariantMap &stamp){return QJsonDocument::fromVariant(stamp).toJson(QJsonDocument::Compact);};
    const auto stamp=json(ac::setup::reportStamp(settings,{"3ds"}));
    for(const auto &key:{"libretroHost","threedsCore","libretroProfiles","threedsGameFolder","threedsStarFoxFile","threedsMarioKartFile"}){auto moved=settings;moved[key]=t.filePath("elsewhere");QVERIFY2(json(ac::setup::reportStamp(moved,{"3ds"}))!=stamp,key);}
    settings["threedsStarFoxFile"]=t.filePath("missing.3ds");
    QVERIFY(!entry(scan(),"notRight","3ds-star-fox-64-3d").isEmpty());
  }
  void timeCrisisNeedsItsProgramAndTheFilesBesideIt() {
    QTemporaryDir t;std::atomic_bool cancel=false;const auto program=t.filePath("build/game.exe");
    const auto scan=[&](const QString &path){return ac::setup::scanSystem("system22",{{"system22Executable",path}},{},{},cancel);};
    QVERIFY(entry(scan(QString()),"notRight","timecris")["reason"].toString().contains("not set up"));
    write(program,"synthetic");QVERIFY(entry(scan(program),"notRight","timecris")["reason"].toString().contains("roms folder"));
    write(t.filePath("build/roms/set.bin"),"synthetic");QVERIFY(!entry(scan(program),"ready","timecris").isEmpty());
  }
  // Arcade guns, Dreamcast, Wii and GameCube: the user's launcher lists its games and views; the Hub starts
  // "<launcher> <key> -View <view>", the usual view first and opt-in and monitor views last.
  void launcherSystemsListTheirGamesAndViews() {
    QTemporaryDir t;const auto ps1=t.filePath("play/ps1/launch-ps1.ps1"),arcade=t.filePath("play/mame-gun/launch-mame-gun-key.ps1");
    write(ps1,"synthetic");
    const auto *system=ac::scripted::system("mamegun");QVERIFY(system);
    QVERIFY(ac::scripted::locate(*system,QString(),{ps1}).isEmpty());
    write(arcade,"synthetic");
    QCOMPARE(QDir::fromNativeSeparators(ac::scripted::locate(*system,QString(),{QString(),ps1})),QDir::cleanPath(arcade));
    QVERIFY(ac::scripted::locate(*system,t.filePath("elsewhere.ps1"),{ps1}).isEmpty()); // a chosen launcher that is gone is not replaced
    // A byte-order mark, a "_" key of the launcher's own, a key the Hub would not accept, views in any order.
    write(t.filePath("play/mame-gun/games.json"),"\xEF\xBB\xBF{\"_defaults\":{\"x\":1},\"Bad Key\":{\"views\":{\"theatre\":{}}},"
      "\"propcycl\":{\"title\":\"Prop Cycle\",\"controls\":\"Pedal.\",\"views\":{\"native-monitor\":{\"label\":\"Native\",\"monitor\":true},\"body\":{},\"open-frame\":{\"controls\":\"Sticks.\"}}},"
      "\"timecris\":{\"title\":\"Time Crisis\",\"group\":\"gun\",\"views\":{\"theatre\":[],\"open-frame\":[]}}}");
    const auto games=ac::scripted::games(arcade);QCOMPARE(games.size(),2);
    const auto prop=games[0].toMap();QCOMPARE(prop["id"].toString(),QString("propcycl"));
    QStringList ids;for(const auto &v:prop["views"].toList())ids<<v.toMap()["id"].toString();QCOMPARE(ids,(QStringList{"open-frame","body","native-monitor"}));
    QCOMPARE(prop["views"].toList()[0].toMap()["controls"].toString(),QString("Sticks."));QCOMPARE(prop["views"].toList()[1].toMap()["controls"].toString(),QString("Pedal."));
    QCOMPARE(prop["views"].toList()[1].toMap()["label"].toString(),QString("Body motion (opt-in)"));
    QVERIFY(ac::scripted::isMonitor(prop["views"].toList()[2].toMap()));QVERIFY(!ac::scripted::isMonitor(prop["views"].toList()[0].toMap()));
    ids.clear();for(const auto &v:games[1].toMap()["views"].toList())ids<<v.toMap()["id"].toString();QCOMPARE(ids,(QStringList{"open-frame","theatre"}));
    const auto plan=ac::scripted::launchPlan(arcade,"timecris","theatre");
    QVERIFY(plan["executable"].toString().endsWith("powershell.exe",Qt::CaseInsensitive));
    QCOMPARE(plan["cwd"].toString(),QDir::toNativeSeparators(t.filePath("play/mame-gun")));
    QCOMPARE(plan["args"].toStringList(),(QStringList{"-NoProfile","-ExecutionPolicy","Bypass","-File",QDir::toNativeSeparators(arcade),"timecris","-View","theatre"}));
    QVERIFY(ac::scripted::problem(arcade,"timecris","body").contains("no such view"));
    QVERIFY(ac::scripted::problem(arcade,"ptblank2","theatre").contains("no entry"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::scripted::launchPlan(arcade,"timecris","body"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::scripted::launchPlan(arcade,"Bad Key","theatre"));
    QCOMPARE(ac::scripted::viewLabel("3d"),QString("Real 3D"));QCOMPARE(ac::scripted::viewLabel("x-new"),QString("x-new"));
  }
  // A launcher's game whose own file is gone is not found, and the path is said: a disc, ROM or file on the
  // entry or on any of its views, a relative one read beside games.json. A game naming no file is not judged.
  void launcherGameFilesAreChecked() {
    QTemporaryDir t;const auto launcher=t.filePath("play/wii/launch-wii.ps1");write(launcher,"synthetic");
    const auto here=t.filePath("discs/here.iso");write(here,"synthetic");write(t.filePath("play/wii/beside.iso"),"synthetic");
    const auto native=[](const QString &path){return QDir::toNativeSeparators(path);};
    write(t.filePath("play/wii/games.json"),QJsonDocument(QJsonObject{
      {"here",QJsonObject{{"disc",native(here)}}},{"beside",QJsonObject{{"rom","beside.iso"}}},
      {"gone",QJsonObject{{"file",native(t.filePath("discs/gone.cdi"))}}},
      {"view-gone",QJsonObject{{"views",QJsonObject{{"headset",QJsonObject{{"disc",native(here)}}},{"pointer",QJsonObject{{"disc",native(t.filePath("discs/other.iso"))}}}}}}},
      {"no-file",QJsonObject{{"views",QJsonObject{{"theatre",QJsonObject{}}}}}}}).toJson());
    QVERIFY(ac::scripted::missingMedia(launcher,{"here"}).isEmpty());
    QVERIFY(ac::scripted::missingMedia(launcher,{"beside"}).isEmpty());
    const auto gone=ac::scripted::missingMedia(launcher,{"gone"});
    QVERIFY2(gone.startsWith("Game file not found: ")&&gone.contains(native(t.filePath("discs/gone.cdi"))),qPrintable(gone));
    QVERIFY(ac::scripted::missingMedia(launcher,{"view-gone"}).contains("other.iso"));
    QVERIFY(ac::scripted::missingMedia(launcher,{"no-file"}).isEmpty());
    QVERIFY(ac::scripted::missingMedia(launcher,{"not-listed"}).isEmpty());
    QVERIFY(!ac::scripted::missingMedia(launcher,{"here","gone"}).isEmpty());
    QVERIFY(ac::scripted::missingMedia(t.filePath("nowhere/launch.ps1"),{"gone"}).isEmpty());
  }
  // A Set-up report's stamp: the same while nothing it reads changed; different once a folder, a launcher's
  // games.json or a setting changes. The order the systems were read in does not matter.
  void setupReportStampFollowsFoldersAndLaunchers() {
    QTemporaryDir t;const auto launcher=t.filePath("play/ps1/launch-ps1.ps1"),games=t.filePath("play/ps1/games.json");
    write(launcher,"synthetic");write(games,"{}");QVERIFY(QDir().mkpath(t.filePath("model3")));
    const QVariantMap settings{{"model3GameFolder",t.filePath("model3")},{"ps1Launcher",launcher}};
    const auto json=[](const QVariantMap &m){return QJsonDocument(QJsonObject::fromVariantMap(m)).toJson(QJsonDocument::Compact);};
    const auto stamp=ac::setup::reportStamp(settings,{"ps1","model3"});
    QCOMPARE(json(ac::setup::reportStamp(settings,{"model3","ps1"})),json(stamp));
    // Saved and read back, it still matches.
    QCOMPARE(json(QJsonDocument::fromJson(json(stamp)).toVariant().toMap()),json(stamp));
    QFile file(games);QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(-3600),QFileDevice::FileModificationTime));file.close();
    QVERIFY(json(ac::setup::reportStamp(settings,{"model3","ps1"}))!=json(stamp));
    auto moved=settings;moved["model3GameFolder"]=t.filePath("elsewhere");
    QVERIFY(json(ac::setup::reportStamp(moved,{"model3","ps1"}))!=json(ac::setup::reportStamp(settings,{"model3","ps1"})));
    QVERIFY(json(ac::setup::reportStamp(settings,{"model3"}))!=json(ac::setup::reportStamp(settings,{"model3","ps1"})));
  }
  // Time Crisis's native port in the views of its old launchers; our own gun's build only when it is there.
  void timeCrisisHasTheViewsOfItsOldLaunchers() {
    QTemporaryDir t;const auto program=t.filePath("tc/port/build/pc/TimeCrisisVR.exe");write(program,"synthetic");
    QStringList ids;for(const auto &v:ac::system22::views(program))ids<<v.toMap()["id"].toString();
    QCOMPARE(ids,(QStringList{"vr","frame","inbetween","monitor"}));
    write(t.filePath("tc/own-gun85/build/pc/TimeCrisisVR.exe"),"synthetic");
    ids.clear();for(const auto &v:ac::system22::views(program))ids<<v.toMap()["id"].toString();
    QCOMPARE(ids,(QStringList{"vr","frame","inbetween","own-gun","monitor"}));
    QCOMPARE(ac::system22::launchPlan(program,"vr")["env"].toMap(),QVariantMap{});
    QCOMPARE(ac::system22::launchPlan(program,"frame")["env"].toMap(),(QVariantMap{{"TCVR_SCREEN_FRAME","1"}}));
    QCOMPARE(ac::system22::launchPlan(program,"inbetween")["env"].toMap(),(QVariantMap{{"TCVR_INBETWEEN","1"}}));
    QCOMPARE(ac::system22::launchPlan(program,"monitor")["args"].toStringList(),QStringList{"--desktop"});
    QCOMPARE(ac::system22::launchPlan(program,"own-gun")["executable"].toString(),QDir::toNativeSeparators(t.filePath("tc/own-gun85/build/pc/TimeCrisisVR.exe")));
    QCOMPARE(ac::system22::launchPlan(program,"own-gun")["cwd"].toString(),QDir::toNativeSeparators(t.filePath("tc/own-gun85/build/pc")));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::system22::launchPlan(program,"wide"));
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
    write(folder+"/Death Crimson 2.cue","FILE \"dc2.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n");write(folder+"/dc2.bin",track("T23201M"));
    write(folder+"/unrelated.cdi",track("T-99999"));
    write(folder+"/no header.cdi",QByteArray(4096,'x'));
    const QVariantMap settings{{"dreamcastGameFolder",folder},{"dreamcastExecutable",program}};
    auto result=ac::setup::scanSystem("dreamcast",settings,{},{},cancel);
    // The identified US release has the full profile; the misleading filename cannot identify the other disc.
    QVERIFY(entry(result,"ready","hotd2")["detail"].toString().contains("USA release"));QVERIFY(entry(result,"ready","hotd2")["file"].toString().endsWith("first.gdi"));
    // Confidential Mission (US) has its own VR profile since 2026-10-20: found and ready.
    QVERIFY(!entry(result,"ready","confmiss").isEmpty());QVERIFY(entry(result,"notRight","confmiss").isEmpty());
    QVERIFY(entry(result,"notRight","dcrimson2")["reason"].toString().contains("no VR profile"));
    QVERIFY(!entry(result,"missing","vcop2").isEmpty());QVERIFY(!entry(result,"missing","dcox").isEmpty());
    QVERIFY(!result["note"].toString().contains("headset")); // Headset acceptance is the owner's list, not the scan's.
    // Discs the Hub does not list are still named, with their own number where it was read.
    QStringList others;for(const auto &value:result["other"].toList())others<<value.toMap()["title"].toString()+"|"+value.toMap()["detail"].toString();
    QCOMPARE(others,(QStringList{"house of the dead 2 other|disc number HDR-0000","no header|number not read","unrelated|disc number T-99999"}));
    // Removing the identified disc leaves the game missing; an unknown number cannot borrow its filename.
    QVERIFY(QFile::remove(folder+"/first.gdi"));
    result=ac::setup::scanSystem("dreamcast",settings,{},{},cancel);
    QVERIFY(entry(result,"notRight","hotd2").isEmpty());QVERIFY(!entry(result,"missing","hotd2").isEmpty());
    // Without the emulator build nothing is startable.
    result=ac::setup::scanSystem("dreamcast",{{"dreamcastGameFolder",folder}},{},{},cancel);
    QVERIFY(entry(result,"notRight","confmiss")["reason"].toString().contains("not set up"));QVERIFY(!entry(result,"notRight","confmiss")["startable"].toBool());
    QCOMPARE(QDir(folder).entryList(QDir::Files).size(),7);
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
  // The report names the files in a system's folder that matched nothing, and the games found twice, from the library result.
  void reportNamesUnrecognisedFilesAndDuplicates() {
    QTemporaryDir t;const auto root=t.filePath("roms"),folder=root+"/wii",program=t.filePath("Dolphin.exe");
    write(program,"synthetic");
    write(folder+"/umbrella.iso",wiiHeader("RBUE08"));write(root+"/other/copy.iso",wiiHeader("RBUE08"));
    write(folder+"/notes.iso",QByteArray(2048,'n'));
    auto ps1=ps2Header("SLES_512.29");ps1.replace(21*2048,5,"BOOT ");write(folder+"/ps1 disc.iso",ps1);
    write(root+"/model3/readme.zip",zip({{"x","y"}}));
    std::atomic_bool cancel=false;
    const auto library=ac::library::scan({root},{},cancel);
    const QVariantMap settings{{"wiiGameFolder",folder},{"wiiExecutable",program}};
    const auto report=ac::setup::scanSystem("wii",settings,{{"library",library}},{},cancel);
    QVERIFY(report["unrecognisedChecked"].toBool());
    QCOMPARE(report["unrecognisedMore"].toInt(),0);
    const auto files=report["unrecognised"].toList();
    QCOMPARE(files.size(),2); // The zip lies outside the folder, and an archive is not an unknown disc in a disc folder.
    QCOMPARE(files[0].toMap()["name"].toString(),QString("notes.iso"));
    QVERIFY(!files[0].toMap()["reason"].toString().isEmpty());
    QCOMPARE(files[1].toMap()["name"].toString(),QString("ps1 disc.iso"));
    QCOMPARE(files[1].toMap()["code"].toString(),QString("otherSystem")); // Another system's disc in this folder.
    const auto twice=report["duplicates"].toList();
    QCOMPARE(twice.size(),1);
    QCOMPARE(twice[0].toMap()["game"].toString(),QString("umbrella"));
    QCOMPARE(twice[0].toMap()["count"].toInt(),2);
    QVERIFY(twice[0].toMap()["sameDisc"].toBool());
    QVERIFY(!report["duplicateRule"].toString().isEmpty());
    // A library that did not look through the folder says nothing about it, and says so.
    QTemporaryDir other;write(other.filePath("x/a.iso"),wiiHeader("RBUE08"));
    const auto elsewhere=ac::library::scan({other.path()},{},cancel);
    const auto unchecked=ac::setup::scanSystem("wii",settings,{{"library",elsewhere}},{},cancel);
    QVERIFY(!unchecked["unrecognisedChecked"].toBool());QVERIFY(unchecked["unrecognised"].toList().isEmpty());
    // Without a library result the report is as it was.
    const auto plain=ac::setup::scanSystem("wii",settings,{},{},cancel);
    QVERIFY(!plain.contains("unrecognised"));QVERIFY(!plain.contains("duplicates"));
    // An arcade folder lists unknown zips, not discs; the pinball folder lists neither.
    const QVariantMap arcade{{"model3GameFolder",root+"/model3"}};
    const auto model3=ac::setup::scanSystem("model3",arcade,{{"library",library}},{},cancel);
    QCOMPARE(model3["unrecognised"].toList().size(),1);QCOMPARE(model3["unrecognised"].toList()[0].toMap()["code"].toString(),QString("unknownArchive"));
    const QVariantMap tables{{"tables",QVariantList{}}};
    QVERIFY(!ac::setup::scanSystem("pinball",{{"pinballFolder",root+"/model3"}},{{"library",library},{"pinballScan",tables}},{},cancel).contains("unrecognised"));
  }
  // A wrong or empty folder is judged by what is inside a few of its files, never by what the folder or files are called.
  void folderChecksAndSuggestionsReadTheFilesNotTheNames() {
    QTemporaryDir t;const auto root=t.filePath("games");
    write(root+"/stuff one/a.iso",ps2Header("SLUS_212.74"));write(root+"/stuff one/b.iso",ps2Header("SLUS_206.32"));write(root+"/stuff one/c.iso",ps2Header("SLES_512.29"));
    write(root+"/stuff two/x.iso",wiiHeader("RBUE08"));write(root+"/stuff two/y.iso",wiiHeader("R3ME01"));
    write(root+"/stuff two/PS2 Game (SLUS-21274).iso",wiiHeader("RBUP08")); // Named for PS2, holds a Wii disc.
    write(root+"/ps2/z.iso",wiiHeader("SBDE08")); // A folder named for PS2 that holds a Wii disc.
    QVERIFY(QDir().mkpath(root+"/chosen"));
    std::atomic_bool cancel=false;
    const auto ps2=ac::setup::suggestFolders("ps2",root+"/chosen",{root+"/chosen"},{},cancel);
    QCOMPARE(ps2.size(),1); // Folders only some of whose files fit are left out when one fits well.
    QCOMPARE(ps2[0].toMap()["folder"].toString(),root+"/stuff one");
    QCOMPARE(ps2[0].toMap()["matched"].toInt(),3);QCOMPARE(ps2[0].toMap()["checked"].toInt(),3);
    QCOMPARE(ps2[0].toMap()["detail"].toString(),QString("3 of the 3 files checked look like Sony PlayStation 2 games."));
    for(const auto &value:ps2){const auto folder=value.toMap()["folder"].toString();QVERIFY2(folder!=root+"/ps2"&&folder!=root+"/stuff two"&&folder!=root+"/chosen",qPrintable(folder));}
    const auto wii=ac::setup::suggestFolders("wii",root+"/chosen",{root+"/chosen"},{},cancel);
    QCOMPARE(wii.size(),2);
    QCOMPARE(wii[0].toMap()["folder"].toString(),root+"/stuff two");QCOMPARE(wii[0].toMap()["matched"].toInt(),3);
    QCOMPARE(wii[1].toMap()["folder"].toString(),root+"/ps2"); // The name says nothing; its file is a Wii disc.
    // The library's own roots are looked through as well.
    QTemporaryDir distant;write(distant.filePath("somewhere/deep/d.iso"),ps2Header("SLUS_212.74"));
    const auto viaLibrary=ac::setup::suggestFolders("ps2",QString(),{},{distant.path()},cancel);
    QCOMPARE(viaLibrary.size(),1);QCOMPARE(viaLibrary[0].toMap()["folder"].toString(),distant.path()+"/somewhere/deep"); // The most specific folder holding them.
    // A system of unreadable kinds, an unknown system, nothing to find, and a cancelled search give nothing.
    QVERIFY(ac::setup::suggestFolders("system22",QString(),{},{root},cancel).isEmpty());
    QVERIFY(ac::setup::suggestFolders("not-a-system",QString(),{},{root},cancel).isEmpty());
    QVERIFY(ac::setup::suggestFolders("dreamcast",root+"/chosen",{root+"/chosen"},{},cancel).isEmpty());
    std::atomic_bool stopped=true;
    QVERIFY(ac::setup::suggestFolders("ps2",root+"/chosen",{root+"/chosen"},{},stopped).isEmpty());
    // The check of one folder says how many of its files are the system's.
    const auto right=ac::setup::folderCheck("ps2",root+"/stuff one",cancel);
    QCOMPARE(right["matched"].toInt(),3);QCOMPARE(right["files"].toInt(),3);
    const auto wrong=ac::setup::folderCheck("ps2",root+"/stuff two",cancel);
    QCOMPARE(wrong["checked"].toInt(),3);QCOMPARE(wrong["matched"].toInt(),0);
    QCOMPARE(wrong["detail"].toString(),QString("None of the 3 files checked look like Sony PlayStation 2 games."));
    const auto empty=ac::setup::folderCheck("ps2",root+"/chosen",cancel);
    QCOMPARE(empty["files"].toInt(),0);QVERIFY(empty["detail"].toString().startsWith("No files"));
    QCOMPARE(ac::setup::folderCheck("ps2",root+"/gone",cancel)["files"].toInt(),0);
  }
  // Pinball tables, cartridges and arcade sets are checked by their bytes or set name as well.
  void folderChecksCoverTablesCartridgesAndArcadeSets() {
    QTemporaryDir t;const auto root=t.filePath("x");
    write(root+"/tables/one.vpx",QByteArray::fromHex("d0cf11e0a1b11ae1")+QByteArray(64,'\0'));write(root+"/tables/two.vpx","not a table");
    write(root+"/carts/a.z64",QByteArray::fromHex("80371240")+QByteArray(64,'\0'));write(root+"/carts/b.z64",QByteArray(64,'\0'));
    write(root+"/model3/scud.zip",zip({{"epr-19731.13","a"}}));write(root+"/model3/pacman.zip",zip({{"pacman.6e","c"}}));
    std::atomic_bool cancel=false;
    const auto tables=ac::setup::folderCheck("pinball",root+"/tables",cancel);QCOMPARE(tables["matched"].toInt(),1);QCOMPARE(tables["checked"].toInt(),2);
    const auto carts=ac::setup::folderCheck("n64",root+"/carts",cancel);QCOMPARE(carts["matched"].toInt(),1);QCOMPARE(carts["checked"].toInt(),2);
    const auto arcade=ac::setup::folderCheck("model3",root+"/model3",cancel);QCOMPARE(arcade["matched"].toInt(),1);QCOMPARE(arcade["checked"].toInt(),2); // Only the set the Hub plays.
  }
};
QTEST_APPLESS_MAIN(SetupTest)
#include "SetupTest.moc"
