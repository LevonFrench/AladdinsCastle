// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace ac::ps1 {
// PlayStation light-gun games through acvr-ps1 and the user's own launcher
// script (Windows PowerShell), which knows each game's disc, gun and core and is
// started with the game's key. The Hub gives each launch its own stop file in
// ACVR_PS1_STOP_FILE; creating it asks the game to quit and save. Only public
// titles and keys are listed here; every location comes from Hub settings:
//   ps1Launcher    the launcher script (games.json beside it lists its keys)
//   ps1GameFolder  the user's own disc folder, shown in Set-up
QVariantList games(); // id (the launcher's key), title, year, controls (a plain sentence), view
// The keys the user's launcher knows, from the games.json beside it. Read-only.
QStringList launcherKeys(const QString &launcher);
// A game's views are separate launcher keys: <id> (its usual view: real 3D or the theatre screen),
// <id>-open-frame (3D drawn 16:9), <id>-diorama (sprites at depths, a prototype) and <id>-no-trim (the aim
// without its measured trim, to compare). The views the launcher lists for the game, each {id, label, help},
// the usual one first. Empty when it lists none.
QVariantList views(const QString &game,const QString &launcher);
QString viewLabel(const QString &game,const QString &view);
// The launcher key for a game and view ("standard", "open-frame", "diorama" or "no-trim"); empty for another view.
QString launcherKey(const QString &game,const QString &view);
// Empty when the game can start; otherwise "What is wrong. What to do."
QString problem(const QString &game,const QString &launcher);
// executable (Windows PowerShell), cwd (the launcher's folder) and args. Throws install::Error.
QVariantMap launchPlan(const QString &game,const QString &launcher,const QString &view=QStringLiteral("standard"));
}
