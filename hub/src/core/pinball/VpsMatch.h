// SPDX-License-Identifier: GPL-3.0-only
// Ported from Visual Pinball Infinite's engine/library/src/matcher.rs
// (https://github.com/VPinInfinite/VisualPinballInfinite), itself a port of VPin Studio's
// vpin-connector-vps matcher (https://github.com/syd711/vpin-studio): TableNameSplitter,
// TableMatcher, TableVersionMatcher and VpsAutomatcher, with the cosine and Jaccard distances of
// Apache Commons Text. Copyright (c) 2022 syd711, MIT licence: hub/packaging/licenses/VPinStudio-MIT.txt.
//
// A table matched to VPS's database by distance: its file name split into name, maker, year,
// version and the rest; the closest game by name, maker and year; then the closest of the game's
// table files by the table's own name, authors and version. VPI's additions are kept: a game
// further than VPin Studio's confirming distance is looked for again by the table info's name,
// then by the ROM its script names; versions compare with the dots aside (1.13 is 1.1.3); the file
// name's authors count beside the table info's; authors match with spaces and digits aside.
// VPin Studio's older, looser matcher is left out: the Hub never takes what it would find.
// This decides nothing alone: Identity.cpp's exact rules come first and guard what it finds.
#pragma once
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>
namespace ac::pinball::vps {
struct TableFile {QString id,version,comment,format;QStringList authors,features;qint64 createdAt=0;};
struct Game {qsizetype index=-1;QString id,name,manufacturer;int year=0;QList<TableFile> tableFiles;QStringList roms;};
// The games of a VPS database array (vpsdb.json), each with its index in the array.
std::vector<Game> games(const QJsonArray &db);

// The parts of a table's file name, "Name (Maker Year) extra" (VPin Studio's TableNameParts).
struct NameParts {QString displayName;std::optional<QString> tableName,manufacturer,extra,version;int year=-1;};
NameParts parseFilename(const QString &fileName);
std::optional<QString> extractVersion(const QString &text);
QString cleanChars(const QString &text);
double cosineDistance(const QString &a,const QString &b);
double jaccardDistance(const QString &a,const QString &b);
double nameDistance(const QString &a,const QString &b);
double versionDistance(const QString &theirs,const QString &ours);
QString plainVersion(const QString &version);
bool sameAuthor(const QString &ours,const QString &theirs);
// The file name says something of the game: its first word, or its initials ("TOTAN").
bool relates(const QString &fileName,const Game &game);
// What's left of a name once the game's own name, maker and year are taken out, in words.
QString residue(const QString &text,const Game &game);

struct Scored {const Game *game=nullptr;double distance=0;};
// The VPX games closest to a parsed name, nearest first (VPin Studio's findAllClosest).
QList<Scored> closestGames(const NameParts &parts,const std::vector<Game> &games);

// What a game's table files are weighed against.
struct FileQuery {QString name;std::optional<QString> infoAuthors,version;QString nameAuthors;bool versionFromName=false;std::optional<qint64> made;};
struct FileScore {const TableFile *file=nullptr;double name=0,authors=0,version=0,total=0;};
QList<FileScore> tableFileScores(const Game &game,const FileQuery &q);
// The game's table file closest to the table (VPin Studio's TableVersionMatcher.findVersion), with
// its distances; a game's only file comes back with them too, so a caller can ask more of it.
std::optional<FileScore> closestTableFile(const Game &game,const FileQuery &q);

// What a table file says about itself: its file name, its table info's name, authors and version,
// the day it was made and the ROM its script names.
struct Clues {QString fileName;std::optional<QString> infoName,infoAuthors,infoVersion,rom;std::optional<qint64> made;};
enum class Found {None,ByFileName,ByTableInfo,ByRom,Guess};
// Sure enough for VPin Studio to take without asking.
bool sure(Found found);
QString describe(Found found);
struct Match {
 const Game *game=nullptr;Found found=Found::None;double distance=0;
 // Another game was exactly as near: the name alone cannot tell them apart (the Hub's addition).
 bool tied=false;
 NameParts parts;FileQuery asked;std::optional<FileScore> tableFile;
};
// What the game's table files are weighed against for this table (the second half of autoMatch).
FileQuery fileQuery(const Game &game,const Clues &clues,NameParts &parts);
// VPin Studio's VpsAutomatcher.autoMatch with VPI's additions: the game, then its table file.
Match matchTable(const std::vector<Game> &games,const Clues &clues);
}
