// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QVariantMap>
#include <QVariantList>
#include <QString>
namespace ac::pinball {
QVariantMap inspect(const QString &path);
QVariantMap scan(const QString &folder,const QString &database={},const QString &romFolder={});
QVariantMap launchPlan(const QString &table,const QString &executable,const QString &ownedRoot,bool vr);
}
