// SPDX-License-Identifier: GPL-3.0-only
// The Wii gun games: disc ids from synthetic headers, the Set-up report, and the exact DolphinXR
// command line. Nothing here starts Dolphin or SteamVR; the discs are a few synthetic bytes.
#include "core/setup/Report.h"
#include "core/wii/Wii.h"
#include <QDir>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
namespace {
QByteArray header(const QByteArray &id) {
  QByteArray h(0x60, '\0');
  h.replace(0, 6, id);
  qToBigEndian<quint32>(0x5D1C9EA3, h.data() + 0x18);
  h.replace(0x20, 9, "Synthetic");
  return h;
}
void write(const QString &path, const QByteArray &bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(bytes);
}
QByteArray iso(const QByteArray &id) { return header(id) + QByteArray(0x400, '\0'); }
QByteArray wbfs(const QByteArray &id) {
  QByteArray w(0x200, '\0');
  w.replace(0, 4, "WBFS");
  qToBigEndian<quint32>(2, w.data() + 4);
  w[8] = 9; w[9] = 20; w[12] = 1;
  return w + header(id) + QByteArray(0x200 - 0x60, '\0');
}
QByteArray rvz(const QByteArray &id) {
  QByteArray r(0x58, '\0');
  r.replace(0, 4, QByteArray("RVZ\x01", 4));
  return r + header(id) + QByteArray(0x200, '\0');
}
} // namespace
class WiiTest : public QObject {
  Q_OBJECT
private slots:
  void discsAreKnownByTheirOwnGameId() {
    QTemporaryDir t;
    write(t.filePath("a.iso"), iso("RBUP08"));
    write(t.filePath("b.wbfs"), wbfs("R2VE01"));
    write(t.filePath("c.rvz"), rvz("SQDE8P"));
    write(t.filePath("not-a-disc.iso"), QByteArray(0x400, 'x'));
    QCOMPARE(ac::wii::discId(t.filePath("a.iso")), QString("RBUP08"));
    QCOMPARE(ac::wii::discId(t.filePath("b.wbfs")), QString("R2VE01"));
    QCOMPARE(ac::wii::discId(t.filePath("c.rvz")), QString("SQDE8P"));
    QVERIFY(ac::wii::discId(t.filePath("not-a-disc.iso")).isEmpty());
    // Explicit regional IDs in the data match; other IDs require their own records.
    QCOMPARE(ac::wii::gameFor("RBUE08"), QString("umbrella"));
    QCOMPARE(ac::wii::gameFor("RHOJ8P"), QString("overkill"));
    QCOMPARE(ac::wii::gameFor("RZPP01"), QString("crossbow"));
    QCOMPARE(ac::wii::gameFor("RZJE69"), QString("deadspace"));
    QCOMPARE(ac::wii::gameFor("R3ME01"), QString("wii-metroid-prime-trilogy")); // Known metadata, without a launch profile.
    QCOMPARE(ac::wii::games().size(), 7);
    QVERIFY(ac::wii::gameFor("RBUE01").isEmpty()); // Same code, another maker.
    // The file name says nothing: a disc named after another game is still itself.
    write(t.filePath("Umbrella.iso"), iso("SBDP08"));
    QCOMPARE(ac::wii::identify(t.filePath("Umbrella.iso")).value("game").toString(), QString("darkside"));
  }
  void exactDolphinXrCommandLinePerGame() {
    QTemporaryDir t;
    const auto program = t.filePath("build/Dolphin.exe");
    write(program, "synthetic");
    const auto native = [](const QString &path) { return QDir::toNativeSeparators(path); };
    for (const auto &value : ac::wii::games()) {
      const auto game = value.toMap();
      const auto disc = t.filePath("discs/" + game.value("id").toString() + ".iso");
      write(disc, iso(game.value("ids").toStringList().first().toLatin1()));
      const auto plan = ac::wii::launchPlan(disc, program);
      QCOMPARE(plan.value("executable").toString(), native(QFileInfo(program).absoluteFilePath()));
      QCOMPARE(plan.value("cwd").toString(), native(QFileInfo(program).absolutePath()));
      // The authored route of the Chronicles recipes, the same for every game, plus no stop question.
      QCOMPARE(plan.value("args").toStringList(),
               QStringList({"--user", native(QFileInfo(program).absolutePath() + "/User"), "-C", "Graphics.VR.EnableOpenXR=True", "-C",
                            "Graphics.VR.ResolutionScale=1.0", "-C", "Graphics.VR.EagerHeartbeat=False", "-C", "Graphics.Stereoscopy.StereoMode=0",
                            "-C", "Dolphin.Interface.ConfirmStop=False", "--exec", native(QFileInfo(disc).absoluteFilePath())}));
    }
    QVERIFY_THROWS_EXCEPTION(std::exception, ac::wii::launchPlan(t.filePath("discs/umbrella.iso"), t.filePath("none/Dolphin.exe")));
    QVERIFY_THROWS_EXCEPTION(std::exception, ac::wii::launchPlan(t.filePath("discs/gone.iso"), program));
  }
  void oldUserFolderGetsTheUniversalControlsOnce() {
    QTemporaryDir t;
    const auto user = t.filePath("build/User");
    const auto text = [](const QString &path) {
      QFile file(path);
      return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    };
    const auto oldChord = QString::fromUtf8("pulse(!`OpenXR/0/OpenXR Controller:Left Button Thumbstick` & !`OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 0.1) & pulse(pulse(`OpenXR/0/OpenXR Controller:Left Button Thumbstick`, 0.25) & pulse(`OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 0.25) & `OpenXR/0/OpenXR Controller:Left Button Thumbstick` & `OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 1.0)");
    const auto oldHotkeys = "[Hotkeys]\r\nDevice = DInput/0/Keyboard Mouse\r\nGeneral/Toggle Pause = @(F10)\r\nVR/Reset VR Position = @(Shift+R) | (" + oldChord.toUtf8() + ")\r\n";
    const QByteArray oldDolphin = "[General]\r\nISOPaths = 0\r\n[Interface]\r\nConfirmStop = True\r\n[Input]\r\nBackgroundInput = True\r\n";
    const QByteArray pad = "[GCPad1]\nButtons/A = `Right Button A`\n";
    write(user + "/Config/Hotkeys.ini", oldHotkeys);
    write(user + "/Config/Dolphin.ini", oldDolphin);
    write(user + "/Config/GCPadNew.ini", pad);
    write(user + "/GameSettings/GALE01.ini", "[Core]\nCPUCore = 1\n");
    const auto changed = ac::wii::refreshUniversalControls(user);
    QCOMPARE(changed.size(), 4);
    const auto hotkeys = text(user + "/Config/Hotkeys.ini");
    QVERIFY(hotkeys.contains("General/Toggle Pause = @(F10)\r\n"));            // the user's own binding stays
    QVERIFY(hotkeys.contains("VR/Reset VR Position = @(Shift+R) | (pulse("));  // upgraded in place
    QVERIFY(!hotkeys.contains("0.25"));
    QVERIFY(hotkeys.contains("General/Exit = hold("));
    QVERIFY(!hotkeys.contains("\n\n") && hotkeys.count("\r\n") == hotkeys.count("\n"));
    const auto dolphin = text(user + "/Config/Dolphin.ini");
    QVERIFY(dolphin.contains("ConfirmStop = False\r\n"));
    QVERIFY(dolphin.contains("HotkeysRequireFocus = False\r\n"));
    QVERIFY(dolphin.contains("BackgroundInput = True\r\n"));
    // First copies kept, once.
    QCOMPARE(text(user + "/Config/Hotkeys.ini.before-universal-controls").toUtf8(), oldHotkeys);
    QCOMPARE(text(user + "/Config/Dolphin.ini.before-universal-controls").toUtf8(), oldDolphin);
    // Other files untouched.
    QCOMPARE(text(user + "/Config/GCPadNew.ini").toUtf8(), pad);
    QVERIFY(!QFileInfo::exists(user + "/Config/GCPadNew.ini.before-universal-controls"));
    QCOMPARE(text(user + "/GameSettings/GALE01.ini"), QString("[Core]\nCPUCore = 1\n"));
    // Second run: nothing to do, files and backups unchanged.
    QVERIFY(ac::wii::refreshUniversalControls(user).isEmpty());
    QCOMPARE(text(user + "/Config/Hotkeys.ini"), hotkeys);
    QCOMPARE(text(user + "/Config/Hotkeys.ini.before-universal-controls").toUtf8(), oldHotkeys);
    // A user's own Exit key is kept and the chord is added next to it.
    write(user + "/Config/Hotkeys.ini", "[Hotkeys]\nDevice = DInput/0/Keyboard Mouse\nGeneral/Exit = @(Alt+F4)\n");
    QCOMPARE(ac::wii::refreshUniversalControls(user).size(), 2);
    QVERIFY(text(user + "/Config/Hotkeys.ini").contains("General/Exit = (@(Alt+F4)) | (hold("));
    // A folder with no Config folder is left alone, and none is invented.
    QVERIFY(ac::wii::refreshUniversalControls(t.filePath("empty/User")).isEmpty());
    QVERIFY(!QFileInfo::exists(t.filePath("empty/User")));
  }
  void setupReportSaysWhatEachGameNeeds() {
    QTemporaryDir t;
    const auto program = t.filePath("build/Dolphin.exe"), folder = t.filePath("wii");
    write(program, "synthetic");
    write(t.filePath("build/User/GameSettingsVR/RBUP08.ini"), "[VR]\n");
    write(folder + "/Resident_Evil_Umbrella.iso", iso("RBUP08"));
    write(folder + "/Darkside.iso", iso("SBDP08"));
    write(folder + "/Overkill.iso", iso("RHOP8P"));
    write(folder + "/Gunblade.iso", iso("SQDE8P"));
    write(folder + "/Sin and Punishment 2.wbfs", wbfs("R2VE01"));
    write(folder + "/Metroid.iso", iso("R3ME01"));
    std::atomic_bool cancel = false;
    const auto report = ac::setup::scanSystem("wii", {{"wiiGameFolder", folder}, {"wiiExecutable", program}}, {}, {}, cancel);
    QCOMPARE(report.value("title").toString(), QString("Nintendo Wii"));
    const auto ids = [&](const char *group) {
      QStringList result;
      for (const auto &value : report.value(group).toList())
        result << value.toMap().value("id").toString();
      return result;
    };
    // Umbrella has its VR settings in the build: ready. The others start, but say what the build lacks.
    QCOMPARE(ids("ready"), QStringList({"umbrella"}));
    QCOMPARE(ids("notRight"), QStringList({"darkside", "overkill", "gunblade", "sinpunish"}));
    QCOMPARE(ids("missing"), QStringList({"deadspace", "crossbow"}));
    for (const auto &value : report.value("notRight").toList()) {
      const auto entry = value.toMap();
      QVERIFY(entry.value("startable").toBool());
      QVERIFY2(entry.value("reason").toString().contains("GameSettingsVR/" + entry.value("discId").toString() + ".ini"), qPrintable(entry.value("reason").toString()));
    }
    for (const auto &value : report.value("notRight").toList())
      if (value.toMap().value("id") == "gunblade")
        QVERIFY(value.toMap().value("reason").toString().contains("docked controls"));
    // Known metadata without a launch profile stays visible with its game id.
    QCOMPARE(report.value("other").toList().size(), 1);
    QCOMPARE(report.value("other").toList().first().toMap().value("detail").toString(), QString("game id R3ME01"));
    // Without the build every found game says so and nothing is started.
    const auto unset = ac::setup::scanSystem("wii", {{"wiiGameFolder", folder}}, {}, {}, cancel);
    QVERIFY(unset.value("ready").toList().isEmpty());
    QCOMPARE(unset.value("notRight").toList().size(), 5);
    QVERIFY(!unset.value("notRight").toList().first().toMap().value("startable").toBool());
  }
};
QTEST_GUILESS_MAIN(WiiTest)
#include "WiiTest.moc"
