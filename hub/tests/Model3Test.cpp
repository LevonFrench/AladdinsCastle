// SPDX-License-Identifier: GPL-3.0-only
#include "core/model3/Model3.h"
#include "core/install/Install.h"
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
namespace {
void touch(const QString &path) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("synthetic");
}
// A synthetic emulator tree shaped like a source build: the program sits a few
// folders below the folder that holds Config/Games.xml.
struct Fixture {
  QTemporaryDir folder;
  QString emulator=folder.filePath("emu"),program=folder.filePath("emu/out/x64/build/program.exe"),media=folder.filePath("my games");
  Fixture() {
    touch(program); touch(emulator+"/Config/Games.xml");
    touch(media+"/scud.zip"); touch(media+"/swtrilgy.zip"); touch(media+"/srally2.zip");
  }
};
QString native(const QString &path){return QDir::toNativeSeparators(path);}
}
class Model3Test : public QObject {
  Q_OBJECT
private slots:
  void listsTenGamesAndThreeViews() {
    QCOMPARE(ac::model3::games().size(),11); // the first ten and Sega Rally 2 DX (srally2x, 2026-10-20)
    QStringList ids; for(const auto &view:ac::model3::views())ids<<view.toMap().value("id").toString();
    QCOMPARE(ids,(QStringList{"screen","wide","inside"}));
    QCOMPARE(ac::model3::viewLabel("inside"),QString("Inside the game"));
  }
  void screenPlanMatchesTheReferenceLauncher() {
    Fixture f;
    const auto plan=ac::model3::launchPlan("scud","screen",f.program,f.media);
    QCOMPARE(plan.value("executable").toString(),native(f.program));
    QCOMPARE(plan.value("cwd").toString(),native(f.emulator));
    QCOMPARE(plan.value("args").toStringList(),(QStringList{native(f.media+"/scud.zip"),"-window","-res=1440,1080","-ss=2","-input-system=xinput"}));
    QCOMPARE(plan.value("env").toMap(),(QVariantMap{{"ACVR_XR","1"},{"ACVR_EYE_SEP","0.065"},{"ACVR_WORLD_SCALE","1"},{"ACVR_XR_DISTANCE","2.5"},{"ACVR_XR_SIZE","1"}}));
  }
  void widePlanAddsTheWiderCamera() {
    Fixture f;
    const auto screen=ac::model3::launchPlan("scud","screen",f.program,f.media),wide=ac::model3::launchPlan("scud","wide",f.program,f.media);
    QCOMPARE(wide.value("args"),screen.value("args"));
    auto expected=screen.value("env").toMap(); expected["ACVR_FOV_SCALE"]="1.5"; expected["ACVR_NO_CULL"]="1";
    QCOMPARE(wide.value("env").toMap(),expected);
  }
  // Sega Rally 2's measured stereo, in both screen views; Inside the game and every other game unchanged.
  void segaRally2HasItsOwnStereo() {
    Fixture f;
    for(const auto &view:{"screen","wide"}) {
      auto expected=ac::model3::launchPlan("scud",view,f.program,f.media).value("env").toMap();
      QCOMPARE(expected.value("ACVR_EYE_SEP").toString(),QString("0.065"));QVERIFY(!expected.contains("ACVR_CONVERGENCE"));
      expected["ACVR_EYE_SEP"]="0.02";expected["ACVR_CONVERGENCE"]="3.6";
      QCOMPARE(ac::model3::launchPlan("srally2",view,f.program,f.media).value("env").toMap(),expected);
    }
    QCOMPARE(ac::model3::launchPlan("srally2","inside",f.program,f.media).value("env"),ac::model3::launchPlan("scud","inside",f.program,f.media).value("env"));
  }
  void insidePlanIsHeadTracked() {
    Fixture f;
    const auto plan=ac::model3::launchPlan("scud","inside",f.program,f.media);
    QCOMPARE(plan.value("args").toStringList(),(QStringList{native(f.media+"/scud.zip"),"-window","-res=1344,1344","-stretch","-ss=2","-input-system=xinput"}));
    QCOMPARE(plan.value("env").toMap(),(QVariantMap{{"ACVR_XR","1"},{"ACVR_VR","1"},{"ACVR_WORLD_SCALE","1"},{"ACVR_HUD_DISTANCE","1.6"},{"ACVR_HUD_WIDTH","1.6"}}));
  }
  void startUpScriptOnlyForTheGameThatNeedsIt() {
    Fixture f;
    for(const auto &view:{"screen","wide","inside"}) {
      QCOMPARE(ac::model3::launchPlan("swtrilgy",view,f.program,f.media).value("env").toMap().value("ACVR_INPUT_SCRIPT").toString(),QString("300-312:TestA:1;700-712:TestA:1"));
      QVERIFY(!ac::model3::launchPlan("scud",view,f.program,f.media).value("env").toMap().contains("ACVR_INPUT_SCRIPT"));
    }
  }
  void workingFolderIsTheProgramFolderForAPlainInstall() {
    QTemporaryDir folder; touch(folder.filePath("program.exe")); touch(folder.filePath("Config/Games.xml"));
    QCOMPARE(ac::model3::workingFolder(folder.filePath("program.exe")),QDir(folder.path()).absolutePath());
  }
  void problemsAreNamedAndRefuseTheLaunch() {
    Fixture f;
    QVERIFY(ac::model3::problem("scud",f.program,f.media).isEmpty());
    QVERIFY(ac::model3::problem("scud",QString(),f.media).contains("Choose it in Set-up"));
    QVERIFY(ac::model3::problem("scud",f.folder.filePath("gone.exe"),f.media).contains("not found"));
    QVERIFY(ac::model3::problem("scud",f.program,QString()).contains("Game folder"));
    QVERIFY(ac::model3::problem("daytona2",f.program,f.media).contains("daytona2.zip"));
    QVERIFY(!ac::model3::problem("not-a-game",f.program,f.media).isEmpty());
    QTemporaryDir bare; touch(bare.filePath("program.exe"));
    QVERIFY(ac::model3::problem("scud",bare.filePath("program.exe"),f.media).contains("Config"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::model3::launchPlan("daytona2","screen",f.program,f.media));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::model3::launchPlan("scud","sideways",f.program,f.media));
  }
  // Each game's own views, as the old numbered launchers offered them.
  void eachGameHasTheViewsOfItsOldLaunchers() {
    Fixture f;
    const auto ids=[](const QString &game){QStringList list;for(const auto &v:ac::model3::views(game))list<<v.toMap().value("id").toString();return list;};
    QCOMPARE(ids("scud"),(QStringList{"screen","open-frame","wide","depth","inside","inside-wheel"}));
    QCOMPARE(ids("srally2"),(QStringList{"screen","open-frame","wide","inside","inside-wheel","sbs-monitor"}));
    QCOMPARE(ids("swtrilgy"),(QStringList{"screen","open-frame","wide","inside","inside-stick"}));
    QCOMPARE(ids("vf3"),(QStringList{"screen","open-frame","wide","inside"}));
    QCOMPARE(ids("srally2x"),(QStringList{"screen","open-frame","inside-wheel"}));
    int count=0;for(const auto &game:ac::model3::games())count+=ac::model3::views(game.toMap().value("id").toString()).size();
    QCOMPARE(count,54); // the 47 archived launchers, plus Wide screen, which the Hub already offered, for seven more games
    // Open frame: Supermodel's wide screen at 16:9.
    QCOMPARE(ac::model3::launchPlan("scud","open-frame",f.program,f.media).value("args").toStringList(),
             (QStringList{native(f.media+"/scud.zip"),"-window","-res=1920,1080","-ss=2","-wide-screen","-input-system=xinput"}));
    // The depth trial: the measured convergence and separation.
    auto env=ac::model3::launchPlan("scud","depth",f.program,f.media).value("env").toMap();
    QCOMPARE(env,(QVariantMap{{"ACVR_XR","1"},{"ACVR_WORLD_SCALE","1"},{"ACVR_XR_DISTANCE","2.5"},{"ACVR_XR_SIZE","1"},{"ACVR_CONVERGENCE","3.8"},{"ACVR_EYE_SEP","0.02"}}));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::model3::launchPlan("swtrilgy","depth",f.program,f.media));
    // Inside with the wheel drawn: this view is the opt-in for two-grip steering, so it alone sets ACVR_WHEEL_GRAB; Star Wars Trilogy's stick; the side-by-side window keeps the default separation.
    env=ac::model3::launchPlan("scud","inside-wheel",f.program,f.media).value("env").toMap();
    QCOMPARE(env.value("ACVR_CONTROLS").toString(),QString("1"));QCOMPARE(env.value("ACVR_WHEEL_GRAB").toString(),QString("1"));QCOMPARE(env.value("ACVR_VR").toString(),QString("1"));
    for(const auto *other:{"screen","open-frame"})QVERIFY(!ac::model3::launchPlan("scud",other,f.program,f.media).value("env").toMap().contains("ACVR_WHEEL_GRAB"));
    env=ac::model3::launchPlan("swtrilgy","inside-stick",f.program,f.media).value("env").toMap();
    QCOMPARE(env.value("ACVR_STICK_GRAB").toString(),QString("1"));QCOMPARE(env.value("ACVR_INPUT_SCRIPT").toString(),QString("300-312:TestA:1;700-712:TestA:1"));
    const auto sbs=ac::model3::launchPlan("srally2","sbs-monitor",f.program,f.media);
    QCOMPARE(sbs.value("env").toMap(),(QVariantMap{{"ACVR_STEREO","1"},{"ACVR_EYE_SEP","0.065"},{"ACVR_WORLD_SCALE","1"},{"ACVR_CONVERGENCE","2.5"}}));
    QCOMPARE(sbs.value("args").toStringList(),(QStringList{native(f.media+"/srally2.zip"),"-window","-res=1280,960","-input-system=xinput"}));
    QVERIFY(ac::model3::isMonitorView("sbs-monitor"));QVERIFY(!ac::model3::isMonitorView("screen"));
    // Sega Rally 2 DX: its own folder beside the game folder, Supermodel's set name, its CPU speed, Sega Rally 2's stereo.
    touch(f.folder.filePath("games-dx/srally2x.zip"));
    QVERIFY(ac::model3::problem("srally2x",f.program,f.media).isEmpty());
    QCOMPARE(ac::model3::setName("srally2x"),QString("srally2dx"));QCOMPARE(ac::model3::setName("scud"),QString("scud"));
    const auto dx=ac::model3::launchPlan("srally2x","open-frame",f.program,f.media);
    QCOMPARE(dx.value("args").toStringList(),(QStringList{native(f.folder.filePath("games-dx/srally2x.zip")),"-window","-res=1920,1080","-ss=2","-wide-screen","-ppc-frequency=100","-input-system=xinput"}));
    QCOMPARE(dx.value("env").toMap().value("ACVR_CONVERGENCE").toString(),QString("3.6"));
    QVERIFY_THROWS_EXCEPTION(ac::install::Error,ac::model3::launchPlan("srally2x","inside",f.program,f.media));
  }
  void planningWritesNothing() {
    Fixture f;
    const auto before=QDir(f.folder.path()).entryList(QDir::AllEntries|QDir::NoDotAndDotDot,QDir::Name),
               emulatorBefore=QDir(f.emulator).entryList(QDir::AllEntries|QDir::NoDotAndDotDot,QDir::Name),
               mediaBefore=QDir(f.media).entryList(QDir::AllEntries|QDir::NoDotAndDotDot,QDir::Name);
    ac::model3::launchPlan("scud","inside",f.program,f.media);
    QCOMPARE(QDir(f.folder.path()).entryList(QDir::AllEntries|QDir::NoDotAndDotDot,QDir::Name),before);
    QCOMPARE(QDir(f.emulator).entryList(QDir::AllEntries|QDir::NoDotAndDotDot,QDir::Name),emulatorBefore);
    QCOMPARE(QDir(f.media).entryList(QDir::AllEntries|QDir::NoDotAndDotDot,QDir::Name),mediaBefore);
  }
};
QTEST_APPLESS_MAIN(Model3Test)
#include "Model3Test.moc"
