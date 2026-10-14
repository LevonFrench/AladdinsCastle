// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
namespace ac::ps2 {
// PlayStation 2 games through the user's own stereo PCSX2 build and its launcher
// script, which holds each game's settings and disc copy and is started with the
// game's key. Only public titles, keys and disc serials are listed here; every
// location comes from Hub settings.
//   ps2Launcher  the launcher script        ps2Python    the Python that runs it
//   ps2StopFile  the file whose appearance asks the running game to quit
//   ps2GameFolder  the user's own PS2 disc folder, read for the report
QVariantList games(); // id (the launcher's key), title, year, controls (a plain sentence)
// The listed game a disc serial belongs to (such as "SLES-51229"), or empty.
QString gameForSerial(const QString &serial);
// What one disc image is: {file, serial, game, title, verified}. Read-only.
QVariantMap identify(const QString &file,std::atomic_bool &cancel);
// The disc images directly inside a folder, identified. Subfolders are not entered.
QVariantList discs(const QString &folder,std::atomic_bool &cancel);
// The keys the user's launcher knows, from the games.json beside it. Read-only.
QStringList launcherKeys(const QString &launcher);
// Empty when the game can start; otherwise "What is wrong. What to do."
QString problem(const QString &game,const QString &launcher,const QString &python,const QString &stopFile);
// executable (the Python), cwd (the launcher's folder) and args (launcher, key). Throws install::Error.
QVariantMap launchPlan(const QString &game,const QString &launcher,const QString &python,const QString &stopFile);
}
