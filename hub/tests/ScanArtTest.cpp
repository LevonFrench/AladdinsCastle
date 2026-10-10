// SPDX-License-Identifier: GPL-3.0-only
#include "core/art/Art.h"
#include "core/scan/HeaderSafety.h"
#include "core/scan/Scan.h"
#include "core/scan/ScanController.h"
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtEndian>
#include <QtTest>
namespace {
void save(const QString &path, const QByteArray &b) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly));
  QCOMPARE(f.write(b), b.size());
}
void append16(QByteArray &b, quint16 v) {
  char x[2];
  qToLittleEndian(v, x);
  b.append(x, 2);
}
void append32(QByteArray &b, quint32 v) {
  char x[4];
  qToLittleEndian(v, x);
  b.append(x, 4);
}
void append64(QByteArray &b, quint64 v) {
  char x[8];
  qToLittleEndian(v, x);
  b.append(x, 8);
}
quint32 crc32(const QByteArray &bytes) {
  quint32 crc = 0xffffffffU;
  for (const auto value : bytes) {
    crc ^= static_cast<unsigned char>(value);
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320U : 0U);
  }
  return ~crc;
}
QByteArray zip(const QVector<QPair<quint32, quint32>> &entries) {
  QByteArray cd;
  int n = 0;
  for (const auto &[crc, size] : entries) {
    QByteArray name = "fixture" + QByteArray::number(n++);
    append32(cd, 0x02014b50);
    for (int i = 0; i < 6; ++i)
      append16(cd, 0);
    append32(cd, crc);
    append32(cd, 0);
    append32(cd, size);
    append16(cd, quint16(name.size()));
    for (int i = 0; i < 4; ++i)
      append16(cd, 0);
    append32(cd, 0);
    append32(cd, 0);
    cd += name;
  }
  QByteArray out = cd;
  append32(out, 0x06054b50);
  append16(out, 0);
  append16(out, 0);
  append16(out, quint16(entries.size()));
  append16(out, quint16(entries.size()));
  append32(out, quint32(cd.size()));
  append32(out, 0);
  append16(out, 0);
  return out;
}
ac::CatalogData catalog(QString kind = "mame-romset") {
  ac::CatalogData c;
  ac::GameRecord g;
  g.id = "synthetic";
  g.raw = {{"title", "Synthetic Test"},
           {"manufacturer", "Synthetic"},
           {"hardware", "sony-ps2"},
           {"hub", {{"accent", "#ff0000"}, {"colour", "#001122"}}},
           {"media", ac::Json::array(
                         {{{"kind", kind.toStdString()}, {"set", "test"}}})}};
  g.runtime.gameId = g.id;
  g.runtime.lastPlayed = 42;
  g.runtime.selectedVariantId = "kept";
  c.games << g;
  return c;
}
QByteArray iso() {
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
  const QByteArray boot = "BOOT2 = cdrom0:\\SLUS_202.19;1\r\n";
  std::memcpy(data.data() + 21 * 2048, boot.data(), size_t(boot.size()));
  return data;
}
QByteArray syntheticChd(quint64 logical = 24 * 2048) {
  constexpr quint32 hunk = 4096;
  const auto hunks = (logical + hunk - 1) / hunk;
  QByteArray bytes(static_cast<qsizetype>(124 + hunks * 4), 0);
  std::memcpy(bytes.data(), "MComprHD", 8);
  qToBigEndian<quint32>(124, bytes.data() + 8);
  qToBigEndian<quint32>(5, bytes.data() + 12);
  qToBigEndian<quint64>(logical, bytes.data() + 32);
  qToBigEndian<quint64>(124, bytes.data() + 40);
  qToBigEndian<quint32>(hunk, bytes.data() + 56);
  qToBigEndian<quint32>(2048, bytes.data() + 60);
  const auto image = iso();
  const auto digest = QCryptographicHash::hash(image, QCryptographicHash::Sha1);
  std::memcpy(bytes.data() + 84, digest.constData(), 20);
  if (logical == quint64(image.size())) {
    const auto first = (quint64(bytes.size()) + hunk - 1) / hunk;
    for (quint64 i = 0; i < hunks; ++i)
      qToBigEndian<quint32>(static_cast<quint32>(first + i), bytes.data() + 124 + i * 4);
    bytes.resize(static_cast<qsizetype>(first * hunk));
    bytes += image;
  } else {
    // A valid map ends before EOF, even when every sparse block is zero.
    bytes.append('\0');
  }
  return bytes;
}
} // namespace
class ScanArtTest : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
#ifdef Q_OS_WIN
    // The offscreen plugin has no native Windows font database. Use an already
    // installed OS font for text assertions; never download or redistribute it.
    QVERIFY(QFontDatabase::addApplicationFont(
                QDir(qEnvironmentVariable("WINDIR"))
                    .filePath("Fonts/segoeui.ttf")) >= 0);
