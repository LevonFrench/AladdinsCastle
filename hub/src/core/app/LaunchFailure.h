// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <functional>
namespace ac {
void attachParentConsole();
QString writeLaunchFailureLog(const QString &root,const QString &gameId,const QString &message);
inline constexpr int launchFailureDialogTimeoutMs=15000;
void runTimedLaunchFailurePresentation(const std::function<void()> &display,const std::function<void()> &dismiss,int timeoutMs=launchFailureDialogTimeoutMs);
void showLaunchFailure(const QString &message,const QString &logPath);
}
