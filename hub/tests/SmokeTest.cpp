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
#include "ProductIdentity.h"
#include <QProcessEnvironment>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QtEndian>
#include <QtTest>
#include <cstring>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QTimer>
class SmokeTest : public QObject {
    Q_OBJECT
private slots:
    void cachedPinballDiagnosticUsesNormalPlay_data() {
        QTest::addColumn<QString>("shape");
        QTest::newRow("current-table") << QString("current");
        QTest::newRow("changed-table-refused") << QString("changed");
        QTest::newRow("invalid-cache-refused") << QString("invalid");
        QTest::newRow("private-ini") << QString("private-ini");
        QTest::newRow("outside-ini-refused") << QString("outside-ini");
        QTest::newRow("ini-without-row-refused") << QString("ini-without-row");
    }
    void cachedPinballDiagnosticUsesNormalPlay() {
        QFETCH(QString, shape);
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const auto root=temp.path(), table=temp.filePath("synthetic table.vpx"), cache=temp.filePath("row.json"), capture=temp.filePath("surface.png");
        QVERIFY(QDir().mkpath(root+"/games/synthetic"));
        const auto write=[](const QString &path,const QByteArray &bytes){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();};
        QVERIFY(write(root+"/games/synthetic/game.toml","id='synthetic'\ntitle='Synthetic'\n"));
        // Same empty-script CFB shape as the retained-table integration fixture.
        // It can be inspected by normal Play but contains no playable game.
        QByteArray tableBytes(5632,'\0');tableBytes.replace(0,8,QByteArray::fromHex("d0cf11e0a1b11ae1"));
        const auto u16=[&](int at,quint16 n){qToLittleEndian(n,tableBytes.data()+at);};
        const auto u32=[&](int at,quint32 n){qToLittleEndian(n,tableBytes.data()+at);};
        u16(26,3);u16(28,0xfffe);u16(30,9);u16(32,6);u32(44,1);u32(48,0);u32(56,4096);u32(60,0xfffffffe);u32(68,0xfffffffe);
        for(int i=0;i<109;++i)u32(76+4*i,i==0?9:0xffffffff);
        tableBytes[512+66]=5;u32(512+76,1);u32(512+116,0xfffffffe);
        for(int e=1;e<3;++e){const auto at=512+e*128;const QString name=e==1?"GameStg":"GameData";
            for(int i=0;i<name.size();++i)u16(at+2*i,name[i].unicode());
            u16(at+64,static_cast<quint16>((name.size()+1)*2));tableBytes[at+66]=e==1?1:2;
            u32(at+68,0xffffffff);u32(at+72,0xffffffff);u32(at+76,e==1?2:0xffffffff);u32(at+116,e==1?0xfffffffe:1);u32(at+120,e==1?0:4096);
        }
        u32(1024,4);tableBytes.replace(1028,4,"ENDB");
        for(int i=0;i<128;++i)u32(5120+4*i,i==0||i==8?0xfffffffe:i==9?0xfffffffd:i<8?static_cast<quint32>(i+1):0xffffffff);
        QVERIFY(write(table,tableBytes));
        const QFileInfo tableInfo(table);
        const auto id=QString::fromLatin1(QCryptographicHash::hash(tableInfo.absoluteFilePath().toUtf8(),QCryptographicHash::Sha256).toHex());
        const QJsonObject row{{"id",id},{"path",table},{"title","Synthetic diagnostic table"},{"vrReady",true},{"romState","unknown"},{"size",tableInfo.size()},{"mtime",tableInfo.lastModified().toMSecsSinceEpoch()+(shape=="changed"?1:0)}};
        QVERIFY(write(cache,shape=="invalid"?QByteArray("["):QJsonDocument(row).toJson()));
        QVERIFY(QDir().mkpath(root+"/.local"));
        const auto ini=temp.filePath(shape=="outside-ini"?"outside.ini":".local/mode settings.ini"),arguments=temp.filePath("child-arguments.json");
        QVERIFY(write(ini,"[PlayerVR]\nAskToTurnOn=2\n"));
        auto hub=QCoreApplication::applicationDirPath()+"/aladdinscastle-hub", child=QCoreApplication::applicationDirPath()+"/integration-tests";
#ifdef Q_OS_WIN
        hub+=".exe"; child+=".exe";
        const QString quitAfter="7000";
#else
        // This platform asks for VPX to exit itself before Hub can close.
        const QString quitAfter="12000";
#endif
        QVERIFY(QFileInfo::exists(child)); QVERIFY(QDir().mkpath(root+"/user"));
        QVERIFY(write(root+"/user/hub-settings.toml",("[ui]\npinballExecutable='"+QDir::fromNativeSeparators(child)+"'\n").toUtf8()));
        auto env=QProcessEnvironment::systemEnvironment();
        env.remove("AC_UI_PLAY");env.remove("AC_UI_PINBALL_INI");env.insert("AC_UI_PINBALL_ROW",cache);env.insert("AC_UI_CAPTURE",capture);env.insert("AC_SYNTHETIC_PINBALL_CHILD_MS","9000");env.insert("AC_SYNTHETIC_PINBALL_ARGUMENTS",arguments);
        if(shape.contains("ini"))env.insert("AC_UI_PINBALL_INI",ini);
        if(shape=="ini-without-row")env.remove("AC_UI_PINBALL_ROW");
        env.insert("QT_QPA_PLATFORM","offscreen");env.insert("QT_QUICK_BACKEND","software");env.insert("QT_OPENGL","software");
        QProcess process;process.setProcessEnvironment(env);process.start(hub,{"--data-root",root,"--install-root",root,"--quit-after-ms",quitAfter});
        QVERIFY(process.waitForStarted());QVERIFY(process.waitForFinished(16000));QCOMPARE(process.exitStatus(),QProcess::NormalExit);QCOMPARE(process.exitCode(),0);
        QFile receipt(capture+".json");QVERIFY(receipt.open(QIODevice::ReadOnly));const auto captured=QJsonDocument::fromJson(receipt.readAll()).object();
        QVERIFY(!QImage(capture).isNull());QVERIFY(captured["imageSaved"].toBool());
        if(shape=="current"||shape=="private-ini"){
            const auto playing=captured["nowPlaying"].toObject();QVERIFY(playing["running"].toBool());QCOMPARE(playing["title"].toString(),QString("Synthetic diagnostic table"));QCOMPARE(playing["pinballId"].toString(),id);QVERIFY(captured["launcherBusy"].toBool());
            QFile exitFile(capture+".exit.json");QVERIFY(exitFile.open(QIODevice::ReadOnly));const auto exited=QJsonDocument::fromJson(exitFile.readAll()).object();QVERIFY(!exited["launcherBusy"].toBool());QVERIFY(!exited["nowPlaying"].toObject()["running"].toBool());
            const auto finished=exited["finished"].toArray();QCOMPARE(finished.size(),1);QCOMPARE(finished[0].toObject()["gameId"].toString(),"pinball-"+id);QCOMPARE(finished[0].toObject()["exitCode"].toInt(),0);QVERIFY(finished[0].toObject()["error"].toString().isEmpty());
            QFile actualArguments(arguments);QVERIFY(actualArguments.open(QIODevice::ReadOnly));
            auto expected=QStringList{"-Play",table};if(shape=="private-ini")expected<<"-Ini"<<ini;
            // Without a diagnostic file a diagnostic launches on the desktop, with VR off in the Hub's own table settings file.
            else{const auto owned=root+"/user/pinball/"+id+".ini";expected<<"-TableIni"<<QDir::toNativeSeparators(owned);QFile settings(owned);QVERIFY(settings.open(QIODevice::ReadOnly));QVERIFY(settings.readAll().contains("AskToTurnOn = 2"));}
            QCOMPARE(QJsonDocument::fromJson(actualArguments.readAll()).toVariant().toStringList(),expected);
        }else{
            QVERIFY(!captured["launcherBusy"].toBool());QVERIFY(!captured["nowPlaying"].toObject()["running"].toBool());
            const auto expected=shape=="changed"?"Table changed":shape=="outside-ini"?"Pinball diagnostic settings refused":shape=="ini-without-row"?"requires AC_UI_PINBALL_ROW":"Invalid Pinball diagnostic row";
            QVERIFY(captured["status"].toString().contains(expected));QVERIFY(!QFileInfo::exists(arguments));
        }
        QFile unchanged(table);QVERIFY(unchanged.open(QIODevice::ReadOnly));QCOMPARE(unchanged.readAll(),tableBytes);
        QFile settingsUnchanged(ini);QVERIFY(settingsUnchanged.open(QIODevice::ReadOnly));QCOMPARE(settingsUnchanged.readAll(),QByteArray("[PlayerVR]\nAskToTurnOn=2\n"));
    }
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
    // One Hub per folder: a second start on the same folder asks the first to come to the front and quits.
    void secondHubHandsOverToTheFirst() {
        QTemporaryDir temp;QVERIFY(temp.isValid());const auto root=temp.path();
        QVERIFY(QDir().mkpath(root+"/games/synthetic"));
        {QFile file(root+"/games/synthetic/game.toml");QVERIFY(file.open(QIODevice::WriteOnly));file.write("id='synthetic'");}
        auto env=QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM","offscreen");env.insert("QT_QUICK_BACKEND","software");env.insert("AC_SINGLE_INSTANCE_TEST","1");
        QString executable=QDir(QCoreApplication::applicationDirPath()).filePath("aladdinscastle-hub");
#ifdef Q_OS_WIN
        executable+=".exe";
#endif
        QProcess first;first.setProcessEnvironment(env);first.start(executable,{"--data-root",root,"--install-root",root,"--quit-after-ms","9000"});
        QVERIFY(first.waitForStarted());
        QByteArray firstOut;QTRY_VERIFY_WITH_TIMEOUT((firstOut+=first.readAllStandardOutput()).contains("1 games"),8000);
        QProcess second;second.setProcessEnvironment(env);second.start(executable,{"--data-root",root,"--install-root",root,"--quit-after-ms","3000"});
        QVERIFY(second.waitForStarted());QVERIFY(second.waitForFinished(10000));
        QCOMPARE(second.exitCode(),0);QVERIFY(second.readAllStandardOutput().contains("Another Hub is open"));
        QTRY_VERIFY_WITH_TIMEOUT((firstOut+=first.readAllStandardOutput()).contains("Asked to come to the front"),5000);
        QVERIFY(first.waitForFinished(15000));QCOMPARE(first.exitCode(),0);
        // Another folder is another Hub.
        QTemporaryDir other;QVERIFY(QDir().mkpath(other.path()+"/games/synthetic"));
        {QFile file(other.path()+"/games/synthetic/game.toml");QVERIFY(file.open(QIODevice::WriteOnly));file.write("id='synthetic'");}
        QProcess third;third.setProcessEnvironment(env);third.start(executable,{"--data-root",other.path(),"--install-root",other.path(),"--quit-after-ms","100"});
        QVERIFY(third.waitForFinished(15000));QVERIFY(third.readAllStandardOutput().contains("1 games"));
    }
    // The gamepad check runs in a copy of its own and prints what it found: here, a library that is no SDL.
    void gamepadProbeRunsApart() {
        QTemporaryDir temp;QVERIFY(temp.isValid());
        {QFile file(temp.filePath("SDL3.dll"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("not a library");}
        QString executable=QDir(QCoreApplication::applicationDirPath()).filePath("aladdinscastle-hub");
#ifdef Q_OS_WIN
        executable+=".exe";
#endif
        for(const auto &library:{temp.filePath("SDL3.dll"),temp.filePath("missing.dll")}){
            QProcess probe;probe.start(executable,{"--probe-gamepads",library});
            QVERIFY(probe.waitForFinished(15000));QCOMPARE(probe.exitCode(),0);
            QCOMPARE(probe.readAllStandardOutput().trimmed(),QByteArray("[]"));
        }
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
    void preflightLogsRotatePerGame(){
        QTemporaryDir temp;QVERIFY(temp.isValid());
        const auto other=ac::writeLaunchFailureLog(temp.path(),"synthetic-long","unrelated synthetic game");
        QString current;for(int i=0;i<27;++i)current=ac::writeLaunchFailureLog(temp.path(),"synthetic","synthetic failure");
        QVERIFY(QFileInfo::exists(current));QVERIFY(QFileInfo::exists(other));
        const auto logs=QDir(temp.filePath("user/logs")).entryList(QDir::Files);QCOMPARE(logs.size(),21);
    }
    void launchErrorPresentationDeadline(){
        QCOMPARE(ac::launchFailureDialogTimeoutMs,15000);
        QEventLoop modal;int displayed=0,dismissed=0;QElapsedTimer elapsed;elapsed.start();
        QTimer::singleShot(3000,&modal,&QEventLoop::quit);
        ac::runTimedLaunchFailurePresentation([&]{++displayed;modal.exec();},[&]{++dismissed;modal.quit();},25);
        QCOMPARE(displayed,1);QCOMPARE(dismissed,1);QVERIFY(elapsed.elapsed()<1500);
        ac::runTimedLaunchFailurePresentation([&]{++displayed;},[&]{++dismissed;},25);QTest::qWait(50);QCOMPARE(dismissed,1);
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
        const auto runtime=ac::parseLaunchOptions({"--overlay","--steamvr-started"});QVERIFY(runtime.error.isEmpty());QVERIFY(!ac::shouldOpenDesktop(runtime,false));QVERIFY(!ac::shouldOpenDesktop(runtime,true));
        QVERIFY(!ac::parseLaunchOptions({"--steamvr-started"}).error.isEmpty());QVERIFY(!ac::shouldQuitAfterLastWindow(true));QVERIFY(ac::shouldQuitAfterLastWindow(false));
        QVERIFY(ac::shouldQuitAfterLaunch(false,false,false));QVERIFY(!ac::shouldQuitAfterLaunch(true,false,false));QVERIFY(!ac::shouldQuitAfterLaunch(false,true,false));QVERIFY(!ac::shouldQuitAfterLaunch(false,false,true));
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
    void missingDllOverlayActionsFailWithoutRuntime(){
#ifdef Q_OS_WIN
        QTemporaryDir temp;QVERIFY(temp.isValid());const auto appDir=QCoreApplication::applicationDirPath();
        const auto copy=temp.filePath("aladdinscastle-hub.exe");QVERIFY(QFile::copy(appDir+"/aladdinscastle-hub.exe",copy));
        const auto manifest=QString("resources/")+ac::product::short_id+".vrmanifest";
        QVERIFY(QDir().mkpath(temp.filePath("resources")));QVERIFY(QFile::copy(appDir+"/"+manifest,temp.filePath(manifest)));
        QVERIFY(QDir().mkpath(temp.filePath("games/synthetic")));QFile metadata(temp.filePath("games/synthetic/game.toml"));QVERIFY(metadata.open(QIODevice::WriteOnly));metadata.write("id='synthetic'\ntitle='Synthetic'\n");metadata.close();
        auto env=QProcessEnvironment::systemEnvironment();QStringList qtDirs;for(const auto &dir:env.value("PATH").split(';'))if(QFileInfo(QDir(dir).filePath("Qt6Core.dll")).isFile())qtDirs<<dir;
        QVERIFY(!qtDirs.isEmpty());qtDirs<<env.value("SystemRoot")+"/System32";env.insert("PATH",qtDirs.join(';'));env.insert("QT_QPA_PLATFORM","offscreen");env.insert("QT_QUICK_BACKEND","software");env.insert("QT_OPENGL","software");
        for(const auto &args:{QStringList{"--overlay","--quit-after-ms","50"},QStringList{"--overlay","--steamvr-started"},QStringList{"--register-overlay"},QStringList{"--unregister-overlay"}}){
            QProcess process;process.setProcessEnvironment(env);process.setWorkingDirectory(temp.path());process.start(copy,args);QVERIFY(process.waitForStarted());QVERIFY(process.waitForFinished(15000));
            QCOMPARE(process.exitStatus(),QProcess::NormalExit);const auto error=process.readAllStandardError();QVERIFY2(error.contains("OpenVR runtime DLL is missing"),error.constData());
            if(args.contains("--quit-after-ms")){QCOMPARE(process.exitCode(),0);QVERIFY(error.contains("opening desktop Hub"));QVERIFY(process.readAllStandardOutput().contains("AladdinsCastle: 1 games"));}
            else {QCOMPARE(process.exitCode(),3);QVERIFY(!error.contains("opening desktop Hub"));}
        }
        QVERIFY(!QFileInfo::exists(temp.filePath("openvr_api.dll")));
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
