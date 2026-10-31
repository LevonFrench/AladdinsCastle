// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "VpsMatch.h"
#include <QJsonArray>
#include <QVariantMap>
#include <QString>
#include <vector>
namespace ac::pinball {
// Prefer the table's file name when embedded TableInfo describes a different table.
void identify(QVariantMap &row,const QString &fileStem);
// A VPS database as read, and its games prepared once for the distance matcher.
struct VpsDatabase {QJsonArray json;std::vector<vps::Game> games;};
VpsDatabase vpsDatabase(const QJsonArray &db);
// Machine identity is independent of a VPX author's version. Exact names come first; the ported
// VPin Studio matcher (VpsMatch.h) is asked only when they find no machine, and what it finds is
// taken only when the file's maker and year agree, no other machine is as near and every word and
// number of the machine's name is in the table's. File features need the table's own authors and
// version on one VPS file, exactly or with the matcher's tolerances (1.1.3 is 1.13, ClarkKent).
void matchVps(QVariantMap &row,const VpsDatabase &db);
void matchVps(QVariantMap &row,const QJsonArray &db);
}
