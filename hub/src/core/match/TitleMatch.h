// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QList>
#include <QString>
#include <QStringList>
namespace ac::match {
// One tolerant way to compare game TITLES, shared by artwork lookup, library search and pinball
// table names. It never decides which disc a file is: disc identity is bytes read from the disc
// (see DiscIndex), and a file name never feeds this. Callers compare only titles that belong to
// the same system, so a game in one system never matches a title from another.
//
// Normalising a title: case, diacritics and punctuation go; "&" is "and"; roman numerals up to XX
// become digits (II = 2); "The"/"A"/"An" and ", The" go; bracketed tags like (USA), (Rev 1), [!]
// and (Disc 2) go; letters and digits are split apart ("kart7" is "kart 7").

// The words of a normalised title: lower case, no punctuation, numerals as digits.
QStringList titleTokens(const QString &title);
// The words joined without spaces. Empty when the title has nothing to compare.
QString titleKey(const QString &title);
// The key of the part before a subtitle (": ..." or " - ..."), and whether there was a subtitle.
QString mainTitleKey(const QString &title, bool *hasSubtitle = nullptr);

// How closely two titles agree, best last. Exact is the same text apart from case and accents;
// Same is the same title after normalising; Subtitle is a wanted title with a subtitle against a
// candidate that is just its main title ("Resident Evil 2: Dual Shock Ver." and "Resident Evil 2").
// Different numbers, extra words or a missing word never match: "Star Fox" is not "Star Fox
// Adventures", "Ridge Racer" is not "Ridge Racer V". No spelling slips here: those are for search.
enum class Quality { None, Subtitle, Same, Exact };
Quality compareTitles(const QString &wanted, const QString &candidate);
// The candidates (by index, in order) that match best, all at one quality of at least `minimum`.
// Empty when none reach it; `found` receives that quality.
QList<int> bestMatches(const QString &wanted, const QStringList &candidates, Quality minimum = Quality::Same,
                       Quality *found = nullptr);

// Spelling slips: the edit distance (insert, delete, change, swap neighbours) up to limit (limit + 1
// when further), and whether a typed word counts as the same word with a typo. Digits never differ
// and short words never differ. typedMayBeShort also accepts the start of a longer word.
int editDistance(const QString &a, const QString &b, int limit);
bool wordsMatchWithTypo(const QString &typed, const QString &word, bool typedMayBeShort);

// Library search: titles prepared once, then ranked against what was typed.
struct SearchKeys {
  QStringList keys;         // titleKey of each title
  QList<QStringList> words; // titleTokens of each title
};
SearchKeys searchKeys(const QStringList &titles);
// None: no hit. Typo: every typed word is a word of a title, the start of one, or one slip away.
// Contains: the typed text is inside a title ("kart7", "starfox"). Prefix: a title starts with it.
// Exact: it is a whole title.
enum class SearchRank { None, Typo, Contains, Prefix, Exact };
SearchRank searchRank(const SearchKeys &keys, const QString &typed);
} // namespace ac::match
