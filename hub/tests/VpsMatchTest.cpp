// SPDX-License-Identifier: GPL-3.0-only
// The ported VPin Studio / VPI distance matcher on its own, with the cases VPI's matcher.rs tests
// for, so the port can be checked against the source. Synthetic names only; no VPS data.
// How the Hub guards what it finds is in pinball_identity_fixtures.py.
#ifdef _MSC_VER
// MSVC's optimiser reports Qt's inlined qstringalgorithms.h as unreachable code (C4702) in this test only.
#pragma warning(disable : 4702)
#endif
#include "core/pinball/VpsMatch.h"
#include <QtTest>
#include <cmath>
using namespace ac::pinball::vps;
namespace {
Game game(const QString &name, const QString &maker, int year, const QStringList &roms = {}) {
    Game g;
    g.name = name;
    g.manufacturer = maker;
    g.year = year;
    g.roms = roms;
    TableFile t;
    t.format = "VPX";
    g.tableFiles << t;
    return g;
}
bool near(double a, double b) { return std::abs(a - b) < 1e-12; }
} // namespace
class VpsMatchTest : public QObject {
    Q_OBJECT
  private slots:
    void splitsNamesAsVpinStudio() {
        const auto p = parseFilename("Attack from Mars (Bally 1995) JPSalas 6.0.0");
        QCOMPARE(p.tableName.value_or("-"), QString("Attack from Mars "));
        QCOMPARE(p.manufacturer.value_or("-"), QString("Bally"));
        QCOMPARE(p.year, 1995);
        QCOMPARE(p.extra.value_or("-"), QString("JPSalas"));
        QCOMPARE(p.version.value_or("-"), QString("6.0.0"));
        const auto q = parseFilename("Tron Legacy (Stern 2011) VPW Mod v1.1");
        QCOMPARE(q.version.value_or("-"), QString("v1.1"));
        QCOMPARE(q.extra.value_or("-"), QString("VPW Mod"));
        // A reference to VPX's version and features in brackets are taken out.
        QCOMPARE(parseFilename("Twister (Sega 1996) VPX10.7 [FSS] v2.0").version.value_or("-"), QString("v2.0"));
        // No maker: the name and the year.
        const auto s = parseFilename("Bad Cats 1989 v3");
        QCOMPARE(s.tableName.value_or("-"), QString("Bad Cats"));
        QCOMPARE(s.year, 1989);
        // The year before the maker, and underscores.
        const auto u = parseFilename("Orbit_(1999_Williams)_v1.2");
        QCOMPARE(u.manufacturer.value_or("-"), QString("Williams"));
        QCOMPARE(u.year, 1999);
    }
    void distancesAsCommonsText() {
        QVERIFY(std::abs(cosineDistance("attack from mars", "attack from mars")) < 1e-12);
        QVERIFY(near(cosineDistance("a b", "a c"), 0.5));
        QCOMPARE(jaccardDistance("abc", "abc"), 0.0);
        QVERIFY(near(jaccardDistance("ab", "bc"), 1.0 - 1.0 / 3.0));
        QCOMPARE(nameDistance("Attack from Mars", "attack from mars"), 0.0);
        // One inside the other: at most half the share left over, less when the words say so.
        const double cd = 1.0 - 2.0 / (2.0 * std::sqrt(2.0)), jd = 1.0 - 10.0 / 15.0;
        QVERIFY(near(nameDistance("terminator 2 judgment day", "terminator 2"), std::min((1.0 - 12.0 / 25.0) / 2.0, cd * jd)));
    }
    void versionsAsVpinStudio() {
        QCOMPARE(versionDistance("1.2", "1.2"), 0.0);
        // The table older than VPS's file costs more.
        QCOMPARE(versionDistance("1.3", "1.2"), 0.1);
        QCOMPARE(versionDistance("1.2", "1.3"), 2.0);
        QCOMPARE(versionDistance("2.0", "2.0b"), 0.2);
        QCOMPARE(versionDistance("VPX10.8 1.2", "1.2"), 0.0);
    }
    void vpiAdditions() {
        QCOMPARE(plainVersion("v1.1.3"), plainVersion("1.13"));
        QCOMPARE(plainVersion("1.0.2"), plainVersion("1.02"));
        QCOMPARE(plainVersion("4.0.0"), plainVersion("4.0"));
        QVERIFY(plainVersion("1.1.0") != plainVersion("1.10"));
        QVERIFY(sameAuthor("clark kent", "ClarkKent"));
        QVERIFY(sameAuthor("javier", "Javier15"));
        QVERIFY(!sameAuthor("tom", "Tomate"));
        const auto taxi = game("Taxi", "Williams", 1988);
        QCOMPARE(residue("Taxi (Williams 1988)", taxi), QString());
        QCOMPARE(residue("Mod of Taxi (Williams 1988) by VPW", taxi), QString("mod of by vpw"));
        QVERIFY(relates("TOTAN4k 1.5", game("Tales of the Arabian Nights", "Williams", 1996)));
        QVERIFY(relates("TimeLord-4.1a", game("Time Lord", "Original", 2022)));
        QVERIFY(relates("tz_94h", game("Twilight Zone", "Bally", 1993)));
        QVERIFY(!relates("JP's Arcade Physics v3.01 Rubber objects", game("Count-Down", "Gottlieb", 1979)));
    }
    void findsGamesByNameTableInfoAndRom() {
        std::vector<Game> all{game("Star Trek", "Bally", 1979), game("Star Trek", "Stern", 2013),
                              game("Twilight Zone", "Bally", 1993, {"tz_94h"}), game("Comet", "Bally", 1962),
                              game("Comet", "Williams", 1985)};
        Clues byName;
        byName.fileName = "JPs Star Trek (Bally 1979)";
        auto m = matchTable(all, byName);
        QCOMPARE(m.found, Found::ByFileName);
        QVERIFY(m.game == &all[0]);
        QVERIFY(!m.tied);
        // The script's ROM, which VPS lists for the game the file name's initials speak of.
        Clues byRom;
        byRom.fileName = "tz_94h_mod";
        byRom.rom = "tz_94h";
        m = matchTable(all, byRom);
        QCOMPARE(m.found, Found::ByRom);
        QVERIFY(m.game == &all[2]);
        // Two games exactly as near: the Hub's tie mark.
        Clues tie;
        tie.fileName = "Comet";
        m = matchTable(all, tie);
        QCOMPARE(m.found, Found::ByFileName);
        QVERIFY(m.tied);
        // A file name that says nothing of a game is at most a guess.
        Clues none;
        none.fileName = "zzqx";
        QVERIFY(!sure(matchTable(all, none).found));
        // A placeholder table info name is not looked up.
        Clues placeholder;
        placeholder.fileName = "zzqx";
        placeholder.infoName = "Table1";
        QVERIFY(!sure(matchTable(all, placeholder).found));
    }
    void picksTheTableFileByAuthorsAndVersion() {
        Game g = game("Comet", "Bally", 1962);
        g.tableFiles.clear();
        TableFile mine, other;
        mine.format = other.format = "VPX";
        mine.authors = {"ClarkKent"};
        mine.version = "1.1.3";
        other.authors = {"Somebody"};
        other.version = "2.0";
        g.tableFiles << mine << other;
        Clues c;
        c.fileName = "Comet (Bally 1962)";
        c.infoAuthors = "Clark Kent";
        c.infoVersion = "1.13";
        auto parts = parseFilename(c.fileName);
        const auto score = closestTableFile(g, fileQuery(g, c, parts));
        QVERIFY(score.has_value());
        QVERIFY(score->file == &g.tableFiles[0]);
        QCOMPARE(score->authors, 0.0);
        QCOMPARE(score->version, 0.0);
        // The file name's version picks the one file that has it, whatever the table info says.
        Clues named = c;
        named.fileName = "Comet (Bally 1962) v2.0";
        auto namedParts = parseFilename(named.fileName);
        const auto byName = closestTableFile(g, fileQuery(g, named, namedParts));
        QVERIFY(byName.has_value());
        QVERIFY(byName->file == &g.tableFiles[1]);
    }
};
QTEST_GUILESS_MAIN(VpsMatchTest)
#include "VpsMatchTest.moc"
