// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QVariantMap>
#include <atomic>
#include <functional>
namespace ac::setup {
// What the user has for one system, in three groups:
//   ready    [{id,title,detail}]   the game, everything it needs and its program were found
//   notRight [{id,title,reason}]   found but unusable, with the reason in plain words
//   missing  [{id,title,expected}] in the Hub's list but not found; only the expected name is given
//   other    [{title,detail}]      found in the folder but not in the Hub's list, so nothing present is hidden
// plus id, title and an optional note. Scanning only reads the user's folders.
using Progress=std::function<void(const QString &)>;
// The files a game needs according to a Supermodel Games.xml: crc -> file name,
// with the parent set's files where the game does not replace them. "parent" is
// set when there is one. Empty when the game is not listed.
QVariantMap supermodelFiles(const QString &gamesXml,const QString &game);
// One game checked against a MAME build's own "-listxml" output for it: {ok,
// reason, standIns}. The game's files are looked for by checksum in <folder>/
// <game>.zip and its parent's zip; each device set that has files of its own, in
// the game's zip, in <folder>/<device>.zip or in a folder of that name. A device
// in "cosmetic" whose files are present but different is accepted and named in
// standIns; for any other device that is a reason. Read-only.
QVariantMap mameCheck(const QByteArray &listXml,const QString &game,const QString &folder,const QStringList &cosmetic,std::atomic_bool &cancel);
// settings: the Hub's own settings values (folders and programs per system).
// context: data a system needs beside them, such as a saved table scan.
QVariantMap scanSystem(const QString &system,const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel);
// What a Set-up report was made from: the report's version, the systems read, the settings it reads, and the
// times of each chosen folder and of each launcher's games.json (names and times only, nothing opened). Saved
// with the report; a saved report whose stamp differs from the one now is out of date and is read again.
QVariantMap reportStamp(const QVariantMap &settings,const QStringList &systems);
}
