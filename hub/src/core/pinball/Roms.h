// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QMap>
#include <QSet>
#include <QString>
#include <QVariantMap>
namespace ac::pinball {
struct RomInventory {
 QSet<QString> present;
 QSet<QString> possible;
 QMap<QString,QString> aliases;
 QSet<QString> unresolvedAliases;
 QVariantMap fingerprint;
 bool configured=false,complete=false;
};
// Reads bounded names and ZIP directory metadata only. pinmameFolder is the
// explicitly selected VPinMAME directory, never guessed from other installs.
RomInventory collectRoms(const QString &romFolder,const QString &pinmameFolder={});
// The parser supplies romCandidates, romCandidatesComplete and
// romRequiresPinmame. A certain legacy row with rom is also understood.
QVariantMap applyRomState(QVariantMap row,const RomInventory &inventory);
}