#endif
  }
  void zipMetadata() {
    QTemporaryDir t;
    auto p = t.filePath("renamed.zip");
    save(p, zip({{0x12345678, 100}}));
    std::atomic_bool stop = false;
    auto f = ac::scan::Scanner::inspect(p, stop);
    QVERIFY(f.error.isEmpty());
    QCOMPARE(f.entries.size(), 1);
    QCOMPARE(f.entries[0].crc, 0x12345678U);
    QCOMPARE(f.entries[0].size, quint64(100));
  }
  void sevenMetadata() {
    QTemporaryDir t;
    auto p = t.filePath("renamed.7z");
    save(p, QByteArray::fromBase64(
                "N3q8ryccAASui8bPfgAAAAAAAAAUAAAAAAAAAOKz5/"
                "gBABpzeW50aGV0aWMgc2Nhbm5lciB0ZXN0IG9ubHkA4ABcAFddAACBMweuD8/"
                "88GwP1GpefeXX3Qg9eTikvynYlW+x4jrYy1J2IM/"
                "VJqNi2OAHnZWI84enIGNVnJOXcYVj5z4mUHzBwp0CqJUZmmIJJ4fGNbs1/"
                "9ADAAAAAAAXBh8BCV8ABwsBAAEhIQEYDF0AAA=="));
    std::atomic_bool stop = false;
    auto f = ac::scan::Scanner::inspect(p, stop);
    QVERIFY2(f.error.isEmpty(), qPrintable(f.error));
    QCOMPARE(f.entries.size(), 1);
    QCOMPARE(f.entries[0].size, quint64(27));
    QVERIFY(f.entries[0].hasCrc);
  }
  void sevenMissingOptionalNames() {
    // A plain, synthetic 7z FilesInfo table with one empty file. Name is an
    // optional property in the format; its absence must remain unverified.
    const auto next = QByteArray::fromHex("0105010e01800f01800000");
    QByteArray start;
    append64(start, 0);
    append64(start, quint64(next.size()));
    append32(start, crc32(next));
    auto archive = QByteArray::fromHex("377abcaf271c0004");
    append32(archive, crc32(start));
    archive += start + next;
    QTemporaryDir t;
    auto path = t.filePath("missing-name.7z");
    save(path, archive);
    std::atomic_bool stop = false;
    const auto result = ac::scan::Scanner::inspect(path, stop);
    QVERIFY2(result.error.contains("names missing"), qPrintable(result.error));
    QVERIFY(result.entries.isEmpty());
  }
  void boundedChdReferences() {
    QByteArray map(40 * 12, 0);
    for (int i = 0; i < 40; ++i)
      map[i * 12] = 4;
    const auto reference = [&](int from, int to) {
      map[from * 12] = 5;
      map[from * 12 + 9] = char(to);
    };
    const auto resolve = [&](quint32 requested, quint32 &terminal,
                             QString &error, std::atomic_bool &stop) {
      return ac::scan::detail::resolveChdHunk(
          reinterpret_cast<const unsigned char *>(map.constData()),
          quint64(map.size()), 40, 12, true, requested, terminal, stop, error);
    };
    std::atomic_bool stop = false;
    quint32 terminal = 0;
    QString error;
    reference(0, 1);
    QVERIFY(resolve(0, terminal, error, stop));
    QCOMPARE(terminal, 1U);
    reference(1, 0);
    QVERIFY(!resolve(0, terminal, error, stop));
    QVERIFY(error.contains("cycle"));
    reference(0, 40);
    QVERIFY(!resolve(0, terminal, error, stop));
    QVERIFY(error.contains("out of bounds"));
    for (int i = 0; i < 33; ++i)
      reference(i, i + 1);
    QVERIFY(!resolve(0, terminal, error, stop));
    QVERIFY(error.contains("32 map steps"));
    map[0] = 6;
    QVERIFY(!resolve(0, terminal, error, stop));
    QVERIFY(error.contains("unsupported indirect"));
    map[0] = 4;
    stop = true;
    QVERIFY(!resolve(0, terminal, error, stop));
    QVERIFY(error.contains("cancelled"));
    stop = false;
    QVERIFY(!ac::scan::detail::resolveChdHunk(
        reinterpret_cast<const unsigned char *>(map.constData()), 12, 40, 12,
        true, 0, terminal, stop, error));
    QVERIFY(error.contains("map bounds"));
    QVERIFY(!ac::scan::detail::resolveChdHunk(
        reinterpret_cast<const unsigned char *>(map.constData()), 32ULL * 1024 * 1024 + 4,
        1, 4, false, 0, terminal, stop, error));
    QVERIFY(error.contains("map bounds"));
    QVERIFY(!ac::scan::detail::resolveChdHunk(
        reinterpret_cast<const unsigned char *>(map.constData()), quint64(map.size()),
        32U * 1024 * 1024 / 4 + 1, 4, false, 0, terminal, stop, error));
    QVERIFY(error.contains("map bounds"));
  }
  void serialHeaders() {
    QTemporaryDir t;
    auto p = t.filePath("unrelated-name.iso");
    save(p, iso());
    std::atomic_bool stop = false;
    auto f = ac::scan::Scanner::inspect(p, stop);
    QCOMPARE(f.identity, QString("SLUS-20219"));
    QCOMPARE(f.kind, QString("disc-serial"));
    ac::scan::ScanOptions o;
    o.mediaRoots = {t.path()};
    o.userRoot = t.filePath("user");
    o.serialIndex = t.filePath("serial.json");
    save(o.serialIndex, "{\"SLUS-20219\":{\"gameId\":\"synthetic\"}}");
    auto c = catalog("disc");
    c.games[0].raw["media"][0].erase("set");
    auto r = ac::scan::Scanner::run(c, o, stop);
    QVERIFY(r.bindings[0].verified);
    QCOMPARE(r.bindings[0].requirementId, QString("synthetic-media-0"));
    QCOMPARE(r.states[0].lastPlayed, qint64(42));
    QCOMPARE(r.states[0].selectedVariantId, QString("kept"));
  }
  void otherDiscHeaders() {
    QTemporaryDir t;
    std::atomic_bool stop = false;
    QByteArray b(124, 0);
    std::memcpy(b.data(), "MComprHD", 8);
    qToBigEndian<quint32>(124, b.data() + 8);
    qToBigEndian<quint32>(5, b.data() + 12);
    std::memcpy(b.data() + 84, "01234567890123456789", 20);
    auto p = t.filePath("header.chd");
    save(p, b);
    auto f = ac::scan::Scanner::inspect(p, stop);
    QCOMPARE(f.kind, QString("chd-sha1"));
    QCOMPARE(f.identity, QString::fromLatin1(b.mid(84, 20).toHex()));
    b = QByteArray(256, ' ');
    std::memcpy(b.data(), "SEGA SEGAKATANA", 15);
    std::memcpy(b.data() + 64, "T-12345   ", 10);
    p = t.filePath("header.bin");
    save(p, b);
    f = ac::scan::Scanner::inspect(p, stop);
    QCOMPARE(f.identity, QString("T-12345"));
    b = QByteArray(256, ' ');
    std::memcpy(b.data(), "SEGA SEGASATURN ", 16);
    std::memcpy(b.data() + 32, "T-54321   ", 10);
    save(p, b);
    f = ac::scan::Scanner::inspect(p, stop);
    QCOMPARE(f.identity, QString("T-54321"));
    b = QByteArray(256, 0);
    std::memcpy(b.data(), "GTEST1", 6);
    qToBigEndian<quint32>(0xc2339f3d, b.data() + 28);
    save(p, b);
    f = ac::scan::Scanner::inspect(p, stop);
    QCOMPARE(f.identity, QString("GTEST1"));
    qToBigEndian<quint32>(0, b.data() + 28);
    qToBigEndian<quint32>(0x5d1c9ea3, b.data() + 24);
    save(p, b);
    f = ac::scan::Scanner::inspect(p, stop);
    QCOMPARE(f.identity, QString("GTEST1"));
  }
  void rawPs1SectorsAndBios() {
    QTemporaryDir t;
    std::atomic_bool stop = false;
    const auto logical = iso();
    QByteArray raw(24 * 2352, 0);
    for (int sector = 0; sector < 24; ++sector)
      std::memcpy(raw.data() + sector * 2352 + 24,
                  logical.constData() + sector * 2048, 2048);
    auto p = t.filePath("raw.bin");
    save(p, raw);
    auto f = ac::scan::Scanner::inspect(p, stop);
    QCOMPARE(f.identity, QString("SLUS-20219"));
    QByteArray bios(512 * 1024, 0);
    auto *d = bios.data() + 128;
    std::memcpy(d, "RESET", 5);
    qToLittleEndian<quint32>(128, d + 12);
    std::memcpy(d + 16, "ROMDIR", 6);
    qToLittleEndian<quint32>(64, d + 28);
    std::memcpy(d + 32, "EXTINFO", 7);
    std::memcpy(d + 48, "ROMVER", 6);
    qToLittleEndian<quint32>(14, d + 60);
    std::memcpy(bios.data() + 192, "0200EC20040614", 14);
    save(p, bios);
    f = ac::scan::Scanner::inspect(p, stop);
    QCOMPARE(f.kind, QString("ps2-bios-romdir"));
    QCOMPARE(f.identity, QString("0200EC20040614"));
    auto rom0 = t.filePath("synthetic.rom0");
    save(rom0, bios);
    f = ac::scan::Scanner::inspect(rom0, stop);
    QCOMPARE(f.kind, QString("ps2-bios-romdir"));
    save(t.filePath("synthetic.rom1"), QByteArray(128, 0));
    ac::scan::ScanOptions options;
    options.mediaRoots = {rom0, t.filePath("synthetic.rom1")};
    auto c = catalog("bios");
    c.games[0].raw["media"][0].erase("set");
    auto result = ac::scan::Scanner::run(c, options, stop);
    QVERIFY(result.bindings[0].verified);
    QCOMPARE(result.bindings[0].supportPaths.size(), 1);
  }
  void compressedChdSerialAndLimits() {
    // Generated by tools/make_synthetic_chd_fixture.py using chdman 0.289,
    // raw zlib CHDv5, 4096-byte hunks, only synthetic ISO9660/CNF bytes.
    const auto data = QByteArray::fromBase64(
        "TUNvbXBySEQAAAB8AAAABXpsaWIAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAEMAAAAAA"
        "AAAAAAABAAAAAIAMS+dwrSjpEnyQjppqKJo4EuG0+5e1dFdZP+"
        "0fh84NvQppsBP2P6VUsAAAAAAAAAAAAAAAAAAAAAAAAAAO3BAQ0AAADCoPdPbQ8HFAAAAP"
        "Bu7cYhAQAgDACw3xGABCQ4GaB/JhQFUIhNLdeumhk/"
        "GtFvWwAAAADPDtNlEGWAAAcGbIAnODI4xNVXz9nPzdqQYRSMglEwCkbBKBgFwwE4+"
        "fuHGCnYKiSnFOXnGljFBPuEBscbGRjpGVpaG/"
        "JyjQbQKBgFo2AUjIJRMAwBAAAAABIAAAAAAHzfoAcAAAAjAxACAhA3i7wKd++"
        "ox46omxY=");
    QTemporaryDir t;
    auto path = t.filePath(QString::fromUtf8("synthetic-\xc3\xa9.chd"));
    save(path, data);
    std::atomic_bool stop = false;
    auto f = ac::scan::Scanner::inspect(path, stop);
    QVERIFY2(f.error.isEmpty(), qPrintable(f.error));
    QCOMPARE(f.kind, QString("chd-disc-serial"));
    QCOMPARE(f.identity, QString("SLUS-20219"));
    QVERIFY(!f.chdHeaderSha1.isEmpty());
    auto bad = data;
    qToBigEndian<quint32>(2 * 1024 * 1024, bad.data() + 56);
    save(path, bad);
    f = ac::scan::Scanner::inspect(path, stop);
    QVERIFY(f.error.contains("dimensions"));
    bad = data;
    bad[150] = char(static_cast<unsigned char>(bad[150]) ^ 0xff);
    save(path, bad);
    f = ac::scan::Scanner::inspect(path, stop);
    QVERIFY(!f.error.isEmpty() || f.identity != "SLUS-20219");
  }
  void embeddedSerialIndexNoFilenameGuess() {
    QTemporaryDir t;
    auto path = t.filePath("unrelated.iso");
    save(path, iso());
    auto c = catalog("disc");
    c.games[0].id = "ps2-time-crisis-2";
    c.games[0].raw["media"][0].erase("set");
    ac::scan::ScanOptions o;
    o.mediaRoots = {t.path()};
    std::atomic_bool stop = false;
    auto r = ac::scan::Scanner::run(c, o, stop);
    QVERIFY(r.bindings[0].verified);
    auto invalid = iso();
    invalid.replace("SLUS_202.19", "SLUS_999.99");
    save(path, invalid);
    r = ac::scan::Scanner::run(c, o, stop);
    QVERIFY(!r.bindings[0].verified);
  }
  void mameParentClone() {
    QTemporaryDir t;
    save(t.filePath("program.zip"), zip({{0x12345678, 100}}));
    save(t.filePath("parent.zip"), zip({{0x11111111, 200}}));
    auto xml = t.filePath("m.xml");
    save(xml,
         "<mame><machine name=\"test\"><rom name=\"graphics\" crc=\"11111111\" "
         "size=\"200\"/><rom name=\"program\" crc=\"22222222\" "
         "size=\"100\"/></machine><machine name=\"regional\" "
         "cloneof=\"test\"><rom name=\"graphics\" merge=\"graphics\" "
         "crc=\"11111111\" size=\"200\"/><rom name=\"program\" "
         "crc=\"12345678\" size=\"100\"/></machine></mame>");
    ac::scan::ScanOptions o;
    o.mediaRoots = {t.path()};
    o.mameXml = xml;
    std::atomic_bool stop = false;
    auto r = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(r.bindings[0].verified);
    QCOMPARE(r.bindings[0].identity, QString("regional"));
    QCOMPARE(r.bindings[0].supportPaths.size(), 1);
  }


  void mameDisksRequireMatchingAdjacentChd() {
    QTemporaryDir t;
    save(t.filePath("renamed.zip"), zip({{0x12345678, 100}}));
    const auto chd = syntheticChd();
    const auto sha = QString::fromLatin1(chd.mid(84, 20).toHex());
    const auto xml = t.filePath("metadata.xml");
    const auto metadata = [&](const QString &digest) {
      return QString("<mame><machine name='test'><rom name='program' crc='12345678' size='100'/>"
                     "<disk name='game' sha1='%1'/></machine></mame>").arg(digest).toUtf8();
    };
    save(xml, metadata(sha));
    ac::scan::ScanOptions o; o.mediaRoots = {t.path()}; o.mameXml = xml;
    std::atomic_bool stop = false;
    auto result = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(!result.bindings[0].verified);
    QVERIFY(result.bindings[0].missing.join(' ').contains("disk:game"));
    save(t.filePath("wrong-folder/game.chd"), chd);
    result = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(!result.bindings[0].verified);
    save(t.filePath("TeSt/game.chd"), chd);
    auto identity = ac::scan::Scanner::inspect(t.filePath("TeSt/game.chd"), stop);
    QVERIFY2(identity.error.isEmpty(), qPrintable(identity.error));
    QCOMPARE(identity.chdHeaderSha1, sha);
    result = ac::scan::Scanner::run(catalog(), o, stop);
#ifdef Q_OS_WIN
    QVERIFY(result.bindings[0].verified);
    QVERIFY(result.bindings[0].supportPaths.contains(t.filePath("TeSt/game.chd")));
#else
    QVERIFY(!result.bindings[0].verified); // Linux MAME folders are case sensitive.
    save(t.filePath("test/game.chd"), chd);
    result = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(result.bindings[0].verified);
    QVERIFY(result.bindings[0].supportPaths.contains(t.filePath("test/game.chd")));
#endif
    const auto receipt=result.toJson();
    QVERIFY(ac::scan::hasValidatedChdBounds(receipt["bindings"][0],receipt["files"]));
    auto unchecked=result;
    for(auto &file:unchecked.files)if(file.kind.startsWith("chd-"))file.chdHeaderBoundsOk=false;
    const auto uncheckedReceipt=unchecked.toJson();
    QVERIFY(!uncheckedReceipt["bindings"][0].contains("chdBounds"));
    save(xml, metadata(QString(40, '0')));
    result = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(!result.bindings[0].verified);
    auto disc = catalog("disc"); disc.games[0].raw["media"][0].erase("set");
    disc.games[0].raw["media"][0]["chd_sha1"] = sha.toStdString();
    result = ac::scan::Scanner::run(disc, o, stop);
    QVERIFY(result.bindings[0].verified);
  }
  void biosArchivesCannotShadowVerifiedClone() {
    QTemporaryDir t;
    save(t.filePath("a-broken.zip"), zip({{0x10000001, 100}, {0x10000002, 100}}));
    save(t.filePath("bios.zip"), zip({{0x20000001, 100}, {0x20000002, 100}, {0x20000003, 100}}));
    save(t.filePath("z-program.zip"), zip({{0x30000001, 100}}));
    const auto xml = t.filePath("metadata.xml");
    save(xml, "<mame><machine name='test'><rom name='a' crc='10000001' size='100'/>"
              "<rom name='b' crc='10000002' size='100'/><rom name='c' crc='10000003' size='100'/>"
              "</machine><machine name='z-regional' cloneof='test'>"
              "<rom name='a' merge='a' crc='20000001' size='100'/>"
              "<rom name='b' merge='b' crc='20000002' size='100'/>"
              "<rom name='c' merge='c' crc='20000003' size='100'/>"
              "<rom name='program' crc='30000001' size='100'/></machine></mame>");
    ac::scan::ScanOptions o; o.mediaRoots = {t.path()}; o.mameXml = xml;
    std::atomic_bool stop = false;
    auto result = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(result.bindings[0].verified);
    QCOMPARE(result.bindings[0].identity, QString("z-regional"));
    QCOMPARE(result.bindings[0].path, t.filePath("z-program.zip"));
    QVERIFY(result.bindings[0].setCandidates.contains("test"));
    QVERIFY(result.bindings[0].setCandidates.contains("z-regional"));
    QCOMPARE(result.toJson()["bindings"][0]["setCandidates"].size(), size_t(2));
    o.mediaRoots = {t.filePath("bios.zip")};
    result = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(!result.bindings[0].verified);
    QVERIFY(result.states[0].mediaFound.isEmpty());
  }
  void biosAlternativesAndSetRows() {
    QTemporaryDir t;
    save(t.filePath("game.zip"), zip({{0x12345678, 100}}));
    save(t.filePath("alternate.zip"), zip({{0xabcdef12, 100}}));
    const auto xml = t.filePath("metadata.xml");
    save(xml, "<mame><machine name='test'><rom name='base' crc='12345678' size='100'/>"
              "<device_ref name='chip'/></machine><machine name='chip' isbios='yes'>"
              "<biosset name='default' default='yes'/><biosset name='alternate'/>"
              "<rom name='firmware' bios='default' crc='11111111' size='100'/>"
              "<rom name='firmware' bios='alternate' crc='abcdef12' size='100'/>"
              "</machine></mame>");
    auto c = catalog();
    c.games[0].raw["media"].push_back({{"kind", "bios"}, {"set", "chip"}});
    ac::scan::ScanOptions o; o.mediaRoots = {t.path()}; o.mameXml = xml;
    std::atomic_bool stop = false;
    auto result = ac::scan::Scanner::run(c, o, stop);
    QVERIFY(!result.bindings[0].verified); // Device alternatives cannot be selected at launch.
    QVERIFY(result.bindings[1].verified);
    QCOMPARE(result.bindings[1].bios, QString("alternate"));
    QCOMPARE(result.bindings[1].proof, QString("mame-header-crc"));
    save(t.filePath("default.zip"), zip({{0x11111111, 100}}));
    result = ac::scan::Scanner::run(c, o, stop);
    QVERIFY(result.bindings[0].verified);
    QCOMPARE(result.bindings[1].bios, QString("default"));
    QVERIFY(QFile::remove(t.filePath("default.zip")));
    o.mediaRoots = {t.filePath("alternate.zip")};
    result = ac::scan::Scanner::run(c, o, stop);
    QVERIFY(!result.bindings[0].verified);
    QVERIFY(result.bindings[1].verified);
    c.games[0].runtime = result.states[0]; ac::resolveState(c.games[0]);
    QVERIFY(!c.games[0].roles["inLibrary"].toBool());
    // Alternatives within the game definition itself may reuse the same name.
    save(xml, "<mame><machine name='test'><biosset name='default' default='yes'/>"
              "<biosset name='alternate'/><rom name='firmware' bios='default' crc='11111111' size='100'/>"
              "<rom name='firmware' bios='alternate' crc='abcdef12' size='100'/></machine></mame>");
    result = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(result.bindings[0].verified);
    QCOMPARE(result.bindings[0].bios, QString("alternate"));
    QCOMPARE(QString::fromStdString(result.toJson()["bindings"][0]["bios"].get<std::string>()), QString("alternate"));
    save(t.filePath("default.zip"), zip({{0x11111111, 100}}));
    o.mediaRoots = {t.path()};
    result = ac::scan::Scanner::run(catalog(), o, stop);
    QCOMPARE(result.bindings[0].bios, QString("default"));
  }
  void diskMergeHeaderAndDiskOnlyAnchor() {
    QTemporaryDir t;
    auto chd = syntheticChd();
    const auto sha = QString::fromLatin1(chd.mid(84, 20).toHex());
    // Parent-dependent sparse decoding is unsupported, with bounded physical offsets.
    chd[104] = 1;
    save(t.filePath("parent/base.chd"), chd);
    save(t.filePath("unrelated.zip"), zip({{0x98765432, 100}}));
    const auto xml = t.filePath("metadata.xml");
    save(xml, QString("<mame><machine name='parent'/><machine name='test' cloneof='parent'>"
                      "<disk name='clone' merge='base' sha1='%1'/></machine></mame>").arg(sha).toUtf8());
    std::atomic_bool stop = false;
    const auto inspected = ac::scan::Scanner::inspect(t.filePath("parent/base.chd"), stop);
    QVERIFY(!inspected.error.isEmpty()); QCOMPARE(inspected.chdHeaderSha1, sha);
    QVERIFY(inspected.chdHeaderBoundsOk);
    ac::scan::ScanOptions options; options.mediaRoots = {t.path()}; options.mameXml = xml;
    auto result = ac::scan::Scanner::run(catalog(), options, stop);
    QVERIFY(result.bindings[0].verified);
    QCOMPARE(result.bindings[0].path, t.filePath("parent/base.chd"));
    QCOMPARE(result.bindings[0].identity, QString("test"));
    auto receipt=result.toJson();
    QVERIFY(ac::scan::hasValidatedChdBounds(receipt["bindings"][0],receipt["files"]));
    QVERIFY(receipt["bindings"][0].contains("chdBounds"));
    auto unchecked=result;
    for(auto &file:unchecked.files)if(file.kind.startsWith("chd-"))file.chdHeaderBoundsOk=false;
    const auto uncheckedReceipt=unchecked.toJson();
    QVERIFY(!uncheckedReceipt["bindings"][0].contains("chdBounds"));
    QVERIFY(!ac::scan::hasValidatedChdBounds(uncheckedReceipt["bindings"][0],uncheckedReceipt["files"]));
    QFile::remove(t.filePath("parent/base.chd"));
    result = ac::scan::Scanner::run(catalog(), options, stop);
    QVERIFY(!result.bindings[0].verified); QVERIFY(result.bindings[0].path.isEmpty());
  }
  void chdPhysicalOffsetsRejectTruncation_data() {
    QTest::addColumn<QString>("kind");
    for (const auto *kind : {"map-beyond", "map-eof", "map-range", "compressed-header",
                            "compressed-range", "metadata-beyond", "metadata-eof", "metadata-header",
                            "metadata-body", "metadata-next", "metadata-next-body", "metadata-cycle", "metadata-budget"})
      QTest::newRow(kind) << QString(kind);
  }
  void chdPhysicalOffsetsRejectTruncation() {
    QFETCH(QString, kind); QTemporaryDir t; auto data = syntheticChd();
    const auto sha = QString::fromLatin1(data.mid(84, 20).toHex());
    const auto size = quint64(data.size());
    if (kind == "map-beyond" || kind == "map-eof")
      qToBigEndian<quint64>(size + (kind == "map-beyond" ? 100 : 0), data.data() + 40);
    else if (kind == "map-range") qToBigEndian<quint64>(size - 4, data.data() + 40);
    else if (kind == "compressed-header" || kind == "compressed-range") {
      qToBigEndian<quint32>(1, data.data() + 16);
      qToBigEndian<quint64>(size - (kind == "compressed-header" ? 4 : 16), data.data() + 40);
      if (kind == "compressed-range") qToBigEndian<quint32>(100, data.data() + data.size() - 16);
    } else if (kind == "metadata-next-body" || kind == "metadata-cycle" || kind == "metadata-budget") {
      data[104]=1; // The sparse decoder would otherwise return early for this parent-dependent CHD.
      const auto first=quint64(data.size());
      const int nodes=kind=="metadata-budget"?257:2;
      data.append(QByteArray(nodes*16,0));
      qToBigEndian<quint64>(first,data.data()+48);
      for(int i=0;i<nodes-1;++i)qToBigEndian<quint64>(first+quint64((i+1)*16),data.data()+first+quint64(i*16)+8);
      if(kind=="metadata-next-body")qToBigEndian<quint32>(100,data.data()+first+20);
      if(kind=="metadata-cycle")qToBigEndian<quint64>(first,data.data()+first+24);
    } else if (kind == "metadata-body" || kind == "metadata-next") {
      qToBigEndian<quint64>(size - 16, data.data() + 48);
      data.replace(data.size()-16,16,QByteArray(16,0));
      if(kind=="metadata-body") qToBigEndian<quint32>(100,data.data()+data.size()-12);
      else qToBigEndian<quint64>(size+100,data.data()+data.size()-8);
    } else qToBigEndian<quint64>(kind == "metadata-beyond" ? size + 100 :
                               kind == "metadata-eof" ? size : size - 4, data.data() + 48);
    save(t.filePath("test/base.chd"), data);
    std::atomic_bool stop = false;
    const auto inspected = ac::scan::Scanner::inspect(t.filePath("test/base.chd"), stop);
    QVERIFY(!inspected.chdHeaderBoundsOk); QVERIFY(inspected.chdHeaderSha1.isEmpty());
    QVERIFY(inspected.error.contains(kind=="metadata-cycle"?"cycle":kind=="metadata-budget"?"budget":"physical file"));
    const auto xml = t.filePath("metadata.xml");
    save(xml, QString("<mame><machine name='test'><disk name='base' sha1='%1'/></machine></mame>").arg(sha).toUtf8());
    ac::scan::ScanOptions options; options.mediaRoots = {t.path()}; options.mameXml = xml;
    const auto result = ac::scan::Scanner::run(catalog(), options, stop);
    QVERIFY(!result.bindings[0].verified); QVERIFY(result.bindings[0].path.isEmpty());
  }
  void requestedSetWinsEqualFamilyScore() {
    QTemporaryDir t;
    save(t.filePath("synthetic.zip"), zip({{0x12345678, 100}}));
    const auto xml = t.filePath("metadata.xml");
    save(xml, "<mame><machine name='a-parent'><rom name='program' crc='12345678' size='100'/></machine>"
              "<machine name='test' cloneof='a-parent'><rom name='program' crc='12345678' size='100'/></machine></mame>");
    ac::scan::ScanOptions options; options.mediaRoots = {t.path()}; options.mameXml = xml;
    std::atomic_bool stop = false;
    const auto result = ac::scan::Scanner::run(catalog(), options, stop);
    QVERIFY(result.bindings[0].verified); QCOMPARE(result.bindings[0].identity, QString("test"));
  }
  void legacyChdCacheRechecksPhysicalBounds() {
    QTemporaryDir t; auto data = syntheticChd();
    const auto sha = QString::fromLatin1(data.mid(84, 20).toHex());
    qToBigEndian<quint64>(quint64(data.size() + 100), data.data() + 40);
    save(t.filePath("test/base.chd"), data);
    const auto xml = t.filePath("metadata.xml");
    save(xml, QString("<mame><machine name='test'><disk name='base' sha1='%1'/></machine></mame>").arg(sha).toUtf8());
    ac::scan::ScanOptions options; options.mediaRoots = {t.path()}; options.mameXml = xml;
    options.userRoot = t.filePath("user"); std::atomic_bool stop = false;
    const auto first = ac::scan::Scanner::run(catalog(), options, stop);
    QCOMPARE(first.files.size(),1); QVERIFY(!first.files[0].chdHeaderBoundsOk);
    const auto cachePath = options.userRoot + "/cache/scan.toml";
    QFile cache(cachePath); QVERIFY(cache.open(QIODevice::ReadOnly)); auto bytes = cache.readAll(); cache.close();
    bytes.replace("\"chdHeaderBoundsOk\":false,", "");
    bytes.replace("\\\"chdHeaderBoundsOk\\\":false,", "");
    QVERIFY(!bytes.contains("chdHeaderBoundsOk")); save(cachePath,bytes);
    const auto second = ac::scan::Scanner::run(catalog(), options, stop);
    QCOMPARE(second.files.size(),1); QVERIFY(!second.files[0].cacheHit);
    QVERIFY(!second.files[0].chdHeaderBoundsOk); QVERIFY(!second.bindings[0].verified);
  }
  void chdMapBudgetAllowsLargeSparseDiscs() {
    QTemporaryDir t;
    const auto path = t.filePath("large-synthetic.chd");
    auto data = syntheticChd(1200001ULL * 4096);
    save(path, data);
    std::atomic_bool stop = false;
    const auto inspected = ac::scan::Scanner::inspect(path, stop);
    QVERIFY2(inspected.error.isEmpty(), qPrintable(inspected.error));
    QCOMPARE(inspected.kind, QString("chd-sha1"));
    QVERIFY(inspected.chdHeaderBoundsOk);
    QVERIFY(!inspected.chdHeaderSha1.isEmpty());
    // Increasing logical size alone leaves a truncated raw map. Physical
    // bounds must reject it before allocation-driving dimensions are checked.
    qToBigEndian<quint64>(100ULL * 1024 * 1024 * 1024, data.data() + 32);
    save(path, data);
    const auto truncated = ac::scan::Scanner::inspect(path, stop);
    QVERIFY(truncated.error.contains("physical file"));
    QVERIFY(!truncated.chdHeaderBoundsOk);
    QVERIFY(truncated.chdHeaderSha1.isEmpty());
    // A complete raw map one entry over 32 MiB isolates the sparse-read budget
    // from physical truncation, without allocating its logical disc contents.
    data = syntheticChd(((32ULL * 1024 * 1024) / 4 + 1) * 4096);
    save(path, data);
    const auto over = ac::scan::Scanner::inspect(path, stop);
    QVERIFY(over.chdHeaderBoundsOk);
    QVERIFY(!over.chdHeaderSha1.isEmpty());
    QVERIFY(over.error.contains("dimensions"));
  }
  void pcProofIsExplicitlyNameOnly() {
    QTemporaryDir t;
    QByteArray pe(68, 0); pe[0] = 'M'; pe[1] = 'Z';
    qToLittleEndian<quint32>(64, pe.data() + 60); pe.replace(64, 4, QByteArray("PE\0\0", 4));
    save(t.filePath("Synthetic.exe"), pe);
    auto c = catalog("pc-game"); c.games[0].raw["media"][0]["find"] = ac::Json::array({"Synthetic.exe"});
    ac::scan::ScanOptions o; o.mediaRoots = {t.path()}; std::atomic_bool stop = false;
    const auto result = ac::scan::Scanner::run(c, o, stop);
    QVERIFY(result.bindings[0].verified);
    QCOMPARE(result.bindings[0].proof, QString("executable-name-only"));
  }
  void directoryLinksAreNotFollowed() {
    QTemporaryDir root, outside;
    save(outside.filePath("synthetic.zip"), zip({{0x12345678, 100}}));
    const auto link = root.filePath("linked-folder");
#ifdef Q_OS_WIN
    QProcess maker;
    maker.start("cmd.exe", {"/c", "mklink", "/J", QDir::toNativeSeparators(link), QDir::toNativeSeparators(outside.path())});
    QVERIFY(maker.waitForFinished(10000)); QCOMPARE(maker.exitCode(), 0);
    QVERIFY(QFileInfo(link).isJunction());
#else
    QVERIFY(QFile::link(outside.path(), link));
    QVERIFY(QFileInfo(link).isSymLink());
#endif
    ac::scan::ScanOptions o; o.mediaRoots = {root.path()}; std::atomic_bool stop = false;
    QVERIFY(ac::scan::Scanner::run(catalog(), o, stop).files.isEmpty());
    o.mediaRoots = {link};
    QVERIFY(ac::scan::Scanner::run(catalog(), o, stop).files.isEmpty());
    QVERIFY(QFileInfo::exists(outside.filePath("synthetic.zip")));
#ifdef Q_OS_WIN
    QVERIFY(QDir().rmdir(link));
#else
    QVERIFY(QFile::remove(link));
#endif
  }
  void arttoolDocumentedCommandExportsFallback() {
    QTemporaryDir t;
    ac::Json request{{"dataRoot",AC_CATALOG_ROOT},{"userRoot",t.filePath("user").toStdString()},{"gameIds",ac::Json::array({"timecris"})},{"outputDirectory",t.filePath("out").toStdString()},{"kind","logo"}};
    const auto path=t.filePath("request.json");save(path,QByteArray::fromStdString(request.dump()));
    QString binary=QCoreApplication::applicationDirPath()+"/arttool";
#ifdef Q_OS_WIN
    binary+=".exe";
#endif
    QProcess child;auto environment=QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    environment.insert("QT_QPA_PLATFORM","offscreen");
#else
    environment.insert("QT_QPA_PLATFORM","offscreen");
#endif
    child.setProcessEnvironment(environment);child.start(binary,{"--request",path});
    QVERIFY(child.waitForFinished(15000));QCOMPARE(child.exitCode(),0);
    const QImage image(t.filePath("out/timecris.png"));QVERIFY(!image.isNull());
    QCOMPARE(image.size(),QSize(920,430));QCOMPARE(image.pixelColor(0,0).alpha(),0);
    QVERIFY(QFileInfo::exists(t.filePath("out/receipt.json")));
    request["width"]=600;request["height"]=900;save(path,QByteArray::fromStdString(request.dump()));
    child.start(binary,{"--request",path});QVERIFY(child.waitForFinished(15000));QCOMPARE(child.exitCode(),0);
    QCOMPARE(QImage(t.filePath("out/timecris.png")).size(),QSize(600,900));
    request["width"]=5000;save(path,QByteArray::fromStdString(request.dump()));child.start(binary,{"--request",path});QVERIFY(child.waitForFinished(15000));QCOMPARE(child.exitCode(),2);
  }

  void scanTimeToolFingerprintSurvivesLaterChange() {
    QTemporaryDir t;
    QByteArray pe(68,0);pe[0]='M';pe[1]='Z';qToLittleEndian<quint32>(64,pe.data()+60);pe.replace(64,4,QByteArray("PE\0\0",4));
    const auto path=t.filePath("mame.exe");save(path,pe);
    ac::scan::ScanOptions options;options.locateOnly=true;options.toolRoots={t.path()};
    std::atomic_bool stop=false;const auto result=ac::scan::Scanner::run(catalog(),options,stop);
    QCOMPARE(result.tools.size(),1);const auto recorded=result.tools.first().mtime;
    save(path,pe+"changed");const auto json=result.toJson();
    QCOMPARE(json["tools"][0]["size"].get<qint64>(),qint64(68));
    QCOMPARE(json["tools"][0]["mtime"].get<qint64>(),recorded);
    QVERIFY(QFileInfo(path).size()!=result.tools.first().size);
  }

  void scopedRootsSkipSecretsAndSymlinks() {
    QTemporaryDir t;
    save(t.filePath(".ssh/secret.zip"), zip({{0x12345678, 100}}));
    ac::scan::ScanOptions o;
    o.mediaRoots = {t.path()};
    std::atomic_bool stop = false;
    auto r = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(r.files.isEmpty());
    QDir().mkpath(t.filePath("pcsx2.exe"));
    o.locateOnly = true;
    o.toolRoots = {t.path()};
    r = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(r.tools.isEmpty());
  }
  void devicesCacheCancel() {
    QTemporaryDir t;
    save(t.filePath("old-name.zip"), zip({{0x12345678, 100}}));
    auto xml = t.filePath("metadata.xml");
    save(xml,
         "<mame><machine name=\"test\"><rom name=\"base\" crc=\"12345678\" "
         "size=\"100\"/><device_ref name=\"chip\"/></machine><machine "
         "name=\"chip\" isdevice=\"yes\"><rom name=\"firmware\" "
         "crc=\"abcdef12\" size=\"200\"/></machine></mame>");
    ac::scan::ScanOptions o;
    o.mediaRoots = {t.path()};
    o.userRoot = t.filePath("user");
    o.mameXml = xml;
    std::atomic_bool stop = false;
    auto r = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(!r.bindings[0].verified);
    QVERIFY(r.bindings[0].missing.join(" ").contains("device:chip"));
    save(t.filePath("another-name.zip"), zip({{0xabcdef12, 200}}));
    r = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(r.bindings[0].verified);
    QCOMPARE(r.bindings[0].supportPaths.size(), 1);
    QVERIFY(r.files.back().cacheHit || r.files.front().cacheHit);
    save(t.filePath("old-name.zip"), zip({{0x12345679, 101}}));
    r = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(!r.bindings[0].verified);
    stop = true;
    auto cancelRoot = t.filePath("cancelled");
    o.userRoot = cancelRoot;
    r = ac::scan::Scanner::run(catalog(), o, stop);
    QVERIFY(r.cancelled);
    QVERIFY(!QFileInfo::exists(cancelRoot + "/cache/scan.toml"));
  }
  void cloneRegionReplacement() {
    QTemporaryDir t;
    save(t.filePath("renamed.zip"),
         zip({{0x12345678, 100}, {0x11111111, 200}}));
    auto xml = t.filePath("games.xml");
    save(xml,
         "<games><game name=\"test\"><roms><region name=\"program\"><file "
         "offset=\"0\" name=\"base\" crc32=\"0x22222222\"/></region><region "
         "name=\"graphics\"><file offset=\"0\" name=\"shared\" "
         "crc32=\"0x11111111\"/></region></roms></game><game "
         "name=\"test-region\" parent=\"test\"><roms><region "
         "name=\"program\"><file offset=\"0\" name=\"clone\" "
         "crc32=\"0x12345678\"/></region></roms></game></games>");
    ac::scan::ScanOptions o;
    o.mediaRoots = {t.path()};
    o.supermodelXml = xml;
    auto c = catalog();
    c.games[0].raw["hardware"] = "sega-model-3";
    std::atomic_bool stop = false;
    auto r = ac::scan::Scanner::run(c, o, stop);
    QVERIFY(r.bindings[0].verified);
    QCOMPARE(r.bindings[0].identity, QString("test-region"));
  }
  void artPriorityFallback() {
    QTemporaryDir t;
    auto c = catalog();
    auto media = t.filePath("roms");
    ac::art::Resolver resolver(c, {t.filePath("user"), {media}});
    QImage red(64, 32, QImage::Format_RGB32);
    red.fill(Qt::red);
    QDir().mkpath(media + "/marquee");
    QVERIFY(red.save(media + "/marquee/test.png"));
    auto a = resolver.resolve("synthetic", "banner", {64, 32});
    QCOMPARE(a.source, QString("local"));
    QCOMPARE(a.image.pixelColor(32, 16), QColor(Qt::red));
    auto user = t.filePath("user/art/synthetic");
    QDir().mkpath(user);
    QImage blue = red;
    blue.fill(Qt::blue);
    QVERIFY(blue.save(user + "/banner.png"));
    a = resolver.resolve("synthetic", "banner", {64, 32});
    QCOMPARE(a.source, QString("user"));
    QCOMPARE(a.image.pixelColor(32, 16), QColor(Qt::blue));
    a = resolver.resolve("unknown", "banner", {300, 150});
    QCOMPARE(a.source, QString("generated"));
    QVERIFY(!a.image.isNull());
    QVERIFY(a.image.pixelColor(1, 1) != a.image.pixelColor(100, 100));
  }
  void artRetroarchThenPcsx2() {
    QTemporaryDir t;
    auto c = catalog("disc");
    c.games[0].raw["title"] = "Synthetic & Test";
    auto root = t.filePath("frontend");
    ac::art::Resolver resolver(c, {t.filePath("user"), {root}});
    ac::scan::Binding binding;
    binding.gameId = "synthetic";
    binding.verified = true;
    binding.identity = "TEST-00001";
    binding.proof = "disc-serial";
    binding.path = t.filePath("roms/unrelated.iso");
    resolver.setBindings({binding});
    QImage red(64, 32, QImage::Format_RGB32);
    red.fill(Qt::red);
    QDir().mkpath(root + "/covers");
    QVERIFY(red.save(root + "/covers/TEST-00001.png"));
    auto a = resolver.resolve("synthetic", "portrait", {64, 32});
    QCOMPARE(a.source, QString("pcsx2"));
    auto thumb = root + "/thumbnails/SyntheticPlaylist/Named_Boxarts";
    QDir().mkpath(thumb);
    QVERIFY(red.save(thumb + "/Synthetic _ Test.png"));
    a = resolver.resolve("synthetic", "portrait", {64, 32});
    QCOMPARE(a.source, QString("retroarch"));
  }
  void directCoverRootAndTransparentLogo() {
    QTemporaryDir t;
    auto c = catalog("disc");
    c.games[0].id = "synthetic-cover";
    auto covers = t.filePath("covers");
    QDir().mkpath(covers);
    QImage image(64, 32, QImage::Format_RGB32);
    image.fill(Qt::green);
    QVERIFY(image.save(covers + "/TEST-00002.png"));
    ac::art::Resolver resolver(c, {t.filePath("user"), {covers}});
    ac::scan::Binding binding;
    binding.gameId = "synthetic-cover";
    binding.verified = true;
    binding.identity = "TEST-00002";
    binding.proof = "disc-serial";
    resolver.setBindings({binding});
    auto r = resolver.resolve("synthetic-cover", "portrait", {64, 32});
    QCOMPARE(r.source, QString("pcsx2"));
    r = resolver.resolve("synthetic-cover", "logo", {300, 150});
    QCOMPARE(r.source, QString("generated"));
    QCOMPARE(r.image.pixelColor(0, 0).alpha(), 0);
    bool text = false;
    for (int y = 0; y < r.image.height(); ++y)
      for (int x = 0; x < r.image.width(); ++x)
        if (r.image.pixelColor(x, y).alpha() > 0)
          text = true;
    QVERIFY(text);
  }
  void futureFailureKeepsLastGoodState() {
    QTemporaryDir t;
    ac::GameListModel model(catalog());
    ac::scan::ScanOptions options;
    int calls = 0;
    ac::scan::ScanController controller(&model, options, nullptr,
        [&](const ac::CatalogData &c, const ac::scan::ScanOptions &, std::atomic_bool &, const ac::scan::Progress &) {
          if (++calls == 2) throw std::runtime_error("synthetic unexpected worker failure");
          ac::scan::ScanResult result;
          auto state = c.games[0].runtime; state.mediaFound = {"last-good"};
          result.states << state;
          return result;
        });
    QSignalSpy done(&controller, &ac::scan::ScanController::scanFinished);
    QSignalSpy ready(&controller, &ac::scan::ScanController::resultsReady);
    QSignalSpy failed(&controller, &ac::scan::ScanController::scanFailed);
    controller.scan(); QTRY_COMPARE(done.count(), 1);
    const auto previous = controller.lastResult().toJson();
    int beats = 0; QTimer heartbeat;
    connect(&heartbeat, &QTimer::timeout, [&] { ++beats; }); heartbeat.start(1);
    controller.scan(); QTRY_COMPARE(done.count(), 2);
    QVERIFY(!controller.running()); QCOMPARE(failed.count(), 1); QCOMPARE(ready.count(), 1);
    QVERIFY(controller.lastResult().toJson() == previous);
    QCOMPARE(model.records()[0].runtime.mediaFound, QStringList{"last-good"});
    QTRY_VERIFY(beats > 0);
    controller.scan(); QTRY_COMPARE(done.count(), 3);
    QCOMPARE(ready.count(), 2); QCOMPARE(failed.count(), 1);
  }
  void malformedRecordIsIsolated() {
    QTemporaryDir t;
    save(t.filePath("test.zip"), zip({{0x12345678, 100}}));
    save(t.filePath("m.xml"), "<mame><machine name=\"test\"><rom name=\"r\" crc=\"12345678\" size=\"100\"/></machine></mame>");
    auto c = catalog();
    auto bad = c.games[0]; bad.id = "malformed"; bad.raw["hardware"] = 7;
    bad.runtime.gameId = bad.id; bad.runtime.mediaFound = {"prior-good"};
    c.games.prepend(bad);
    ac::scan::ScanOptions options; options.mediaRoots = {t.path()}; options.mameXml = t.filePath("m.xml");
    std::atomic_bool cancel{false};
    ac::scan::ScanResult result;
    try { result = ac::scan::Scanner::run(c, options, cancel); }
    catch (const std::exception &e) { QFAIL(e.what()); }
    QCOMPARE(result.states.size(), 1);
    QCOMPARE(result.states[0].gameId, QString("synthetic"));
    QVERIFY(result.states[0].mediaFound.contains("test"));
    QCOMPARE(result.bindings.size(), 1);
    QVERIFY(result.diagnostics.join("\n").contains("malformed"));
    ac::GameListModel model(c);
    ac::scan::ScanController controller(&model, options);
    QSignalSpy done(&controller, &ac::scan::ScanController::scanFinished);
    QSignalSpy ready(&controller, &ac::scan::ScanController::resultsReady);
    controller.scan();
    QTRY_COMPARE(done.count(), 1);
    QVERIFY(!controller.running()); QCOMPARE(ready.count(), 1);
    QCOMPARE(model.find("malformed")->runtime.mediaFound, QStringList{"prior-good"});
    QVERIFY(model.find("synthetic")->runtime.mediaFound.contains("test"));
  }
  void threadResultPreservesRuntime() {
    QTemporaryDir t;
    save(t.filePath("archive.zip"), zip({{0x12345678, 100}}));
    save(t.filePath("m.xml"),
         "<mame><machine name=\"test\"><rom name=\"r\" crc=\"12345678\" "
         "size=\"100\"/></machine></mame>");
    ac::GameListModel model(catalog());
    ac::scan::ScanOptions o;
    o.mediaRoots = {t.path()};
    o.mameXml = t.filePath("m.xml");
    ac::scan::ScanController controller(&model, o);
    QSignalSpy done(&controller, &ac::scan::ScanController::scanFinished);
    controller.scan();
    auto state = model.records()[0].runtime;
    state.lastPlayed = 999;
    state.jobStatus = ac::JobStatus::Running;
    model.applyRuntimeStates({state});
    QTRY_COMPARE(done.count(), 1);
    QCOMPARE(model.records()[0].runtime.lastPlayed, qint64(999));
    QCOMPARE(model.records()[0].runtime.jobStatus, ac::JobStatus::Running);
    QVERIFY(model.records()[0].runtime.mediaFound.contains("test"));
  }
  void asyncProviderCancellationAndQuery() {
    QTemporaryDir t;
    auto resolver = std::make_shared<ac::art::Resolver>(
        catalog(), ac::art::ArtOptions{t.filePath("user"), {}});
    ac::art::Provider provider(resolver);
    std::unique_ptr<QQuickImageResponse> response(
        provider.requestImageResponse("synthetic/banner?v=1", {300, 150}));
    QSignalSpy done(response.get(), &QQuickImageResponse::finished);
    response->cancel();
    QTRY_COMPARE(done.count(), 1);
    response.reset(
        provider.requestImageResponse("synthetic/logo?v=2", {300, 150}));
    QSignalSpy second(response.get(), &QQuickImageResponse::finished);
    QTRY_COMPARE(second.count(), 1);
    std::unique_ptr<QQuickTextureFactory> texture(response->textureFactory());
    QVERIFY(texture);
    QCOMPARE(texture->image().pixelColor(0, 0).alpha(), 0);
  }
};
QTEST_MAIN(ScanArtTest)
#include "ScanArtTest.moc"
