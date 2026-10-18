// SPDX-License-Identifier: GPL-3.0-only
// The watchdog's rules with stand-in process lists and window probes: nothing here touches a real process.
#include "core/launch/Watchdog.h"
#include "core/install/Support.h"
#include <QDir>
#include <QSignalSpy>
#include <QtTest>
using ac::launch::ProcessInfo;
namespace {
const QList<ac::launch::SystemRule> rules{
    {"pinball", "Visual Pinball", {"/Games/VPX"}, {"/Games/VPX/Tables"}, {"vpinball*", "b2sbackglassserver*"}},
    {"model3", "Sega Model 3", {"/Emu/Supermodel"}, {}, {"supermodel*"}}};
ProcessInfo process(qint64 pid, qint64 parent, const QString &path, qint64 created) {
  return {pid, parent, created, path, path.section('/', -1)};
}
} // namespace
class WatchdogTest : public QObject {
  Q_OBJECT
private slots:
  // What a crash record keeps of a log names no account: the home folder, in either spelling, is written as a name.
  void sharedTextHidesTheHomeFolder() {
#ifdef Q_OS_WIN
    const QString home("%USERPROFILE%");
#else
    const QString home("~");
#endif
    const QString text = "opened /home/sample/AppData/game.cfg; " + QDir::toNativeSeparators("/home/sample/Saved") +
                         " and /HOME/Sample\nothing, but /home/sampler/other and /home/sample";
    const auto shared = ac::install::shareable(text, "/home/sample");
    QVERIFY2(!shared.contains("sample/", Qt::CaseInsensitive) && !shared.contains("sample\\", Qt::CaseInsensitive), qPrintable(shared));
    QVERIFY(shared.contains(home + "/AppData/game.cfg"));QVERIFY(shared.contains("/home/sampler/other"));QVERIFY(shared.endsWith(home));
    QCOMPARE(ac::install::shareable("nothing personal", "/home/sample"), QString("nothing personal"));
    QCOMPARE(ac::install::shareable("/x/y", "/"), QString("/x/y"));
  }
  void onlyConfiguredFoldersBelongToASystem() {
    QCOMPARE(ac::launch::attribute(process(101, 0, "/Games/VPX/VPinballX64.exe", 0), rules), QString("pinball"));
#ifdef Q_OS_WIN
    // Its own folder: any program. Windows paths ignore case and take either separator.
    QCOMPARE(ac::launch::attribute(process(102, 0, "\\games\\vpx\\Tools.exe", 0), rules), QString("pinball"));
#else
    // Its own folder: any program. Elsewhere a folder is exactly as written: another spelling is another folder.
    QCOMPARE(ac::launch::attribute(process(102, 0, "/Games/VPX/Tools", 0), rules), QString("pinball"));
    QVERIFY(ac::launch::attribute(process(110, 0, "/games/vpx/Tools", 0), rules).isEmpty());
    QVERIFY(ac::launch::attribute(process(111, 0, "\\Games\\VPX\\Tools", 0), rules).isEmpty());
    QVERIFY(ac::launch::attribute(process(112, 0, "/games/vpx/tables/VPinballX64", 0), rules).isEmpty());
#endif
    QCOMPARE(ac::launch::attribute(process(103, 0, "/Games/VPX/Tables/B2SBackglassServerEXE.exe", 0), rules), QString("pinball"));
    QVERIFY(ac::launch::attribute(process(104, 0, "/Games/VPX/Tables/notepad.exe", 0), rules).isEmpty()); // Around it: known names only.
    QVERIFY(ac::launch::attribute(process(105, 0, "/Other/VPinballX64.exe", 0), rules).isEmpty());           // Same name elsewhere.
    QCOMPARE(ac::launch::attribute(process(106, 0, "/Emu/Supermodel/Supermodel.exe", 0), rules), QString("model3"));
    // SteamVR and Steam are never the watchdog's, even inside a configured folder.
    QVERIFY(ac::launch::attribute(process(107, 0, "/Games/VPX/vrserver.exe", 0), rules).isEmpty());
    QVERIFY(ac::launch::neverTouch(process(108, 0, "/steam/steamapps/common/SteamVR/bin/win64/vrcompositor.exe", 0)));
    QVERIFY(ac::launch::neverTouch(process(109, 0, "/Program Files (x86)/Steam/steam.exe", 0)));
  }
  void exitCodesInWords() {
    QVERIFY(ac::launch::isCrashCode(0xC0000005));
    QVERIFY(ac::launch::exitCodeMeaning(0xC0000005).contains("access violation"));
    QVERIFY(ac::launch::exitCodeMeaning(0xC00000FD).contains("stack overflow"));
    QVERIFY(ac::launch::isCrashCode(0xE06D7363));
    QVERIFY(!ac::launch::isCrashCode(0));
    QVERIFY(!ac::launch::isCrashCode(1));          // How the Hub itself ends a game.
    QVERIFY(!ac::launch::isCrashCode(0xC000013A)); // A console closed with Ctrl+C.
    QCOMPARE(ac::launch::exitCodeText(int(0xC0000409)), QString("a stack buffer overrun, or the program ended itself on a fatal error (0xC0000409)"));
    QCOMPARE(ac::launch::exitCodeText(3), QString("exit code 3"));
  }
  void straysSessionsAndErrorReports() {
    const qint64 start = 1'000'000;
    QList<ProcessInfo> running{process(10, 1, "/Games/VPX/VPinballX64.exe", start + 100),                  // The game.
                               process(11, 10, "/Games/VPX/PinMAME.exe", start + 200),                      // Its child.
                               process(12, 1, "/Games/VPX/Tables/B2SBackglassServerEXE.exe", start - 60000), // Left from before.
                               process(13, 1, "/Games/VPX/Tables/B2SBackglassServerEXE.exe", start + 300),   // Started for it by Windows.
                               process(14, 1, "/Emu/Supermodel/Supermodel.exe", start - 5000),              // Another system's.
                               process(15, 1, "/Other/VPinballX64.exe", start),                             // Not the Hub's.
                               process(16, 10, "/Windows/System32/WerFault.exe", start + 900),              // Reporting on the game.
                               process(17, 1, "/Windows/System32/WerFault.exe", start)};                     // Reporting on something else.
    ac::launch::Watchdog dog;
    dog.setRules(rules);
    dog.setLister([&] { return running; });
    dog.setWindowProbe([](const QList<qint64> &) { return ac::launch::WindowState{true, true, false, false}; });
    QSignalSpy reports(&dog, &ac::launch::Watchdog::errorReportSeen);
    dog.beginSession({"pinball-x", "Table", "pinball", start, true, [] { return QList<qint64>{10}; }});
    const auto pids = [&] {
      QList<qint64> result;
      for (const auto &value : dog.strays())
        result << value.toMap().value("pid").toLongLong();
      return result;
    };
    QCOMPARE(pids(), QList<qint64>({12, 14}));
    QCOMPARE(reports.size(), 1);
    QCOMPARE(reports.first()[0].toLongLong(), 16);
    QCOMPARE(reports.first()[1].toLongLong(), 10);
    dog.scan();
    QCOMPARE(reports.size(), 1); // Said once.
    // The game ended and left its child and the backglass server running: now they are strays.
    running.removeFirst();
    running.removeLast();
    dog.endSession();
    QCOMPARE(pids(), QList<qint64>({11, 12, 13, 14}));
  }
  void endingIsGracefulThenTheTreeOnlyTheSameProcesses() {
    QList<ProcessInfo> running{process(20, 1, "/Emu/Supermodel/Supermodel.exe", 100), process(21, 20, "/Windows/System32/conhost.exe", 150),
                               process(22, 1, "/Emu/Supermodel/Supermodel.exe", 300), process(23, 1, "/Other/Supermodel.exe", 100)};
    ac::launch::Watchdog dog;
    dog.setRules(rules);
    dog.setTimings(1000, 100, 0, 150);
    dog.setLister([&] { return running; });
    QList<QList<qint64>> gentle, ended;
    dog.setGraceful([&](const QString &system, const QList<qint64> &pids) { QCOMPARE(system, QString("model3")); gentle << pids; });
    dog.setEnder([&](const QList<qint64> &pids) { ended << pids; });
    dog.scan();
    QCOMPARE(dog.strays().size(), 2);
    dog.end(23); // Not a stray: nothing happens.
    QTest::qWait(250);
    QVERIFY(gentle.isEmpty());
    dog.end(20);
    QCOMPARE(gentle.size(), 1);
    QCOMPARE(gentle.first(), QList<qint64>({20, 21})); // The program and what it started.
    QTRY_COMPARE(ended.size(), 1);
    QCOMPARE(ended.first(), QList<qint64>({20, 21}));
    // A number Windows gave to a new process in the meantime is never ended.
    dog.end(22);
    running[2].createdMs = 999;
    QTest::qWait(300);
    QCOMPARE(ended.size(), 1);
  }
  void troubleIsSaidAfterItLasts() {
    ac::launch::WindowState state{true, true, false, false};
    ac::launch::Watchdog dog;
    dog.setTimings(1000, 120, 0, 100);
    dog.setLister([] { return QList<ProcessInfo>{}; });
    dog.setWindowProbe([&](const QList<qint64> &) { return state; });
    dog.beginSession({"model3-scud", "Scud Race", "model3", 0, true, [] { return QList<qint64>{30}; }});
    QVERIFY(dog.trouble().isEmpty());
    state.hung = true;
    dog.scan();
    QVERIFY(dog.trouble().isEmpty()); // Not yet: a moment's stall is not trouble.
    QTest::qWait(150);
    dog.scan();
    QCOMPARE(dog.trouble(), QString("hung"));
    state = {true, false, false, false};
    dog.scan();
    QTest::qWait(150);
    dog.scan();
    QCOMPARE(dog.trouble(), QString("hidden"));
    state = {true, true, true, false};
    dog.scan();
    QTest::qWait(150);
    dog.scan();
    QCOMPARE(dog.trouble(), QString("minimized"));
    state = {true, true, false, false};
    dog.scan();
    QVERIFY(dog.trouble().isEmpty()); // Cleared at once.
    // A game with no window of its own (Model 2's viewer) is never "hidden".
    dog.beginSession({"model2-vcop2", "Virtua Cop 2", "model2", 0, false, [] { return QList<qint64>{31}; }});
    state = {false, false, false, false};
    dog.scan();
    QTest::qWait(150);
    dog.scan();
    QVERIFY(dog.trouble().isEmpty());
  }
};
QTEST_GUILESS_MAIN(WatchdogTest)
#include "WatchdogTest.moc"
