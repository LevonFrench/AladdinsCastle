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
// settings: the Hub's own settings values (folders and programs per system).
// context: data a system needs beside them, such as a saved table scan.
QVariantMap scanSystem(const QString &system,const QVariantMap &settings,const QVariantMap &context,const Progress &progress,std::atomic_bool &cancel);
}
