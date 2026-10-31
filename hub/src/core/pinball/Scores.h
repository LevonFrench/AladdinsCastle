// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>
namespace ac::pinball {
// Where PinMAME keeps its .nv files: VPinMAME's own nvram_directory setting (Windows), else the
// VPinMAME folder's nvram, else an nvram folder beside the ROM folder. Empty when none exists.
// AC_TEST_PINBALL_NVRAM names the folder instead, for tests.
QString nvramFolder(const QString &pinmameFolder,const QString &romFolder);
// The high scores and mode champions in one PinMAME .nv file, read with the Pinball Memory Maps entry
// for its ROM set from maps (the Hub's own data/pinball-score-maps.json when empty).
// {"set","scores":[{"kind":"high"|"champion","label","short","initials","score","text"}]}, or {"set","note"}
// when the set has no map. Entries with neither initials nor a score are left out.
QVariantMap nvramScores(const QString &set,const QByteArray &nvram,const QJsonObject &maps={});
// The table's scores from the newest .nv file of its possible ROM sets in folder, with "source" and
// "saved" (ISO date) added. Empty when the table has no ROM set or PinMAME saved nothing for it yet.
// Read-only, at most 1 MB a file.
QVariantMap tableScores(const QVariantMap &row,const QString &folder,const QJsonObject &maps={});
}
