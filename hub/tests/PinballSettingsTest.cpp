// SPDX-License-Identifier: GPL-3.0-only
#include "core/pinball/Pinball.h"
#include "core/pinball/Gamepad.h"
#include "core/pinball/Roms.h"
#include "core/pinball/Build.h"
#include "core/launch/UniversalControls.h"
#include "core/install/Support.h"
#include <QtTest>
#include <QDir>
#include <QTemporaryDir>
using ac::pinball::vrOnSettings;
class PinballSettingsTest : public QObject {
  Q_OBJECT
private slots:
  void uncertainRomsStayStartableButErrorsAndKnownMissingRomsDoNot() {
    for(const auto &state:{"ready","unknown","needs no ROM"})QVERIFY(ac::pinball::canStart({{"romState",state}}));
    QVERIFY(!ac::pinball::canStart({{"romState","ROM missing"}}));
    QVERIFY(!ac::pinball::canStart({{"romState","ready"},{"error","Unreadable table"}}));
    const auto locked=ac::pinball::scriptMetadata("LoadVPM \"x\"",true);
    QVERIFY(locked.value("romReason").toString().contains("locked"));QVERIFY(ac::pinball::canStart(locked));
  }
  void nestedRomsAndAliasEditsInvalidateTheFingerprint() {
    QTemporaryDir folder;const auto roms=folder.filePath("roms"),vpm=folder.filePath("VPinMAME");
    QVERIFY(QDir().mkpath(roms+"/nested"));QVERIFY(QDir().mkpath(vpm));
    ac::install::atomicWrite(roms+"/nested/fixture.zip","fixture");
    ac::install::atomicWrite(vpm+"/VPMAlias.txt","alias,fixture\n");
    const auto first=ac::pinball::fingerprint(folder.path(),{},roms,vpm);
    ac::install::atomicWrite(vpm+"/VPMAlias.txt","alias,missing\n");
    const auto second=ac::pinball::fingerprint(folder.path(),{},roms,vpm);QVERIFY(first!=second);
    QVERIFY(QFile::remove(roms+"/nested/fixture.zip"));
    QVERIFY(second!=ac::pinball::fingerprint(folder.path(),{},roms,vpm));
  }
  void emptySettingsAskForTheHeadset() {
    QCOMPARE(vrOnSettings({}),QByteArray("[PlayerVR]\nAskToTurnOn = 0\n"));
  }
  void autodetectAndOffBecomeOn() {
    QCOMPARE(vrOnSettings("[PlayerVR]\nAskToTurnOn = 1\nTableX = 2.5\n"),QByteArray("[PlayerVR]\nAskToTurnOn = 0\nTableX = 2.5\n"));
    QCOMPARE(vrOnSettings("[playervr]\nasktoturnon=2\n"),QByteArray("[playervr]\nAskToTurnOn = 0\n"));
  }
  void otherSectionsCommentsAndLineEndingsAreKept() {
    const QByteArray before="; a table's own tweaks\r\n[Player]\r\nAskToTurnOn = 5\r\nBallTrail = 1\r\n\r\n[PlayerVR]\r\n; view\r\nOrientation = 12\r\n[TableOverride]\r\nDifficulty = 0.5\r\n";
    const QByteArray after="; a table's own tweaks\r\n[Player]\r\nAskToTurnOn = 5\r\nBallTrail = 1\r\n\r\n[PlayerVR]\r\nAskToTurnOn = 0\r\n; view\r\nOrientation = 12\r\n[TableOverride]\r\nDifficulty = 0.5\r\n";
    QCOMPARE(vrOnSettings(before),after);
  }
  void aMissingSectionIsAppended() {
    QCOMPARE(vrOnSettings("[Player]\nBallTrail = 1"),QByteArray("[Player]\nBallTrail = 1\n\n[PlayerVR]\nAskToTurnOn = 0\n"));
  }
  void editingTwiceChangesNothingMore() {
    for(const QByteArray &text:{QByteArray(),QByteArray("[Player]\nBallTrail = 1\n"),QByteArray("[PlayerVR]\r\nAskToTurnOn = 1\r\n")})
      QCOMPARE(vrOnSettings(vrOnSettings(text)),vrOnSettings(text));
  }
  void ownedFileStartsFromTheTablesOwnSettingsWhichStayUntouched() {
    QTemporaryDir folder;const auto library=folder.filePath("library"),owned=folder.filePath("hub/user/pinball/table.ini"),seed=library+"/table.ini";
    QVERIFY(QDir().mkpath(library));
    const QByteArray own="[TableOverride]\nDifficulty = 0.5\n";ac::install::atomicWrite(seed,own);
    const QVariantMap plan{{"tableSettings",owned},{"tableSettingsSeed",seed}};
    ac::pinball::writeVrSettings(plan);
    QCOMPARE(ac::install::readBytes(owned),QByteArray("[TableOverride]\nDifficulty = 0.5\n\n[PlayerVR]\nAskToTurnOn = 0\n"));
    QCOMPARE(ac::install::readBytes(seed),own);QCOMPARE(QDir(library).entryList(QDir::Files),QStringList{"table.ini"});
    // Later choices made in the table are kept: only the headset line is put back.
    ac::install::atomicWrite(owned,"[TableOverride]\nDifficulty = 0.9\n[PlayerVR]\nAskToTurnOn = 1\nTableX = 3\n");
    ac::pinball::writeVrSettings(plan);
    QCOMPARE(ac::install::readBytes(owned),QByteArray("[TableOverride]\nDifficulty = 0.9\n[PlayerVR]\nAskToTurnOn = 0\nTableX = 3\n"));
  }
  void desktopTurnsVrOff() {
    QCOMPARE(ac::pinball::vrModeSettings("[PlayerVR]\nAskToTurnOn = 0\n",2),QByteArray("[PlayerVR]\nAskToTurnOn = 2\n"));
    QCOMPARE(ac::pinball::vrModeSettings({},2),QByteArray("[PlayerVR]\nAskToTurnOn = 2\n"));
    QTemporaryDir folder;const auto owned=folder.filePath("hub/user/pinball/table.ini");
    ac::pinball::writeVrSettings({{"tableSettings",owned},{"vrMode",2}});QCOMPARE(ac::install::readBytes(owned),QByteArray("[PlayerVR]\nAskToTurnOn = 2\n"));
    ac::pinball::writeVrSettings({{"tableSettings",owned},{"vrMode",0}});QCOMPARE(ac::install::readBytes(owned),QByteArray("[PlayerVR]\nAskToTurnOn = 0\n"));
  }
  void gamepadMappingsAreAddedAndTheUsersAreKept() {
    const QString uid="SDLJoy_030000005e0400008e02000000007801_1";
    const QVariantList pads{QVariantMap{{"uid",uid},{"name","Synthetic pad"},{"elements",QVariantMap{{"A",0},{"B",1},{"X",2},{"Y",3},{"LeftShoulder",4},{"RightShoulder",5},{"Back",6},{"Start",7},
      {"LeftStick",8},{"RightStick",9},{"DPadLeft",0x100},{"DPadRight",0x101},{"DPadUp",0x102},{"DPadDown",0x103},{"LeftX",0x200},{"LeftY",0x201},{"LeftTrigger",0x202},{"RightX",0x203},{"RightY",0x204},{"RightTrigger",0x205}}}}};
    const QByteArray mine="; mine\r\n[Player]\r\nBallTrail = 1\r\n\r\n[Input]\r\nNudgeSensorCount = 1\r\nPlungerSensorCount = 1\r\nDevices = Key;OpenXR\r\n"
      "Mapping.LeftFlipper = Key;225 | OpenXR;16;o;0.300000\r\nMapping.Start = Key;30 | OpenXR;10\r\nMapping.Plunger0.Position = OpenXR;23;P;0.000000;-1.000000;1.000000\r\n"
      "Mapping.Nudge0.Type = 0\r\nMapping.Nudge0.X = OpenXR;20;P;0.000000;1.000000;1.000000\r\n\r\n[PlayerVR]\r\nAskToTurnOn = 1\r\n";
    const auto out=ac::pinball::gamepadSettings(mine,pads);const auto text=QString::fromUtf8(out);
    // Every action of the layout, with the user's own bindings first.
    const QMap<QString,QString> expected{
      {"LeftFlipper","Key;225 | OpenXR;16;o;0.300000 | "+uid+";4 | "+uid+";514;o;-0.300000"},
      {"RightFlipper",uid+";5 | "+uid+";517;o;-0.300000"},
      {"LeftStagedFlipper",uid+";4 | "+uid+";514;o;0.300000"},{"RightStagedFlipper",uid+";5 | "+uid+";517;o;0.300000"},
      {"LaunchBall",uid+";0"},{"Start","Key;30 | OpenXR;10 | "+uid+";7"},{"Credit1",uid+";6"},{"ExitGame",uid+";7 & "+uid+";6"},
      {"LeftMagna",uid+";8"},{"RightMagna",uid+";9"},{"UIUp",uid+";258"},{"UIDown",uid+";259"},{"UILeft",uid+";256"},{"UIRight",uid+";257"},
      {"Plunger1.Position",uid+";516;P;0.100000;1.000000;1.000000"},{"Nudge1.X",uid+";512;P;0.100000;1.000000;1.000000"},{"Nudge1.Y",uid+";513;P;0.100000;1.000000;1.000000"},{"Nudge1.Type","0"}};
    for(auto it=expected.cbegin();it!=expected.cend();++it)QVERIFY2(text.contains("Mapping."+it.key()+" = "+it.value()+"\r\n"),qPrintable(it.key()));
    for(const auto &line:QStringList{"Devices = Key;OpenXR;"+uid,"Device."+uid+".NoAutoLayout = 1","Device."+uid+".Type = 2","PlungerSensorCount = 2","NudgeSensorCount = 2",
                          QString("Mapping.Plunger0.Position = OpenXR;23;P;0.000000;-1.000000;1.000000"),QString("; mine\r\n[Player]\r\nBallTrail = 1\r\n"),QString("[PlayerVR]\r\nAskToTurnOn = 1\r\n")})
      QVERIFY2(text.contains(line),qPrintable(line));
    QVERIFY((!QByteArray(out).replace("\r\n","").contains('\n'))); // Line endings kept.
    // Made again from its own output, nothing is doubled.
    QCOMPARE(ac::pinball::gamepadSettings(out,pads),out);
    // No [Input] section yet: one is added. No gamepads: unchanged.
    QVERIFY(ac::pinball::gamepadSettings("[Player]\nBallTrail = 1\n",pads).contains("\n\n[Input]\nDevices = "+uid.toUtf8()+"\n"));
    QCOMPARE(ac::pinball::gamepadSettings(mine,{}),mine);
    // A gamepad name cannot break the file.
    auto odd=pads[0].toMap();odd["name"]="Pad\r\n[PlayerVR]\r\nAskToTurnOn = 0";QVERIFY(!ac::pinball::gamepadSettings(mine,{odd}).contains("AskToTurnOn = 0"));
    QCOMPARE(ac::pinball::gamepadLayout().size(),9);
  }
  void theHeadsetGetsTheUniversalLayer() {
    // VPX 10.8's own VR controller numbers: left stick click 12, right 13, A 10, B 11, X 8, Y 9, menus 2 and 3.
    const QByteArray mine="[Input]\nDevices = Key;OpenXR\nMapping.LeftMagna = Key;224 | OpenXR;4 | OpenXR;12\nMapping.LaunchBall = Key;40 | OpenXR;13\nMapping.Credit1 = Key;34 | OpenXR;11\n"
      "Mapping.Start = Key;30 | OpenXR;10\nMapping.ExitGame = Key;41 | OpenXR;9 | OpenXR;9 & OpenXR;12\nMapping.InGameUI = Key;69 | OpenXR;8 | OpenXR;2\nMapping.VRCenter = Key;93\n"
      "Mapping.VRControllerCalibration = OpenXR;12\nDevice.OpenXR.Element2 = 2;B;user_hand_left_input_menu_click\nDevice.OpenXR.Element12 = 12;B;user_hand_left_input_thumbstick_click\n\n[PlayerVR]\nAskToTurnOn = 0\n";
    const auto out=ac::pinball::headsetControls(mine);const auto text=QString::fromUtf8(out);
    const QMap<QString,QString> expected{
      {"Start","Key;30 | OpenXR;12"},{"Credit1","Key;34 | OpenXR;13"},{"LaunchBall","Key;40 | OpenXR;10"},{"ExitGame","Key;41"},
      {"VRControllerCalibration","OpenXR;12 & OpenXR;13"},{"VRCenter","Key;93"},
      {"LeftMagna","Key;224 | OpenXR;4"},{"InGameUI","Key;69 | OpenXR;8"}}; // Grips and X stay; the stick click and the menu button go.
    for(auto it=expected.cbegin();it!=expected.cend();++it)QVERIFY2(text.contains("Mapping."+it.key()+" = "+it.value()+"\n"),qPrintable(it.key()));
    QVERIFY(text.contains("Devices = Key;OpenXR\n"));QVERIFY(text.contains("Device.OpenXR.NoAutoLayout = 1\n"));QVERIFY(text.contains("Device.OpenXR.Type = 4\n"));
    QVERIFY(text.contains("\n\n[PlayerVR]\nAskToTurnOn = 0\n")); // The other sections stay as they were.
    QCOMPARE(ac::pinball::headsetControls(out),out);
    QCOMPARE(ac::pinball::exitOnChord(mine),out);
    // No mappings, no [Input] section and no OpenXR device yet: the layer is written with VPX's keyboard keys kept.
    const auto bare=QString::fromUtf8(ac::pinball::headsetControls("[Player]\nX = 1\n"));
    QVERIFY(bare.startsWith("[Player]\nX = 1\n\n[Input]\n"));
    for(const auto &line:QStringList{"Mapping.Start = Key;30 | OpenXR;12","Mapping.Credit1 = Key;34 | OpenXR;13","Mapping.LaunchBall = Key;40 | OpenXR;10","Mapping.ExitGame = Key;41",
                                     "Mapping.VRControllerCalibration = OpenXR;12 & OpenXR;13","Devices = Key;OpenXR"})
      QVERIFY2(bare.contains(line+"\n"),qPrintable(line));
    // The numbers VPX lists win, and a CRLF file stays CRLF.
    const auto odd=ac::pinball::headsetControls("[Input]\r\nDevice.OpenXR.Element5 = 5;B;user_hand_left_input_thumbstick_click\r\nDevice.OpenXR.Element6 = 6;B;user_hand_right_input_thumbstick_click\r\n");
    QVERIFY(odd.contains("Mapping.Start = Key;30 | OpenXR;5\r\n"));QVERIFY(odd.contains("Mapping.VRControllerCalibration = OpenXR;5 & OpenXR;6\r\n"));
    QVERIFY(!QByteArray(odd).replace("\r\n","").contains('\n'));
  }
  // The patched AladdinsCastle VPX is told by its product version ending in "-acvr"; the program is never run.
  void thePatchedBuildIsToldByItsProductVersion() {
    QVERIFY(ac::pinball::isPatchedVersion("10.8.1.5436.af26b2d93-acvr"));
    QVERIFY(ac::pinball::isPatchedVersion(" 10.8.1.5436.af26b2d93-ACVR "));
    QVERIFY(!ac::pinball::isPatchedVersion("10.8.1.5436.af26b2d93"));
    QVERIFY(!ac::pinball::isPatchedVersion("10.8.1.5436.acvr.af26b2d93"));
    QVERIFY(!ac::pinball::isPatchedVersion({}));
    QTemporaryDir folder;
    QCOMPARE(ac::pinball::productVersion(folder.filePath("missing.exe")),QString());
    ac::install::atomicWrite(folder.filePath("plain.exe"),"not a program");
    QCOMPARE(ac::pinball::productVersion(folder.filePath("plain.exe")),QString());
    QVERIFY(!ac::pinball::isPatchedBuild(folder.filePath("plain.exe")));
#ifdef Q_OS_WIN
    // A real version resource is read: Windows' own kernel32.dll has a product version and is not the patched VPX.
    const auto kernel=QDir(qEnvironmentVariable("SystemRoot")).filePath("System32/kernel32.dll");
    const auto version=ac::pinball::productVersion(kernel);
    QVERIFY2(version.contains('.'),qPrintable(version));
    QVERIFY(!ac::pinball::isPatchedBuild(kernel));
#endif
  }
  // The Hub's pinball text follows the build: the standard build is left through the Hub panel and its table adjust
  // toggles; the patched build quits on a 3 s hold and moves the table only while both clicks are held.
  void thePinballTextFollowsTheBuild() {
    const auto standard=ac::universal::pinballControls(false),patched=ac::universal::pinballControls(true);
    QCOMPARE(ac::universal::pinballControls(),standard);
    QVERIFY(standard.contains("click both sticks again to keep it there"));QVERIFY(standard.contains("Exit game"));
    QVERIFY(standard.contains("only in the patched AladdinsCastle build"));
    QVERIFY(patched.contains("let go to keep it"));QVERIFY(patched.contains("Hold both stick clicks for 3 seconds with your hands still = quit and save."));
    QVERIFY(!patched.contains("standard"));
    const auto plain=ac::pinball::launchGuidance(false).value("help").toString(),acvr=ac::pinball::launchGuidance(true).value("help").toString();
    for(const auto &help:{plain,acvr}){QVERIFY(help.contains("normal settings"));QVERIFY(help.contains("scores"));QVERIFY(help.contains("Esc"));}
    QVERIFY(plain.contains("only in the patched"));QVERIFY(acvr.contains("This is the patched AladdinsCastle build"));
    QCOMPARE(ac::pinball::launchGuidance(true).value("patchedBuild").toBool(),true);
  }
  void aPlanWithoutAnOwnedFileWritesNothing() {
    QTemporaryDir folder;ac::pinball::writeVrSettings({{"args",QStringList{"-Play","x"}}});
    QVERIFY(QDir(folder.path()).isEmpty());
  }
};
QTEST_APPLESS_MAIN(PinballSettingsTest)
#include "PinballSettingsTest.moc"
