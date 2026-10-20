// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace ac::n64 {
// Nintendo 64 games through acvr-ps1 (setups/retro-ps1-vr, --controls pad|wheel|aim) and the user's own
// launcher script (Windows PowerShell), which knows each game's cartridge dump, core options, controls,
// depth and widescreen settings and is started with a key. A game's views are separate keys:
//   <id>-3d       real 3D           <id>          flat picture       <id>-theatre   4:3 theatre screen
// The Hub offers only the views whose key the launcher's games.json lists. Like PS1, each launch gets its own
// stop file in ACVR_PS1_STOP_FILE (acvr-ps1 saves and quits when it appears). Only public titles and keys are
// listed here; every location comes from Hub settings:
//   n64Launcher    the launcher script (launch-n64.ps1; games.json beside it lists its keys)
//   n64GameFolder  the user's own cartridge-dump folder, shown in Set-up
QVariantList games(); // id, title, year, controls (plain sentences), frame ("open" | "wide" | "standard")
// The keys the user's launcher knows, from the games.json beside it. Read-only.
QStringList launcherKeys(const QString &launcher);
// The views the launcher has for this game, each {id, label, help}, real 3D first. Empty when it has none.
QVariantList views(const QString &game,const QString &launcher);
QString viewLabel(const QString &game,const QString &view);
// The launcher key for a game and view ("3d", "flat" or "theatre"); empty for an unknown view.
QString launcherKey(const QString &game,const QString &view);
// Empty when the game can start in some view; otherwise "What is wrong. What to do."
QString problem(const QString &game,const QString &launcher);
// executable (Windows PowerShell), cwd (the launcher's folder) and args. Throws install::Error.
QVariantMap launchPlan(const QString &game,const QString &launcher,const QString &view);
}
