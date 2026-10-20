// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace ac::saturn {
// Sega Saturn light-gun games through acvr-ps1 (setups/retro-ps1-vr, --gun stunner with the Beetle Saturn core) and
// the user's own launcher script (Windows PowerShell, launch-saturn.ps1), which knows each game's disc, aim trim,
// flash setting and saved calibration and is started with the game's key. Saturn games play on a theatre screen:
// the console hands its graphics chip flat 2D shapes, so there is no depth for real 3D. Like PS1, each launch gets
// its own stop file in ACVR_PS1_STOP_FILE (acvr-ps1 saves and quits when it appears). Only public titles and keys are
// listed here; every location comes from Hub settings:
//   saturnLauncher    the launcher script (games.json beside it lists its keys)
//   saturnGameFolder  the user's own disc folder, shown in Set-up
QVariantList games(); // id (the launcher's key), title, year, controls (plain sentences), view ("theatre")
// The keys the user's launcher knows, from the games.json beside it. Read-only.
QStringList launcherKeys(const QString &launcher);
// Empty when the game can start; otherwise "What is wrong. What to do."
QString problem(const QString &game,const QString &launcher);
// executable (Windows PowerShell), cwd (the launcher's folder) and args. Throws install::Error.
QVariantMap launchPlan(const QString &game,const QString &launcher);
}
