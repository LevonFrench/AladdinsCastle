// SPDX-License-Identifier: GPL-3.0-only
#include "ProductIdentity.h"
#include "LaunchFailure.h"
#include "core/install/Support.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QUuid>
#include <QTimer>
#include <QDebug>
#include <algorithm>
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
    install::atomicWrite(file,QByteArray(product::display_name)+" launch failed\n"+message.toUtf8()+"\n");
    // Bound only this game's generated preflight history; deletion is advisory.
    const QRegularExpression owned("^launch-"+QRegularExpression::escape(safeId)+"-\\d{8}-\\d{6}-\\d{3}-[a-f0-9]{8}\\.log$");
    auto history=QDir(QFileInfo(file).absolutePath()).entryInfoList(QDir::Files|QDir::NoSymLinks,QDir::Time|QDir::Reversed);
    history.erase(std::remove_if(history.begin(),history.end(),[&](const QFileInfo &entry){return entry.absoluteFilePath()==file||!owned.match(entry.fileName()).hasMatch();}),history.end());
    while(history.size()>=20){const auto oldest=history.takeFirst();if(!QFile::remove(oldest.absoluteFilePath()))qWarning("Preflight log retention could not remove an old log; the error log remains available.");}
    return file;
}
void runTimedLaunchFailurePresentation(const std::function<void()> &display,const std::function<void()> &dismiss,int timeoutMs){
    QTimer timer;timer.setSingleShot(true);QObject::connect(&timer,&QTimer::timeout,&timer,dismiss);timer.start(std::max(1,timeoutMs));display();timer.stop();
}
void showLaunchFailure(const QString &message,const QString &logPath){
#ifdef Q_OS_WIN
    // CI/operator headless mode is explicitly GPU-free and never opens a dialog.
    const auto platform=qEnvironmentVariable("QT_QPA_PLATFORM");
    if(platform=="offscreen"||platform=="minimal")return;
    const auto text=message+"\n\n"+(logPath.isEmpty()?"A portable launch log could not be written.":"Log: "+logPath);
    const auto caption=QString::fromUtf8(product::display_name)+" launch failed";
    runTimedLaunchFailurePresentation([&]{MessageBoxW(nullptr,reinterpret_cast<LPCWSTR>(text.utf16()),reinterpret_cast<LPCWSTR>(caption.utf16()),MB_OK|MB_ICONERROR|MB_SETFOREGROUND);},[&]{
        EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM context)->BOOL{wchar_t title[256]{};GetWindowTextW(window,title,256);if(wcscmp(title,reinterpret_cast<LPCWSTR>(context))==0)PostMessageW(window,WM_COMMAND,IDOK,0);return TRUE;},reinterpret_cast<LPARAM>(caption.utf16()));
    });
#else
    Q_UNUSED(message);Q_UNUSED(logPath);
#endif
}
}
