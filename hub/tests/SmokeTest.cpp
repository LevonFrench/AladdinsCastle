// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "core/app/LaunchFailure.h"
#include "core/app/NetworkPolicy.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTcpServer>
#include "core/catalog/CatalogPaths.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtTest>
#include <cstring>
class SmokeTest : public QObject {
    Q_OBJECT
private slots:
    void countOnlyDirectGameMetadata() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QDir dir(temp.path());
        QVERIFY(dir.mkpath("games/example-game"));
        QVERIFY(dir.mkpath("games/no-metadata"));
        QVERIFY(dir.mkpath("games/no-metadata/nested"));
        for (const auto &name : {"games/example-game/game.toml", "games/no-metadata/nested/game.toml"}) {
            QFile file(dir.filePath(name)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("[game]\nid='synthetic'\n");
        }
        QCOMPARE(ac::countGames(temp.path()), qsizetype(1));
        QCOMPARE(ac::countGames({}), qsizetype(0));
    }
    void catalogScale() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); QDir dir(temp.path());
        for (int i = 0; i < 413; ++i) {
            const QString folder = QString("games/synthetic-%1").arg(i);
            QVERIFY(dir.mkpath(folder));
            QFile file(dir.filePath(folder + "/game.toml"));
            QVERIFY(file.open(QIODevice::WriteOnly)); file.write("id='synthetic'\n");
        }
        QCOMPARE(ac::countGames(temp.path()), qsizetype(413));
    }
    void discoveryAndPortableState() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QDir dir(temp.path()); QVERIFY(dir.mkpath("games")); QVERIFY(dir.mkpath("build/nested"));
        QCOMPARE(ac::findCatalogRoot(dir.filePath("build/nested"), {}), dir.absolutePath());
        // An invalid explicit root must not silently use some other catalog.
        QVERIFY(ac::findCatalogRoot(temp.path(), temp.path(), dir.filePath("missing")).isEmpty());
        QCOMPARE(ac::portableUserRoot(temp.path()), dir.absoluteFilePath("user"));
        QVERIFY(!dir.exists("user")); // discovery does not mutate the filesystem
    }
    void unicodeCatalogCommandLine() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); QDir dir(temp.path());
        const QString root = dir.filePath(QString::fromUtf8("catalog-漢字"));
        QVERIFY(dir.mkpath(root + "/games/synthetic"));
        QFile file(root + "/games/synthetic/game.toml");
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write("id='synthetic'"); file.close();
        QProcess process;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM", "offscreen");
        env.insert("QT_QUICK_BACKEND", "software");
        process.setProcessEnvironment(env);
        QString executable = QDir(QCoreApplication::applicationDirPath()).filePath("aladdinscastle-hub");
#ifdef Q_OS_WIN
        executable += ".exe";
#endif
        process.start(executable, {"--data-root", root, "--quit-after-ms", "100"});
        QVERIFY(process.waitForStarted()); QVERIFY(process.waitForFinished(15000));
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        QVERIFY(process.readAllStandardOutput().contains("AladdinsCastle: 1 games"));
    }
    void hubtoolRejectsIncompleteArguments() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QDir().mkpath(temp.path() + "/games/synthetic");
        QFile metadata(temp.path() + "/games/synthetic/game.toml");
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write("id='synthetic'\ntitle='Synthetic'\n");
        metadata.close();
        auto executable = QCoreApplication::applicationDirPath() + "/hubtool";
#ifdef Q_OS_WIN
        executable += ".exe";
