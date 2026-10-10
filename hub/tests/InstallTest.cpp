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
#include <filesystem>
#include <archive_entry.h>
using namespace ac;
using namespace ac::install;
namespace {
class HttpsServer : public QTcpServer {
public:
  QByteArray payload = "synthetic tool artifact";
  bool notModified = false;
  QByteArray releasePayload = "{\"tag_name\":\"v1\",\"assets\":[]}";
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
      if (notModified && route == "/artifact")
        response = "HTTP/1.1 304 Not Modified\r\nETag: test-v1\r\nContent-Length: 0\r\n\r\n";
      else if (route == "/redirect")
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
        const QByteArray body = releasePayload;
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
QString catalogFixtureRoot() {
  return QDir(QString(AC_TEST_FIXTURES) + "/../../..").absolutePath();
}
Json testGuard() { return loadContentGuard(catalogFixtureRoot()); }
Request fixture(const QString &root) {
  atomicWrite(root + "/data/content-guard.toml",
              readBytes(catalogFixtureRoot() + "/data/content-guard.toml"));
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
    const auto goodPrevious = readBytes(file + ".previous");
    writeEnvelope(file, Json{{"v", 3}});
    QCOMPARE(readBytes(file + ".previous"), goodPrevious);
    atomicWrite(file, "corrupt again");
    QCOMPARE(readEnvelope(file).value("v", 0), 1);
    atomicWrite(file, "schema = 2\n");
    QVERIFY_THROWS_EXCEPTION(Error, readEnvelope(file));
    QVERIFY_THROWS_EXCEPTION(Error, writeEnvelope(file, Json{{"v", 3}}));
  }
  void names_data() {
    QTest::addColumn<QString>("name");
    for (const auto &name :
         QStringList{"../escape", "/absolute", QDir::rootPath()+"evil", "x:stream", "con.txt",
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
  void emulatorPolicy_data() {
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("consent");
    const QDir dir(catalogFixtureRoot() + "/data/emulators");
    const auto files = dir.entryList({"*.toml"}, QDir::Files);
    QVERIFY(!files.isEmpty());
    for (const auto &file : files)
      for (const bool consent : {false, true})
        QTest::newRow(qPrintable(file + (consent ? "-consent" : "-automatic")))
            << dir.filePath(file) << consent;
  }
  void emulatorPolicy() {
    QFETCH(QString, file);
    QFETCH(bool, consent);
    QTemporaryDir temp;
    const auto manifest = CatalogLoader::parseToml(file);
    const auto gate = string(manifest, "gate");
    const bool blocked = string(manifest, "redistribution") != "download-from-upstream-only" ||
                         (manifest.contains("gate") && gate != "consent-install");
    const bool requiresConsent = !blocked && gate == "consent-install" && !consent;
    try {
      auto request = Engine::emulatorRequest(temp.path(), manifest, consent);
      QVERIFY(!blocked && !requiresConsent);
      request.catalogRoot = catalogFixtureRoot();
      // Unresolved pins/platforms remain rejected; this exercise never fetches.
      try {
        Engine().plan(request);
      } catch (const Error &error) {
        QCOMPARE(error.code, QString("E_PLAN_INVALID"));
      }
    } catch (const Error &error) {
      if (blocked) {
        QCOMPARE(error.code, QString("E_STEP_DISABLED"));
        if (!gate.isEmpty())
          QVERIFY(QString::fromUtf8(error.what()).contains(gate));
      } else if (requiresConsent) {
        QCOMPARE(error.code, QString("E_CONSENT_REQUIRED"));
        QVERIFY(QString::fromUtf8(error.what()).contains(string(manifest, "consent")));
      } else
        QCOMPARE(error.code, QString("E_PLATFORM_UNVERIFIED"));
    }
    QVERIFY(tree(temp.path()).isEmpty());
  }
  void unknownEmulatorPoliciesFailClosed() {
    const Json base{{"id", "synthetic"}, {"redistribution", "download-from-upstream-only"}};
    for (const auto &gate : {Json(""), Json("owner-review"), Json(false)}) {
      auto manifest = base;
      manifest["gate"] = gate;
      for (const bool consent : {false, true}) {
        try {
          Engine::emulatorRequest("unused", manifest, consent);
          QFAIL("Unknown gate allowed automatic install");
        } catch (const Error &error) {
          QCOMPARE(error.code, QString("E_STEP_DISABLED"));
        }
      }
    }
    for (const auto &policy : {Json("manual-only"), Json("not-redistributable"), Json("unknown"), Json(false)}) {
      auto manifest = base;
      manifest["redistribution"] = policy;
      manifest["gate"] = "consent-install";
      try {
        Engine::emulatorRequest("unused", manifest, true);
        QFAIL("Consent bypassed redistribution policy");
      } catch (const Error &error) {
        QCOMPARE(error.code, QString("E_STEP_DISABLED"));
      }
    }
  }
  void contentGuardCatalogPolicy() {
    QTemporaryDir runtime, catalog;
    auto request = fixture(runtime.path());
    // Runtime-root policy must not override a supplied catalog root.
    request.catalogRoot = catalog.path();
    QDir().mkpath(catalog.path() + "/games/synthetic/setup");
    atomicWrite(catalog.path() + "/games/synthetic/setup/example.exe", "synthetic");
    request.recipe["variant"]["flat"]["step"][0]["from"] =
        (catalog.path() + "/games/synthetic/setup/example.exe").toStdString();
    for (const auto &bytes : QList<QByteArray>{QByteArray(), "not toml [", "format=1\nnames=[]\nsha256=[]\n", "format=1\nextensions=[]\nnames=[]\nsha256=[]\n", "format=1\nextensions=[1]\nnames=[]\nsha256=[]\n"}) {
      if (!bytes.isEmpty())
        atomicWrite(catalog.path() + "/data/content-guard.toml", bytes);
      try {
        Engine().plan(request);
        QFAIL("Missing or malformed content guard allowed planning");
      } catch (const Error &error) {
        QCOMPARE(error.code, QString("E_CONTENT_GUARD"));
      }
      QVERIFY(!QFileInfo(runtime.path() + "/user").exists());
    }
    atomicWrite(catalog.path() + "/data/content-guard.toml",
                "format=1\nextensions=[\"custom\"]\nnames=[]\nsha256=[]\n");
    Engine().plan(request);
    const auto guard = loadContentGuard(catalog.path());
    QVERIFY_THROWS_EXCEPTION(Error, contentGuard("GAME.CUSTOM", guard));
    contentGuard("tool.exe", guard);
    atomicWrite(catalog.path() + "/games/synthetic/game.toml", "[[media]]\nset=\"synthetic-set\"\n");
    request.recipe["variant"]["flat"]["step"][0]["from"] =
        (catalog.path() + "/games/synthetic/setup/synthetic-set.zip").toStdString();
    QVERIFY_THROWS_EXCEPTION(Error, Engine().plan(request));
    atomicWrite(catalog.path() + "/games/synthetic/game.toml", "malformed [");
    try {
      Engine().plan(request);
      QFAIL("Unreadable catalog media names allowed planning");
    } catch (const Error &error) {
      QCOMPARE(error.code, QString("E_CONTENT_GUARD"));
    }
  }
  void contentGuardExtensionsAndArchiveMembers() {
    QTemporaryDir temp;
    const auto guard = testGuard();
    for (const auto &extension : guard["extensions"]) {
      const auto name = "synthetic." + QString::fromStdString(extension.get<std::string>()).toUpper();
      QVERIFY_THROWS_EXCEPTION(Error, contentGuard(name, guard));
      const auto path = temp.path() + "/package.zip";
      archiveFile(path, {name});
      try {
        Archive::inspect(path, {}, guard);
        QFAIL("Archive member passed content guard");
      } catch (const Error &error) {
        QCOMPARE(error.code, QString("E_CONTENT_GUARD"));
      }
    }
    for (const auto *extension : {"cdi", "gcm", "pbp", "rvz", "wbfs"})
      QVERIFY_THROWS_EXCEPTION(Error, contentGuard(QString("synthetic.") + extension, guard));
    QVERIFY_THROWS_EXCEPTION(Error, contentGuard("tool.exe", Json::object()));
  }
  void setupExtractSourcePlanning() {
    QTemporaryDir temp;
    auto request = fixture(temp.path());
    auto &step = request.recipe["variant"]["flat"]["step"][0];
    step = Json{{"id", "extract"}, {"do", "extract"},
                {"from", (temp.path() + "/games/synthetic/setup/tool.zip").toStdString()},
                {"to", "${install_dir}"}};
    Engine().plan(request);
    step["from"] = (temp.path() + "/package.zip").toStdString();
    Engine().plan(request);
    step["from"] = (temp.path() + "/games/synthetic/setup/disc.ISO").toStdString();
    try {
      Engine().plan(request);
      QFAIL("Content extension allowed as setup extraction source");
    } catch (const Error &error) {
      QCOMPARE(error.code, QString("E_CONTENT_GUARD"));
    }
    QVERIFY(!QFileInfo(temp.path() + "/user").exists());
  }
  void mediaSourceRejectedBeforeIO_data() {
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("source");
    for (const auto &kind : QStringList{"extract", "copy"})
      for (const auto &source : QStringList{"${steps.required.path}", "${steps.required.dir}/tool.zip",
                                           "${steps.linked.path}", "${media.disc.path}", "direct", "directory"})
        QTest::newRow(qPrintable(kind + "-" + source)) << kind << source;
    QTest::newRow("extract-shorthand") << QString("extract") << QString("required");
    QTest::newRow("copy-hardlink") << QString("copy") << QString("hardlink");
    QTest::newRow("extract-hardlink") << QString("extract") << QString("hardlink");
    QTest::newRow("copy-hardlink-directory") << QString("copy") << QString("hardlink-directory");
  }
  void mediaSourceRejectedBeforeIO() {
    QFETCH(QString, kind);
    QFETCH(QString, source);
    QTemporaryDir temp;
    auto request = fixture(temp.path());
    const auto media = temp.path() + (source.startsWith("hardlink") ? "/library/disc.dat" : "/games/synthetic/setup/disc.dat");
    atomicWrite(media, "synthetic media");
    request.bindings["media"]["disc"] = Json{{"path", media.toStdString()}};
    if (source.startsWith("hardlink")) {
      const auto alias = temp.path()+"/games/synthetic/setup/aliases/media.dat";QDir().mkpath(QFileInfo(alias).absolutePath());std::error_code error;
#ifdef Q_OS_WIN
      std::filesystem::create_hard_link(std::filesystem::path(media.toStdWString()),std::filesystem::path(alias.toStdWString()),error);
#else
      std::filesystem::create_hard_link(std::filesystem::path(media.toStdString()),std::filesystem::path(alias.toStdString()),error);
#endif
      QVERIFY2(!error,error.message().c_str());source=source=="hardlink-directory"?QFileInfo(alias).absolutePath():alias;
    } else if (source == "direct")
      source = media;
    else if (source == "directory")
      source = QFileInfo(media).absolutePath();
    request.recipe["variant"]["flat"]["step"] = Json::array({
        Json{{"id", "required"}, {"do", "require-media"}, {"media", "disc"}},
        Json{{"id", "linked"}, {"do", "copy-media"}, {"media", "disc"}, {"to", "${install_dir}/disc.dat"}},
        Json{{"id", "consumer"}, {"do", kind.toStdString()}, {"from", source.toStdString()}, {"to", "${install_dir}"}}});
    const auto before = tree(temp.path());
    try {
      Engine().plan(request);
      QFAIL("Media source allowed ordinary extract/copy");
    } catch (const Error &error) {
      QCOMPARE(error.code, QString("E_SOURCE_OUT_OF_SCOPE"));
    }
    QCOMPARE(tree(temp.path()), before);
    QVERIFY(!QFileInfo(temp.path() + "/user").exists());
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
  void repairRemergesManagedKeysKeepsUserFields() {
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
             QByteArray("[Global]\nenabled = 1\nunknown = mine\n"));
    QCOMPARE(Engine(o).uninstall(r).state, QString("uninstall-incomplete"));
    QVERIFY(!readBytes(file).contains("enabled"));
    QVERIFY(readBytes(file).contains("unknown = mine"));
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
    const auto cached = store.acquire(step, testGuard());
    QCOMPARE(readBytes(cached), server.payload);
    QCOMPARE(server.requests.size(), 2);
    QVERIFY(!server.requests[0].contains("Cookie:"));
    QVERIFY(!server.requests[0].contains("Authorization:"));
    QCOMPARE(store.acquire(step, testGuard()), cached);
    QCOMPARE(server.requests.size(), 2);
    QFile::remove(cached);
    const auto part = cached + ".part";
    atomicWrite(part, server.payload.left(5));
    atomicWrite(part + ".validator", "test-v1");
    step["url"] = "https://github.com/artifact";
    QCOMPARE(readBytes(store.acquire(step, testGuard())), server.payload);
    QVERIFY(server.requests.last().toLower().contains("range: bytes=5-"));
    QVERIFY(server.requests.last().toLower().contains("if-range: test-v1"));
    step["url"] = "https://github.com/denied";
    QFile::remove(cached);
    QVERIFY_THROWS_EXCEPTION(Error, store.acquire(step, testGuard()));
    step["url"] = "https://github.com/html";
    QVERIFY_THROWS_EXCEPTION(Error, store.acquire(step, testGuard()));
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
  void validatorLifecycleAndWrongPin() {
    HttpsServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir temp;
    TestTrust trust;
    trust.baseUrl = server.url(); trust.hosts = {"localhost"};
    trust.authorities = QSslCertificate::fromPath(QString(AC_TEST_FIXTURES) + "/tls/localhost-cert.pem");
    ArtifactStore store(temp.path(), {}, trust);
    const auto pin = sha256(server.payload);
    Json step{{"do", "download"}, {"url", "https://github.com/artifact"}, {"sha256", pin.toStdString()}};
    const auto part = temp.path() + "/user/cache/artifacts/" + pin + ".part";
    atomicWrite(part + ".validator", "stale");
    const auto acquired = store.acquire(step, testGuard());
    QCOMPARE(hashFile(acquired), pin);
    QVERIFY(!server.requests.last().contains("If-None-Match"));
    QVERIFY(!server.requests.last().contains("If-Range"));
    QVERIFY(!QFileInfo(part + ".validator").exists());
    QVERIFY(QFile::remove(acquired));
    server.notModified = true;
    try { store.acquire(step, testGuard()); QFAIL("304 artifact accepted"); }
    catch (const Error &e) { QCOMPARE(e.code, QString("E_NETWORK")); }
    server.notModified = false;
    step["sha256"] = std::string(64, '0');
    const auto wrongPart = temp.path() + "/user/cache/artifacts/" + QString(64, '0') + ".part";
    atomicWrite(wrongPart, server.payload.left(4)); atomicWrite(wrongPart + ".validator", "test-v1");
    try { store.acquire(step, testGuard()); QFAIL("Wrong artifact pin accepted"); }
    catch (const Error &e) { QCOMPARE(e.code, QString("E_HASH_MISMATCH")); }
    QVERIFY(!QFileInfo(wrongPart).exists());
    QVERIFY(!QFileInfo(wrongPart + ".validator").exists());
    QVERIFY(server.requests.last().toLower().contains("range: bytes=4-"));
    QVERIFY(server.requests.last().toLower().contains("if-range: test-v1"));
  }
  void githubCrosscheckUnavailableUsesPin() {
    HttpsServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir temp;
    TestTrust trust;
    trust.baseUrl = server.url(); trust.hosts = {"localhost"};
    trust.authorities = QSslCertificate::fromPath(QString(AC_TEST_FIXTURES) + "/tls/localhost-cert.pem");
    QStringList warnings, notices;
    Options options; options.event = [&](const QVariantMap &e) {
      if (e.value("kind") == "warn") warnings << e.value("text").toString();
      if (e.value("kind") == "info") notices << e.value("text").toString();
    };
    ArtifactStore store(temp.path(), options, trust);
    Json step{{"do", "github-release"}, {"repo", "synthetic/example"}, {"tag", "missing"},
              {"asset", "tool.dat"}, {"sha256", sha256(server.payload).toStdString()}};
    QCOMPARE(readBytes(store.acquire(step, testGuard())), server.payload);
    QVERIFY(warnings.isEmpty()); QVERIFY(!notices.isEmpty());
    QCOMPARE(server.requests.size(), 2);
    QVERIFY(server.requests[1].contains("/releases/download/missing/tool.dat"));
  }
  void githubMalformedCrosscheckAndContradictoryDigest() {
    for (const bool malformed : {true, false}) {
      HttpsServer server; QVERIFY(server.listen(QHostAddress::LocalHost)); QTemporaryDir temp;
      server.releasePayload = malformed ? QByteArray("{\"assets\":\"invalid\"}")
        : QByteArray::fromStdString(Json{{"assets", Json::array({Json{{"name", "tool.dat"}, {"digest", "sha256:" + std::string(64, '0')}}})}}.dump());
      TestTrust trust; trust.baseUrl = server.url(); trust.hosts = {"localhost"};
      trust.authorities = QSslCertificate::fromPath(QString(AC_TEST_FIXTURES) + "/tls/localhost-cert.pem");
      QStringList warnings, notices; Options options;
      options.event = [&](const QVariantMap &e) {
        if (e.value("kind") == "warn") warnings << e.value("text").toString();
        if (e.value("kind") == "info") notices << e.value("text").toString();
      };
      ArtifactStore store(temp.path(), options, trust);
      Json step{{"do", "github-release"}, {"repo", "synthetic/example"}, {"tag", "v1"},
        {"asset", "tool.dat"}, {"sha256", sha256(server.payload).toStdString()}};
      if (malformed) {
        QCOMPARE(readBytes(store.acquire(step, testGuard())), server.payload); QVERIFY(warnings.isEmpty()); QVERIFY(!notices.isEmpty());
      } else {
        try { store.acquire(step, testGuard()); QFAIL("Contradictory API digest accepted"); }
        catch (const Error &e) { QCOMPARE(e.code, QString("E_HASH_MISMATCH")); }
        QVERIFY(warnings.isEmpty()); QCOMPARE(server.requests.size(), 1);
      }
    }
  }
  void mediaLinkUpdateAndRemovalWithoutBackup() {
    QTemporaryDir temp; auto r = fixture(temp.path());
    const auto source = temp.path() + "/synthetic.media";
    atomicWrite(source, QByteArray(1024 * 1024, 'm'));
    const auto mediaHash = hashFile(source);
    r.bindings["media"]["disc"] = Json{{"path", source.toStdString()}, {"sha256", mediaHash.toStdString()}, {"verified", true}};
    r.recipe["variant"]["flat"]["step"].push_back(Json{{"id", "media"}, {"do", "copy-media"}, {"media", "disc"}, {"to", "${install_dir}/disc.media"}, {"mode", "link"}});
    Options options; options.survivalMs = 0;
    auto installed = Engine(options).install(r); QVERIFY2(installed.success, qPrintable(installed.message));
    r.operation = "update"; r.recipe["variant"]["flat"]["version"] = "v2";
    atomicWrite(temp.path() + "/games/synthetic/setup/example.exe", "synthetic binary v2");
    auto updated = Engine(options).install(r); QVERIFY2(updated.success, qPrintable(updated.message));
    const auto link = temp.path() + "/installed/synthetic/flat/disc.media";
    QVERIFY(QFileInfo(link).isFile()); QCOMPARE(hashFile(link), mediaHash);
    QVERIFY(!QFileInfo(temp.path() + "/user/state/backups/" + mediaHash).exists());
    auto removed = Engine(options).uninstall(r); QVERIFY2(removed.success, qPrintable(removed.message));
    QVERIFY(QFileInfo(source).exists()); QCOMPARE(hashFile(source), mediaHash);
    QVERIFY(!QFileInfo(link).exists());
    QVERIFY(!QFileInfo(temp.path() + "/user/state/backups/" + mediaHash).exists());
    const auto journal = readBytes(stateBase(r) + ".journal.jsonl");
    QVERIFY(journal.contains("media-unlink"));
  }
  void explicitMediaCopyStreamsAcrossUpdate() {
    QTemporaryDir temp; auto r = fixture(temp.path());
    const auto source = temp.path() + "/synthetic.media";
    atomicWrite(source, QByteArray(2 * 1024 * 1024 + 7, 's'));
    const auto expected = hashFile(source);
    r.allowMediaCopy = true;
    r.bindings["media"]["disc"] = Json{{"path", source.toStdString()}, {"sha256", expected.toStdString()}, {"verified", true}};
    r.recipe["variant"]["flat"]["step"].push_back(Json{{"do", "copy-media"}, {"media", "disc"}, {"to", "${install_dir}/disc.media"}, {"mode", "copy"}});
    Options options; options.survivalMs = 0; QVERIFY(Engine(options).install(r).success);
    const auto installed = temp.path() + "/installed/synthetic/flat/disc.media";
    QCOMPARE(hashFile(installed), expected);
    r.operation = "update"; r.recipe["variant"]["flat"]["version"] = "v2";
    const auto updated = Engine(options).install(r); QVERIFY2(updated.success, qPrintable(updated.message));
    QCOMPARE(hashFile(installed), expected);
    QVERIFY(Engine(options).uninstall(r).success);
    QVERIFY(QFileInfo(source).exists()); QCOMPARE(hashFile(source), expected);
    QVERIFY(!QFileInfo(temp.path() + "/user/state/backups/" + expected).exists());
    QVERIFY(QDir(temp.path() + "/user/state/media-removals").entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
  }
  void configRemergeKeepsUnmanagedEditsAndOriginalPrior() {
    QTemporaryDir temp; auto r = fixture(temp.path());
    const auto cfg = temp.path() + "/installed/synthetic/flat/settings.ini";
    atomicWrite(cfg, "[Global]\nenabled=9\nuntouched=original\n");
    Options options; options.survivalMs = 0;
    QVERIFY(Engine(options).install(r).success);
    atomicWrite(cfg, "[Global]\nenabled=7\nuntouched=edited\nuserkey=yes\n");
    r.operation = "repair";
    const auto repaired = Engine(options).install(r); QVERIFY2(repaired.success, qPrintable(repaired.message));
    QVERIFY(readBytes(cfg).contains("enabled=1")); QVERIFY(readBytes(cfg).contains("untouched=edited"));
    const auto removed = Engine(options).uninstall(r);
    QCOMPARE(removed.state, QString("uninstall-incomplete"));
    QVERIFY(readBytes(cfg).contains("enabled=9")); QVERIFY(readBytes(cfg).contains("userkey=yes"));
  }
  void failedUpdateRecordsPriorStateAndError_data() {
    QTest::addColumn<QString>("priorState");
    for (const auto &state : QStringList{"installed", "installed-with-warnings", "installed-with-skipped"})
      QTest::newRow(qPrintable(state)) << state;
  }
  void failedUpdateRecordsPriorStateAndError() {
    QFETCH(QString, priorState);
    QTemporaryDir temp; auto r = fixture(temp.path()); Options options; options.survivalMs = 0;
    QVERIFY(Engine(options).install(r).success);
    auto prior = readEnvelope(stateBase(r) + ".toml"); prior["state"] = priorState.toStdString();
    writeEnvelope(stateBase(r) + ".toml", prior);
    r.operation = "update"; r.recipe["variant"]["flat"]["version"] = "v2";
    r.recipe["variant"]["flat"]["installed_when"] = "file:${install_dir}/missing.exe";
    const auto failed = Engine(options).install(r); QVERIFY(!failed.success);
    const auto state = readEnvelope(stateBase(r) + ".toml");
    QCOMPARE(string(state, "state"), priorState);
    QCOMPARE(string(state, "previous_state"), priorState);
    const auto runtime = Engine(options).runtimeState(r);
    QVERIFY(runtime.variants[0].verified); QVERIFY(runtime.variants[0].installedWhenExists);
    GameRecord game; game.id = r.gameId; game.runtime = runtime; game.hasRecipe = true;
    Variant variant; variant.id = r.variantId; variant.status = "stable"; game.variants.push_back(variant);
    resolveState(game); QCOMPARE(game.roles.value("baseState").toInt(), static_cast<int>(GameState::Installed));
    QCOMPARE(string(state, "last_error"), QString("E_VERIFY_FAILED"));
    QCOMPARE(string(state, "installed_version"), QString("v1"));
    QCOMPARE(readBytes(temp.path() + "/installed/synthetic/flat/example.exe"), QByteArray("synthetic binary v1"));
  }
  void uninstallErrorPreservesInstallAndActualCode() {
    QTemporaryDir temp; auto r = fixture(temp.path()); Options options; options.survivalMs = 0;
    const auto live = temp.path() + "/installed/synthetic/flat/example.exe";
    atomicWrite(live, "synthetic prior tool"); const auto priorHash = hashFile(live);
    QVERIFY(Engine(options).install(r).success);
    atomicWrite(temp.path() + "/user/state/backups/" + priorHash, "synthetic corrupt backup");
    const auto removed = Engine(options).uninstall(r); QVERIFY(!removed.success);
    QCOMPARE(removed.code, QString("E_ROLLBACK_INCOMPLETE"));
    const auto state = readEnvelope(stateBase(r) + ".toml");
    QCOMPARE(string(state, "state"), QString("installed"));
    QCOMPARE(string(state, "last_error"), removed.code);
    QVERIFY(Engine(options).runtimeState(r).variants[0].verified);
  }
  void missingOriginalMediaLinkRemains() {
    QTemporaryDir temp; auto r = fixture(temp.path()); Options options; options.survivalMs = 0;
    const auto source = temp.path() + "/synthetic.media"; atomicWrite(source, "synthetic medium");
    r.bindings["media"]["disc"] = Json{{"path", source.toStdString()}, {"sha256", hashFile(source).toStdString()}, {"verified", true}};
    r.recipe["variant"]["flat"]["step"].push_back(Json{{"do", "copy-media"}, {"media", "disc"}, {"to", "${install_dir}/disc.media"}, {"mode", "link"}});
    QVERIFY(Engine(options).install(r).success); QVERIFY(QFile::remove(source));
    const auto removed = Engine(options).uninstall(r);
    QCOMPARE(removed.state, QString("uninstall-incomplete")); QVERIFY(removed.code.isEmpty());
    QCOMPARE(removed.manifest["file"].size(), size_t(1));
    QCOMPARE(string(removed.manifest["file"][0], "origin"), QString("user-media-link"));
    QVERIFY(QFileInfo(temp.path() + "/installed/synthetic/flat/disc.media").isFile());
  }
  void unrelatedMalformedCatalogDoesNotBlockPlan() {
    QTemporaryDir temp; auto r = fixture(temp.path());
    atomicWrite(temp.path() + "/games/unrelated/game.toml", "broken = [");
    QVERIFY(!Engine().plan(r).steps.empty());
    atomicWrite(temp.path() + "/games/synthetic/game.toml", "broken = [");
    try { Engine().plan(r); QFAIL("Malformed requested game allowed planning"); }
    catch (const Error &e) { QCOMPARE(e.code, QString("E_CONTENT_GUARD")); }
    atomicWrite(temp.path() + "/games/synthetic/game.toml", "id=\"synthetic\"\n");
    atomicWrite(temp.path() + "/games/alias/game.toml", "id=\"synthetic\"\n[[media]]\nset=7\n");
    try { Engine().plan(r); QFAIL("Malformed requested game ID under alias allowed planning"); }
    catch (const Error &e) { QCOMPARE(e.code, QString("E_CONTENT_GUARD")); }
    QVERIFY(QFile::remove(temp.path() + "/games/alias/game.toml"));
    r.recipe["variant"]["flat"]["step"][0]["from"] = "${install_root}/games/synthetic/setup/disc.ISO";
    try { Engine().plan(r); QFAIL("Content guard was weakened"); }
    catch (const Error &e) { QCOMPARE(e.code, QString("E_CONTENT_GUARD")); }
  }
  void mediaCopyRemovalRecovery() {
    for (const auto &point : QStringList{"write", "commit"}) {
      QTemporaryDir temp; auto r = fixture(temp.path()); Options options; options.survivalMs = 0;
      const auto source = temp.path() + "/synthetic.media"; atomicWrite(source, "synthetic medium");
      const auto hash = hashFile(source); r.allowMediaCopy = true;
      r.bindings["media"]["disc"] = Json{{"path", source.toStdString()}, {"sha256", hash.toStdString()}, {"verified", true}};
      r.recipe["variant"]["flat"]["step"].push_back(Json{{"do", "copy-media"}, {"media", "disc"}, {"to", "${install_dir}/disc.media"}, {"mode", "copy"}});
      QVERIFY(Engine(options).install(r).success);
      const auto live = temp.path() + "/installed/synthetic/flat/disc.media";
      auto faulted = options; bool reached = false;
      faulted.fault = [&](const QString &at) { if (at == point && !QFileInfo(live).exists()) { reached = true; throw std::runtime_error("synthetic media removal crash"); } };
      bool crashed = false; try { Engine(faulted).uninstall(r); } catch (...) { crashed = true; }
      QVERIFY(crashed); QVERIFY(reached); QVERIFY(Engine(options).recover(r).success);
      if (point == "write") { QVERIFY(QFileInfo(live).exists()); QCOMPARE(hashFile(live), hash); }
      else QVERIFY(!QFileInfo(live).exists());
      QVERIFY(!QFileInfo(temp.path() + "/user/state/backups/" + hash).exists());
      QVERIFY(QDir(temp.path() + "/user/state/media-removals").entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }
  }
  void unlinkRecoveryRestoresOriginalIdentityWithoutBackup() {
    QTemporaryDir temp; auto r = fixture(temp.path());
    const auto source = temp.path() + "/synthetic.media"; atomicWrite(source, "synthetic medium");
    r.bindings["media"]["disc"] = Json{{"path", source.toStdString()}, {"sha256", hashFile(source).toStdString()}, {"verified", true}};
    r.recipe["variant"]["flat"]["step"].push_back(Json{{"id", "media"}, {"do", "copy-media"}, {"media", "disc"}, {"to", "${install_dir}/disc.media"}, {"mode", "link"}});
    Options normal; normal.survivalMs = 0; QVERIFY(Engine(normal).install(r).success);
    bool unlinked = false; Options faulted; faulted.survivalMs = 0;
    faulted.fault = [&](const QString &point) {
      if (point == "write" && !QFileInfo(temp.path() + "/installed/synthetic/flat/disc.media").exists()) {
        unlinked = true; throw std::runtime_error("synthetic abrupt unlink");
      }
    };
    bool crashed = false; try { Engine(faulted).uninstall(r); } catch (...) { crashed = true; }
    QVERIFY(crashed);
    QVERIFY(unlinked);
    QVERIFY(Engine(normal).recover(r).success);
    QCOMPARE(hashFile(temp.path() + "/installed/synthetic/flat/disc.media"), hashFile(source));
    QVERIFY(!QFileInfo(temp.path() + "/user/state/backups/" + hashFile(source)).exists());
  }
  void mediaVerificationModesAndGeneratedIds() {
    QTemporaryDir temp; auto r = fixture(temp.path());
    const auto media = temp.path() + "/synthetic.media"; atomicWrite(media, "synthetic fixture");
    r.bindings["media"]["disc"] = Json{{"path", media.toStdString()}, {"verified", false}};
    auto &steps = r.recipe["variant"]["flat"]["step"];
    steps[0].erase("id"); steps[1]["id"] = "auto-step-1";
    steps.push_back(Json{{"do", "require-media"}, {"media", "disc"}, {"verify", "none"}});
    const auto plan = Engine().plan(r);
    QCOMPARE(string(plan.steps[0], "id"), QString("auto-step-2"));
    Options options; options.survivalMs = 0;
    auto result = Engine(options).install(r); QVERIFY2(result.success, qPrintable(result.message));
    steps.back()["verify"] = "name"; steps.back()["set"] = "synthetic";
    QVERIFY(Engine(options).install(r).success);
    steps.back()["set"] = "different";
    QCOMPARE(Engine(options).install(r).code, QString("E_MEDIA_MISMATCH"));
    steps.back()["verify"] = "unsupported";
    QVERIFY_THROWS_EXCEPTION(Error, Engine(options).plan(r));
  }
  void hotdSelfContainedDryRun() {
    QTemporaryDir temp; Request r;
    r.root = temp.path(); r.catalogRoot = catalogFixtureRoot();
    r.gameId = "dc-house-of-the-dead-2"; r.variantId = "hotd2-vr-pcvr";
    r.recipe = CatalogLoader::parseToml(r.catalogRoot + "/games/dc-house-of-the-dead-2/install.toml");
    r.bindings["media"]["disc"] = Json{{"path", (temp.path() + "/synthetic-disc.chd").toStdString()}};
    const auto before = tree(temp.path()); const auto plan = Engine().plan(r);
    QCOMPARE(plan.steps.size(), size_t(4));
    QCOMPARE(string(plan.steps[0], "tag"), QString("v0.3-test1"));
    QCOMPARE(string(plan.steps[2], "verify"), QString("none"));
    QCOMPARE(tree(temp.path()), before);
  }
  void hubtoolSettingsAreAvailableToRecipe() {
    QTemporaryDir temp; auto r = fixture(temp.path());
    auto &set = r.recipe["variant"]["flat"]["step"][1]["set"][0];
    set.erase("value"); set["from"] = "settings.enabled";
    const auto recipe = temp.path() + "/recipe.toml";
    atomicWrite(recipe, "format=1\n[variant.flat]\nstatus=\"stable\"\nversion=\"v1\"\ninstalled_when=\"file:${install_dir}/settings.ini\"\n[[variant.flat.step]]\ndo=\"write-config\"\nfile=\"${install_dir}/settings.ini\"\nformat=\"ini\"\ncreate=true\n[[variant.flat.step.set]]\nsection=\"Global\"\nkey=\"enabled\"\nfrom=\"settings.enabled\"\ntype=\"int\"\n");
    const auto settings = temp.path() + "/settings.json"; atomicWrite(settings, "{\"enabled\":42}");
    QProcess accepted;
    accepted.start(QCoreApplication::applicationDirPath() + "/hubtool", {"--data-root", catalogFixtureRoot(), "--install-root", temp.path(), "--recipe", recipe, "--settings", settings,
      "install", "synthetic", "flat"});
    QVERIFY(accepted.waitForFinished(30000));
    const auto cliOutput = accepted.readAllStandardOutput() + accepted.readAllStandardError();
    QVERIFY2(accepted.exitCode() == 0, qPrintable(QString::number(accepted.exitCode()) + ": " + QString::fromUtf8(cliOutput)));
    QVERIFY(readBytes(temp.path() + "/installed/synthetic/flat/settings.ini").contains("42"));
    atomicWrite(settings, "[]");
    QProcess process;
    process.start(QCoreApplication::applicationDirPath() + "/hubtool", {"--data-root", catalogFixtureRoot(), "--install-root", temp.path(), "--settings", settings,
      "plan", "dc-house-of-the-dead-2", "hotd2-vr-pcvr"});
    QVERIFY(process.waitForFinished(30000)); QCOMPARE(process.exitCode(), 64);
    QVERIFY(process.readAllStandardError().contains("Settings must be a JSON object"));
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
        entries = Archive::inspect(path, {1024, 10}, testGuard());
      } catch (const Error &e) {
        QFAIL(qPrintable(e.code + ": " + QString::fromUtf8(e.what())));
      }
      QCOMPARE(entries.size(), 1);
      QStringList files;
      try {
        files = Archive::extract(path, temp.path() + "/stage-" + format,
                                 {1024, 10}, testGuard(), 1);
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
    for (const auto &name : QStringList{"../escape", QDir::rootPath()+"evil", "con.txt",
                                        "x:stream", "game.iso"}) {
      const auto path = temp.path() + "/bad.zip";
      archiveFile(path, {name});
      QVERIFY_THROWS_EXCEPTION(Error, Archive::inspect(path, {1024, 10}, testGuard()));
    }
    archiveFile(temp.path() + "/collision.zip", {"a.exe", "A.exe"});
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::inspect(temp.path() + "/collision.zip", {1024, 10}, testGuard()));
    archiveFile(temp.path() + "/symlink.tar", {"link"}, "tar", true);
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::inspect(temp.path() + "/symlink.tar", {1024, 10}, testGuard()));
    archiveFile(temp.path() + "/cap.zip", {"a.exe", "b.exe"});
    auto guard = testGuard();
    guard["sha256"] = Json::array({sha256("test").toStdString()});
    QVERIFY_THROWS_EXCEPTION(Error, Archive::inspect(
        temp.path() + "/cap.zip", {1024, 10}, guard));
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::inspect(temp.path() + "/cap.zip", {4, 10}, testGuard()));
    QVERIFY_THROWS_EXCEPTION(
        Error, Archive::inspect(temp.path() + "/cap.zip", {1024, 1}, testGuard()));
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
        Json{{"media", Json::array({Json{{"kind", "disc"}, {"id", "disc"}},
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
