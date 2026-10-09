// SPDX-License-Identifier: GPL-3.0-only
#include "LaunchFailure.h"
#include "core/install/Support.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>
#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif
namespace ac {
void attachParentConsole(){
#ifdef Q_OS_WIN
    // QProcess and shell redirection supply their own handles. Preserve them.
    const auto output=GetStdHandle(STD_OUTPUT_HANDLE);
    if(output&&output!=INVALID_HANDLE_VALUE)return;
    if(AttachConsole(ATTACH_PARENT_PROCESS)){
        FILE *stream=nullptr;
        freopen_s(&stream,"CONOUT$","w",stdout);
        freopen_s(&stream,"CONOUT$","w",stderr);
    }
#endif
}
QString writeLaunchFailureLog(const QString &root,const QString &gameId,const QString &message){
    const auto safeId=QRegularExpression("^[a-z0-9]+(?:-[a-z0-9]+)*$").match(gameId).hasMatch()?gameId:QString("unknown-game");
    const auto stamp=QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::Id128).left(8);
    const auto file=install::scopedPath("user/logs/launch-"+safeId+"-"+stamp+".log",root);
    QDir().mkpath(QFileInfo(file).absolutePath());
    install::atomicWrite(file,"AladdinsCastle launch failed\n"+message.toUtf8()+"\n");
    return file;
}
void showLaunchFailure(const QString &message,const QString &logPath){
#ifdef Q_OS_WIN
    // CI/operator headless mode is explicitly GPU-free and never opens a dialog.
    const auto platform=qEnvironmentVariable("QT_QPA_PLATFORM");
    if(platform=="offscreen"||platform=="minimal")return;
    const auto text=message+"\n\n"+(logPath.isEmpty()?"A portable launch log could not be written.":"Log: "+logPath);
    MessageBoxW(nullptr,reinterpret_cast<LPCWSTR>(text.utf16()),L"AladdinsCastle launch failed",MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
#else
    Q_UNUSED(message);Q_UNUSED(logPath);
#endif
}
}
