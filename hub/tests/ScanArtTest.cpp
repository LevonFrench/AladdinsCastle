// SPDX-License-Identifier: GPL-3.0-only
#include "core/art/Art.h"
#include "core/scan/HeaderSafety.h"
#include "core/scan/Scan.h"
#include "core/scan/ScanController.h"
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QTemporaryDir>
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
    environment.insert("QT_QPA_PLATFORM","windows");
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
    binding.identity = "SLUS-20219";
    binding.proof = "disc-serial";
    binding.path = t.filePath("roms/unrelated.iso");
    resolver.setBindings({binding});
    QImage red(64, 32, QImage::Format_RGB32);
    red.fill(Qt::red);
    QDir().mkpath(root + "/covers");
    QVERIFY(red.save(root + "/covers/SLUS-20219.png"));
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
    c.games[0].id = "ps2-time-crisis-3";
    auto covers = t.filePath("covers");
    QDir().mkpath(covers);
    QImage image(64, 32, QImage::Format_RGB32);
    image.fill(Qt::green);
    QVERIFY(image.save(covers + "/SCES_518.44.png"));
    ac::art::Resolver resolver(c, {t.filePath("user"), {covers}});
    auto r = resolver.resolve("ps2-time-crisis-3", "portrait", {64, 32});
    QCOMPARE(r.source, QString("pcsx2"));
    r = resolver.resolve("ps2-time-crisis-3", "logo", {300, 150});
    QCOMPARE(r.source, QString("generated"));
    QCOMPARE(r.image.pixelColor(0, 0).alpha(), 0);
    bool text = false;
    for (int y = 0; y < r.image.height(); ++y)
      for (int x = 0; x < r.image.width(); ++x)
        if (r.image.pixelColor(x, y).alpha() > 0)
          text = true;
    QVERIFY(text);
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
