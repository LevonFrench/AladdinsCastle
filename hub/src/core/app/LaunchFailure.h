// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
namespace ac {
void attachParentConsole();
QString writeLaunchFailureLog(const QString &root,const QString &gameId,const QString &message);
void showLaunchFailure(const QString &message,const QString &logPath);
}
