// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace ac::scripted {
// Systems whose games the user's own launcher script starts, one game and one view at a time:
//   powershell -File <launcher> <key> -View <view>
// The launcher keeps everything private (game files, emulator builds, settings) in a games.json beside it:
//   { "<key>": { "title": "...", "controls": "...", "group": "...", "views": { "<view>": <settings or {label, controls, help}> } } }
// Keys beginning with "_" are the launcher's own and are not games. Nothing here names a game or a path: the Hub
// lists what the user's games.json lists. Every location comes from Hub settings.
struct System {
  QString id;        // the Hub's system id and game id prefix ("mamegun" -> "mamegun-timecris")
  QString pill;      // card pill
  QString settingKey;// Hub setting holding the launcher path
  QString folder;    // the launcher's usual folder beside the other launchers' ("mame-gun")
  QString script;    // the launcher's usual file name ("launch-mame-gun-key.ps1")
};
const QList<System> &systems();
const System *system(const QString &id);
// The launcher to use: the chosen one when it is a file; otherwise the system's usual launcher beside one of the
// other launchers the user chose (their play folder: <play>/<folder>/<script>), when that file exists. Read-only.
QString locate(const System &system, const QString &chosen, const QStringList &otherLaunchers);
// The games the launcher lists, by title: id (the key), title, year, controls, group, status and views
// ({id, label, help, controls, monitor}), the usual order of views first (3D before open frame before the rest,
// opt-in views and the monitor last). Empty when there is no readable games.json.
QVariantList games(const QString &launcher);
// The label of a view id the Hub knows ("3d" -> "Real 3D"); the id itself otherwise.
QString viewLabel(const QString &view);
// True for a view that plays on the PC monitor with no headset.
bool isMonitor(const QVariantMap &view);
// Empty when the game can start in this view; otherwise "What is wrong. What to do."
QString problem(const QString &launcher, const QString &key, const QString &view);
// The game's own files as a launcher's games.json names them under these keys: "file", "disc" or "rom" on the
// entry and on each of its views, a relative path read beside games.json. Empty when every one is there or
// none is named; otherwise "Game file not found: <path>. ..." for the first one missing. Any launcher with a
// games.json beside it (PS1, PS2, N64, Saturn too). An arcade set named only by its key is not checked: MAME
// also finds a clone's files inside its parent's set.
QString missingMedia(const QString &launcher, const QStringList &keys);
// executable (Windows PowerShell), cwd (the launcher's folder) and args. Throws install::Error.
QVariantMap launchPlan(const QString &launcher, const QString &key, const QString &view);
} // namespace ac::scripted
