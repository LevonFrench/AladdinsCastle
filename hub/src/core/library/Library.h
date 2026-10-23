// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <functional>
namespace ac::library {
// "Game library": one or more big folders the user points at, searched below for every game the Hub
// plays. Read-only: nothing under the roots is written, renamed or moved. Games are known by what is
// inside the files where that can be read (a Wii disc's game id, a PS2 disc's serial, a Dreamcast
// disc's product number), and arcade sets by their zip's set name, which Set-up then checks by CRC.
//
// The result, kept in the Hub's cache:
//   roots, scanned (date), seconds, files (looked at), depth
//   systems: { system: { copies: { game: [ {file, game, title, identity, region, verified, ...} ] },
//                        games (how many listed games were found), other (discs of the system not in
//                        the Hub's list) } }
//   pinballFolders: [ {folder, tables} ] (folders holding .vpx tables, most first)
//   unrecognised: { discs, archives } (files of a kind the Hub reads that are none of its games)
//   reused (files whose last reading was used again), cancelled (stopped part-way: the result is partial)
//
// With an index, a search is quick the second time: a file whose size and time are unchanged since the last
// reading is not opened again. The index ({stamp, files: {path: {size, modified, outcome}}}) is read and
// replaced; one made against other lists of games is not used. Stopped part-way, the files not reached keep
// their last reading.
using Progress = std::function<void(const QString &)>;
QVariantMap scan(const QStringList &roots, const Progress &progress, std::atomic_bool &cancel, int depth = 6, QVariantMap *index = nullptr);
// The copy of a game to use: the user's own choice ("system/game" -> file) when it is still there,
// otherwise by the rule: read from the disc itself, then the release with a VR profile (Dreamcast),
// then USA, World, Europe, Japan, then the plainest format, then the shortest path.
QVariantMap chosen(const QVariantMap &library, const QString &system, const QString &game, const QVariantMap &choices);
QVariantList copies(const QVariantMap &library, const QString &system, const QString &game);
// The rule alone, said in words for the game's page.
QString rule();
} // namespace ac::library