#endif
        for (const auto &command : {QStringList{"explain"}, QStringList{"count", "extra"},
                                   QStringList{"explain", "synthetic", "extra"}}) {
            QProcess process;
            process.start(executable, QStringList{"--data-root", temp.path()} + command);
            QVERIFY(process.waitForStarted());
            QVERIFY(process.waitForFinished(15000));
            QCOMPARE(process.exitStatus(), QProcess::NormalExit);
            QCOMPARE(process.exitCode(), 64);
            QVERIFY(process.readAllStandardError().contains("Usage:"));
        }
    }
    void launchFailureLogAndWindowSubsystem(){
        QTemporaryDir temp;QVERIFY(temp.isValid());
        const auto log=ac::writeLaunchFailureLog(temp.path(),"../escape","synthetic preflight failure");
        QVERIFY(log.startsWith(temp.path()+"/user/logs/launch-unknown-game-"));
        QFile file(log);QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(file.readAll().contains("synthetic preflight failure"));
        QVERIFY(log!=ac::writeLaunchFailureLog(temp.path(),"../escape","second failure"));
#ifdef Q_OS_WIN
        QFile binary(QCoreApplication::applicationDirPath()+"/aladdinscastle-hub.exe");QVERIFY(binary.open(QIODevice::ReadOnly));const auto bytes=binary.readAll();
        QVERIFY(bytes.size()>256);quint32 pe=0;memcpy(&pe,bytes.constData()+0x3c,4);QVERIFY(pe+94<quint32(bytes.size()));quint16 subsystem=0;memcpy(&subsystem,bytes.constData()+pe+24+68,2);QCOMPARE(subsystem,quint16(2));
#endif
    }
    void preflightFailureWritesPortableLog(){
        QTemporaryDir temp;QVERIFY(temp.isValid());auto executable=QCoreApplication::applicationDirPath()+"/aladdinscastle-hub";
#ifdef Q_OS_WIN
        executable+=".exe";
#endif
        QProcess process;auto env=QProcessEnvironment::systemEnvironment();env.insert("QT_QPA_PLATFORM","offscreen");env.insert("QT_QUICK_BACKEND","software");env.insert("QT_OPENGL","software");process.setProcessEnvironment(env);
        const auto root=temp.filePath(QString::fromUtf8("portable space 漢字"));QDir().mkpath(root);
        process.start(executable,{"--launch","synthetic","--variant","flat-mame","--install-root",root,"--data-root",temp.filePath("missing-catalog")});
        QVERIFY(process.waitForStarted());QVERIFY(process.waitForFinished(15000));QCOMPARE(process.exitCode(),2);
        const auto logs=QDir(root+"/user/logs").entryList({"launch-synthetic-*.log"},QDir::Files);QCOMPARE(logs.size(),1);QFile log(root+"/user/logs/"+logs.first());QVERIFY(log.open(QIODevice::ReadOnly));QVERIFY(log.readAll().contains("Catalog missing"));
    }
    void qmlRemoteResourcesDenied() {
        ac::LocalQmlNetworkFactory factory; std::unique_ptr<QNetworkAccessManager> manager(factory.create(nullptr));
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        for(const auto &url : {QUrl(QString("http://127.0.0.1:%1/private.png").arg(server.serverPort())), QUrl("https://synthetic.invalid/private.png")}) {
            auto *reply=manager->get(QNetworkRequest(url)); QSignalSpy done(reply,&QNetworkReply::finished);
            QTRY_COMPARE(done.count(),1); QCOMPARE(reply->error(),QNetworkReply::ContentAccessDenied); QVERIFY(!server.hasPendingConnections()); delete reply;
        }
        QTemporaryDir temp; QFile file(temp.filePath("local.txt")); QVERIFY(file.open(QIODevice::WriteOnly));file.write("local resource");file.close();
        auto *reply=manager->get(QNetworkRequest(QUrl::fromLocalFile(file.fileName())));QSignalSpy done(reply,&QNetworkReply::finished);QTRY_COMPARE(done.count(),1);QCOMPARE(reply->readAll(),QByteArray("local resource"));delete reply;
    }
    void overlayFailureDesktopPolicy() {
        const auto overlay=ac::parseLaunchOptions({"--overlay"});QVERIFY(ac::shouldOpenDesktop(overlay,false));QVERIFY(!ac::shouldOpenDesktop(overlay,true));
        QVERIFY(ac::shouldOpenDesktop(ac::parseLaunchOptions({"--overlay","--window"}),true));QVERIFY(ac::shouldOpenDesktop({},false));
    }
    void cliWorksWithoutOpenVrRuntime() {
#ifdef Q_OS_WIN
        QTemporaryDir temp; const auto binary=QCoreApplication::applicationDirPath()+"/aladdinscastle-hub.exe";const auto copy=temp.filePath("aladdinscastle-hub.exe");QVERIFY(QFile::copy(binary,copy));
        QProcess process;auto env=QProcessEnvironment::systemEnvironment();QStringList qtDirs;
        for(const auto &dir:env.value("PATH").split(';'))if(QFileInfo(QDir(dir).filePath("Qt6Core.dll")).isFile())qtDirs.append(dir);
        QVERIFY(!qtDirs.isEmpty());qtDirs.append(env.value("SystemRoot")+"/System32");env.insert("PATH",qtDirs.join(';'));process.setProcessEnvironment(env);
        process.start(copy,{"--help"});QVERIFY(process.waitForStarted());QVERIFY(process.waitForFinished(15000));QCOMPARE(process.exitCode(),0);QVERIFY(process.readAllStandardOutput().contains("Usage:"));QVERIFY(!QFileInfo::exists(temp.filePath("openvr_api.dll")));
#endif
    }
    void modesAndRejections() {
        QCOMPARE(ac::parseLaunchOptions({}).mode, ac::Mode::Desktop);
        auto overlay = ac::parseLaunchOptions({"--overlay", "--window"});
        QCOMPARE(overlay.mode, ac::Mode::Overlay); QVERIFY(overlay.window); QVERIFY(overlay.error.isEmpty());QVERIFY(!ac::wantsParentConsole(overlay));QVERIFY(ac::wantsParentConsole(ac::parseLaunchOptions({"--help"})));
        auto launch = ac::parseLaunchOptions({"--launch", "synthetic-game"});
        QCOMPARE(launch.mode, ac::Mode::Launch); QCOMPARE(launch.gameId, QString("synthetic-game"));QVERIFY(ac::wantsParentConsole(launch));
        for (const auto &args : {QStringList{"--launch"}, QStringList{"--launch", "../outside"},
             QStringList{"--overlay", "--launch", "example"}, QStringList{"--window"},
             QStringList{"--quit-after-ms", "0"}, QStringList{"--launch","synthetic","--variant","../escape"}, QStringList{"--unknown"}})
            QVERIFY2(!ac::parseLaunchOptions(args).error.isEmpty(), qPrintable(args.join(' ')));
    }
};
QTEST_GUILESS_MAIN(SmokeTest)
#include "SmokeTest.moc"
