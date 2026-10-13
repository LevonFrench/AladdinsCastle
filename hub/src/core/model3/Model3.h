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
QVariantList views(); // id, label, help
QString viewLabel(const QString &view);
// The folder holding Config/Games.xml: the program's own folder or one above it.
QString workingFolder(const QString &executable);
// Empty when the game can start; otherwise "What is wrong. What to do." The
// card shows the first sentence, the launch page both.
QString problem(const QString &game,const QString &executable,const QString &gameFolder);
// executable, cwd, args (QStringList) and env (name -> value) for one game and
// view. Read-only: nothing is created or changed. Throws install::Error.
QVariantMap launchPlan(const QString &game,const QString &view,const QString &executable,const QString &gameFolder);
}
