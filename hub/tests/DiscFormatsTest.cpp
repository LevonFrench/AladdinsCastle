// SPDX-License-Identifier: GPL-3.0-only
// Authored metadata fixtures only; no game executable or media content.
#include "core/scan/Scan.h"
#include "core/scan/DiscReader.h"
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
#include <atomic>
#include <thread>
#include <zlib.h>
namespace {
void le32(QByteArray &b, qsizetype p, quint32 v) { qToLittleEndian(v, b.data() + p); }
void le64(QByteArray &b, qsizetype p, quint64 v) { qToLittleEndian(v, b.data() + p); }
void be32(QByteArray &b, qsizetype p, quint32 v) { qToBigEndian(v, b.data() + p); }
QByteArray packed(const QByteArray &bytes, int window = -15) {
  z_stream s{};
  if (deflateInit2(&s, Z_BEST_SPEED, Z_DEFLATED, window, 8, Z_DEFAULT_STRATEGY) != Z_OK) return {};
  QByteArray result(static_cast<qsizetype>(compressBound(static_cast<uLong>(bytes.size()))) + 64, '\0');
  s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(bytes.constData()));
  s.avail_in = static_cast<uInt>(bytes.size());
  s.next_out = reinterpret_cast<Bytef *>(result.data());
  s.avail_out = static_cast<uInt>(result.size());
  const int status = deflate(&s, Z_FINISH);
  result.resize(static_cast<qsizetype>(s.total_out));
  deflateEnd(&s);
  return status == Z_STREAM_END ? result : QByteArray();
}
QByteArray iso(bool ps2) {
  QByteArray b(24 * 2048, '\0');
  const QByteArray cnf = ps2 ? "BOOT2 = cdrom0:\\SLUS_999.98;1\r\n" : "BOOT = cdrom:\\SLUS_999.98;1\r\n";
  const qsizetype p = 16 * 2048;
  b[p] = 1; b.replace(p + 1, 5, "CD001"); b[p + 6] = 1;
  b[p + 156] = 34; le32(b, p + 158, 20); le32(b, p + 166, 2048);
  const qsizetype d = 20 * 2048;
  b[d] = 46; le32(b, d + 2, 21); le32(b, d + 10, static_cast<quint32>(cnf.size()));
  b[d + 32] = 12; b.replace(d + 33, 12, "SYSTEM.CNF;1");
  b.replace(21 * 2048, cnf.size(), cnf);
  return b;
}
QByteArray cso(const QByteArray &image, bool compressed) {
  const int blocks = static_cast<int>(image.size() / 2048);
  QByteArray b(24 + (blocks + 1) * 4, '\0');
  b.replace(0, 4, "CISO"); le32(b, 4, 24); le64(b, 8, static_cast<quint64>(image.size()));
  le32(b, 16, 2048); b[20] = 1;
  for (int i = 0; i < blocks; ++i) {
    le32(b, 24 + i * 4, static_cast<quint32>(b.size()) | (compressed ? 0U : 0x80000000U));
    b += compressed ? packed(image.mid(i * 2048, 2048)) : image.mid(i * 2048, 2048);
  }
  le32(b, 24 + blocks * 4, static_cast<quint32>(b.size()));
  return b;
}
QByteArray nintendo(bool wii) {
  QByteArray b(0x400, '\0'); b.replace(0, 6, wii ? "RZZE01" : "GZZE01");
  be32(b, wii ? 0x18 : 0x1c, wii ? 0x5d1c9ea3U : 0xc2339f3dU);
  return b;
}
QByteArray gcz(bool compressed) {
  const QByteArray image = nintendo(false);
  const QByteArray payload = compressed ? packed(image, 15) : image;
  QByteArray b(44, '\0'); le32(b, 0, 0xb10bc001U);
  le64(b, 8, static_cast<quint64>(payload.size())); le64(b, 16, static_cast<quint64>(image.size()));
  le32(b, 24, 1024); le32(b, 28, 1); le64(b, 32, compressed ? 0 : (quint64(1) << 63));
  le32(b, 40, static_cast<quint32>(adler32(0, reinterpret_cast<const Bytef *>(payload.constData()), static_cast<uInt>(payload.size()))));
  return b + payload;
}
bool write(const QString &path, const QByteArray &bytes) {
  QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}
