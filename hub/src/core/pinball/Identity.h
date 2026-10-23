// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QJsonArray>
#include <QVariantMap>
#include <QString>
namespace ac::pinball {
// Prefer the table's file name when embedded TableInfo describes a different table.
void identify(QVariantMap &row,const QString &fileStem);
// Machine identity is independent of a VPX author's version. File features need an exact file match.
void matchVps(QVariantMap &row,const QJsonArray &db);
}
