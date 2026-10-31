// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>
namespace ac::pinball {
struct RomInventory {
 QSet<QString> present;
 QSet<QString> possible;
 QMap<QString,QString> aliases;
 QSet<QString> unresolvedAliases;
 // The ZIP each present set name was found in, the first in folder order (PinMAME's own search order is not modelled).
 QMap<QString,QString> zips;
 QVariantMap fingerprint;
 bool configured=false,complete=false;
};
// One file of a ZIP, from its central directory: the name without folders, the unpacked size and the CRC-32.
struct ZipFile {QString name;quint32 size=0,crc=0;};
// A ZIP's files from its central directory alone (nothing unpacked), bounded; nothing when it cannot be read safely.
std::optional<QList<ZipFile>> zipFiles(const QString &path);
// One file PinMAME's driver lists for a ROM set: its name, expected length, expected CRC-32 (0 when no good
// dump is known) and the flags of its memory region (0x20 is ROMREGION_SOUNDONLY: loaded only for sound).
struct ExpectedRom {QString name;quint32 length=0,crc=0,flags=0;};
struct ExpectedSet {QString cloneOf;QList<ExpectedRom> roms;};
using ExpectedSets=QMap<QString,ExpectedSet>; // by set name, case-folded
struct RomSetCheck {QStringList missing,differ,soundMissing;};
// Each file found as PinMAME finds one in a ZIP: by its CRC whatever its name, else by its name; a clone's
// files may also come from its parent set's ZIP. A file with no known good dump that is absent is not needed.
RomSetCheck checkRomSet(const ExpectedSet &set,const QList<ZipFile> &own,const QList<ZipFile> &parent={});
// The ROM sets the scan's rows can be checked for: one certain ROM, ready, held in a ZIP.
QStringList romCheckSets(const QVariantList &rows,const RomInventory &inventory);
// Adds romCheck (and romMissing, romDiffer, romSoundMissing) to a ready row whose set is listed. A missing
// program file makes the ROM missing; a file that differs makes it unknown (VPinMAME may stop on a warning);
// missing sound files leave it ready, with a note.
QVariantMap applyRomCheck(QVariantMap row,const RomInventory &inventory,const ExpectedSets &sets);
// Reads bounded names and ZIP directory metadata only. pinmameFolder is the
// explicitly selected VPinMAME directory, never guessed from other installs.
RomInventory collectRoms(const QString &romFolder,const QString &pinmameFolder={});
// The parser supplies romCandidates, romCandidatesComplete and
// romRequiresPinmame. A certain legacy row with rom is also understood.
QVariantMap applyRomState(QVariantMap row,const RomInventory &inventory);
}
