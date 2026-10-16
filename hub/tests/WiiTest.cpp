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
  w[8] = 9; // 512-byte host sectors: the disc header follows at 0x200.
  return w + header(id) + QByteArray(0x200, '\0');
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
    // Any region of a listed game, by its game code and maker; other games are not taken for it.
    QCOMPARE(ac::wii::gameFor("RBUE08"), QString("umbrella"));
    QCOMPARE(ac::wii::gameFor("RHOJ8P"), QString("overkill"));
    QCOMPARE(ac::wii::gameFor("RZPP01"), QString("crossbow"));
    QCOMPARE(ac::wii::gameFor("RZJE69"), QString("deadspace"));
    QVERIFY(ac::wii::gameFor("R3ME01").isEmpty()); // Metroid Prime Trilogy: not a gun game here.
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
    // A disc not in the list is said with its own game id.
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
