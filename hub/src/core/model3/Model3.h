// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QVariantList>
#include <QVariantMap>
namespace ac::model3 {
// Sega Model 3 games through the user's own stereo Supermodel build. The public
// list names arcade titles and their standard set names only; the emulator and
// game file locations come from Hub settings and never from the repository.
QVariantList games(); // id (= set name), title, year
QVariantList views(); // id, label, help: the three views every first-ten game has
// The views one game has, each {id, label, help, controls, monitor}, as the old numbered launchers offered them:
// screen, open-frame (Supermodel -wide-screen, 16:9), wide, depth (the measured depth trial), inside,
// inside-wheel (the drawn wheel or handlebars to grab), inside-stick (Star Wars Trilogy's stick to grab) and
// sbs-monitor (both eyes side by side on the PC monitor, no headset). Empty for a game not in the list.
QVariantList views(const QString &game);
bool isMonitorView(const QString &view);
QString viewLabel(const QString &view);
// Games whose cabinet control the VR build can draw in front of you inside the game (ACVR_CONTROLS=1):
// the wheel of the driving games, Harley-Davidson's handlebars. Off unless the user asks for it.
bool hasCabinetControl(const QString &game);
QString cabinetControlName(const QString &game);
// The game's zip in a game folder: <folder>/<id>.zip, or for Sega Rally 2 DX its own folder beside it
// (../games-dx, with the parent set srally2.zip it needs) when it is not in the game folder itself.
QString gameFile(const QString &game,const QString &folder);
// The emulator's own name for the set in Config/Games.xml ("srally2x.zip" is Supermodel's "srally2dx").
QString setName(const QString &game);
// The folder holding Config/Games.xml: the program's own folder or one above it.
QString workingFolder(const QString &executable);
// Empty when the game can start; otherwise "What is wrong. What to do." The
// card shows the first sentence, the launch page both.
QString problem(const QString &game,const QString &executable,const QString &gameFolder);
// executable, cwd, args (QStringList) and env (name -> value) for one game and
// view. Read-only: nothing is created or changed. Throws install::Error.
QVariantMap launchPlan(const QString &game,const QString &view,const QString &executable,const QString &gameFolder);
}
