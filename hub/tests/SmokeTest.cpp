// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "core/catalog/CatalogPaths.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtTest>
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
    void modesAndRejections() {
        QCOMPARE(ac::parseLaunchOptions({}).mode, ac::Mode::Desktop);
        auto overlay = ac::parseLaunchOptions({"--overlay", "--window"});
        QCOMPARE(overlay.mode, ac::Mode::Overlay); QVERIFY(overlay.window); QVERIFY(overlay.error.isEmpty());
        auto launch = ac::parseLaunchOptions({"--launch", "synthetic-game"});
        QCOMPARE(launch.mode, ac::Mode::Launch); QCOMPARE(launch.gameId, QString("synthetic-game"));
        for (const auto &args : {QStringList{"--launch"}, QStringList{"--launch", "../outside"},
             QStringList{"--overlay", "--launch", "example"}, QStringList{"--window"},
             QStringList{"--quit-after-ms", "0"}, QStringList{"--unknown"}})
            QVERIFY2(!ac::parseLaunchOptions(args).error.isEmpty(), qPrintable(args.join(' ')));
    }
};
QTEST_GUILESS_MAIN(SmokeTest)
#include "SmokeTest.moc"
