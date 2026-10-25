// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace ac::libretro {
// Games the Hub starts straight through acvr-ps1 and a per-game profile (setups/retro-ps1-vr/profiles/<id>.toml),
// with no launcher script of the user's in between. Only the public id, title and profile name are listed here;
// the program, the core and the game file come from Hub settings and are never named by the repo:
//   libretroHost       acvr-ps1.exe (a build that reads --profile)
//   threedsCore        the Nintendo 3DS core (Azahar's libretro dll)
//   threedsGameFolder  the user's own 3DS dumps (decrypted, as Azahar needs them)
//   libretroProfiles   the profile folder; when empty, setups/retro-ps1-vr/profiles is looked for above the Hub
// Each game's id is its catalog id (games/<id>/game.toml), so the Hub's row and the catalog card are one game.
QVariantList games(); // id, title, year, system ("3ds"), profile (file name), controls (a plain sentence)
// The profile file name of a game; empty for a game not listed.
QString profileName(const QString &game);
// The profile folder: the chosen one when it is a folder; otherwise setups/retro-ps1-vr/profiles looked for in
// startFrom and the folders above it (the Hub's own folder in a source tree). Empty when there is none. Read-only.
QString profilesFolder(const QString &chosen, const QString &startFrom);
// The game's file in the dumps folder: the file whose name is the game's title (spaces, case and punctuation
// ignored), with a 3DS dump extension. Empty when the folder has none. Read-only; folders below it are not entered.
QString gameFile(const QString &game, const QString &folder);
// The views, usual one first, each {id, label, help}. "theatre" is the profile's own default view and passes no --view.
QVariantList views();
QString viewLabel(const QString &view);
// Empty when the game can start; otherwise "What is wrong. What to do."
QString problem(const QString &game, const QString &host, const QString &core, const QString &gameFolder, const QString &profiles);
// executable (the host), cwd (the host's folder) and args:
//   --core <core> --game <file> --system <dataFolder>/system --saves <dataFolder>/saves --profile <file> [--view <view>]
// The Hub creates dataFolder (its own, under user/) before the start. Throws install::Error.
QVariantMap launchPlan(const QString &game, const QString &host, const QString &core, const QString &file, const QString &dataFolder, const QString &profiles, const QString &view);
}