ac::scan::FileIdentity inspect(const QString &path) { std::atomic_bool cancel{false}; return ac::scan::Scanner::inspect(path, cancel); }
}
class DiscFormatsTest : public QObject {
  Q_OBJECT
private slots:
  void playstation_data() {
    QTest::addColumn<QString>("extension"); QTest::addColumn<QByteArray>("bytes"); QTest::addColumn<QString>("system");
    QTest::newRow("ps1-boot") << QString("iso") << iso(false) << QString("ps1");
    QTest::newRow("ps2-boot2") << QString("iso") << iso(true) << QString("ps2");
    QTest::newRow("ciso-raw") << QString("cso") << cso(iso(true), false) << QString("ps2");
    QTest::newRow("ciso-deflate") << QString("cso") << cso(iso(true), true) << QString("ps2");
    QTest::newRow("gzip") << QString("gz") << packed(iso(true), 31) << QString("ps2");
  }
  void playstation() {
    QFETCH(QString, extension); QFETCH(QByteArray, bytes); QFETCH(QString, system);
    QTemporaryDir d; QVERIFY(d.isValid()); const auto p = d.filePath("unrelated." + extension); QVERIFY(write(p, bytes));
    const auto r = inspect(p); QCOMPARE(r.identity, QString("SLUS-99998")); QCOMPARE(r.system, system); QCOMPARE(r.kind, QString("disc-serial"));
  }
  void pbpBlocks_data() { QTest::addColumn<bool>("compressed"); QTest::newRow("raw") << false; QTest::newRow("deflated") << true; }
  void pbpBlocks() {
    QFETCH(bool, compressed);
    const QByteArray logical = iso(false); QByteArray raw(24 * 2352, '\0');
    for (int i = 0; i < 24; ++i) raw.replace(i * 2352 + 24, 2048, logical.mid(i * 2048, 2048));
    const int blockSize = 16 * 2352; raw.append(QByteArray(2 * blockSize - raw.size(), '\0'));
    const int psar = 0x100; QByteArray b(psar + 0x100000, '\0'); b.replace(0, 4, QByteArray("\0PBP", 4));
    le32(b, 4, 0x00010000); for (int i = 0; i < 8; ++i) le32(b, 8 + i * 4, psar);
    b.replace(psar, 12, "PSISOIMG0000"); le32(b, psar + 0xbfc, 0x100000);
    quint32 offset = 0;
    for (int i = 0; i < 2; ++i) {
      const auto block = raw.mid(i * blockSize, blockSize); const auto payload = compressed ? packed(block) : block;
      le32(b, psar + 0x4000 + i * 32, offset); le32(b, psar + 0x4004 + i * 32, static_cast<quint32>(payload.size()));
      offset += static_cast<quint32>(payload.size()); b += payload;
    }
    QTemporaryDir d; QVERIFY(d.isValid()); const auto p = d.filePath("unrelated.pbp"); QVERIFY(write(p, b));
    const auto r = inspect(p); QCOMPARE(r.identity, QString("SLUS-99998")); QCOMPARE(r.system, QString("ps1"));
    // A table entry cannot redirect the decompressor outside the file.
    le32(b, psar + 0x4000 + 32, 0xfffffff0U); QVERIFY(write(p, b)); QVERIFY(inspect(p).identity.isEmpty());
  }
  void wadTicket() {
    QByteArray b(0x40 + 0x2a4, '\0'); be32(b, 0, 0x20); b[4] = 'I'; b[5] = 's'; be32(b, 0x10, 0x2a4);
    be32(b, 0x40, 0x00010001); be32(b, 0x40 + 0x1dc, 0x00010001); be32(b, 0x40 + 0x1e0, 0x575a5a45);
    QTemporaryDir d; QVERIFY(d.isValid()); const auto p = d.filePath("unrelated.wad"); QVERIFY(write(p, b));
    const auto r = inspect(p); QCOMPARE(r.identity, QString("00010001575A5A45")); QCOMPARE(r.system, QString("wii"));
    b.truncate(0x40 + 0x1e3); QVERIFY(write(p, b)); QVERIFY(inspect(p).identity.isEmpty());
  }
  void cueAndPlaylist() {
    QTemporaryDir d; QVERIFY(d.isValid());
    QVERIFY(write(d.filePath("track.bin"), iso(true)));
    QVERIFY(write(d.filePath("disc.cue"), "FILE \"track.bin\" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n"));
    QVERIFY(write(d.filePath("second.iso"), iso(false)));
    QVERIFY(write(d.filePath("set.m3u"), "# first disc is authoritative\n\ndisc.cue\nsecond.iso\n"));
    for (const auto &name : {"disc.cue", "set.m3u"}) { const auto r = inspect(d.filePath(name)); QCOMPARE(r.identity, QString("SLUS-99998")); QCOMPARE(r.system, QString("ps2")); }
    QVERIFY(write(d.filePath("loop.m3u"), "loop.m3u\n")); QVERIFY(inspect(d.filePath("loop.m3u")).identity.isEmpty());
  }
  void nintendoContainers_data() {
    QTest::addColumn<QString>("extension"); QTest::addColumn<QByteArray>("bytes"); QTest::addColumn<QString>("system"); QTest::addColumn<QString>("id");
    QTest::newRow("wii-iso") << QString("iso") << nintendo(true) << QString("wii") << QString("RZZE01");
    QTest::newRow("gc-gcm") << QString("gcm") << nintendo(false) << QString("gc") << QString("GZZE01");
    QTest::newRow("nkit-iso") << QString("nkit.iso") << nintendo(true) << QString("wii") << QString("RZZE01");
    QTest::newRow("gcz-raw") << QString("gcz") << gcz(false) << QString("gc") << QString("GZZE01");
    QTest::newRow("gcz-deflate") << QString("gcz") << gcz(true) << QString("gc") << QString("GZZE01");
    QByteArray wbfs(0x200, '\0'); wbfs.replace(0, 4, "WBFS"); be32(wbfs, 4, 3); wbfs[8] = 9; wbfs[9] = 20; wbfs[12] = 1;
    QTest::newRow("wbfs") << QString("wbfs") << (wbfs + nintendo(true)) << QString("wii") << QString("RZZE01");
    for (bool rvz : {false, true}) {
      QByteArray b(0x58 + 0x80, '\0'); b.replace(0, 4, QByteArray(rvz ? "RVZ\1" : "WIA\1", 4));
      be32(b, 4, 0x01000000); be32(b, 8, 0x01000000); be32(b, 12, 0x80);
      b.replace(0x58, 0x80, nintendo(true).left(0x80));
      QTest::newRow(rvz ? "rvz" : "wia") << QString(rvz ? "rvz" : "wia") << b << QString("wii") << QString("RZZE01");
    }
  }
  void nintendoContainers() {
    QFETCH(QString, extension); QFETCH(QByteArray, bytes); QFETCH(QString, system); QFETCH(QString, id);
    QTemporaryDir d; QVERIFY(d.isValid()); const auto p = d.filePath("unrelated." + extension); QVERIFY(write(p, bytes));
    const auto r = inspect(p); QCOMPARE(r.identity, id); QCOMPARE(r.system, system);
  }
  void gzipDistantMultiSectorRoot() {
    auto image = iso(true); const quint32 root = 100 * 1024 * 1024 / 2048;
    const auto record = image.mid(20 * 2048, 46); const auto cnf = image.mid(21 * 2048, 128);
    image = QByteArray((root + 7) * 2048, '\0');
    const auto original = iso(true); image.replace(16 * 2048, 2048, original.mid(16 * 2048, 2048));
    le32(image, 16 * 2048 + 158, root); le32(image, 16 * 2048 + 166, 6 * 2048);
    image.replace((root + 5) * 2048, record.size(), record); le32(image, (root + 5) * 2048 + 2, root + 6);
    image.replace((root + 6) * 2048, cnf.size(), cnf);
    QTemporaryDir d; QVERIFY(d.isValid()); const auto p = d.filePath("distant.gz"); QVERIFY(write(p, packed(image, 31)));
    const auto r = inspect(p); QCOMPARE(r.system, QString("ps2")); QCOMPARE(r.identity, QString("SLUS-99998"));
  }
  void audioFirstCueWithDataIndex() {
    const auto logical = iso(true); QByteArray raw((75 + 24) * 2352, '\0');
    for (int i = 0; i < 24; ++i) raw.replace((75 + i) * 2352 + 24, 2048, logical.mid(i * 2048, 2048));
    QTemporaryDir d; QVERIFY(d.isValid()); QVERIFY(write(d.filePath("tracks.bin"), raw));
    QVERIFY(write(d.filePath("disc.cue"), "FILE \"tracks.bin\" BINARY\n TRACK 01 AUDIO\n INDEX 01 00:00:00\n TRACK 02 MODE2/2352\n INDEX 00 00:00:70\n INDEX 01 00:01:00\n"));
    const auto r = inspect(d.filePath("disc.cue")); QCOMPARE(r.system, QString("ps2")); QCOMPARE(r.identity, QString("SLUS-99998"));
  }
  void gdiThirdTrackFileOffset() {
    QByteArray header(256, ' '); header.replace(0, 16, "SEGA SEGAKATANA  "); header.replace(0x40, 10, "T-99999N  ");
    QByteArray track(4096, '\0'); track += header;
    QTemporaryDir d; QVERIFY(d.isValid()); QVERIFY(write(d.filePath("track03.bin"), track));
    QVERIFY(write(d.filePath("disc.gdi"), "3\n1 0 4 2352 absent01.bin 0\n2 450 0 2352 absent02.bin 0\n3 45000 4 2352 track03.bin 4096\n"));
    const auto r = inspect(d.filePath("disc.gdi")); QCOMPARE(r.system, QString("dreamcast")); QCOMPARE(r.identity, QString("T-99999N"));
  }
  void lateDreamcastHeader() {
    QTemporaryDir d; QVERIFY(d.isValid()); const auto p = d.filePath("unrelated.cdi"); QFile f(p); QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.resize(17 * 1024 * 1024)); QVERIFY(f.seek(f.size())); QByteArray header(256, ' ');
    header.replace(0, 16, "SEGA SEGAKATANA  "); header.replace(0x40, 10, "T-99999N  "); QCOMPARE(f.write(header), qint64(header.size())); f.close();
    const auto r = inspect(p); QCOMPARE(r.identity, QString("T-99999N")); QCOMPARE(r.system, QString("dreamcast"));
  }
  void invalidBounds_data() {
    QTest::addColumn<QString>("extension"); QTest::addColumn<QByteArray>("bytes");
    auto bad = cso(iso(true), true); le32(bad, 24 + 16 * 4, 0x7fffffffU); QTest::newRow("cso-offset") << QString("cso") << bad;
    bad = cso(iso(true), false); le64(bad, 8, ~quint64(0)); QTest::newRow("cso-size-overflow") << QString("cso") << bad;
    bad = cso(iso(true), true); bad.truncate(28); QTest::newRow("cso-truncated-table") << QString("cso") << bad;
    bad = iso(true); le32(bad, 16 * 2048 + 158, 0xffffffffU); QTest::newRow("iso-root-overflow") << QString("iso") << bad;
    bad = gcz(false); le64(bad, 32, 0x7fffffffffffffffULL); QTest::newRow("gcz-offset") << QString("gcz") << bad;
    bad = packed(iso(true), 31); bad.truncate(12); QTest::newRow("gzip-truncated") << QString("gz") << bad;
    bad = nintendo(true); be32(bad, 0x18, 0); QTest::newRow("nintendo-id-without-magic") << QString("iso") << bad;
    QTest::newRow("filename-is-not-proof") << QString("iso") << QByteArray("SLUS_999.98 RZZE01");
  }
  void invalidBounds() {
    QFETCH(QString, extension); QFETCH(QByteArray, bytes); QTemporaryDir d; QVERIFY(d.isValid());
    const auto p = d.filePath("SLUS_999.98." + extension); QVERIFY(write(p, bytes)); QVERIFY(inspect(p).identity.isEmpty());
  }
  void cancellation() {
    QTemporaryDir d; QVERIFY(d.isValid()); const auto p = d.filePath("large.cdi"); QFile f(p); QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.resize(qint64(2) * 1024 * 1024 * 1024)); f.close();
    std::atomic_bool cancel{true}; QVERIFY(ac::scan::Scanner::inspect(p, cancel).identity.isEmpty());
    cancel = false; std::atomic_bool entered{false}; ac::scan::FileIdentity result; QElapsedTimer timer; timer.start();
    std::thread worker([&] { entered = true; result = ac::scan::Scanner::inspect(p, cancel); });
    while (!entered.load()) std::this_thread::yield();
    QTest::qSleep(2); cancel = true; worker.join();
    QVERIFY(result.identity.isEmpty()); QVERIFY2(timer.elapsed() < 5000, "Cancellation must bound the long CDI read");
  }
  void cancellationDuringAnActualRead(){
    QTemporaryDir d;QFile file(d.filePath("authored.cdi"));QVERIFY(file.open(QIODevice::ReadWrite));QCOMPARE(file.write(QByteArray(4*1024*1024,0)),qint64(4*1024*1024));
    std::atomic_bool cancel{false};int reads=0;QString error;
    const auto reader=[&](qint64 at,qint64 bytes){if(!file.seek(at))return QByteArray();auto piece=file.read(bytes);if(++reads==2)cancel=true;return piece;};
    QVERIFY(ac::scan::detail::scanKatana(reader,file.size(),cancel,error).isEmpty());QCOMPARE(reads,2);QCOMPARE(error,QString("Cancelled"));
  }
  void protectedNamesAreNeverDiscReferences(){
    QTemporaryDir d;QVERIFY(write(d.filePath("auth.bin"),iso(true)));QVERIFY(write(d.filePath("disc.cue"),"FILE \"auth.bin\" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n"));
    const auto direct=inspect(d.filePath("auth.bin"));QCOMPARE(direct.error,QString("Protected media name"));QVERIFY(direct.identity.isEmpty());QVERIFY(inspect(d.filePath("disc.cue")).identity.isEmpty());
  }
};
QTEST_GUILESS_MAIN(DiscFormatsTest)
#include "DiscFormatsTest.moc"
