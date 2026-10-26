// SPDX-License-Identifier: GPL-3.0-only
#include "TitleMatch.h"
#include <QHash>
#include <QRegularExpression>
#include <algorithm>
namespace ac::match {
namespace {
// Accents come off Latin, Greek and Cyrillic letters; kana and hangul keep their marks, because
// dropping a dakuten would turn one kana into another.
bool accentsDrop(QChar base) {
  const auto u = base.unicode();
  return u < 0x0250 || (u >= 0x0370 && u <= 0x052f) || (u >= 0x1e00 && u <= 0x1fff);
}
QString folded(QString s) {
  s = s.normalized(QString::NormalizationForm_D).toCaseFolded();
  QString out;
  out.reserve(s.size());
  QChar base;
  for (const QChar c : s) {
    const auto category = c.category();
    if (category == QChar::Mark_NonSpacing || category == QChar::Mark_SpacingCombining ||
        category == QChar::Mark_Enclosing) {
      if (!accentsDrop(base))
        out += c;
      continue;
    }
    base = c;
    switch (c.unicode()) {
    case 0x00e6: out += "ae"; break; // ae ligature
    case 0x0153: out += "oe"; break; // oe ligature
    case 0x00f8: out += 'o'; break;  // o with stroke
    case 0x0142: out += 'l'; break;  // l with stroke
    case 0x0111: out += 'd'; break;  // d with stroke
    case 0x00df: out += "ss"; break; // sharp s
    case 0x2018: case 0x2019: case 0x02bc: case 0x00b4: out += '\''; break;
    default: out += c;
    }
  }
  return out.normalized(QString::NormalizationForm_C);
}
// Lower case, no tags, "&" as "and". What is left still has its ":" and " - " so a subtitle shows.
QString prepare(QString s) {
  s = folded(s);
  static const QRegularExpression tags("\\([^)]*\\)|\\[[^\\]]*\\]");
  auto bare = s;
  bare.remove(tags);
  bool letters = false;
  for (const QChar c : bare)
    letters |= c.isLetterOrNumber();
  if (letters)
    s = bare;
  else // a title that is only a tag, such as "(1942)": keep what is inside
    s.remove('(').remove(')').remove('[').remove(']');
  // libretro writes "_" for "&" (and for ":", "/" and a few more): a lone "_" is "and", any other is a space.
  static const QRegularExpression lone("(^|\\s)_(?=\\s|$)");
  s.replace(lone, "\\1 and ");
  s.replace('_', ' ');
  s.replace('&', " and ");
  return s.simplified();
}
// Where a subtitle starts: the first ":" or a spaced dash. skip is how much separator to drop.
qsizetype subtitleAt(const QString &s, qsizetype *skip) {
  qsizetype at = s.indexOf(':');
  *skip = 1;
  for (const QChar dash : {QChar('-'), QChar(0x2013), QChar(0x2014)}) {
    const auto d = s.indexOf(QString(" ") + dash + " ");
    if (d >= 0 && (at < 0 || d < at)) {
      at = d;
      *skip = 3;
    }
  }
  return at;
}
QString dropArticles(QString s) {
  s = s.simplified();
  static const QRegularExpression trailing(",\\s*(the|a|an)$");
  s.remove(trailing);
  static const QRegularExpression first("^(the|a|an)\\s+");
  s.remove(first);
  return s;
}
int roman(const QString &t) {
  static const QHash<QString, int> table{
      {"ii", 2},     {"iii", 3},  {"iv", 4},    {"vi", 6},   {"vii", 7},   {"viii", 8}, {"ix", 9},
      {"xi", 11},    {"xii", 12}, {"xiii", 13}, {"xiv", 14}, {"xv", 15},   {"xvi", 16}, {"xvii", 17},
      {"xviii", 18}, {"xix", 19}, {"xx", 20}};
  return table.value(t, 0);
}
// Words of a segment: apostrophes vanish inside a word, other marks split, letters and digits split.
// A lone "v" (or a final "i") after the first word is a numeral; a lone "x" never is ("Mega Man X").
QStringList words(const QString &segment, bool notFirst) {
  QStringList out;
  QString word;
  int kind = 0; // 1 letter, 2 digit
  const auto flush = [&] {
    if (!word.isEmpty())
      out << word;
    word.clear();
    kind = 0;
  };
  for (const QChar c : segment) {
    if (c == '\'')
      continue;
    const int k = c.isLetter() ? 1 : c.isNumber() ? 2 : 0;
    if (!k || (kind && k != kind))
      flush();
    if (k) {
      word += c;
      kind = k;
    }
  }
  flush();
  for (qsizetype i = 0; i < out.size(); ++i) {
    const bool later = notFirst || i > 0;
    if (const int n = roman(out[i]))
      out[i] = QString::number(n);
    else if (out[i] == "v" && later)
      out[i] = "5";
    else if (out[i] == "i" && later && i == out.size() - 1)
      out[i] = "1";
  }
  return out;
}
struct Parts {
  QStringList main, sub;
  QString exact;
};
Parts parts(const QString &title) {
  Parts p;
  p.exact = folded(title).simplified();
  const auto s = prepare(title);
  qsizetype skip = 1;
  const auto at = subtitleAt(s, &skip);
  const auto main = at < 0 ? s : s.left(at), sub = at < 0 ? QString() : s.mid(at + skip);
  p.main = words(dropArticles(main), false);
  p.sub = words(dropArticles(sub), !p.main.isEmpty());
  return p;
}
} // namespace

QStringList titleTokens(const QString &title) {
  const auto p = parts(title);
  return p.main + p.sub;
}
QString titleKey(const QString &title) { return titleTokens(title).join(QString()); }
QString mainTitleKey(const QString &title, bool *hasSubtitle) {
  const auto p = parts(title);
  if (hasSubtitle)
    *hasSubtitle = !p.sub.isEmpty() && !p.main.isEmpty();
  return p.main.join(QString());
}

int editDistance(const QString &a, const QString &b, int limit) {
  const auto n = a.size(), m = b.size();
  if (qAbs(n - m) > limit)
    return limit + 1;
  QList<int> previous2(m + 1), previous(m + 1), row(m + 1);
  for (qsizetype j = 0; j <= m; ++j)
    previous[j] = int(j);
  for (qsizetype i = 1; i <= n; ++i) {
    row[0] = int(i);
    int best = row[0];
    for (qsizetype j = 1; j <= m; ++j) {
      const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
      row[j] = std::min({previous[j] + 1, row[j - 1] + 1, previous[j - 1] + cost});
      if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
        row[j] = std::min(row[j], previous2[j - 2] + 1);
      best = std::min(best, row[j]);
    }
    if (best > limit)
      return limit + 1;
    previous2 = previous;
    previous = row;
  }
  return previous[m];
}
bool wordsMatchWithTypo(const QString &typed, const QString &word, bool typedMayBeShort) {
  if (typed == word)
    return true;
  if (typed.isEmpty() || word.isEmpty())
    return false;
  for (const QChar c : typed + word)
    if (!c.isLetter())
      return false; // numbers must be exact
  const auto size = std::min(typed.size(), word.size());
  if (size < 4)
    return false;
  const int budget = size >= 9 ? 2 : 1;
  // Slips on the first letter are rare; allow them only in long words.
  if (size < 8 && typed[0] != word[0])
    return false;
  if (editDistance(typed, word, budget) <= budget)
    return true;
  // A word still being typed: its start, with a slip in it.
  return typedMayBeShort && typed.size() >= 5 && typed.size() < word.size() &&
         editDistance(typed, word.left(typed.size()), budget) <= budget;
}

Quality compareTitles(const QString &wanted, const QString &candidate) {
  const auto w = parts(wanted), c = parts(candidate);
  const auto wantedWords = w.main + w.sub, candidateWords = c.main + c.sub;
  if (wantedWords.isEmpty() || candidateWords.isEmpty())
    return Quality::None;
  if (!w.exact.isEmpty() && w.exact == c.exact)
    return Quality::Exact;
  if (wantedWords.join(QString()) == candidateWords.join(QString()))
    return Quality::Same;
  if (!w.sub.isEmpty() && c.sub.isEmpty() && w.main == candidateWords)
    return Quality::Subtitle;
  return Quality::None;
}
QList<int> bestMatches(const QString &wanted, const QStringList &candidates, Quality minimum, Quality *found) {
  QList<int> best;
  Quality quality = Quality::None;
  for (qsizetype i = 0; i < candidates.size(); ++i) {
    const auto q = compareTitles(wanted, candidates[i]);
    if (q == Quality::None || q < minimum)
      continue;
    if (q > quality) {
      quality = q;
      best.clear();
    }
    if (q == quality)
      best << int(i);
  }
  if (found)
    *found = quality;
  return best;
}

SearchKeys searchKeys(const QStringList &titles) {
  SearchKeys out;
  for (const auto &title : titles) {
    const auto w = titleTokens(title);
    if (w.isEmpty())
      continue;
    out.keys << w.join(QString());
    out.words << w;
  }
  return out;
}
SearchRank searchRank(const SearchKeys &keys, const QString &typed) {
  const auto want = titleTokens(typed);
  const auto key = want.join(QString());
  if (key.isEmpty())
    return SearchRank::None;
  SearchRank best = SearchRank::None;
  for (qsizetype i = 0; i < keys.keys.size(); ++i) {
    const auto &title = keys.keys[i];
    if (title == key)
      return SearchRank::Exact;
    if (title.startsWith(key))
      best = std::max(best, SearchRank::Prefix);
    else if (title.contains(key))
      best = std::max(best, SearchRank::Contains);
    if (best >= SearchRank::Contains)
      continue;
    bool all = true;
    for (const auto &t : want) {
      bool found = false;
      for (const auto &word : keys.words[i])
        if (word == t || (t.size() >= 2 && word.startsWith(t)) || wordsMatchWithTypo(t, word, true)) {
          found = true;
          break;
        }
      if (!found) {
        all = false;
        break;
      }
    }
    if (all)
      best = std::max(best, SearchRank::Typo);
  }
  return best;
}
} // namespace ac::match
