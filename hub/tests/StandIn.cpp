// SPDX-License-Identifier: GPL-3.0-only
// The stand-in game for the integration and smoke tests: a small program the tests copy under an emulator's
// name and start, so the Hub sees a real process begin, report what it was given, and end. It was the test
// program itself; a large unsigned test binary copying itself into the temporary folder under game names
// looked like malware to Windows Defender, which deleted it mid-run.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QWindow>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
void appendLine(const QString &path,const QByteArray &line){if(path.isEmpty())return;QFile file(path);if(file.open(QIODevice::Append))file.write(line+"\n");}
QJsonObject launchVariables(){
    QJsonObject variables;const auto environment=QProcessEnvironment::systemEnvironment();
    for(const auto &name:environment.keys())if(name.startsWith("ACVR_")||name=="XR_RUNTIME_JSON")variables[name]=environment.value(name);
    return variables;
}
}

int main(int argc,char **argv){
    // Synthetic games for the watchdog, chosen by AC_SYNTHETIC_GAME (set only around one test):
    // "crash" ends with an access violation's code (no real fault, so no error dialog ever shows),
    // "hidden" runs with a window that is never shown, "werfault" starts a stand-in Windows Error
    // Reporting program and waits, "grandchild" starts a copy of itself and leaves it behind.
    if(const auto mode=qEnvironmentVariable("AC_SYNTHETIC_GAME");!mode.isEmpty()&&argc==1){
        QCoreApplication game(argc,argv);
        appendLine(qEnvironmentVariable("AC_SYNTHETIC_PIDS"),QByteArray::number(QCoreApplication::applicationPid()));
#ifdef Q_OS_WIN
        if(mode=="crash")QTimer::singleShot(600,&game,[]{TerminateProcess(GetCurrentProcess(),0xC0000005);});
        if(mode=="hidden"){
            WNDCLASSW kind{};kind.lpfnWndProc=DefWindowProcW;kind.hInstance=GetModuleHandleW(nullptr);kind.lpszClassName=L"AcSyntheticGame";RegisterClassW(&kind);
            CreateWindowExW(0,L"AcSyntheticGame",L"Synthetic game",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,kind.hInstance,nullptr); // Never shown.
        }
#endif
        if(mode=="werfault")QProcess::startDetached(qEnvironmentVariable("AC_SYNTHETIC_WERFAULT"),{"--synthetic-child"});
        if(mode=="grandchild"){
            qint64 pid=0;QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--synthetic-child"},{},&pid);
            appendLine(qEnvironmentVariable("AC_SYNTHETIC_PIDS"),"grandchild "+QByteArray::number(pid));
            QTimer::singleShot(800,&game,&QCoreApplication::quit);
        }
        QTimer::singleShot(30000,&game,&QCoreApplication::quit);return game.exec();
    }
    // A launcher that starts two children, the first of which starts a grandchild. Each writes its
    // process id and waits; the launcher waits too ("stay") or ends after a moment ("leave").
    if(argc>=4&&(QByteArray(argv[1])=="--spawn-tree"||QByteArray(argv[1])=="--tree-child")){
        QCoreApplication child(argc,argv);const auto folder=QString::fromLocal8Bit(argv[2]),name=QString::fromLocal8Bit(argv[3]);
        const auto self=QCoreApplication::applicationFilePath();
        {QFile pid(folder+"/"+name+".pid.part");if(pid.open(QIODevice::WriteOnly)){pid.write(QByteArray::number(QCoreApplication::applicationPid()));pid.close();QFile::remove(folder+"/"+name+".pid");pid.rename(folder+"/"+name+".pid");}}
        if(QByteArray(argv[1])=="--spawn-tree"){
            QProcess::startDetached(self,{"--tree-child",folder,"child1","grand"});QProcess::startDetached(self,{"--tree-child",folder,"child2"});
            if(argc>=5&&QByteArray(argv[4])=="leave"){QTimer::singleShot(2500,&child,&QCoreApplication::quit);return child.exec();}
        }else if(argc>=5&&QByteArray(argv[4])=="grand")QProcess::startDetached(self,{"--tree-child",folder,"grandchild"});
        QTimer::singleShot(60000,&child,&QCoreApplication::quit);return child.exec();
    }
    for(int i=1;i<argc;++i)if(QByteArray(argv[i])=="--synthetic-child"){QCoreApplication app(argc,argv);QTimer::singleShot(30000,&app,&QCoreApplication::quit);return app.exec();}
    // PS2 stand-in for the Python running the launcher: it waits for the stop file, then ends normally.
    if(argc==3&&QByteArray(argv[1]).endsWith("launch_ps2.py")){
        QCoreApplication child(argc,argv);const auto stop=qEnvironmentVariable("AC_SYNTHETIC_PS2_STOP");QTimer poll;
        // With AC_SYNTHETIC_PS2_IGNORE the game takes no notice of the stop file, as a stuck one would.
        const bool ignore=qEnvironmentVariableIsSet("AC_SYNTHETIC_PS2_IGNORE");
        QObject::connect(&poll,&QTimer::timeout,&child,[&]{if(!ignore&&QFileInfo::exists(stop))child.exit(0);});poll.start(100);
        QTimer::singleShot(20000,&child,[&]{child.exit(7);});return child.exec();
    }
    // Model 2 stand-ins. The emulator: any "-rompath" run, which waits to be ended. The viewer: one numeric argument.
    for(int i=1;i<argc;++i)if(QByteArray(argv[i])=="-rompath"){QCoreApplication child(argc,argv);QTimer::singleShot(30000,&child,&QCoreApplication::quit);return child.exec();}
    if(argc==2&&QByteArray(argv[1]).toLongLong()>0){
        QCoreApplication child(argc,argv);const auto environment=QProcessEnvironment::systemEnvironment();
        const auto id=QByteArray(argv[1]).toLongLong();bool alive=false;
#ifdef Q_OS_WIN
        if(const auto handle=OpenProcess(SYNCHRONIZE,FALSE,static_cast<DWORD>(id))){alive=WaitForSingleObject(handle,0)==WAIT_TIMEOUT;CloseHandle(handle);}
#else
        alive=QFileInfo::exists("/proc/"+QString::number(id));
#endif
        QFile file(qEnvironmentVariable("AC_SYNTHETIC_MODEL2_REPORT"));if(!file.open(QIODevice::WriteOnly))return 3;
        file.write(QJsonDocument(QJsonObject{{"companion",id},{"companionAlive",alive},{"cwd",QDir::currentPath()},{"env",launchVariables()},{"path",environment.value("PATH")}}).toJson());file.close();
        // Like the real viewer: with AC_SYNTHETIC_MODEL2_WAIT_STOP it plays until its stop file appears, then ends normally.
        if(qEnvironmentVariableIsSet("AC_SYNTHETIC_MODEL2_WAIT_STOP")){const auto stop=qEnvironmentVariable("ACVR_M2_STOP_FILE");QTimer poll;
            QObject::connect(&poll,&QTimer::timeout,&child,[&]{if(QFileInfo::exists(stop))child.exit(0);});poll.start(100);QTimer::singleShot(20000,&child,[&]{child.exit(7);});return child.exec();}
        QTimer::singleShot(600,&child,&QCoreApplication::quit);return child.exec();
    }
    for(int i=1;i<argc;++i)if(QByteArray(argv[i]).startsWith("-input-system=")){
        QCoreApplication child(argc,argv);
        QFile file(qEnvironmentVariable("AC_SYNTHETIC_MODEL3_REPORT"));if(!file.open(QIODevice::WriteOnly))return 3;
        file.write(QJsonDocument(QJsonObject{{"cwd",QDir::currentPath()},{"args",QJsonArray::fromStringList(child.arguments().mid(1))},{"env",launchVariables()}}).toJson());file.close();
        // A slow game: it writes its completion marker only after the given time, then ends.
        const auto duration=qEnvironmentVariableIntValue("AC_SYNTHETIC_CHILD_MS");const auto marker=qEnvironmentVariable("AC_SYNTHETIC_MARKER");
        QTimer::singleShot(duration>0?duration:600,&child,[&child,marker]{if(!marker.isEmpty()){QFile done(marker);if(done.open(QIODevice::WriteOnly))done.write("complete");}child.quit();});
        return child.exec();
    }
    // VPX stand-in: any run with -Play, which -Ini may come before.
    bool play=false;for(int i=1;i<argc;++i)play=play||QByteArray(argv[i])=="-Play";
    if(play){
        QGuiApplication child(argc,argv);QWindow window;window.show();
        const auto arguments=qEnvironmentVariable("AC_SYNTHETIC_PINBALL_ARGUMENTS");
        if(!arguments.isEmpty()){
            QFile file(arguments);if(!file.open(QIODevice::WriteOnly))return 3;
            file.write(QJsonDocument::fromVariant(child.arguments().mid(1)).toJson(QJsonDocument::Compact));
        }
        // With AC_SYNTHETIC_PINBALL_RELEASE the table plays until that file appears, however slow the machine is;
        // the 60 s limit is only an orphan failsafe.
        if(const auto release=qEnvironmentVariable("AC_SYNTHETIC_PINBALL_RELEASE");!release.isEmpty()){
            QTimer poll;QObject::connect(&poll,&QTimer::timeout,&child,[&]{if(QFileInfo::exists(release))child.quit();});poll.start(50);
            QTimer::singleShot(60000,&child,&QCoreApplication::quit);return child.exec();
        }
        const auto duration=qEnvironmentVariableIntValue("AC_SYNTHETIC_PINBALL_CHILD_MS");
        QTimer::singleShot(duration>0?duration:600,&child,&QCoreApplication::quit);return child.exec();
    }
    // Anything else (a tool probe, a help run): start, then end normally a moment later.
    QCoreApplication app(argc,argv);QTimer::singleShot(600,&app,&QCoreApplication::quit);return app.exec();
}
