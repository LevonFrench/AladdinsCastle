// SPDX-License-Identifier: GPL-3.0-only
#include "core/pinball/Pinball.h"
#include "core/pinball/Gamepad.h"
#include "core/install/Support.h"
#include <QtTest>
#include <QDir>
#include <QTemporaryDir>
using ac::pinball::vrOnSettings;
class PinballSettingsTest : public QObject {
  Q_OBJECT
private slots:
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
    for(const QByteArray text:{QByteArray(),QByteArray("[Player]\nBallTrail = 1\n"),QByteArray("[PlayerVR]\r\nAskToTurnOn = 1\r\n")})
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
  void aSingleYTapNoLongerExitsInTheHeadset() {
    const QByteArray mine="[Input]\nDevices = Key;OpenXR\nMapping.ExitGame = Key;41 | OpenXR;9\nDevice.OpenXR.Element9 = 9;B;user_hand_left_input_y_click\nDevice.OpenXR.Element12 = 12;B;user_hand_left_input_thumbstick_click\n";
    const auto out=ac::pinball::exitOnChord(mine);
    QVERIFY(out.contains("Mapping.ExitGame = Key;41 | OpenXR;9 & OpenXR;12\n"));QVERIFY(!out.contains("| OpenXR;9\n"));
    QCOMPARE(ac::pinball::exitOnChord(out),out);
    // VPX's own default would exit on Y: with no mapping of the user's own, the chord is written.
    QVERIFY(ac::pinball::exitOnChord("[Input]\nDevices = Key;OpenXR\n").contains("Mapping.ExitGame = Key;41 | OpenXR;9 & OpenXR;12\n"));
    // No VR controller, or no [Input] section: nothing changes.
    QCOMPARE(ac::pinball::exitOnChord("[Input]\nDevices = Key\n"),QByteArray("[Input]\nDevices = Key\n"));QCOMPARE(ac::pinball::exitOnChord("[Player]\nX = 1\n"),QByteArray("[Player]\nX = 1\n"));
  }
  void aPlanWithoutAnOwnedFileWritesNothing() {
    QTemporaryDir folder;ac::pinball::writeVrSettings({{"args",QStringList{"-Play","x"}}});
    QVERIFY(QDir(folder.path()).isEmpty());
  }
};
QTEST_APPLESS_MAIN(PinballSettingsTest)
#include "PinballSettingsTest.moc"
