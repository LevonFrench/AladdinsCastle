// SPDX-License-Identifier: GPL-3.0-only
// The Game library on synthetic files only: a big folder of subfolders is searched for every game the
// Hub plays, by what is inside the files, read-only, with duplicates chosen by a stated rule.
#include "core/library/Library.h"
#include "core/setup/Report.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
#include <cstring>
namespace {
void write(const QString &path, const QByteArray &bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (file.open(QIODevice::WriteOnly))
    file.write(bytes);
}
QByteArray wiiIso(const QByteArray &id) {
  QByteArray h(0x400, '\0');
  h.replace(0, 6, id);
  qToBigEndian<quint32>(0x5D1C9EA3, h.data() + 0x18);
  return h;
}
QByteArray wbfs(const QByteArray &id) {
  QByteArray w(0x200, '\0');
  w.replace(0, 4, "WBFS");
  w[8] = 9;
  return w + wiiIso(id);
}
// An ISO 9660 image whose SYSTEM.CNF names the boot file, as a PS2 disc's does.
QByteArray ps2Iso(const QByteArray &bootFile) {
  QByteArray data(24 * 2048, 0);
  auto *p = data.data() + 16 * 2048;
  p[0] = 1;
  std::memcpy(p + 1, "CD001", 5);
  p[6] = 1;
  p[156] = 34;
  qToLittleEndian<quint32>(20, p + 158);
  qToLittleEndian<quint32>(2048, p + 166);
  auto *d = data.data() + 20 * 2048;
  const QByteArray name = "SYSTEM.CNF;1";
  d[0] = char(33 + name.size());
  qToLittleEndian<quint32>(21, d + 2);
  qToLittleEndian<quint32>(64, d + 10);
  d[32] = char(name.size());
  std::memcpy(d + 33, name.data(), size_t(name.size()));
  const QByteArray boot = "BOOT2 = cdrom0:\\" + bootFile + ";1\r\n";
  std::memcpy(data.data() + 21 * 2048, boot.data(), size_t(boot.size()));
  return data;
}
// A raw Dreamcast track: the header marker, then the product number 64 bytes on.
QByteArray dreamcastTrack(const QByteArray &product) {
  QByteArray bytes(2352, '\0');
  bytes.replace(16, 15, "SEGA SEGAKATANA");
  bytes.replace(16 + 64, 10, product.leftJustified(10, ' '));
  return bytes;
}
// A stored zip of one member: enough for a directory read.
QByteArray zip(const QByteArray &member, const QByteArray &data) {
  QByteArray local(30, '\0'), central(46, '\0'), end(22, '\0');
  const auto crc = 0u;
  qToLittleEndian<quint32>(0x04034b50, local.data());
  qToLittleEndian<quint32>(crc, local.data() + 14);
  qToLittleEndian<quint32>(quint32(data.size()), local.data() + 18);
  qToLittleEndian<quint32>(quint32(data.size()), local.data() + 22);
  qToLittleEndian<quint16>(quint16(member.size()), local.data() + 26);
  qToLittleEndian<quint32>(0x02014b50, central.data());
  qToLittleEndian<quint32>(quint32(data.size()), central.data() + 20);
  qToLittleEndian<quint32>(quint32(data.size()), central.data() + 24);
  qToLittleEndian<quint16>(quint16(member.size()), central.data() + 28);
  const auto body = local + member + data;
  qToLittleEndian<quint32>(0x06054b50, end.data());
  qToLittleEndian<quint16>(1, end.data() + 8);
  qToLittleEndian<quint16>(1, end.data() + 10);
  qToLittleEndian<quint32>(quint32(central.size() + member.size()), end.data() + 12);
  qToLittleEndian<quint32>(quint32(body.size()), end.data() + 16);
  return body + central + member + end;
}
// Every file under a folder with its size and time, to show nothing was changed.
QByteArray fingerprint(const QString &root) {
  QStringList lines;
  for (QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories); it.hasNext();) {
    const QFileInfo info(it.next());
    lines << info.absoluteFilePath() + "|" + QString::number(info.size()) + "|" + QString::number(info.lastModified().toMSecsSinceEpoch());
  }
  lines.sort();
  return QCryptographicHash::hash(lines.join('\n').toUtf8(), QCryptographicHash::Sha256);
}
QStringList files(const QVariantList &copies) {
  QStringList result;
  for (const auto &value : copies)
    result << QFileInfo(value.toMap().value("file").toString()).fileName();
  return result;
}
} // namespace
class LibraryTest : public QObject {
  Q_OBJECT
private slots:
  void findsGamesInABigFolderOfSubfolders() {
    QTemporaryDir t;
    const auto root = t.filePath("roms");
    // Wii: the same game twice (USA and Europe), under folders named anything at all.
    write(root + "/consoles/nintendo stuff/Umbrella (USA)/game.wbfs", wbfs("RBUE08"));
    write(root + "/misc/re-umbrella-eu.iso", wiiIso("RBUP08"));
    write(root + "/misc/metroid.iso", wiiIso("R3ME01")); // A Wii disc not in the Hub's list.
    // PS2, by the serial on the disc, whatever the file is called.
    write(root + "/sony/Some Game/disc.iso", ps2Iso("SLES_512.29"));
    // Dreamcast, by the product number in the disc's own header.
    write(root + "/sega/dc/hotd2 (USA).cdi", QByteArray(3000, 'x') + dreamcastTrack("MK-51002"));
    // A cue sheet's track is read through the sheet, never on its own.
    write(root + "/sega/dc/Death Crimson 2/Death Crimson 2.cue", "FILE \"Death Crimson 2 (Track 1).bin\" BINARY\n");
    write(root + "/sega/dc/Death Crimson 2/Death Crimson 2 (Track 1).bin", dreamcastTrack("T23201M"));
    // Arcade sets in a mixed MAME folder and in a Model 3 folder; the one in the system's folder first.
    write(root + "/MAME/roms/scud.zip", zip("epr-19731.13", "a"));
    write(root + "/arcade/model3/scud.zip", zip("epr-19731.13", "a"));
    write(root + "/MAME/roms/vcop2.zip", zip("epr-18523.ic15", "b"));
    write(root + "/MAME/roms/pacman.zip", zip("pacman.6e", "c")); // Not a game the Hub plays.
    write(root + "/junk/not a disc.iso", QByteArray(4096, 'j'));
    // Pinball tables.
    write(root + "/pinball/Tables/one.vpx", "vpx");
    write(root + "/pinball/Tables/two.vpx", "vpx");
    // Too deep to be looked at (more than six folders down).
    write(root + "/a/b/c/d/e/f/g/deep.iso", wiiIso("SBDE08"));
    const auto before = fingerprint(root);
    std::atomic_bool cancel = false;
    QStringList said;
    const auto library = ac::library::scan({root}, [&](const QString &text) { said << text; }, cancel);
    QCOMPARE(fingerprint(root), before); // Read-only: nothing under the root changed.
    QVERIFY(!said.isEmpty());
    const auto systems = library.value("systems").toMap();
    QCOMPARE(files(ac::library::copies(library, "wii", "umbrella")), QStringList({"game.wbfs", "re-umbrella-eu.iso"})); // USA first.
    QCOMPARE(systems.value("wii").toMap().value("other").toInt(), 1);
    QVERIFY(ac::library::copies(library, "wii", "darkside").isEmpty()); // The one too deep.
    QCOMPARE(files(ac::library::copies(library, "ps2", "virtua-cop")), QStringList({"disc.iso"}));
    QCOMPARE(ac::library::copies(library, "ps2", "virtua-cop").first().toMap().value("identity").toString(), QString("SLES-51229"));
    QCOMPARE(files(ac::library::copies(library, "dreamcast", "hotd2")), QStringList({"hotd2 (USA).cdi"}));
    QCOMPARE(files(ac::library::copies(library, "dreamcast", "dcrimson2")), QStringList({"Death Crimson 2.cue"}));
    const auto scud = ac::library::copies(library, "model3", "scud");
    QCOMPARE(scud.size(), 2);
    QVERIFY(scud.first().toMap().value("file").toString().contains("/arcade/model3/"));
    QCOMPARE(files(ac::library::copies(library, "model2", "vcop2")), QStringList({"vcop2.zip"}));
    QCOMPARE(library.value("pinballFolders").toList().first().toMap().value("tables").toInt(), 2);
    // What it did not recognise is counted, never dropped in silence.
    QCOMPARE(library.value("unrecognised").toMap().value("archives").toInt(), 1);
    QCOMPARE(library.value("unrecognised").toMap().value("discs").toInt(), 1);
    // The user's own choice wins over the rule while that file is there.
    QCOMPARE(QFileInfo(ac::library::chosen(library, "wii", "umbrella", {}).value("file").toString()).fileName(), QString("game.wbfs"));
    const QVariantMap choice{{"wii/umbrella", ac::library::copies(library, "wii", "umbrella").last().toMap().value("file")}};
    QCOMPARE(QFileInfo(ac::library::chosen(library, "wii", "umbrella", choice).value("file").toString()).fileName(), QString("re-umbrella-eu.iso"));
    QVERIFY(!ac::library::rule().isEmpty());
    // Set-up finds them through the library when a system has no folder of its own.
    write(t.filePath("build/Dolphin.exe"), "synthetic");
    const auto report = ac::setup::scanSystem("wii", {{"wiiExecutable", t.filePath("build/Dolphin.exe")}}, {{"library", library}, {"libraryChoice", choice}}, {}, cancel);
    QVariantMap umbrella;
    for (const auto &value : report.value("ready").toList())
      if (value.toMap().value("id") == "umbrella")
        umbrella = value.toMap();
    QVERIFY(QDir::fromNativeSeparators(umbrella.value("file").toString()).endsWith("re-umbrella-eu.iso")); // The chosen copy.
    QCOMPARE(report.value("note").toString(), QString("Started through your DolphinXR build with its own settings; the discs stay where they are."));
  }
  void aBigTreeIsQuick() {
    // Thousands of files over many folders, as in a large collection: most are not games the Hub plays.
    QTemporaryDir t;
    const auto root = t.filePath("roms");
    for (int system = 0; system < 40; ++system)
      for (int folder = 0; folder < 5; ++folder)
        for (int file = 0; file < 12; ++file)
          write(QString("%1/system%2/sub%3/game%4.zip").arg(root).arg(system).arg(folder).arg(file), "x");
    for (int i = 0; i < 300; ++i)
      write(QString("%1/discs/set%2/disc%3.iso").arg(root).arg(i % 20).arg(i), QByteArray(2048, 'd'));
    std::atomic_bool cancel = false;
    QElapsedTimer clock;
    clock.start();
    const auto library = ac::library::scan({root}, {}, cancel);
    const auto ms = clock.elapsed();
    qInfo("Library over %d files: %lld ms", library.value("files").toInt(), qint64(ms));
    QCOMPARE(library.value("files").toInt(), 40 * 5 * 12 + 300);
    QVERIFY2(ms < 20000, qPrintable(QString::number(ms)));
    // Cancelling stops it.
    cancel = true;
    QVERIFY(ac::library::scan({root}, {}, cancel).value("cancelled").toBool());
  }
};
QTEST_GUILESS_MAIN(LibraryTest)
#include "LibraryTest.moc"
