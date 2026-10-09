// SPDX-License-Identifier: GPL-3.0-only
#include "core/install/Install.h"
#include "core/catalog/CatalogLoader.h"
#include "core/install/ArtifactStore.h"
#include "core/install/Support.h"
#include <QDirIterator>
#include <QProcess>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>
#include <archive.h>
#include <archive_entry.h>
using namespace ac;
using namespace ac::install;
namespace {
class HttpsServer : public QTcpServer {
public:
  QByteArray payload = "synthetic tool artifact";
  QList<QByteArray> requests;
  QString url() const {
    return "https://localhost:" + QString::number(serverPort());
  }
  void incomingConnection(qintptr descriptor) override {
    auto *socket = new QSslSocket(this);
    socket->setSocketDescriptor(descriptor);
    socket->setLocalCertificate(QSslCertificate(
        readBytes(QString(AC_TEST_FIXTURES) + "/tls/localhost-cert.pem")));
    socket->setPrivateKey(
        QSslKey(readBytes(QString(AC_TEST_FIXTURES) + "/tls/localhost-key.pem"),
                QSsl::Rsa));
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    QObject::connect(socket, &QSslSocket::readyRead, socket, [this, socket] {
      const auto req = socket->readAll();
      if (!req.contains("\r\n\r\n"))
        return;
      requests << req;
      const auto route = req.split(' ').value(1);
      QByteArray response;
      if (route == "/redirect")
        response = "HTTP/1.1 302 Found\r\nLocation: " + url().toUtf8() +
                   "/artifact\r\nContent-Length: 0\r\n\r\n";
      else if (route == "/denied")
        response =
            "HTTP/1.1 302 Found\r\nLocation: "
            "https://unlisted.invalid/artifact\r\nContent-Length: 0\r\n\r\n";
      else if (route.contains("/releases/tags/missing"))
        response = "HTTP/1.1 404 Missing\r\nContent-Length: 0\r\n\r\n";
      else if (route.contains("/releases/tags/limited"))
        response = "HTTP/1.1 429 Limited\r\nRetry-After: 60\r\nContent-Length: "
                   "0\r\n\r\n";
      else if (route.contains("/releases/tags/")) {
        const QByteArray body = "{\"tag_name\":\"v1\",\"assets\":[]}";
        response = "HTTP/1.1 200 OK\r\nETag: test-v1\r\nContent-Length: " +
                   QByteArray::number(body.size()) + "\r\n\r\n" + body;
      } else if (route == "/html") {
        const QByteArray body = "<html>error</html>";
        response =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
            QByteArray::number(body.size()) + "\r\n\r\n" + body;
      } else {
        qint64 offset = 0;
        const auto range = req.toLower().indexOf("range: bytes=");
        if (range >= 0)
          offset = req.mid(range + 13).split('-').first().toLongLong();
        const auto body = payload.mid(offset);
        response = offset ? "HTTP/1.1 206 Partial\r\nContent-Range: bytes " +
                                QByteArray::number(offset) + "-" +
                                QByteArray::number(payload.size() - 1) + "/" +
                                QByteArray::number(payload.size()) + "\r\n"
                          : QByteArray("HTTP/1.1 200 OK\r\n");
        response += "ETag: test-v1\r\nContent-Type: "
                    "application/octet-stream\r\nContent-Length: " +
                    QByteArray::number(body.size()) + "\r\n\r\n" + body;
      }
      socket->write(response);
      socket->disconnectFromHost();
    });
    QObject::connect(socket, &QSslSocket::disconnected, socket,
                     &QObject::deleteLater);
    socket->startServerEncryption();
  }
};
Request fixture(const QString &root) {
  Request r;
  r.root = root;
  r.gameId = "synthetic";
  r.variantId = "flat";
  r.runtime.gameId = r.gameId;
  QDir().mkpath(root + "/games/synthetic/setup");
  atomicWrite(root + "/games/synthetic/setup/example.exe",
              "synthetic binary v1");
  r.recipe =
      Json{{"format", 1},
           {"variant",
            {{"flat",
              {{"status", "stable"},
               {"version", "v1"},
               {"installed_when", "file:${install_dir}/example.exe"},
               {"step",
                Json::array(
                    {Json{{"id", "copy"},
                          {"do", "copy"},
                          {"from", (root + "/games/synthetic/setup/example.exe")
                                       .toStdString()},
                          {"to", "${install_dir}/example.exe"}},
                     Json{{"id", "settings"},
                          {"do", "write-config"},
                          {"file", "${install_dir}/settings.ini"},
                          {"format", "ini"},
                          {"create", true},
                          {"set", Json::array({Json{{"section", "Global"},
                                                    {"key", "enabled"},
                                                    {"value", 1}}})}}})}}}}}};
  return r;
}
QMap<QString, QByteArray> tree(const QString &root) {
  QMap<QString, QByteArray> out;
  QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const auto path = it.next();
    if (QFileInfo(path).isDir())
      out[QDir(root).relativeFilePath(path) + "/"] = QByteArray();
    else
      out[QDir(root).relativeFilePath(path)] = readBytes(path);
  }
  return out;
}
void archiveFile(const QString &path, const QStringList &names,
                 const QString &format = "zip", bool symlink = false) {
  auto *a = archive_write_new();
  if (format == "zip")
    archive_write_set_format_zip(a);
  else if (format == "7z")
    archive_write_set_format_7zip(a);
  else
    archive_write_set_format_pax_restricted(a);
  QVERIFY(archive_write_open_filename(a, QFile::encodeName(path).constData()) ==
          ARCHIVE_OK);
  for (const auto &name : names) {
    auto *e = archive_entry_new();
    archive_entry_set_pathname(e, name.toUtf8().constData());
    archive_entry_set_size(e, symlink ? 0 : 4);
    archive_entry_set_filetype(e, symlink ? AE_IFLNK : AE_IFREG);
    archive_entry_set_perm(e, 0644);
    if (symlink)
      archive_entry_set_symlink(e, "../escape");
    QVERIFY(archive_write_header(a, e) == ARCHIVE_OK);
    if (!symlink)
      QVERIFY(archive_write_data(a, "test", 4) == 4);
    archive_entry_free(e);
  }
  archive_write_close(a);
  archive_write_free(a);
}
} // namespace
class InstallTest : public QObject {
  Q_OBJECT
private slots:
  void canonical() {
    const Json j{{"z", 1}, {"a", "\n\t\"\\"}, {"b", true}};
    QCOMPARE(
        canonicalJson(j),
        QByteArray("{\"a\":\"\\u000a\\u0009\\\"\\\\\",\"b\":true,\"z\":1}"));
    QVERIFY_THROWS_EXCEPTION(Error, canonicalJson(Json{{"float", 1.2}}));
  }
  void envelope() {
    QTemporaryDir temp;
    const auto file = temp.path() + "/state.toml";
    writeEnvelope(file, Json{{"v", 1}});
    writeEnvelope(file, Json{{"v", 2}});
    atomicWrite(file, "corrupt");
    QStringList warnings;
    QCOMPARE(readEnvelope(file, &warnings).value("v", 0), 1);
    QVERIFY(!warnings.isEmpty());
    atomicWrite(file, "schema = 2\n");
    QVERIFY_THROWS_EXCEPTION(Error, readEnvelope(file));
    QVERIFY_THROWS_EXCEPTION(Error, writeEnvelope(file, Json{{"v", 3}}));
  }
  void names_data() {
    QTest::addColumn<QString>("name");
    for (const auto &name :
         QStringList{"../escape", "/absolute", "C:/evil", "x:stream", "con.txt",
                     "AUX", "x.", "x ", "a/../../evil"})
      QTest::newRow(qPrintable(name)) << name;
  }
  void names() {
    QFETCH(QString, name);
    QVERIFY_THROWS_EXCEPTION(Error, validateRelative(name));
  }
  void expansion() {
    QMap<QString, QString> vars{{"install_dir", "/fixture"}};
    QCOMPARE(expand("$${literal} ${install_dir}", vars),
             QString("${literal} /fixture"));
    QVERIFY_THROWS_EXCEPTION(Error, expand("${missing}", vars));
    QVERIFY_THROWS_EXCEPTION(Error, expand("${env:PATH}", vars, true));
  }
  void pathReparseEscape() {
    QTemporaryDir temp;
    const auto root = temp.path() + "/Root",
               other = temp.path() + "/root-sibling";
    QDir().mkpath(root);
    QDir().mkpath(other);
#ifdef Q_OS_WIN
    const auto junction = root + "/escape";
    QProcess p;
    p.start("cmd.exe",
            {"/c", "mklink", "/J", QDir::toNativeSeparators(junction),
             QDir::toNativeSeparators(other)});
    QVERIFY(p.waitForFinished());
    QCOMPARE(p.exitCode(), 0);
#else
    QVERIFY(QFile::link(other, root + "/escape"));
#endif
    QVERIFY_THROWS_EXCEPTION(Error,
                             scopedPath(root + "/escape/file.exe", root));
    QVERIFY_THROWS_EXCEPTION(Error, scopedPath(root + "/escape", root));
  }
  void userAliasIdentity() {
    QTemporaryDir temp;
    const auto source = temp.path() + "/local-media.zip";
    archiveFile(source, {"source.bin"});
    const auto original = readBytes(source);
    const Json requirements = Json::array(
        {Json{{"set", "synthetic-device"},
              {"sourcePath", source.toStdString()},
              {"entries", Json::array({Json{{"sourceName", "source.bin"},
                                            {"targetName", "device.bin"},
                                            {"crc32", 3632233996U},
                                            {"size", 4}}})}}});
    const auto destination = temp.path() + "/alias.zip";
    Archive::makeUserMediaAlias(requirements, destination);
    QVERIFY(QFileInfo(destination).isFile());
    auto bad = requirements;
    bad[0]["entries"][0]["crc32"] = 1;
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::makeUserMediaAlias(bad, temp.path() + "/bad.zip"));
    QVERIFY(!QFileInfo(temp.path() + "/bad.zip").exists());
    QCOMPARE(readBytes(source), original);
  }
  void internalFoldersCannotRedirectWrites() {
    for (const auto &folder :
         QStringList{"user/state", "user/state/locks", "user/state/backups",
                     "user/logs/install", "user/cache/artifacts"}) {
      QTemporaryDir temp;
      auto r = fixture(temp.path() + "/hub");
      const auto outside = temp.path() + "/outside",
                 link = r.root + "/" + folder;
      QDir().mkpath(outside);
      QDir().mkpath(QFileInfo(link).absolutePath());
#ifdef Q_OS_WIN
      QProcess p;
      p.start("cmd.exe", {"/c", "mklink", "/J", QDir::toNativeSeparators(link),
                          QDir::toNativeSeparators(outside)});
      QVERIFY(p.waitForFinished());
      QCOMPARE(p.exitCode(), 0);
#else
      QVERIFY(QFile::link(outside, link));
#endif
      QVERIFY_THROWS_EXCEPTION(Error, Engine().plan(r));
      QVERIFY(!Engine().install(r).success);
      QVERIFY(!Engine().uninstall(r).success);
      QVERIFY(!Engine().recover(r).success);
      QVERIFY(tree(outside).empty());
    }
  }
  void copySourceAndPredicateCannotEscape() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    r.recipe["variant"]["flat"]["step"][0]["from"] =
        (r.root + "/games/synthetic/setup/../../../../outside.txt")
            .toStdString();
    QVERIFY_THROWS_EXCEPTION(Error, Engine().plan(r));
    QVERIFY(!QFileInfo(r.root + "/user").exists());
    r = fixture(temp.path());
    r.recipe["variant"]["flat"]["installed_when"] =
        "file:/outside/existing.txt";
    QVERIFY_THROWS_EXCEPTION(Error, Engine().plan(r));
    QVERIFY(!QFileInfo(r.root + "/user").exists());
    r = fixture(temp.path());
    r.recipe["variant"]["flat"]["step"][1] =
        Json{{"id", "escape"},
             {"do", "copy"},
             {"from", "${steps.copy.dir}/../../outside.txt"},
             {"to", "${install_dir}/escape.txt"}};
    QVERIFY_THROWS_EXCEPTION(Error, Engine().plan(r));
    QVERIFY(!QFileInfo(r.root + "/user").exists());
  }
  void updateRefusesUnownedLinks() {
    QTemporaryDir temp;
    auto r = fixture(temp.path() + "/hub");
    Options o;
    o.survivalMs = 0;
    QVERIFY(Engine(o).install(r).success);
    const auto outside = temp.path() + "/outside",
               link = r.root + "/installed/synthetic/flat/owner-link";
    QDir().mkpath(outside);
    atomicWrite(outside + "/save.dat", "owner save");
#ifdef Q_OS_WIN
    QProcess p;
    p.start("cmd.exe", {"/c", "mklink", "/J", QDir::toNativeSeparators(link),
                        QDir::toNativeSeparators(outside)});
    QVERIFY(p.waitForFinished());
    QCOMPARE(p.exitCode(), 0);
#else
    QVERIFY(QFile::link(outside, link));
#endif
    r.operation = "update";
    r.recipe["variant"]["flat"]["version"] = "v2";
    QVERIFY(!Engine(o).install(r).success);
    QVERIFY(QFileInfo(link).isSymLink() || QFileInfo(link).isJunction());
    QCOMPARE(readBytes(outside + "/save.dat"), QByteArray("owner save"));
  }
  void plansFailBeforeIO() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    auto &steps = r.recipe["variant"]["flat"]["step"];
    for (const auto &kind : QStringList{"run", "adb-install", "registry",
                                        "patch-text", "unknown"}) {
      steps[0]["do"] = kind.toStdString();
      QVERIFY_THROWS_EXCEPTION(Error, Engine().plan(r));
      QVERIFY(!QFileInfo(temp.path() + "/user").exists());
    }
  }
  void stagingStepIdCannotEscape() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    r.recipe["variant"]["flat"]["step"][0] =
        Json{{"id", "../../../../../escape"},
             {"do", "extract"},
             {"from", (temp.path() + "/synthetic.zip").toStdString()},
             {"to", "${install_dir}"}};
    QVERIFY_THROWS_EXCEPTION(Error, Engine().plan(r));
    QVERIFY(!QFileInfo(temp.path() + "/user").exists());
    r.recipe["variant"]["flat"]["step"][0]["id"] = "a\\b";
    QVERIFY_THROWS_EXCEPTION(Error, Engine().plan(r));
  }
  void repeatedConfigWritesRetainOwnership() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    auto &steps = r.recipe["variant"]["flat"]["step"];
    steps.push_back(Json{
        {"id", "settings-again"},
        {"do", "write-config"},
        {"file", "${install_dir}/settings.ini"},
        {"format", "ini"},
        {"set", Json::array({Json{
                    {"section", "Global"}, {"key", "other"}, {"value", 2}}})}});
    Options o;
    o.survivalMs = 0;
    const auto installed = Engine(o).install(r);
    QVERIFY2(installed.success, qPrintable(installed.message));
    const auto &rows = installed.manifest["file"];
    QCOMPARE(rows.size(), size_t(2));
    QCOMPARE(rows[1]["keys"].size(), size_t(2));
    QCOMPARE(string(rows[1], "prior"), QString("absent"));
    QVERIFY(Engine(o).uninstall(r).success);
    QVERIFY(!QFileInfo(temp.path() + "/installed/synthetic/flat/settings.ini")
                 .exists());
  }
  void repairKeepsManagedUserEdits() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    Options o;
    o.survivalMs = 0;
    QVERIFY(Engine(o).install(r).success);
    const auto file = temp.path() + "/installed/synthetic/flat/settings.ini";
    atomicWrite(file, "[Global]\nenabled = 9\nunknown = mine\n");
    r.operation = "repair";
    const auto repaired = Engine(o).install(r);
    QVERIFY2(repaired.success, qPrintable(repaired.message));
    QCOMPARE(repaired.state, QString("installed-with-warnings"));
    QCOMPARE(readBytes(file),
             QByteArray("[Global]\nenabled = 9\nunknown = mine\n"));
    QCOMPARE(Engine(o).uninstall(r).state, QString("uninstall-incomplete"));
    QVERIFY(readBytes(file).contains("enabled = 9"));
    QVERIFY(readBytes(file).contains("unknown = mine"));
  }
  void hostGuard() {
    QVERIFY_THROWS_EXCEPTION(Error,
                             ArtifactStore::validateUrl("http://github.com/a"));
    QVERIFY_THROWS_EXCEPTION(
        Error, ArtifactStore::validateUrl("https://not-allowed.example/a"));
    QVERIFY_THROWS_EXCEPTION(
        Error, ArtifactStore::validateUrl("https://user:secret@github.com/a"));
    ArtifactStore::validateUrl(
        "https://release-assets.githubusercontent.com/a");
  }
  void fakeHttps() {
    QTemporaryDir temp;
    HttpsServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    TestTrust trust{QSslCertificate::fromData(readBytes(
                        QString(AC_TEST_FIXTURES) + "/tls/localhost-cert.pem")),
                    {"localhost"},
                    server.url()};
    ArtifactStore store(temp.path(), {}, trust);
    Json step{{"do", "download"},
              {"url", "https://github.com/redirect"},
              {"version", "v1"},
              {"sha256", sha256(server.payload).toStdString()},
              {"name", "tool.dat"}};
    const auto cached = store.acquire(step);
    QCOMPARE(readBytes(cached), server.payload);
    QCOMPARE(server.requests.size(), 2);
    QVERIFY(!server.requests[0].contains("Cookie:"));
    QVERIFY(!server.requests[0].contains("Authorization:"));
    QCOMPARE(store.acquire(step), cached);
    QCOMPARE(server.requests.size(), 2);
    QFile::remove(cached);
    const auto part = cached + ".part";
    atomicWrite(part, server.payload.left(5));
    atomicWrite(part + ".validator", "test-v1");
    step["url"] = "https://github.com/artifact";
    QCOMPARE(readBytes(store.acquire(step)), server.payload);
    QVERIFY(server.requests.last().toLower().contains("range: bytes=5-"));
    QVERIFY(server.requests.last().toLower().contains("if-range: test-v1"));
    step["url"] = "https://github.com/denied";
    QFile::remove(cached);
    QVERIFY_THROWS_EXCEPTION(Error, store.acquire(step));
    step["url"] = "https://github.com/html";
    QVERIFY_THROWS_EXCEPTION(Error, store.acquire(step));
    const auto release = store.githubRelease("synthetic/example", "v1");
    QCOMPARE(string(release, "tag_name"), QString("v1"));
    const auto calls = server.requests.size();
    store.githubRelease("synthetic/example", "v1");
    QCOMPARE(server.requests.size(), calls);
    QVERIFY_THROWS_EXCEPTION(
        Error, store.githubRelease("synthetic/example", "missing"));
    QVERIFY_THROWS_EXCEPTION(
        Error, store.githubRelease("synthetic/example", "limited"));
  }
  void configs() {
    const QByteArray initial =
        "; keep\r\n[Global]\r\n  enabled = 0  ; comment\r\nunknown=keep\r\n";
    const auto edit = editConfig(
        initial, "ini",
        Json::array(
            {Json{{"section", "Global"}, {"key", "enabled"}, {"value", 1}}}),
        {}, {});
    QCOMPARE(edit.bytes, QByteArray("; keep\r\n[Global]\r\n  enabled = 1  ; "
                                    "comment\r\nunknown=keep\r\n"));
    bool kept = false;
    QCOMPARE(undoConfig(edit.bytes, "ini", edit.keys, kept), initial);
    QVERIFY(!kept);
    const auto changed =
        QByteArray(edit.bytes).replace("enabled = 1", "enabled = 3");
    QCOMPARE(undoConfig(changed, "ini", edit.keys, kept), changed);
    QVERIFY(kept);
    QVERIFY_THROWS_EXCEPTION(
        Error,
        editConfig("x=1\nx=2\n", "cfg",
                   Json::array({Json{{"key", "x"}, {"value", 3}}}), {}, {}));
  }
  void configTypes() {
    Json settings{{"cover", "hold"}, {"laser", true}, {"pitch", 4}};
    const auto e =
        editConfig("# keep\n", "cfg",
                   Json::array({Json{{"key", "cover"},
                                     {"from", "settings.cover"},
                                     {"map", {{"hold", 1}}}},
                                Json{{"key", "laser"},
                                     {"from", "settings.laser"},
                                     {"bool", {{"on", 1}, {"off", 0}}}},
                                Json{{"key", "pitch"},
                                     {"from", "settings.pitch"},
                                     {"type", "int"}}}),
                   settings, {});
    QVERIFY(e.bytes.contains("cover = 1"));
    QVERIFY(e.bytes.contains("laser = 1"));
    QVERIFY(e.bytes.contains("pitch = 4"));
  }
  void jsonTomlAndEncoding() {
    const Json settings{{"enabled", true}, {"gain", 2.3456}};
    const auto json = editConfig(
        "{\"unknown\":\"keep\",\"Video\":{\"enabled\":false}}", "json",
        Json::array({Json{{"key", "Video.enabled"},
                          {"from", "settings.enabled"},
                          {"type", "bool"}},
                     Json{{"key", "gain"},
                          {"from", "settings.gain"},
                          {"type", "float"},
                          {"decimals", 2}}}),
        settings, {});
    const auto parsed = Json::parse(json.bytes.toStdString());
    QVERIFY(parsed["Video"]["enabled"].is_boolean());
    QCOMPARE(parsed["gain"].get<double>(), 2.35);
    QCOMPARE(parsed["unknown"].get<std::string>(), std::string("keep"));
    QVERIFY(json.reserialized);
    QVERIFY_THROWS_EXCEPTION(
        Error, editConfig("{\"x\":1,\"x\":2}", "json", Json::array(), {}, {}));
    const auto toml = editConfig(
        "# keep\r\n[Video]\r\nenabled = false\r\nunknown = \"keep\"\r\n",
        "toml",
        Json::array({Json{{"key", "Video.enabled"}, {"value", true}},
                     Json{{"key", "Video.gain"},
                          {"from", "settings.gain"},
                          {"type", "float"},
                          {"decimals", 2}}}),
        settings, {});
    QVERIFY(toml.bytes.contains("# keep\r\n"));
    QVERIFY(toml.bytes.contains("enabled = true"));
    QVERIFY(toml.bytes.contains("gain = 2.35"));
    QVERIFY(toml.bytes.contains("unknown = \"keep\""));
    QByteArray utf16 = QByteArray::fromHex("fffe");
    for (const auto c : QByteArray("# keep\r\nx = 0\r\n")) {
      utf16 += c;
      utf16 += '\0';
    }
    const auto encoded = editConfig(
        utf16, "cfg", Json::array({Json{{"key", "x"}, {"value", 1}}}), {}, {});
    QVERIFY(encoded.bytes.startsWith(QByteArray::fromHex("fffe")));
    bool kept = false;
    QCOMPARE(undoConfig(encoded.bytes, "cfg", encoded.keys, kept), utf16);
  }
  void roundtripUninstall() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    const auto live = temp.path() + "/installed/synthetic/flat";
    QDir().mkpath(live);
    atomicWrite(live + "/settings.ini",
                "; original\n[Global]\nenabled = 0\nunknown = yes\n");
    atomicWrite(live + "/other.txt", "unowned");
    const auto before = tree(live);
    Options o;
    o.survivalMs = 0;
    auto installed = Engine(o).install(r);
    QVERIFY2(installed.success, qPrintable(installed.message));
    const auto removed = Engine(o).uninstall(r);
    QVERIFY2(removed.success, qPrintable(removed.message));
    QCOMPARE(tree(live), before);
  }
  void editedUninstall() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    Options o;
    o.survivalMs = 0;
    QVERIFY(Engine(o).install(r).success);
    const auto live = temp.path() + "/installed/synthetic/flat";
    atomicWrite(live + "/example.exe", "user changed");
    const auto result = Engine(o).uninstall(r);
    QCOMPARE(result.state, QString("uninstall-incomplete"));
    QCOMPARE(readBytes(live + "/example.exe"), QByteArray("user changed"));
    QCOMPARE(result.manifest["file"].size(), size_t(1));
  }
  void recoveryEveryBoundary() {
    QTemporaryDir baseline;
    auto first = fixture(baseline.path());
    Options plain;
    plain.survivalMs = 0;
    int count = 0;
    plain.fault = [&](const QString &) { ++count; };
    QVERIFY(Engine(plain).install(first).success);
    QVERIFY(count > 12);
    for (int cut = 1; cut <= count; ++cut) {
      QTemporaryDir temp;
      auto r = fixture(temp.path());
      Options faulted;
      faulted.survivalMs = 0;
      int n = 0;
      faulted.fault = [&](const QString &) {
        if (++n == cut)
          throw std::runtime_error("simulated crash");
      };
      bool crashed = false;
      try {
        Engine(faulted).install(r);
      } catch (...) {
        crashed = true;
      }
      QVERIFY(crashed);
      const auto recovered = Engine().recover(r);
      QVERIFY2(
          recovered.success,
          qPrintable(QString("cut %1: %2").arg(cut).arg(recovered.message)));
      const auto state = readEnvelope(stateBase(r) + ".toml");
      const auto live = temp.path() + "/installed/synthetic/flat";
      if (string(state, "state").startsWith("installed")) {
        QVERIFY(QFileInfo(live + "/example.exe").isFile());
        QVERIFY(QFileInfo(live + "/settings.ini").isFile());
      } else
        QVERIFY(tree(live).isEmpty());
    }
  }
  void archives() {
    QTemporaryDir temp;
    for (const auto &format : QStringList{"zip", "7z", "tar"}) {
      const auto path = temp.path() + "/good." + format;
      archiveFile(path, {"wrapper/example.exe"}, format);
      QList<ArchiveEntry> entries;
      try {
        entries = Archive::inspect(path, {1024, 10});
      } catch (const Error &e) {
        QFAIL(qPrintable(e.code + ": " + QString::fromUtf8(e.what())));
      }
      QCOMPARE(entries.size(), 1);
      QStringList files;
      try {
        files = Archive::extract(path, temp.path() + "/stage-" + format,
                                 {1024, 10}, Json::object(), 1);
      } catch (const Error &e) {
        QFAIL(qPrintable(format + " " + e.code + ": " +
                         QString::fromUtf8(e.what())));
      }
      QCOMPARE(files, QStringList{"example.exe"});
      QCOMPARE(readBytes(temp.path() + "/stage-" + format + "/example.exe"),
               QByteArray("test"));
    }
  }
  void unsafeArchives() {
    QTemporaryDir temp;
    for (const auto &name : QStringList{"../escape", "C:/evil", "con.txt",
                                        "x:stream", "game.iso"}) {
      const auto path = temp.path() + "/bad.zip";
      archiveFile(path, {name});
      QVERIFY_THROWS_EXCEPTION(Error, Archive::inspect(path, {1024, 10}));
    }
    archiveFile(temp.path() + "/collision.zip", {"a.exe", "A.exe"});
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::inspect(temp.path() + "/collision.zip", {1024, 10}));
    archiveFile(temp.path() + "/symlink.tar", {"link"}, "tar", true);
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::inspect(temp.path() + "/symlink.tar", {1024, 10}));
    archiveFile(temp.path() + "/cap.zip", {"a.exe", "b.exe"});
    QVERIFY_THROWS_EXCEPTION(
        Error,
        Archive::inspect(
            temp.path() + "/cap.zip", {1024, 10},
            Json{{"sha256", Json::array({sha256("test").toStdString()})}}));
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::inspect(temp.path() + "/cap.zip", {4, 10}));
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::inspect(temp.path() + "/cap.zip", {1024, 1}));
  }
  void updateRollback() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    Options o;
    o.survivalMs = 0;
    QVERIFY(Engine(o).install(r).success);
    const auto live = temp.path() + "/installed/synthetic/flat";
    atomicWrite(live + "/save.dat", "unowned save");
    const auto before = tree(live);
    r.recipe["variant"]["flat"]["version"] = "v2";
    r.recipe["variant"]["flat"]["installed_when"] =
        "file:${install_dir}/missing.exe";
    atomicWrite(temp.path() + "/games/synthetic/setup/example.exe", "v2");
    const auto result = Engine(o).install(r);
    QVERIFY(!result.success);
    QCOMPARE(tree(live), before);
  }
  void runtimePreservesOtherFields() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    r.runtime.lastPlayed = 42;
    r.runtime.mediaFound << "synthetic";
    r.runtime.artSource = ArtSource::User;
    const auto snapshot = Engine().runtimeState(r);
    QCOMPARE(snapshot.lastPlayed, qint64(42));
    QCOMPARE(snapshot.artSource, ArtSource::User);
    QCOMPARE(snapshot.mediaFound, r.runtime.mediaFound);
  }
  void extractMediaAndShortcut() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    const auto package = temp.path() + "/package.zip";
    archiveFile(package, {"wrapper/example.exe"});
    const auto pin = hashFile(package);
    atomicWrite(temp.path() + "/user/cache/artifacts/" + pin,
                readBytes(package));
    const auto mediaPath = temp.path() + "/synthetic.media";
    atomicWrite(mediaPath, "synthetic media");
    r.bindings = Json{{"media",
                       {{"synthetic",
                         {{"path", mediaPath.toStdString()},
                          {"verified", true},
                          {"sha256", hashFile(mediaPath).toStdString()}}}}}};
    r.handover = package;
    auto &steps = r.recipe["variant"]["flat"]["step"];
    steps = Json::array(
        {Json{{"id", "github"},
              {"do", "github-release"},
              {"repo", "synthetic/example"},
              {"tag", "v1"},
              {"asset", "tool.zip"},
              {"archive", "zip"},
              {"sha256", pin.toStdString()}},
         Json{{"id", "download"},
              {"do", "download"},
              {"url", "https://github.com/synthetic/example/tool.zip"},
              {"version", "v1"},
              {"name", "tool.zip"},
              {"archive", "zip"},
              {"sha256", pin.toStdString()}},
         Json{{"id", "locate"},
              {"do", "locate-package"},
              {"pattern", "package.zip"},
              {"kind", "archive"},
              {"sha256", pin.toStdString()}},
         Json{{"id", "require"},
              {"do", "require-media"},
              {"media", "synthetic"}},
         Json{{"id", "extract"},
              {"do", "extract"},
              {"from", "${steps.locate.path}"},
              {"to", "${install_dir}"},
              {"strip", "auto"}},
         Json{{"id", "media"},
              {"do", "copy-media"},
              {"media", "synthetic"},
              {"to", "${install_dir}/synthetic.media"},
              {"mode", "auto"}},
         Json{{"id", "shortcut"},
              {"do", "shortcut"},
              {"name", "Synthetic"},
              {"exe", "${install_dir}/example.exe"},
              {"targets", Json::array({"desktop"})}}});
    Options o;
    o.survivalMs = 0;
    QStringList transcript;
    o.event = [&](const QVariantMap &v) {
      transcript << v.value("kind").toString() + ":" +
                        v.value("text").toString();
    };
    const auto result = Engine(o).install(r);
    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(result.state, QString("installed-with-warnings"));
    QVERIFY(
        transcript.join('\n').contains("warn:Shortcut adapter is unavailable"));
    QVERIFY(Engine(o).uninstall(r).success);
    QCOMPARE(readBytes(mediaPath), QByteArray("synthetic media"));
    QVERIFY(!QFileInfo(temp.path() + "/installed/synthetic/flat").exists());
  }
  void updateAndUninstallEveryBoundary() {
    for (const auto &operation : QStringList{"update", "uninstall"}) {
      int count = 0;
      {
        QTemporaryDir temp;
        auto r = fixture(temp.path());
        Options o;
        o.survivalMs = 0;
        QVERIFY(Engine(o).install(r).success);
        const auto live = temp.path() + "/installed/synthetic/flat";
        QDir().mkpath(live + "/unowned-empty");
        atomicWrite(live + "/unowned-save.dat", "save");
        if (operation == "update") {
          r.recipe["variant"]["flat"]["version"] = "v2";
          atomicWrite(temp.path() + "/games/synthetic/setup/example.exe",
                      "synthetic binary v2");
        }
        o.fault = [&](const QString &) { ++count; };
        const auto result = operation == "update" ? Engine(o).install(r)
                                                  : Engine(o).uninstall(r);
        QVERIFY2(result.success, qPrintable(result.message));
      }
      for (int cut = 1; cut <= count; ++cut) {
        QTemporaryDir temp;
        auto r = fixture(temp.path());
        Options normal;
        normal.survivalMs = 0;
        QVERIFY(Engine(normal).install(r).success);
        const auto live = temp.path() + "/installed/synthetic/flat";
        QDir().mkpath(live + "/unowned-empty");
        atomicWrite(live + "/unowned-save.dat", "save");
        const auto before = tree(live);
        if (operation == "update") {
          r.recipe["variant"]["flat"]["version"] = "v2";
          atomicWrite(temp.path() + "/games/synthetic/setup/example.exe",
                      "synthetic binary v2");
        }
        Options faulted;
        faulted.survivalMs = 0;
        int n = 0;
        faulted.fault = [&](const QString &) {
          if (++n == cut)
            throw std::runtime_error("crash");
        };
        bool crashed = false;
        try {
          if (operation == "update")
            Engine(faulted).install(r);
          else
            Engine(faulted).uninstall(r);
        } catch (...) {
          crashed = true;
        }
        QVERIFY2(crashed,
                 qPrintable(operation + QString(" cut %1 observed %2 of %3")
                                            .arg(cut)
                                            .arg(n)
                                            .arg(count)));
        const auto recovered = Engine(normal).recover(r);
        QVERIFY2(recovered.success,
                 qPrintable(operation + QString(" cut %1: ").arg(cut) +
                            recovered.message));
        const auto state = readEnvelope(stateBase(r) + ".toml");
        const bool committed = operation == "update"
                                   ? string(state, "installed_version") == "v2"
                                   : string(state, "state") == "not-installed";
        if (!committed)
          QCOMPARE(tree(live), before);
        else if (operation == "update")
          QCOMPARE(readBytes(live + "/example.exe"),
                   QByteArray("synthetic binary v2"));
        else
          QVERIFY(!QFileInfo(live + "/example.exe").exists());
        QVERIFY(QFileInfo(live + "/unowned-empty").isDir());
        QCOMPARE(readBytes(live + "/unowned-save.dat"), QByteArray("save"));
      }
    }
  }
  void flatProfileIsolation() {
    QTemporaryDir temp;
    const auto tool = temp.path() + "/located";
    QDir().mkpath(tool + "/resources");
    atomicWrite(tool + "/pcsx2-qt.exe", "synthetic executable");
    atomicWrite(tool + "/portable.ini", "marker");
    atomicWrite(tool + "/owner.ini", "do not read");
    atomicWrite(tool + "/resources/game-index.yaml", "public resource");
    for(const auto *name:{"shader.fx","shader.h","shader.hlsl","icon.ico","catalog.mo","sound.wav","LICENSESCN"})
      atomicWrite(tool + "/resources/" + name, "synthetic public resource");
    const auto disc = temp.path() + "/synthetic.disc",
               bios = temp.path() + "/synthetic-bios.dat";
    atomicWrite(disc, "disc");
    atomicWrite(bios, "bios");
    const auto before = tree(tool);
    GameRecord game;
    game.id = "synthetic";
    game.raw =
        Json{{"media", Json::array({Json{{"kind", "ps2-disc"}, {"id", "disc"}},
                                    Json{{"kind", "bios"}, {"id", "bios"}}})}};
    const Json emulator{
        {"id", "pcsx2"},
        {"launch",
         {{"args",
           Json::array({"-batch", "-nogui", "-nofullscreen", "-datapath",
                        "${profile_dir}", "--", "${media.disc}"})},
          {"cwd", "${profile_dir}"}}}};
    const Json bindings{
        {"tools",
         {{"pcsx2", {{"path", (tool + "/pcsx2-qt.exe").toStdString()}}}}},
        {"media",
         {{"disc", {{"path", disc.toStdString()}, {"verified", true}}},
          {"bios", {{"path", bios.toStdString()}, {"verified", true}}}}}};
    const auto plan = makeFlatLaunchPlan(game, emulator, temp.path(), bindings);
    QVERIFY(plan.executable.contains("/application/"));
    const auto receipt = prepareFlatLaunch(plan, temp.path());
    QCOMPARE(tree(tool), before);
    for(const auto *name:{"shader.fx","shader.h","shader.hlsl","icon.ico","catalog.mo","sound.wav","LICENSESCN"})
      QVERIFY(QFileInfo(QFileInfo(plan.executable).absolutePath()+"/resources/"+name).isFile());
    QVERIFY(
        !QFileInfo(QFileInfo(plan.executable).absolutePath() + "/portable.ini")
             .exists());
    QVERIFY(!QFileInfo(QFileInfo(plan.executable).absolutePath() + "/owner.ini")
                 .exists());
    QVERIFY(!receipt["files"].empty());
    const auto profile = plan.cwd + "/PCSX2/inis/PCSX2.ini";
    QVERIFY(readBytes(profile).contains("SetupWizardIncomplete = false"));
    atomicWrite(profile, "; user config left untouched\n");
    prepareFlatLaunch(plan, temp.path());
    QCOMPARE(readBytes(profile), QByteArray("; user config left untouched\n"));
    const auto originalExecutable = readBytes(plan.executable);
    atomicWrite(plan.executable, "edited clone");
    QVERIFY_THROWS_EXCEPTION(Error, prepareFlatLaunch(plan, temp.path()));
    QCOMPARE(readBytes(plan.executable), QByteArray("edited clone"));
    atomicWrite(plan.executable, originalExecutable);
    const auto marker =
        QFileInfo(plan.executable).absolutePath() + "/portable.txt";
    atomicWrite(marker, "outside routing\n");
    QVERIFY_THROWS_EXCEPTION(Error, prepareFlatLaunch(plan, temp.path()));
    QCOMPARE(readBytes(marker), QByteArray("outside routing\n"));
  }
  void goldenPlan() {
    QTemporaryDir temp;
    auto r = fixture(temp.path());
    QCOMPARE(Engine().plan(r).text,
             QString("PLAN  synthetic / flat  format 1\n  1/2  copy  copy\n  "
                     "2/2  settings  write-config\n  verify  "
                     "file:${install_dir}/example.exe\n"));
  }
};
QTEST_GUILESS_MAIN(InstallTest)
#include "InstallTest.moc"
