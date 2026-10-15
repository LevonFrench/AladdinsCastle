// SPDX-License-Identifier: GPL-3.0-only
#include "app/LaunchProof.h"
#include "app/HubServices.h"
#include "core/install/Install.h"
#include "core/launch/Launch.h"
#include "ui/UiController.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <memory>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace ac {
LaunchProofOptions launchProofOptions(){
    LaunchProofOptions options;
    options.output=qEnvironmentVariable("AC_LAUNCH_PROOF");
    options.gameId=qEnvironmentVariable("AC_LAUNCH_PROOF_GAME");
    options.view=qEnvironmentVariable("AC_LAUNCH_PROOF_VIEW");
    options.childEnv=qEnvironmentVariable("AC_LAUNCH_PROOF_ENV");
    options.childArgs=qEnvironmentVariable("AC_LAUNCH_PROOF_ARGS");
    bool ok=false;const int hold=qEnvironmentVariableIntValue("AC_LAUNCH_PROOF_HOLD",&ok);if(ok&&hold>0&&hold<=120)options.holdSeconds=hold;
    return options;
}
namespace {
struct Proof {
    LaunchProofOptions options;QElapsedTimer clock;QJsonArray events;QJsonObject result;
    bool started=false,finished=false,exitAsked=false,stopAsked=false;
    void note(const QString &kind,const QJsonObject &detail={}){QJsonObject e=detail;e["ms"]=qint64(clock.elapsed());e["event"]=kind;events.append(e);}
    void write(){
        result["game"]=options.gameId;result["view"]=options.view;result["childEnv"]=options.childEnv;result["childArgs"]=options.childArgs;
        result["holdSeconds"]=options.holdSeconds;result["events"]=events;result["written"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        install::atomicWrite(options.output,QJsonDocument(result).toJson());
    }
};
QString text(const QVariant &value){return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));}
}
void runLaunchProof(const LaunchProofOptions &options,HubServices &services,UiController &ui){
    auto proof=std::make_shared<Proof>();proof->options=options;proof->clock.start();
#ifdef Q_OS_WIN
    // Below normal priority, inherited by the game the Hub starts.
    SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
#endif
    auto *launcher=services.launcher();
    const auto finish=[proof]{proof->write();QTimer::singleShot(0,qApp,[]{QCoreApplication::quit();});};
    // Everything the Hub shows along the way.
    QObject::connect(&ui,&UiController::statusChanged,qApp,[proof,&ui]{proof->note("status",{{"text",ui.status()}});});
    QObject::connect(&ui,&UiController::nowPlayingChanged,qApp,[proof,&ui]{proof->note("card",{{"nowPlaying",text(ui.nowPlaying())},{"lastSession",text(ui.lastSession())}});});
    QObject::connect(launcher,&launch::LaunchService::starting,qApp,[proof](const QString &,const QString &message){proof->note("starting",{{"text",message}});});
    QObject::connect(launcher,&launch::LaunchService::warning,qApp,[proof](const QString &,const QString &message){proof->note("warning",{{"text",message}});});
    QObject::connect(launcher,&launch::LaunchService::started,qApp,[proof,launcher,options,&ui,finish](const QString &){
        proof->started=true;
        proof->result["pid"]=qint64(launcher->processId());proof->result["program"]=launcher->processProgram();
        if(launcher->companionProcessId())proof->result["companionPid"]=qint64(launcher->companionProcessId());
        proof->note("started",{{"pid",qint64(launcher->processId())},{"program",launcher->processProgram()}});
        // Still running after the hold time, then ended the way a player would: Exit, and Stop only if Exit does not end it.
        QTimer::singleShot(options.holdSeconds*1000,qApp,[proof,launcher,options,&ui,finish]{
            if(proof->finished)return;
            proof->result["runningAfterHold"]=launcher->childRunning();
            proof->note("hold-check",{{"running",launcher->childRunning()},{"companionRunning",launcher->companionRunning()}});
            proof->exitAsked=true;proof->note("exit-game");ui.closeLaunch();
            QTimer::singleShot(options.exitWaitSeconds*1000,qApp,[proof,&ui,finish]{
                if(proof->finished)return;
                proof->stopAsked=true;proof->note("stop");ui.stopLaunch();
                QTimer::singleShot(15000,qApp,[proof,finish]{if(proof->finished)return;proof->result["outcome"]="did not end after Stop";finish();});
            });
        });
    });
    QObject::connect(launcher,&launch::LaunchService::finished,qApp,[proof,&ui,finish](const QString &,int code,const QString &error,const QString &log){
        proof->finished=true;proof->result["exitCode"]=code;proof->result["error"]=error;proof->result["log"]=log;
        proof->result["endedBy"]=proof->stopAsked?"Stop":proof->exitAsked?"Exit game":"itself";
        proof->note("finished",{{"exitCode",code},{"error",error}});
        // Let the card settle on how the game ended before writing.
        QTimer::singleShot(1500,qApp,[proof,&ui,finish]{proof->result["lastSession"]=QJsonObject::fromVariantMap(ui.lastSession());proof->result["finalStatus"]=ui.status();finish();});
    });
    QTimer::singleShot(1000,qApp,[proof,&services,&ui,options,finish]{
        proof->note("play",{{"game",options.gameId},{"view",options.view}});
        services.setDiagnosticChild(options.childEnv,options.childArgs);
        services.startForDiagnostic(options.gameId,options.view);
        // A refusal ends the proof at once: nothing was started.
        // Only when the Hub holds no launch; a launch still preparing is waited for.
        QTimer::singleShot(4000,qApp,[proof,finish,&ui,&services]{if(!proof->started&&!proof->finished&&!services.launchBusy()){proof->result["outcome"]="not started";proof->result["finalStatus"]=ui.status();finish();}});
    });
}
}
