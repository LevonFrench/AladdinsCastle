// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
namespace ac::scan{class DiscMemo;}
namespace ac::wii {
// Wii gun games through the user's own DolphinXR build (decisions D52-D54). Only public titles and
// game ids are listed; discs and the build are found through Hub settings and never named here.
// Each row: id, catalogId, title, year, hold ("split" or "docked"), route ("working", "imperfect",
// "unknown": how far the game is known to play in Dolphin).
QVariantList games();
// The six-character game id a Wii disc image carries in its own header (.iso, .wbfs, .rvz, .wia),
// such as "RBUP08"; empty when the shared scanner cannot read a Wii identity.
QString discId(const QString &file);
QString discId(const QString &file,std::atomic_bool &cancel);
// Which listed game a game id is: an exact ID listed in the public index. Empty when none.
QString gameFor(const QString &discId);
// What one disc image is: {file, id (game id), game, title, verified}. Read-only.
QVariantMap identify(const QString &file);
QVariantMap identify(const QString &file,std::atomic_bool &cancel,scan::DiscMemo *memo=nullptr);
// The Wii disc images directly inside a folder, identified. Subfolders are not entered.
QVariantList discs(const QString &folder, std::atomic_bool &cancel, scan::DiscMemo *memo = nullptr);
// What a game is still missing in this DolphinXR build, said plainly: its own VR settings
// (User/GameSettingsVR/<game id>.ini) and, for a game held docked, its docked controls. Empty when
// nothing is missing.
QStringList missing(const QString &game, const QString &discId, const QString &executable);
// executable, cwd and args for one disc: the authored DolphinXR route of the Chronicles recipes,
// with the build's own User folder, OpenXR on, and Dolphin's stop confirmation off so Exit game
// closes it. Throws install::Error.
QVariantMap launchPlan(const QString &file, const QString &executable);
// Brings an existing DolphinXR User folder up to the universal controls: the both-click recentre
// and 3 s quit hotkeys in Config/Hotkeys.ini and the stop setting in Config/Dolphin.ini. Only
// missing or older universal values change; the user's own bindings and every other line stay.
// A file that changes is copied first to <name>.before-universal-controls (once, never replaced).
// Returns what changed ("Hotkeys.ini General/Exit added"), empty when nothing did. A folder with
// no Config folder is left alone. Same rules as setups/dolphin-vr/motion-controls/reseed_controls.py.
QStringList refreshUniversalControls(const QString &userFolder);
} // namespace ac::wii
