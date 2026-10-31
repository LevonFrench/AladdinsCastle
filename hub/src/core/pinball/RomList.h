// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Roms.h"
#include <QByteArray>
#include <QString>
#include <QStringList>
namespace ac::pinball {
// ROM sets' file lists as JSON, {"sets":{"<set>":{"cloneOf":"","roms":[{"name","length","crc","flags"}]}},
// "unknown":["<set>"]}, and back. Names that are not plain ROM set names are left out.
ExpectedSets expectedSetsFromJson(const QByteArray &json);
QByteArray expectedSetsToJson(const ExpectedSets &sets,const QStringList &unknown={});
// PinMAME's file lists for the named sets, from the VPinMAME registered from pinmameFolder (the VPX
// install's own, never another one), asked through `hubtool pinmame-roms` beside this program in a hidden,
// below-normal, time-limited process, so VPinMAME never loads into the Hub. AC_TEST_PINMAME_ROM_LIST names
// a JSON file to read instead, for tests. Empty, with why saying so, when there is nothing to ask.
ExpectedSets expectedRomSets(const QStringList &sets,const QString &pinmameFolder,QString &why);
// hubtool's side, Windows only: VPinMAME's driver lists over COM. Nothing is audited, started or written.
// VPinMAME is created only when it is registered from pinmameFolder and was set up before (its "default"
// settings key exists), so its first-run About box, welcome message and settings dialogs never open.
ExpectedSets vpinmameRomSets(const QStringList &sets,const QString &pinmameFolder,QStringList &unknown,QString &error);
}
