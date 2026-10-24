// SPDX-License-Identifier: GPL-3.0-only
// The Game library on synthetic files only: a big folder of subfolders is searched for every game the
// Hub plays, by what is inside the files, read-only, with duplicates chosen by a stated rule.
#include "core/library/Library.h"
#include "core/setup/Report.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QJsonDocument>
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
  qToBigEndian<quint32>(3, w.data() + 4);
  w[8] = 9; w[9] = 20; w[12] = 1;
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
    write(root + "/misc/metroid.iso", wiiIso("R3ME01")); // Known disc metadata, without a launch profile.
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
    QCOMPARE(systems.value("wii").toMap().value("other").toInt(), 0);
    QCOMPARE(files(ac::library::copies(library, "wii", "wii-metroid-prime-trilogy")), QStringList({"metroid.iso"}));
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
  void sheetsReopenChangedTracksAndOldCacheIsRejected() {
    QTemporaryDir t; QVERIFY(t.isValid()); const auto root = t.filePath("roms");
    write(root + "/disc.bin", ps2Iso("SLES_512.29"));
    write(root + "/disc.cue", "FILE \"disc.bin\" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n");
    write(root + "/set.m3u", "disc.cue\n");
    std::atomic_bool cancel{false}; QVariantMap index;
    const auto first = ac::library::scan({root}, {}, cancel, 6, &index);
    QCOMPARE(files(ac::library::copies(first, "ps2", "virtua-cop")).size(), 2);
    const auto cueBefore = QFileInfo(root + "/disc.cue").lastModified(); const auto listBefore = QFileInfo(root + "/set.m3u").lastModified();
    // Only the referenced BIN changes, with exactly the old size. Sheet cache entries must not hide it.
    auto replacement = ps2Iso("SLES_512.29"); replacement.replace(21 * 2048, 5, "BOOT "); write(root + "/disc.bin", replacement);
    const auto second = ac::library::scan({root}, {}, cancel, 6, &index);
    QCOMPARE(QFileInfo(root + "/disc.cue").lastModified(), cueBefore); QCOMPARE(QFileInfo(root + "/set.m3u").lastModified(), listBefore);
    QVERIFY(ac::library::copies(second, "ps2", "virtua-cop").isEmpty()); QCOMPARE(second.value("systems").toMap().value("ps2").toMap().value("other").toInt(), 0);
    QCOMPARE(second.value("systems").toMap().value("ps1").toMap().value("other").toInt(), 2);
    // A pre-reader-rules cache with forged old PS2 outcomes must be discarded even when size/time match.
    auto old = index; auto entries = old.value("files").toMap();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
      auto entry = it.value().toMap(); entry["outcome"] = QVariantMap{{"other", "ps2"}}; it.value() = entry;
    }
    old["files"] = entries; old["stamp"] = "old-disc-reader-rules-v1";
    const auto fresh = ac::library::scan({root}, {}, cancel, 6, &old);
    QCOMPARE(fresh.value("reused").toInt(), 0); QCOMPARE(fresh.value("systems"), second.value("systems"));
  }
  void arcadeAliasAndMisleadingDiscNames() {
    QTemporaryDir t; QVERIFY(t.isValid()); const auto root = t.filePath("roms");
    write(root + "/model2/hotd.zip", zip("authored.bin", "metadata fixture"));
    write(root + "/ps2/virtua-cop.iso", ps2Iso("SLUS_999.98"));
    auto ps1 = ps2Iso("SLES_512.29"); ps1.replace(21 * 2048, 5, "BOOT "); write(root + "/ps2/virtua-cop-ps1.iso", ps1);
    std::atomic_bool cancel{false}; const auto result = ac::library::scan({root}, {}, cancel);
    QCOMPARE(files(ac::library::copies(result, "model2", "hotdo")), QStringList({"hotd.zip"}));
    QVERIFY(ac::library::copies(result, "ps2", "virtua-cop").isEmpty());
    QCOMPARE(result.value("systems").toMap().value("ps2").toMap().value("other").toInt(), 1);
    QCOMPARE(result.value("systems").toMap().value("ps1").toMap().value("other").toInt(), 1);
  }
  void gameCubeUsesTheFrontendSystemKey(){
    QTemporaryDir t;auto header=wiiIso("GFZE01");qToBigEndian<quint32>(0,header.data()+24);qToBigEndian<quint32>(0xc2339f3d,header.data()+28);write(t.filePath("longer-disc.iso"),header);
    QByteArray packed(44,0);qToLittleEndian<quint32>(0xb10bc001,packed.data());qToLittleEndian<quint64>(quint64(header.size()),packed.data()+8);qToLittleEndian<quint64>(quint64(header.size()),packed.data()+16);
    qToLittleEndian<quint32>(quint32(header.size()),packed.data()+24);qToLittleEndian<quint32>(1,packed.data()+28);qToLittleEndian<quint64>(quint64(1)<<63,packed.data()+32);
    quint32 a=1,b=0;for(const auto byte:header){a=(a+quint8(byte))%65521;b=(b+a)%65521;}qToLittleEndian<quint32>((b<<16)|a,packed.data()+40);write(t.filePath("a.gcz"),packed+header);
    std::atomic_bool cancel=false;const auto result=ac::library::scan({t.path()},{},cancel);const auto copies=ac::library::copies(result,"gamecube","gc-f-zero-gx");QCOMPARE(copies.size(),2);QVERIFY(copies.first().toMap().value("file").toString().endsWith("longer-disc.iso"));QVERIFY(!result.value("systems").toMap().contains("gc"));
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
  // The second search uses the first one's reading of each unchanged file and opens only what is new or changed;
  // the result is the same as reading everything again.
  void anUnchangedFileIsNotReadAgain() {
    QTemporaryDir t;
    const auto root = t.filePath("roms");
    write(root + "/wii/umbrella.wbfs", wbfs("RBUE08"));
    write(root + "/ps2/disc.iso", ps2Iso("SLES_512.29"));
    write(root + "/dc/hotd2.cdi", QByteArray(3000, 'x') + dreamcastTrack("MK-51002"));
    write(root + "/arcade/scud.zip", zip("epr-19731.13", "a"));
    write(root + "/pinball/one.vpx", "vpx");
    write(root + "/junk/not a disc.iso", QByteArray(4096, 'j'));
    std::atomic_bool cancel = false;
    QVariantMap index;
    const auto first = ac::library::scan({root}, {}, cancel, 6, &index);
    QCOMPARE(first.value("reused").toInt(), 0);
    QCOMPARE(index.value("files").toMap().size(), 6);
    // Saved and read back as the Hub does, between two runs.
    index = QJsonDocument::fromJson(QJsonDocument::fromVariant(index).toJson()).toVariant().toMap();
    const auto second = ac::library::scan({root}, {}, cancel, 6, &index);
    QCOMPARE(second.value("reused").toInt(), 6);
    QCOMPARE(QJsonDocument::fromVariant(second.value("systems")).toJson(), QJsonDocument::fromVariant(first.value("systems")).toJson());
    QCOMPARE(second.value("unrecognised"), first.value("unrecognised"));
    QCOMPARE(second.value("pinballFolders"), first.value("pinballFolders"));
    // A deferred timed read is retried even when the file itself has not changed.
    auto deferred=index;auto records=deferred.value("files").toMap();const auto key=QDir::fromNativeSeparators(root+"/ps2/disc.iso");auto record=records.value(key).toMap();
    record["outcome"]=QVariantMap{{"unknown","disc"},{"retry",true}};records[key]=record;deferred["files"]=records;
    const auto retried=ac::library::scan({root},{},cancel,6,&deferred);QCOMPARE(retried.value("reused").toInt(),5);QCOMPARE(ac::library::copies(retried,"ps2","virtua-cop").size(),1);
    // A file replaced by another game is read again; a new one is read; one removed is gone from the result.
    write(root + "/wii/umbrella.wbfs", wbfs("RBUP08") + QByteArray(16, '\0'));
    write(root + "/wii/more.iso", wiiIso("RBUE08"));
    QFile::remove(root + "/ps2/disc.iso");
    const auto third = ac::library::scan({root}, {}, cancel, 6, &index);
    QCOMPARE(third.value("reused").toInt(), 4);
    QCOMPARE(files(ac::library::copies(third, "wii", "umbrella")), QStringList({"more.iso", "umbrella.wbfs"}));
    QVERIFY(ac::library::copies(third, "ps2", "virtua-cop").isEmpty());
    QVERIFY(!index.value("files").toMap().contains(QDir::fromNativeSeparators(root + "/ps2/disc.iso")));
    // An index made against other lists of games is not used.
    index["stamp"] = "older";
    QCOMPARE(ac::library::scan({root}, {}, cancel, 6, &index).value("reused").toInt(), 0);
    // Stopped before anything was read: the result says so, and the index keeps every last reading.
    const auto kept = index.value("files").toMap().size();
    cancel = true;
    QVERIFY(ac::library::scan({root}, {}, cancel, 6, &index).value("cancelled").toBool());
    QCOMPARE(index.value("files").toMap().size(), kept);
  }
};
QTEST_GUILESS_MAIN(LibraryTest)
#include "LibraryTest.moc"
