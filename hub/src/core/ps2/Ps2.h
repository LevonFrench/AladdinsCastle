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
// The games whose launcher entry has an inside profile, so it can open the game around you: the one
// list, in Ps2.cpp, filled in as each is confirmed. Empty until then.
QStringList insideGames();
// The views a game can be played in, each {id, label, help}: 3D screen always, and Inside the game for a
// game in the inside list that the launcher knows. The list is given only by tests.
QVariantList views(const QString &game,const QString &launcher,const QStringList &inside=insideGames());
// Empty when the game can start; otherwise "What is wrong. What to do."
QString problem(const QString &game,const QString &launcher,const QString &python,const QString &stopFile);
// executable (the Python), cwd (the launcher's folder) and args (launcher, key, and --inside for the
// Inside view). Throws install::Error, also for Inside on a game the launcher has no inside profile for.
QVariantMap launchPlan(const QString &game,const QString &launcher,const QString &python,const QString &stopFile,const QString &view=QStringLiteral("screen"),
                       const QStringList &inside=insideGames());
// "3D screen" or "Inside the game".
QString viewLabel(const QString &view);
}
