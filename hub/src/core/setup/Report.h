// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <functional>
namespace ac::scan{class DiscMemo;}
namespace ac::setup {
// What the user has for one system, in three groups:
//   ready    [{id,title,detail}]   the game, everything it needs and its program were found
//   notRight [{id,title,reason}]   found but unusable, with the reason in plain words
//   missing  [{id,title,expected}] in the Hub's list but not found; only the expected name is given
//   other    [{title,detail}]      found in the folder but not in the Hub's list, so nothing present is hidden
// plus id, title and an optional note. Scanning only reads the user's folders.
// When the context holds a Game library result ("library"), a report may also carry, for a system with a chosen folder:
//   unrecognised [{file,name,folder,size,kind,code,reason,detail?,system?}]  files in that folder the library looked at and
//                matched to nothing, with why in plain words (at most 50), unrecognisedMore (how many are not listed) and
//                unrecognisedChecked (false when the library did not look through the folder: the list then says nothing)
//   duplicates   [{title,game,count,sameDisc,kept,copies:[{file,name,size,kept,why}]}] games found in more than one file,
//                by disc identity, with the one kept (library::duplicateRule says how), and duplicateRule in words
// Nothing is read for these: they come from what the library result already holds.
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
// memo: the identities PS2, Dreamcast and Wii discs gave when last read; unchanged discs are not opened again, and
// what is read now is added to it.
QVariantMap scanSystem(const QString &system,const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel,scan::DiscMemo *memo=nullptr);
// What a Set-up report was made from: the report's version, the systems read, the settings it reads, and the
// times of each chosen folder and of each launcher's games.json (names and times only, nothing opened). Saved
// with the report; a saved report whose stamp differs from the one now is out of date and is read again.
QVariantMap reportStamp(const QVariantMap &settings,const QStringList &systems);
// What a folder holds for a system, found by reading a few of its files (never by their names): {folder, files (the
// system's kinds of file found in it and a level or two below), checked, matched, detail in words}. A disc is told by its
// own header, a table or cartridge by its magic bytes, an arcade set by its set name and a zip that opens. Cancellable.
QVariantMap folderCheck(const QString &system,const QString &folder,std::atomic_bool &cancel);
// Folders that probably hold the system's games, for a folder that is empty or wrong: looked for beside and above the
// chosen folders, in a "roms" folder there, in the library's roots and below the configured folder, and kept only when a
// few of the files in them are the system's by their contents. At most three, best first: [{folder, files, checked,
// matched, detail}]. The configured folder itself is left out. Reads files, so run it off the interface thread.
QVariantList suggestFolders(const QString &system,const QString &configured,const QStringList &knownFolders,const QStringList &libraryRoots,std::atomic_bool &cancel);
}
