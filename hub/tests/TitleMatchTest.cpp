// SPDX-License-Identifier: GPL-3.0-only
// The shared tolerant TITLE matcher: table-driven, with many near misses that must not match.
// It compares titles only. Disc identity never comes from a file name (see DiscIndexTest and
// LibraryTest); nothing here reads a file, a disc or the network.
#include "core/match/TitleMatch.h"
#include <QtTest>
using ac::match::Quality;
using ac::match::SearchRank;
namespace {
using Q = Quality;
using R = SearchRank;
} // namespace
class TitleMatchTest : public QObject {
    Q_OBJECT
  private slots:
    // Two titles and how closely they agree. "NOT" rows are near misses: a different game.
    void compare_data() {
        QTest::addColumn<QString>("wanted");
        QTest::addColumn<QString>("candidate");
        QTest::addColumn<Quality>("expected");
        QTest::newRow("case: mario kart 7 | MARIO KART 7") << QString(u"mario kart 7") << QString(u"MARIO KART 7") << Q::Exact;
        QTest::newRow("case and region: Mario Kart 7 | mario kart 7 (USA)") << QString(u"Mario Kart 7") << QString(u"mario kart 7 (USA)") << Q::Same;
        QTest::newRow("punctuation: Pac-Man | Pacman") << QString(u"Pac-Man") << QString(u"Pacman") << Q::Same;
        QTest::newRow("punctuation: Dr. Mario | Dr Mario") << QString(u"Dr. Mario") << QString(u"Dr Mario") << Q::Same;
        QTest::newRow("apostrophe: Don't Starve | Dont Starve") << QString(u"Don't Starve") << QString(u"Dont Starve") << Q::Same;
        QTest::newRow("curly apostrophe: Don_t Starve | Don't Starve") << QString(u"Don\u2019t Starve") << QString(u"Don't Starve") << Q::Exact;
        QTest::newRow("underscore: Foo_ Bar | Foo: Bar") << QString(u"Foo_ Bar") << QString(u"Foo: Bar") << Q::Same;
        QTest::newRow("roman: Time Crisis II | Time Crisis 2") << QString(u"Time Crisis II") << QString(u"Time Crisis 2") << Q::Same;
        QTest::newRow("roman 7: Ridge Racer VII | Ridge Racer 7 (USA)") << QString(u"Ridge Racer VII") << QString(u"Ridge Racer 7 (USA)") << Q::Same;
        QTest::newRow("roman v: Ridge Racer V | Ridge Racer 5") << QString(u"Ridge Racer V") << QString(u"Ridge Racer 5") << Q::Same;
        QTest::newRow("roman xiii: Final Fantasy XIII | Final Fantasy 13") << QString(u"Final Fantasy XIII") << QString(u"Final Fantasy 13") << Q::Same;
        QTest::newRow("roman mid: Ridge Racer V Arcade Battle | Ridge Racer 5 Arcade Battle") << QString(u"Ridge Racer V Arcade Battle") << QString(u"Ridge Racer 5 Arcade Battle") << Q::Same;
        QTest::newRow("the: The House of the Dead 2 | House of the Dead 2, The (USA)") << QString(u"The House of the Dead 2") << QString(u"House of the Dead 2, The (USA)") << Q::Same;
        QTest::newRow("the sub: Legend of Zelda, The - Ocarina of Time | The Legend of Zelda: Ocarina of Time") << QString(u"Legend of Zelda, The - Ocarina of Time") << QString(u"The Legend of Zelda: Ocarina of Time") << Q::Same;
        QTest::newRow("a: A Bug's Life | Bugs Life") << QString(u"A Bug's Life") << QString(u"Bugs Life") << Q::Same;
        QTest::newRow("revision: Virtua Cop 2 (Rev 1) | Virtua Cop 2 [!]") << QString(u"Virtua Cop 2 (Rev 1)") << QString(u"Virtua Cop 2 [!]") << Q::Same;
        QTest::newRow("disc: Final Fantasy VII (Disc 1) | Final Fantasy 7 (Disc 2)") << QString(u"Final Fantasy VII (Disc 1)") << QString(u"Final Fantasy 7 (Disc 2)") << Q::Same;
        QTest::newRow("ampersand: Ghosts & Goblins | Ghosts and Goblins (World)") << QString(u"Ghosts & Goblins") << QString(u"Ghosts and Goblins (World)") << Q::Same;
        QTest::newRow("diacritics: Pok_mon Red | Pokemon Red (USA, Europe)") << QString(u"Pok\u00e9mon Red") << QString(u"Pokemon Red (USA, Europe)") << Q::Same;
        QTest::newRow("diacritics: Caf_ Racer | Cafe Racer") << QString(u"Caf\u00e9 Racer") << QString(u"Cafe Racer") << Q::Exact;
        QTest::newRow("sharp s: Stra_e | Strasse") << QString(u"Stra\u00dfe") << QString(u"Strasse") << Q::Exact;
        QTest::newRow("spacing: Mariokart 7 | Mario Kart 7") << QString(u"Mariokart 7") << QString(u"Mario Kart 7") << Q::Same;
        QTest::newRow("digit split: R4 Ridge Racer Type 4 | R 4 Ridge Racer Type 4") << QString(u"R4 Ridge Racer Type 4") << QString(u"R 4 Ridge Racer Type 4") << Q::Same;
        QTest::newRow("sub dash: Resident Evil 2 - Dual Shock Ver. | Resident Evil 2: Dual Shock Ver.") << QString(u"Resident Evil 2 - Dual Shock Ver.") << QString(u"Resident Evil 2: Dual Shock Ver.") << Q::Same;
        QTest::newRow("cjk: ________ 64 | ________ 64 (Japan)") << QString(u"\u30b9\u30bf\u30fc\u30d5\u30a9\u30c3\u30af\u30b9 64") << QString(u"\u30b9\u30bf\u30fc\u30d5\u30a9\u30c3\u30af\u30b9 64 (Japan)") << Q::Same;
        QTest::newRow("subtitle: Resident Evil 2: Dual Shock Ver. | Resident Evil 2") << QString(u"Resident Evil 2: Dual Shock Ver.") << QString(u"Resident Evil 2") << Q::Subtitle;
        QTest::newRow("subtitle dash: Resident Evil Survivor 2 - Code: Veronica | Resident Evil Survivor 2 (USA)") << QString(u"Resident Evil Survivor 2 - Code: Veronica") << QString(u"Resident Evil Survivor 2 (USA)") << Q::Subtitle;
        QTest::newRow("subtitle comma the: Legend of Zelda, The: Majora's Mask | The Legend of Zelda (USA)") << QString(u"Legend of Zelda, The: Majora's Mask") << QString(u"The Legend of Zelda (USA)") << Q::Subtitle;
        QTest::newRow("NOT sequel: Star Fox | Star Fox Adventures") << QString(u"Star Fox") << QString(u"Star Fox Adventures") << Q::None;
        QTest::newRow("NOT sequel: Star Fox Adventures | Star Fox") << QString(u"Star Fox Adventures") << QString(u"Star Fox") << Q::None;
        QTest::newRow("NOT sequel: Ridge Racer | Ridge Racer V") << QString(u"Ridge Racer") << QString(u"Ridge Racer V") << Q::None;
        QTest::newRow("NOT sequel: Ridge Racer V | Ridge Racer") << QString(u"Ridge Racer V") << QString(u"Ridge Racer") << Q::None;
        QTest::newRow("NOT number: Ridge Racer 2 | Ridge Racer 7") << QString(u"Ridge Racer 2") << QString(u"Ridge Racer 7") << Q::None;
        QTest::newRow("NOT number: Mario Kart 7 | Mario Kart 64") << QString(u"Mario Kart 7") << QString(u"Mario Kart 64") << Q::None;
        QTest::newRow("NOT number: Time Crisis 2 | Time Crisis 3") << QString(u"Time Crisis 2") << QString(u"Time Crisis 3") << Q::None;
        QTest::newRow("NOT number: Final Fantasy VII | Final Fantasy VIII") << QString(u"Final Fantasy VII") << QString(u"Final Fantasy VIII") << Q::None;
        QTest::newRow("NOT number: Resident Evil 4 | Resident Evil 5") << QString(u"Resident Evil 4") << QString(u"Resident Evil 5") << Q::None;
        QTest::newRow("NOT X is 10: Mega Man X | Mega Man 10") << QString(u"Mega Man X") << QString(u"Mega Man 10") << Q::None;
        QTest::newRow("NOT X is 10: Mega Man 10 | Mega Man X") << QString(u"Mega Man 10") << QString(u"Mega Man X") << Q::None;
        QTest::newRow("NOT letter: Metal Slug 3 | Metal Slug X") << QString(u"Metal Slug 3") << QString(u"Metal Slug X") << Q::None;
        QTest::newRow("NOT edition: Star Fox 64 | Star Fox 64 3D") << QString(u"Star Fox 64") << QString(u"Star Fox 64 3D") << Q::None;
        QTest::newRow("NOT word: Dead Aim | Dead Eye") << QString(u"Dead Aim") << QString(u"Dead Eye") << Q::None;
        QTest::newRow("NOT word: Virtua Striker | Virtua Fighter") << QString(u"Virtua Striker") << QString(u"Virtua Fighter") << Q::None;
        QTest::newRow("NOT prefix word: Tekken | Tekken 3") << QString(u"Tekken") << QString(u"Tekken 3") << Q::None;
        QTest::newRow("NOT subtitle reversed: Resident Evil 2 | Resident Evil 2: Dual Shock Ver.") << QString(u"Resident Evil 2") << QString(u"Resident Evil 2: Dual Shock Ver.") << Q::None;
        QTest::newRow("NOT subtitle other: Mario Kart: Double Dash | Mario Kart 64") << QString(u"Mario Kart: Double Dash") << QString(u"Mario Kart 64") << Q::None;
        QTest::newRow("NOT subtitle other: Ridge Racer: Revolution | Ridge Racer V") << QString(u"Ridge Racer: Revolution") << QString(u"Ridge Racer V") << Q::None;
        QTest::newRow("NOT subtitle other: Zelda: Majora's Mask | Zelda: Ocarina of Time") << QString(u"Zelda: Majora's Mask") << QString(u"Zelda: Ocarina of Time") << Q::None;
        QTest::newRow("NOT i: I Am Setsuna | 1 Am Setsuna") << QString(u"I Am Setsuna") << QString(u"1 Am Setsuna") << Q::None;
        QTest::newRow("NOT spelling: Mario Kart | Mario Karts") << QString(u"Mario Kart") << QString(u"Mario Karts") << Q::None;
        QTest::newRow("empty:  | Star Fox") << QString(u"") << QString(u"Star Fox") << Q::None;
        QTest::newRow("only tag: (USA) | Star Fox") << QString(u"(USA)") << QString(u"Star Fox") << Q::None;
        QTest::newRow("empty both:  | ") << QString(u"") << QString(u"") << Q::None;
        QTest::newRow("1942: 1942 | 1942 (Japan)") << QString(u"1942") << QString(u"1942 (Japan)") << Q::Same;
        QTest::newRow("tag only title: (1942) | 1942") << QString(u"(1942)") << QString(u"1942") << Q::Same;
        QTest::newRow("kana marks: Gundam vs Katakam") << QString(u"ガンダム") << QString(u"カンタム") << Q::None;
        QTest::newRow("kana marks: same title") << QString(u"ガンダム (Japan)") << QString(u"ガンダム") << Q::Same;
    }
    void compare() {
        QFETCH(QString, wanted);
        QFETCH(QString, candidate);
        QFETCH(Quality, expected);
        QCOMPARE(ac::match::compareTitles(wanted, candidate), expected);
    }
    void symmetricUnlessSubtitle() {
        const QList<QPair<QString, QString>> same{{"Time Crisis II", "Time Crisis 2 (USA)"},
                                                  {"Ghosts & Goblins", "Ghosts and Goblins"},
                                                  {"House of the Dead 2, The", "The House of the Dead 2"}};
        for (const auto &[a, b] : same) {
            QVERIFY(ac::match::compareTitles(a, b) >= Q::Same);
            QVERIFY(ac::match::compareTitles(b, a) >= Q::Same);
        }
        // The wanted title's subtitle is extra detail; a candidate with a subtitle the wanted one lacks is another game.
        QCOMPARE(ac::match::compareTitles("Resident Evil 2: Dual Shock Ver.", "Resident Evil 2"), Q::Subtitle);
        QCOMPARE(ac::match::compareTitles("Resident Evil 2", "Resident Evil 2: Dual Shock Ver."), Q::None);
    }
    void tokensAndKeys() {
        QCOMPARE(ac::match::titleTokens("Ridge Racer VII (USA) [!]"), QStringList({"ridge", "racer", "7"}));
        QCOMPARE(ac::match::titleTokens("Mario Kart7"), QStringList({"mario", "kart", "7"}));
        QCOMPARE(ac::match::titleTokens("Mega Man X2"), QStringList({"mega", "man", "x", "2"}));
        QCOMPARE(ac::match::titleTokens("Rock & Roll Racing"), QStringList({"rock", "and", "roll", "racing"}));
        QCOMPARE(ac::match::titleTokens("Legend of Zelda, The - Ocarina of Time"),
                 QStringList({"legend", "of", "zelda", "ocarina", "of", "time"}));
        QVERIFY(ac::match::titleTokens("").isEmpty());
        QVERIFY(ac::match::titleTokens("   ").isEmpty());
        QVERIFY(ac::match::titleTokens("(USA)").isEmpty() == false); // a title that is only a tag keeps its words
        QVERIFY(ac::match::titleKey("!!!").isEmpty());
        QCOMPARE(ac::match::titleKey("Pac-Man (USA)"), QString("pacman"));
        bool sub = false;
        QCOMPARE(ac::match::mainTitleKey("Resident Evil Survivor 2 - Code: Veronica", &sub), QString("residentevilsurvivor2"));
        QVERIFY(sub);
        QCOMPARE(ac::match::mainTitleKey("Resident Evil 2", &sub), QString("residentevil2"));
        QVERIFY(!sub);
        // A hyphen inside a word is not a subtitle.
        QCOMPARE(ac::match::mainTitleKey("Spider-Man 2", &sub), QString("spiderman2"));
        QVERIFY(!sub);
    }
    void bestMatchesKeepOnlyTheBestQuality() {
        const QStringList listing{"Time Crisis (Japan)", "Time Crisis", "Time Crisis 2 (USA)", "Time Crisis (USA)", "Time Crisis 3"};
        Quality found = Q::None;
        // The text-for-text entry beats the ones that only agree after normalising.
        QCOMPARE(ac::match::bestMatches("Time Crisis", listing, Q::Same, &found), QList<int>({1}));
        QCOMPARE(found, Q::Exact);
        // Without an exact entry every normalised equal is kept, in listing order, for the caller's tie-break.
        QCOMPARE(ac::match::bestMatches("time crisis (World)", listing, Q::Same, &found), QList<int>({0, 1, 3}));
        QCOMPARE(found, Q::Same);
        QCOMPARE(ac::match::bestMatches("Time Crisis II", listing, Q::Same), QList<int>({2}));
        QVERIFY(ac::match::bestMatches("Time Crisis 4", listing, Q::Same, &found).isEmpty());
        QCOMPARE(found, Q::None);
        QVERIFY(ac::match::bestMatches("Time", listing).isEmpty());
        QVERIFY(ac::match::bestMatches("", listing).isEmpty());
        QVERIFY(ac::match::bestMatches("Time Crisis", {}).isEmpty());
        // A subtitle match only counts when the caller asks for it.
        QVERIFY(ac::match::bestMatches("Time Crisis: Project Titan", listing, Q::Same).isEmpty());
        QCOMPARE(ac::match::bestMatches("Time Crisis: Project Titan", {"Time Crisis 3", "Time Crisis (USA)"}, Q::Subtitle), QList<int>({1}));
    }
    void editDistanceAndTypos() {
        QCOMPARE(ac::match::editDistance("kart", "kart", 2), 0);
        QCOMPARE(ac::match::editDistance("kart", "krat", 2), 1);   // neighbours swapped
        QCOMPARE(ac::match::editDistance("mario", "maro", 2), 1);
        QCOMPARE(ac::match::editDistance("mario", "marioo", 2), 1);
        QCOMPARE(ac::match::editDistance("mario", "luigi", 2), 3); // limit + 1: too far
        QCOMPARE(ac::match::editDistance("a", "abcdef", 2), 3);
        QVERIFY(ac::match::wordsMatchWithTypo("mraio", "mario", false));
        QVERIFY(ac::match::wordsMatchWithTypo("residnet", "resident", false));
        QVERIFY(!ac::match::wordsMatchWithTypo("fox", "fax", false));             // short words are exact
        QVERIFY(!ac::match::wordsMatchWithTypo("kart7", "kart8", false));         // numbers are exact
        QVERIFY(!ac::match::wordsMatchWithTypo("7", "8", false));
        QVERIFY(!ac::match::wordsMatchWithTypo("mart", "kart", false));           // first letter of a short word
        QVERIFY(ac::match::wordsMatchWithTypo("kart", "kart", false));
        QVERIFY(!ac::match::wordsMatchWithTypo("", "kart", false));
        // Still typing: the start of a longer word, with a slip in it.
        QVERIFY(ac::match::wordsMatchWithTypo("marri", "mario", true) == false);  // same length, two slips
        QVERIFY(ac::match::wordsMatchWithTypo("resid", "resident", true));
        QVERIFY(ac::match::wordsMatchWithTypo("resdi", "resident", true));
        QVERIFY(!ac::match::wordsMatchWithTypo("resdi", "resident", false));
    }
    // Library search ranking: what was typed against one game's titles.
    void searchRank_data() {
        QTest::addColumn<QStringList>("titles");
        QTest::addColumn<QString>("typed");
        QTest::addColumn<SearchRank>("expected");
        const QStringList kart{"Mario Kart 7"}, fox{"Star Fox 64 3D"}, ocarina{"The Legend of Zelda: Ocarina of Time"};
        QTest::newRow("exact") << kart << "mario kart 7" << R::Exact;
        QTest::newRow("exact, closed up") << kart << "mario kart7" << R::Exact;
        QTest::newRow("exact, roman") << kart << "mario kart vii" << R::Exact;
        QTest::newRow("exact, case and tag") << kart << "MARIO KART 7 (USA)" << R::Exact;
        QTest::newRow("prefix") << kart << "mario kart" << R::Prefix;
        QTest::newRow("prefix, part word") << kart << "mari" << R::Prefix;
        QTest::newRow("prefix, run together") << fox << "starfox" << R::Prefix;
        QTest::newRow("exact, run together") << fox << "starfox643d" << R::Exact;
        QTest::newRow("contains") << kart << "kart 7" << R::Contains;
        QTest::newRow("contains, subtitle") << ocarina << "zelda ocarina" << R::Contains;
        QTest::newRow("prefix, article ignored") << ocarina << "legend of zelda" << R::Prefix;
        QTest::newRow("typo, swapped") << kart << "mraio kart" << R::Typo;
        QTest::newRow("typo, swapped in the last word") << kart << "mario krat 7" << R::Typo;
        QTest::newRow("typo, missing letter") << ocarina << "ocarina of tmie" << R::Typo;
        QTest::newRow("typo, still typing") << QStringList{"Resident Evil: Dead Aim"} << "residnet ev" << R::Typo;
        QTest::newRow("typo, other word") << fox << "sttar fox" << R::Typo;
        QTest::newRow("alternate title") << QStringList{"Equal", "Alias-b"} << "alias b" << R::Exact;
        QTest::newRow("NOT other number") << kart << "mario kart 8" << R::None;
        QTest::newRow("NOT other game") << fox << "star wars" << R::None;
        QTest::newRow("NOT extra words") << fox << "star fox adventures" << R::None;
        QTest::newRow("NOT other subtitle") << ocarina << "zelda majora" << R::None;
        QTest::newRow("NOT short word slip") << fox << "star fax" << R::None;
        QTest::newRow("NOT roman") << QStringList{"Ridge Racer"} << "ridge racer v" << R::None;
        QTest::newRow("NOT unrelated") << kart << "xyz" << R::None;
        QTest::newRow("NOT empty") << kart << "" << R::None;
        QTest::newRow("NOT punctuation only") << kart << "!!" << R::None;
        QTest::newRow("NOT no titles") << QStringList{} << "mario" << R::None;
        QTest::newRow("blank titles are skipped") << QStringList{"", "(USA)"} << "usa" << R::Exact; // a title that is only a tag keeps its words
    }
    void searchRank() {
        QFETCH(QStringList, titles);
        QFETCH(QString, typed);
        QFETCH(SearchRank, expected);
        QCOMPARE(ac::match::searchRank(ac::match::searchKeys(titles), typed), expected);
    }
};
QTEST_GUILESS_MAIN(TitleMatchTest)
#include "TitleMatchTest.moc"
