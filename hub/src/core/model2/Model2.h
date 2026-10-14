// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace ac::model2 {
// Sega Model 2 games through the user's own stereo MAME build and its viewer.
// Two programs run together: the emulator, with no window of its own, and the
// viewer that shows its picture in the headset. View only for now: the viewer
// has no controls yet. Locations come from Hub settings, never the repository.
//
// The build's folders, relative to the emulator program's folder:
//   ../presenter/model2-viewer.exe   the viewer
//   ../runtime                       the emulator's working folder
//   ../captures                      where the emulator keeps its own snapshots
QVariantList games(); // id (= set name), title, year
// Empty when the game can start; otherwise "What is wrong. What to do." Which
// device sets a game needs beside it is the emulator's own knowledge: the set-up
// scan asks the emulator (see setup::mameCheck) instead of keeping a list here.
QString problem(const QString &game,const QString &program,const QString &gameFolder);
// The whole launch in one place: executable, cwd and args for the viewer, the
// same for the emulator under "companion", and env (name -> value). The
// viewer's argument is "${companion_pid}", filled in by the launcher.
// Read-only: nothing is created. Throws install::Error.
QVariantMap launchPlan(const QString &game,const QString &program,const QString &gameFolder);
}
