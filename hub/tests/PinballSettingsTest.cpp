// SPDX-License-Identifier: GPL-3.0-only
#include "core/pinball/Pinball.h"
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
  void aPlanWithoutAnOwnedFileWritesNothing() {
    QTemporaryDir folder;ac::pinball::writeVrSettings({{"args",QStringList{"-Play","x"}}});
    QVERIFY(QDir(folder.path()).isEmpty());
  }
};
QTEST_APPLESS_MAIN(PinballSettingsTest)
#include "PinballSettingsTest.moc"
