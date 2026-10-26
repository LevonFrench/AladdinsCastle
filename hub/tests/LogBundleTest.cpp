// SPDX-License-Identifier: GPL-3.0-only
// The problem report: what is taken out of a log, which files go in and which never do, the size limits, and a ZIP that
// opens. Everything is fixture text in a temporary folder; the "private" names, keys and folders are made up.
#include "core/app/LogBundle.h"
#include "ProductIdentity.h"
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>
#include <zlib.h>
using namespace ac::report;
namespace {
// A drive path, built so that no real-looking path sits in this file.
QString drive(const char *letter, const QString &tail) { return QString(letter) + ":/" + tail; }
QDateTime utc(const QString &iso) { return QDateTime::fromString(iso, Qt::ISODate); }
void saveFile(const QString &path, const QByteArray &bytes, const QDateTime &time = {}) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(bytes), qint64(bytes.size()));
  QVERIFY(file.flush());
  if (time.isValid())
    QVERIFY(file.setFileTime(time, QFileDevice::FileModificationTime));
}
quint16 u16(const QByteArray &b, qsizetype pos) { return static_cast<quint16>(static_cast<quint8>(b[pos]) | (static_cast<quint8>(b[pos + 1]) << 8)); }
quint32 u32(const QByteArray &b, qsizetype pos) { return u16(b, pos) | (static_cast<quint32>(u16(b, pos + 2)) << 16); }
// Reads a ZIP through its central directory and checks each entry's CRC with zlib; empty on any fault.
QMap<QString, QByteArray> readZip(const QByteArray &zip, QString *fault) {
  QMap<QString, QByteArray> files;
  const auto end = zip.size() - 22;
  if (end < 0 || u32(zip, end) != 0x06054B50U) {
    *fault = "no end record";
    return {};
  }
  const auto count = u16(zip, end + 10);
  auto at = static_cast<qsizetype>(u32(zip, end + 16));
  for (int i = 0; i < count; ++i) {
    if (u32(zip, at) != 0x02014B50U) {
      *fault = "bad directory entry";
      return {};
    }
    const auto method = u16(zip, at + 10), nameSize = u16(zip, at + 28), extraSize = u16(zip, at + 30), commentSize = u16(zip, at + 32);
    const auto crc = u32(zip, at + 16), size = u32(zip, at + 20);
    const auto local = static_cast<qsizetype>(u32(zip, at + 42));
    const auto name = QString::fromUtf8(zip.mid(at + 46, nameSize));
    if (method != 0 || u32(zip, at + 24) != size || u32(zip, local) != 0x04034B50U || (u16(zip, local + 6) & 0x0800) == 0) {
      *fault = "unexpected header for " + name;
      return {};
    }
    const auto data = zip.mid(local + 30 + u16(zip, local + 26) + u16(zip, local + 28), size);
    if (data.size() != qsizetype(size) || ::crc32(0, reinterpret_cast<const Bytef *>(data.constData()), static_cast<uInt>(data.size())) != crc) {
      *fault = "bad data for " + name;
      return {};
    }
    files.insert(name, data);
    at += 46 + nameSize + extraSize + commentSize;
  }
  return files;
}
Redaction sample() {
  Redaction who;
  who.home = drive("C", "Users/sample");
  who.root = drive("H", "portable/Hub");
  who.names = {"sample", "Computer-77", "admin"};
  return who;
}
} // namespace
class LogBundleTest : public QObject {
  Q_OBJECT
private slots:
  void foldersBecomeNames_data() {
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");
    QTest::newRow("home") << "opened " + drive("C", "Users/sample/AppData/x.cfg") + " ok" << "opened <home>/AppData/x.cfg ok";
    QTest::newRow("home, backslashes") << "opened " + drive("C", "Users/sample").replace('/', '\\') + "\\Saved\\x" << "opened <home>\\Saved\\x";
    QTest::newRow("home, any case") << "at " + drive("C", "USERS/Sample/y") << "at <home>/y";
    QTest::newRow("hub folder") << "wrote " + drive("H", "portable/Hub/user/logs/a.log") + ";" << "wrote <hub>/user/logs/a.log;";
    QTest::newRow("a longer name is not the hub folder") << drive("H", "portable/Hubcap/x") << "<path>/x";
    QTest::newRow("another account") << "see " + drive("C", "Users/someone/Saved Games/x") << "see <home>/Saved Games/x";
    QTest::newRow("a home on another system") << "read /home/someone/.config/a and /Users/Other/Library/b" << "read <home>/.config/a and <home>/Library/b";
    QTest::newRow("other folders keep their last part") << "disc " + drive("D", "Games/Racer/disc.iso") + ", done" << "disc <path>/disc.iso, done";
    QTest::newRow("other folders, backslashes") << "disc " + drive("D", "Games/Racer").replace('/', '\\') + "\\disc.iso" << "disc <path>\\disc.iso";
    QTest::newRow("a bare drive") << "on " + drive("E", "") << "on <path>";
    QTest::newRow("a network share") << QString("share ") + R"(\\nas\games\racer\disc.iso)" + " gone" << "share <network path> gone";
  }
  void foldersBecomeNames() {
    QFETCH(QString, text);
    QFETCH(QString, expected);
    QCOMPARE(redact(text, sample()), expected);
  }
  void personalDetailsAreTakenOut_data() {
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");
    const auto token = "ghp_" + QString("a1B2").repeated(9);
    QTest::newRow("email") << "mail jane.doe@example.com now" << "mail <email> now";
    QTest::newRow("web login") << "GET https://jane:hunter2@example.org/path" << "GET https://<redacted>@example.org/path";
    QTest::newRow("web login, name only") << "GET https://jane@example.org/path" << "GET https://<redacted>@example.org/path";
    QTest::newRow("key in a link") << "GET https://api.example.org/x?token=abcDEF123&page=2" << "GET https://api.example.org/x?token=<redacted>&page=2";
    QTest::newRow("header") << "Authorization: Bearer abc.def.ghi" << "Authorization: <redacted>";
    QTest::newRow("password") << "password=hunter2;" << "password=<redacted>;";
    QTest::newRow("json key") << R"({"api_key": "abcdef0123456789xyz", "n": 3})" << R"({"api_key": "<redacted>", "n": 3})";
    QTest::newRow("a key by its name") << "steamGridDbKey=0123456789abcdef0123" << "steamGridDbKey=<redacted>";
    QTest::newRow("token by its look") << "pushed with " + token + " done" << "pushed with <token> done";
    QTest::newRow("web token") << "jwt eyJhbGciOiJIUzI1.eyJzdWIiOiIxMjM0.SflKxwRJSMeK end" << "jwt <token> end";
    QTest::newRow("steam account") << "steam 76561198012345678 here" << "steam <steam id> here";
    QTest::newRow("account name as a word") << "user Sample logged in; samples and Computer-77." << "user <name> logged in; samples and <name>.";
    QTest::newRow("a common name is left alone") << "admin ran it" << "admin ran it";
  }
  void personalDetailsAreTakenOut() {
    QFETCH(QString, text);
    QFETCH(QString, expected);
    QCOMPARE(redact(text, sample()), expected);
  }
  void ordinaryLogLinesAreNotTouched() {
    const QStringList lines{"--- [3/5] Unpack the archive ---",
                            " [XX] E_DISK_FULL: not enough space on the drive",
                            "sha256 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08 matched",
                            "GET https://github.com/owner/repo/releases/download/v1.2/tool.zip 200 in 1.4 s",
                            "launch model2-vcop2 view=screen exit code 0x00000000",
                            "Looking through 3 folders; found 41 games in 212 files"};
    for (const auto &line : lines)
      QCOMPARE(redact(line, sample()), line);
  }
  void redactingTwiceChangesNothingMore() {
    const auto text = "A " + drive("C", "Users/sample/x") + " B " + drive("D", "Games/a/b.iso") + " C jane@example.org D password=x E https://u:p@h.org/?key=abc F ghp_" +
                      QString("z9").repeated(15) + " G " + drive("H", "portable/Hub/user") + " H Computer-77";
    const auto once = redact(text, sample());
    QVERIFY(once != text);
    QCOMPARE(redact(once, sample()), once);
  }
  void thisPcNamesTheAccountAndTheComputer() {
    const auto who = Redaction::thisPc(drive("H", "portable/Hub"));
    QCOMPARE(who.root, drive("H", "portable/Hub"));
    QCOMPARE(who.home, QDir::homePath());
    QVERIFY(who.names.contains(QDir(QDir::homePath()).dirName()));
  }
  void zipOpensAndHoldsExactBytes() {
    QList<QPair<QString, QByteArray>> entries;
    entries << qMakePair(QString("about.txt"), QByteArray("hello\n")) << qMakePair(QString("hub-logs/a.log"), QByteArray(5000, 'x'))
            << qMakePair(QString("empty.log"), QByteArray());
    QString fault;
    const auto files = readZip(zipOf(entries, utc("2026-10-25T12:34:56Z")), &fault);
    QVERIFY2(fault.isEmpty(), qPrintable(fault));
    QCOMPARE(files.size(), 3);
    for (const auto &entry : entries)
      QCOMPARE(files.value(entry.first), entry.second);
    // The zip's own checks: a count of nothing is still a ZIP.
    QVERIFY(readZip(zipOf({}, utc("2026-10-25T12:34:56Z")), &fault).isEmpty() && fault.isEmpty());
  }
  void bundleHoldsTheHubAndLastLaunchLogsOnly() {
    QTemporaryDir hub, out;
    QVERIFY(hub.isValid() && out.isValid());
    auto who = sample();
    who.root = hub.path();
    const auto logs = hub.path() + "/user/logs/";
    const auto old = utc("2026-10-20T10:00:00Z"), recent = utc("2026-10-25T10:00:00Z");
    saveFile(logs + "install/run1.log", "step 1\nopened " + hub.path().toUtf8() + "/emulators/x\nmail jane@example.org\n", old);
    saveFile(logs + "crashes.json", R"([{"title":"Racer","log":"token=abc123 at )" + hub.path().toUtf8() + R"( by sample"}])", old);
    saveFile(logs + "launch-racer-20261024-120000-000-aaaaaaaa.log", "launch failed\nfolder " + who.home.toUtf8() + "/Games\n", old);
    saveFile(logs + "overlay-input.log", "B pressed\n", old);
    saveFile(logs + "racer/20261024-100000-000-bbbbbbbb.log", "older launch\n", old);
    saveFile(logs + "racer/20261025-100000-000-cccccccc.log", "newest launch for sample\n", recent);
    saveFile(logs + "other/20261023-100000-000-dddddddd.log", "another game\n", old);
    // None of these may be read: not a log, an odd name, a game file, a save, or outside the logs.
    saveFile(logs + "notes.sav", "SAVEDATA", recent);
    saveFile(logs + "racer/save.bin", "SAVEDATA", recent);
    saveFile(logs + "bad$name.log", "odd name", recent);
    saveFile(hub.path() + "/emulators/game.iso", "GAMEDATA", recent);
    saveFile(hub.path() + "/user/saves/slot1.sav", "SAVEDATA", recent);
    const auto when = utc("2026-10-25T12:00:00Z");
    const auto bundle = writeBundle(hub.path(), out.path(), who, "Test Hub", "1.2.3", {}, when);
    QVERIFY2(bundle.ok, qPrintable(bundle.error));
    QCOMPARE(bundle.path, out.path() + "/" + QString::fromUtf8(ac::product::short_id) + "-report-20261025-120000.zip");
    QString fault;
    QFile zip(bundle.path);
    QVERIFY(zip.open(QIODevice::ReadOnly));
    const auto files = readZip(zip.readAll(), &fault);
    QVERIFY2(fault.isEmpty(), qPrintable(fault));
    const QSet<QString> expected{"about.txt", "hub-logs/install/run1.log", "hub-logs/crashes.json", "hub-logs/launch-racer-20261024-120000-000-aaaaaaaa.log",
                                 "hub-logs/overlay-input.log", "last-launch/racer/20261025-100000-000-cccccccc.log"};
    QCOMPARE(QSet<QString>(files.keyBegin(), files.keyEnd()), expected);
    QCOMPARE(bundle.included.size(), 5);
    QCOMPARE(bundle.included.first(), QString("last-launch/racer/20261025-100000-000-cccccccc.log")); // The last launch comes first.
    QVERIFY(bundle.leftOut.isEmpty());
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
      const auto text = QString::fromUtf8(it.value());
      for (const auto &secret : {hub.path(), QString("jane@example.org"), QString("abc123"), QString("sample"), QString("SAVEDATA"), QString("GAMEDATA")})
        QVERIFY2(!text.contains(secret, Qt::CaseInsensitive), qPrintable(it.key() + " holds " + secret));
    }
    QVERIFY(files["hub-logs/install/run1.log"].contains("opened <hub>/emulators/x"));
    QVERIFY(files["hub-logs/install/run1.log"].contains("mail <email>"));
    QVERIFY(files["hub-logs/crashes.json"].contains("token=<redacted>"));
    QVERIFY(files["hub-logs/launch-racer-20261024-120000-000-aaaaaaaa.log"].contains("folder <home>/Games"));
    QVERIFY(files["last-launch/racer/20261025-100000-000-cccccccc.log"].contains("newest launch for <name>"));
    const auto about = QString::fromUtf8(files["about.txt"]);
    QVERIFY(about.startsWith("Test Hub problem report\nVersion: 1.2.3\nMade: 2026-10-25T12:00:00Z\n"));
    QVERIFY(about.contains("No game files, BIOS files or saves"));
    QVERIFY(about.contains("- hub-logs/overlay-input.log"));
  }
  void bigLogsKeepTheirEndAndTheReportHasALimit() {
    QTemporaryDir hub, out;
    QVERIFY(hub.isValid() && out.isValid());
    auto who = sample();
    who.root = hub.path();
    QByteArray big;
    for (int i = 0; i < 100; ++i)
      big += "line " + QByteArray::number(i) + " of the log\n";
    const auto logs = hub.path() + "/user/logs/";
    saveFile(logs + "install/big.log", big, utc("2026-10-25T10:00:00Z"));
    // One log is cut to its end, from a whole line.
    BundleLimits limits;
    limits.perFile = 120;
    auto bundle = writeBundle(hub.path(), out.path(), who, "Test Hub", "1", limits, utc("2026-10-25T12:00:00Z"));
    QVERIFY2(bundle.ok, qPrintable(bundle.error));
    QString fault;
    QFile zip(bundle.path);
    QVERIFY(zip.open(QIODevice::ReadOnly));
    auto files = readZip(zip.readAll(), &fault);
    const auto cut = QString::fromUtf8(files.value("hub-logs/install/big.log"));
    QVERIFY(cut.startsWith("[The start of this log was left out"));
    QVERIFY(cut.endsWith("line 99 of the log\n"));
    QVERIFY(!cut.contains("line 0 of"));
    QVERIFY(cut.size() < 120 + 80);
    // The total: the first file that does not fit is named and left out, the rest still go in.
    saveFile(logs + "install/small.log", "small one\n", utc("2026-10-24T10:00:00Z"));
    saveFile(logs + "install/medium.log", QByteArray(200, 'm') + "\n", utc("2026-10-23T10:00:00Z"));
    limits = {};
    limits.total = 150;
    bundle = writeBundle(hub.path(), out.path(), who, "Test Hub", "1", limits, utc("2026-10-25T12:01:00Z"));
    QVERIFY(bundle.ok);
    QCOMPARE(bundle.included, QStringList{"hub-logs/install/small.log"});
    QCOMPARE(bundle.leftOut.size(), 2);
    QVERIFY(bundle.leftOut.join("|").contains("hub-logs/install/big.log: would make the report too big"));
    QVERIFY(bundle.bytes <= limits.total);
    // The number of files.
    limits = {};
    limits.files = 1;
    bundle = writeBundle(hub.path(), out.path(), who, "Test Hub", "1", limits, utc("2026-10-25T12:02:00Z"));
    QCOMPARE(bundle.included.size(), 1);
    QCOMPARE(bundle.included.first(), QString("hub-logs/install/big.log")); // The newest.
    QVERIFY(bundle.leftOut.join("|").contains("too many files"));
  }
  void nothingToCollectStillMakesAReport() {
    QTemporaryDir hub, out;
    QVERIFY(hub.isValid() && out.isValid());
    auto who = sample();
    who.root = hub.path();
    const auto bundle = writeBundle(hub.path(), out.path() + "/reports", who, "Test Hub", "1", {}, utc("2026-10-25T12:00:00Z"));
    QVERIFY2(bundle.ok, qPrintable(bundle.error));
    QVERIFY(bundle.included.isEmpty());
    QFile zip(bundle.path);
    QVERIFY(zip.open(QIODevice::ReadOnly));
    QString fault;
    const auto files = readZip(zip.readAll(), &fault);
    QCOMPARE(files.size(), 1);
    QVERIFY(files["about.txt"].contains("No logs were found yet."));
  }
  void onlyTheNewestFiveReportsAreKept() {
    QTemporaryDir hub, out;
    QVERIFY(hub.isValid() && out.isValid());
    auto who = sample();
    who.root = hub.path();
    for (int second = 0; second < 8; ++second)
      QVERIFY(writeBundle(hub.path(), out.path(), who, "Test Hub", "1", {}, utc("2026-10-25T12:00:0" + QString::number(second) + "Z")).ok);
    const auto kept = QDir(out.path()).entryList({"*.zip"}, QDir::Files, QDir::Name);
    QCOMPARE(kept.size(), 5);
    QVERIFY(kept.first().endsWith("-report-20261025-120003.zip"));
    QVERIFY(kept.last().endsWith("-report-20261025-120007.zip"));
  }
  void aFolderThatCannotBeMadeGivesAPlainError() {
    QTemporaryDir hub, out;
    QVERIFY(hub.isValid() && out.isValid());
    saveFile(out.path() + "/blocker", "a file, not a folder");
    auto who = sample();
    who.root = hub.path();
    const auto bundle = writeBundle(hub.path(), out.path() + "/blocker/inside", who, "Test Hub", "1", {}, utc("2026-10-25T12:00:00Z"));
    QVERIFY(!bundle.ok);
    QVERIFY(!bundle.error.isEmpty());
    QVERIFY(bundle.path.isEmpty());
  }
};
QTEST_GUILESS_MAIN(LogBundleTest)
#include "LogBundleTest.moc"
