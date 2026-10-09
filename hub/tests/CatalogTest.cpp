// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogLoader.h"
#include "core/install/Support.h"
#include "models/FilterSortModel.h"
#include "models/GameListModel.h"
#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
namespace {
void write(const QString &root, const QString &path, const QString &text) {
    const auto filePath = QDir(root).filePath(path);
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("fixture write failed");
    file.write(text.toUtf8());
}
void vocab(const QString &root) {
    QDir().mkpath(root + "/data/vocab");
    QDir dir(QStringLiteral(AC_CATALOG_ROOT) + "/data/vocab");
    for (const auto &file : dir.entryList({"*.toml"}, QDir::Files))
        QFile::copy(dir.filePath(file), root + "/data/vocab/" + file);
}
void game(const QString &root, const QString &id, const QString &title,
          const QString &genre = "gun", const QString &maker = "namco", int year = 1995,
          const QString &hardware = "namco-super-system-22", const QString &graphics = "polygon-3d",
          int players = 1, const QString &controls = "gun", const QString &best = "theatre",
          bool planned = false) {
    write(root, "games/" + id + "/game.toml",
          QString(R"(id='%1'
title='%2'
alt_titles=['Alias-%1']
genre='%3'
subgenre=['%11']
year=%4
manufacturer='%5'
developer='Synthetic Studio'
hardware='%6'
graphics='%7'
players=%8
[controls]
type='%9'
[routes.vr]
best='%10'
planned='%12'
[hub]
pill='FAKE'
colour='#101020'
accent='#203040'
blurb='Synthetic test entry'
badges=['roomscale','seated','3-players']
[meta]
sources=['https://example.invalid/fixture']
)")
              .arg(id, title, genre, QString::number(year), maker, hardware, graphics,
                   QString::number(players), controls, best,
                   genre == "gun" ? "rail-shooter" : "rally", planned ? "true3d-planned" : ""));
}
ac::CatalogData fixtures(const QString &root) {
    vocab(root);
    game(root, "a", "Equal", "gun", "namco", 1995, "namco-super-system-22", "polygon-3d", 1, "gun",
         "true3d");
    game(root, "b", "Equal", "racing", "sega", 2005, "sega-model-3", "mixed", 2, "wheel", "theatre",
         true);
    game(root, "c", "Zulu", "gun", "sega", 1985, "sega-model-2", "2d", 2, "joystick", "none");
    write(root, "games/a/install.toml",
          "[variant.fake]\ntitle='Fake "
          "setup'\nquality='true3d'\nstatus='stable'\nneeds={media=['fake-media'],tools=['fake-"
          "tool']}\n");
    write(root, "games/c/install.toml",
          "[variant.future]\ntitle='Future setup'\nquality='true3d'\nstatus='planned'\n");
    return ac::CatalogLoader().load(root);
}
QStringList visible(ac::FilterSortModel &proxy) {
    auto *source = qobject_cast<ac::GameListModel *>(proxy.sourceModel());
    QStringList out;
    for (int i = 0; i < proxy.rowCount(); ++i)
        out << proxy.data(proxy.index(i, 0), source->roleForName("gameId")).toString();
    return out;
}
} // namespace
class CatalogTest : public QObject {
    Q_OBJECT
  private slots:
    void realCatalog() {
        const auto data = ac::CatalogLoader().load(QStringLiteral(AC_CATALOG_ROOT));
        QCOMPARE(data.games.size(), 413);
        QCOMPARE(data.report.errors, 0);
        QCOMPARE(data.report.warnings, 2);
        QVERIFY(data.find("timecris"));
        QVERIFY(data.find("timecris")->hasRecipe);
        QVERIFY(!data.emulators.empty());
        QVERIFY(!data.vocab.empty());
        QVERIFY(!data.find("raverace")->setup.empty());
        for (const auto &g : data.games)
            QVERIFY(!g.roles.value("title").toString().isEmpty());
        auto *scud = data.find("scud");
        QVERIFY(scud);
        bool flat = false;
        for (const auto &v : scud->variants)
            flat |= v.id == "supermodel" && v.quality == "flat";
        QVERIFY(flat);
    }
    void unnamedDiscAndBiosCannotBecomeReady() {
        QTemporaryDir temp; vocab(temp.path()); game(temp.path(), "fake", "Synthetic");
        QFile metadata(temp.path() + "/games/fake/game.toml");
        QVERIFY(metadata.open(QIODevice::Append));
        metadata.write("\n[[media]]\nkind='disc'\n[[media]]\nkind='bios'\n[routes]\npcsx2='working'\n");
        metadata.close();
        write(temp.path(), "data/emulators/pcsx2.toml", "id='pcsx2'\nname='Synthetic Tool'\n[launch]\nargs=[]\n");
        ac::GameListModel model(ac::CatalogLoader().load(temp.path()));
        const auto *record = model.find("fake"); QVERIFY(record);
        QCOMPARE(record->variants.size(), 1);
        QCOMPARE(record->variants[0].media, QStringList({"fake-media-0", "fake-media-1"}));
        ac::RuntimeState state; state.gameId = "fake"; state.toolsOk = {"pcsx2"};
        model.applyRuntimeStates({state});
        QCOMPARE(model.find("fake")->roles.value("baseState").toInt(), int(ac::GameState::NeedsFiles));
        state.mediaFound = {"fake-media-0"}; model.applyRuntimeStates({state});
        QCOMPARE(model.find("fake")->roles.value("baseState").toInt(), int(ac::GameState::NeedsFiles));
        state.mediaFound << "fake-media-1"; model.applyRuntimeStates({state});
    QCOMPARE(model.find("fake")->roles.value("baseState").toInt(), int(ac::GameState::Installed));
    }
    void routeMediaAndOptionalBios() {
        QTemporaryDir temp; vocab(temp.path());
        game(temp.path(), "fake", "Synthetic");
        QFile f(temp.path() + "/games/fake/game.toml"); QVERIFY(f.open(QIODevice::Append));
        f.write("\n[[media]]\nkind='mame-romset'\nset='test'\n"
                "[[media]]\nkind='pc-game'\nid='pc-release'\nfind=['Synthetic.exe']\n"
                "[routes]\nmame='working'\nteknoparrot='playable'\n"); f.close();
        for (const auto &tool : QStringList{"mame", "teknoparrot"})
            write(temp.path(), "data/emulators/" + tool + ".toml",
                  "id='" + tool + "'\nname='Synthetic Tool'\n[launch]\nargs=['${rom.set}','${media.file}']\n");
        auto data = ac::CatalogLoader().load(temp.path());
        const auto *g = data.find("fake"); QVERIFY(g);
        QCOMPARE(g->variants.size(), 2);
        for (const auto &v : g->variants)
            QCOMPARE(v.media, v.id == "mame" ? QStringList{"test"} : QStringList{"pc-release"});
        write(temp.path(), "synthetic-tool.exe", "synthetic tool; never executed");
        write(temp.path(), "synthetic.zip", "synthetic media; never executed");
        ac::Json bindings{{"tools", {{"mame", {{"path", (temp.path() + "/synthetic-tool.exe").toStdString()}}}}},
                          {"media", {{"test", {{"path", (temp.path() + "/synthetic.zip").toStdString()}, {"verified", true}}}}}};
        bindings["media"]["test"]["identity"] = "synthetic-regional";
        bindings["media"]["test"]["proof"] = "mame-header-crc";
        bindings["media"]["test"]["setCandidates"] = ac::Json::array({"test", "synthetic-regional"});
        const auto plan = ac::install::makeFlatLaunchPlan(*g, data.emulators["mame"], temp.path(), bindings);
        QCOMPARE(plan.args, QStringList({"synthetic-regional", temp.path() + "/synthetic.zip"}));
        auto invalid = bindings;
        invalid["media"]["test"].erase("setCandidates");
        QVERIFY_THROWS_EXCEPTION(ac::install::Error,
            ac::install::makeFlatLaunchPlan(*g, data.emulators["mame"], temp.path(), invalid));
        invalid = bindings; invalid["media"]["test"]["identity"] = "foreign";
        QVERIFY_THROWS_EXCEPTION(ac::install::Error,
            ac::install::makeFlatLaunchPlan(*g, data.emulators["mame"], temp.path(), invalid));
        for (const auto &id : QStringList{"--option", "regional --option", "../escape", "regional\n"}) {
            invalid = bindings; invalid["media"]["test"]["identity"] = id.toStdString();
            invalid["media"]["test"]["setCandidates"] = ac::Json::array({"test", id.toStdString()});
            QVERIFY_THROWS_EXCEPTION(ac::install::Error,
                ac::install::makeFlatLaunchPlan(*g, data.emulators["mame"], temp.path(), invalid));
        }
        invalid = bindings; invalid["media"]["test"].erase("identity"); invalid["media"]["test"].erase("setCandidates");
        QCOMPARE(ac::install::makeFlatLaunchPlan(*g, data.emulators["mame"], temp.path(), invalid).args[0], QString("test"));
        invalid["media"]["test"]["identity"] = "test";
        QCOMPARE(ac::install::makeFlatLaunchPlan(*g, data.emulators["mame"], temp.path(), invalid).args[0], QString("test"));
        ac::GameListModel model(data); ac::RuntimeState state; state.gameId = "fake";
        state.toolsOk = {"mame"}; state.mediaFound = {"test"}; model.applyRuntimeStates({state});
        QCOMPARE(model.find("fake")->roles["baseState"].toInt(), int(ac::GameState::Installed));
        QVERIFY(model.find("fake")->roles["inLibrary"].toBool());

        game(temp.path(), "dc-fake", "Synthetic Console", "gun", "sega", 2000, "sega-dreamcast");
        QFile console(temp.path() + "/games/dc-fake/game.toml"); QVERIFY(console.open(QIODevice::Append));
        console.write("\n[[media]]\nkind='disc'\nid='disc'\n[[media]]\nkind='bios'\nid='bios'\noptional=true\n"
                      "[routes]\nflycast='working'\n"); console.close();
        write(temp.path(), "games/dc-fake/install.toml", "[variant.authored]\nquality='true3d'\nstatus='stable'\nneeds={media=['disc','bios']}\n");
        write(temp.path(), "data/emulators/flycast.toml", "id='flycast'\nname='Synthetic Tool'\n[launch]\nargs=['${media.disc}']\n");
        data = ac::CatalogLoader().load(temp.path()); g = data.find("dc-fake"); QVERIFY(g);
        QCOMPARE(g->variants.size(), 2);
        for (const auto &v : g->variants) QCOMPARE(v.media, QStringList{"disc"});
        write(temp.path(), "synthetic.iso", "synthetic disc");
        bindings = {{"tools", {{"flycast", {{"path", (temp.path() + "/synthetic-tool.exe").toStdString()}}}}},
                    {"media", {{"disc", {{"path", (temp.path() + "/synthetic.iso").toStdString()}, {"verified", true}}}}}};
        const auto consolePlan = ac::install::makeFlatLaunchPlan(*g, data.emulators["flycast"], temp.path(), bindings);
        QCOMPARE(consolePlan.args, QStringList{temp.path() + "/synthetic.iso"});
        ac::GameListModel consoleModel(data); state = {}; state.gameId = "dc-fake";
        state.toolsOk = {"flycast"}; state.mediaFound = {"disc"}; consoleModel.applyRuntimeStates({state});
        QCOMPARE(consoleModel.find("dc-fake")->roles["baseState"].toInt(), int(ac::GameState::Installed));
        QCOMPARE(consoleModel.find("dc-fake")->roles["mediaStatus"].toInt(), int(ac::MediaStatus::Found));
        state.mediaFound = {"bios"}; consoleModel.applyRuntimeStates({state});
        QVERIFY(!consoleModel.find("dc-fake")->roles["inLibrary"].toBool());
    }
    void invalidOptionalMetadataFailsValidation() {
        QTemporaryDir temp; vocab(temp.path()); game(temp.path(), "fake", "Synthetic");
        QFile f(temp.path() + "/games/fake/game.toml"); QVERIFY(f.open(QIODevice::Append));
        f.write("\n[[media]]\nkind='bios'\noptional='true'\n"); f.close();
        const auto data = ac::CatalogLoader().load(temp.path());
        QVERIFY(data.find("fake")->validationErrors.contains("media.optional must be boolean"));
    }
    void layeringAndProvenance() {
        try {
            QTemporaryDir temp;
            QVERIFY(temp.isValid());
            vocab(temp.path());
            game(temp.path(), "fake", "Base");
            write(temp.path(), "games/fake/setup/controls.toml",
                  "unknown='keep'\n[[element]]\nid='wheel'\nrange=90\nnote='inherited'\n[[element]]"
                  "\nid='pedal'\nrange=1\n");
            write(temp.path(), "packs/z/pack.toml", "priority=1\n");
            write(temp.path(), "packs/a/pack.toml", "priority=5\n");
            write(temp.path(), "packs/z/games/fake/game.toml",
                  "title='First pack'\n[hub]\nblurb='pack text'\nextra='plugin'\n");
            write(temp.path(), "packs/a/games/fake/game.toml",
                  "title='Last pack'\n[hub]\ncolour='!delete'\n");
            write(temp.path(), "user/overrides/games/fake/game.toml",
                  "title='User'\n[unknown]\nplugin={enabled=true}\n");
            write(temp.path(), "user/overrides/games/fake/setup/controls.toml",
                  "[[element]]\nid='wheel'\nrange=270\nnote='!delete'\n[[element]]\nid='extra'"
                  "\nrange=2\n");
            const auto data = ac::CatalogLoader().load(temp.path());
            const auto *g = data.find("fake");
            QVERIFY(g);
            QCOMPARE(g->roles.value("title").toString(), QString("User"));
            QVERIFY(!g->raw["hub"].contains("colour"));
            QCOMPARE(g->raw["hub"]["extra"].get<std::string>(), std::string("plugin"));
            QVERIFY(g->raw["unknown"]["plugin"]["enabled"].get<bool>());
            QCOMPARE(g->setup["controls"]["element"].size(), size_t(3));
            QCOMPARE(g->setup["controls"]["element"][0]["range"].get<int>(), 270);
            QVERIFY(!g->setup["controls"]["element"][0].contains("note"));
            QCOMPARE(g->setup["controls"]["element"][1]["range"].get<int>(), 1);
            QCOMPARE(g->provenance["/title"].get<std::string>(),
                     std::string("user/overrides/games/fake/game.toml"));
            QCOMPARE(g->provenance["/hub/blurb"].get<std::string>(),
                     std::string("packs/z/games/fake/game.toml"));
            QVERIFY(!g->provenance.contains("/hub/colour"));
            QCOMPARE(g->setupProvenance["/controls/element/0/range"].get<std::string>(),
                     std::string("user/overrides/games/fake/setup/controls.toml"));
            QVERIFY(!g->setupProvenance.contains("/controls/element/0/note"));
            QCOMPARE(g->setupProvenance["/controls/element/1/range"].get<std::string>(),
                     std::string("games/fake/setup/controls.toml"));
            ac::Json base = {{"a", {1, 2}}, {"table", {{"x", 1}, {"y", 2}}}};
            ac::CatalogLoader::merge(base, {{"a", {3}}, {"table", {{"x", "!delete"}, {"new", 7}}}});
            QCOMPARE(base["a"].size(), size_t(1));
            QVERIFY(!base["table"].contains("x"));
            QCOMPARE(base["table"]["y"].get<int>(), 2);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
    }
    void facets_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QVariant>("value");
        QTest::addColumn<QStringList>("expected");
        const QList<std::tuple<QString, QVariant, QStringList>> cases{
            {"genre", "racing", {"b"}},
            {"manufacturerIds", QStringList{"sega"}, {"b", "c"}},
            {"graphicsIds", QStringList{"2d"}, {"c"}},
            {"yearMin", 2000, {"b"}},
            {"yearMax", 1990, {"c"}},
            {"decades", QStringList{"1990"}, {"a"}},
            {"hardwareIds", QStringList{"sega-model-2"}, {"c"}},
            {"hardwareIds", QStringList{"sega"}, {"b", "c"}},
            {"hardwareIds", QStringList{"arcade/sega"}, {"b", "c"}},
            {"hardwareKinds", QStringList{"arcade"}, {"a", "b", "c"}},
            {"hardwareFamilies", QStringList{"sega"}, {"b", "c"}},
            {"vrKeys", QStringList{"planned"}, {"b"}},
            {"vrKeys", QStringList{"true3d"}, {"a"}},
            {"vrKeys", QStringList{"theatre"}, {"b"}},
            {"playersBuckets", QStringList{"2+"}, {"b", "c"}},
            {"controlsTypes", QStringList{"wheel"}, {"b"}},
            {"genre", "not-in-vocab", {"a", "b", "c"}},
            {"graphicsIds", QStringList{"nonsense"}, {"a", "b", "c"}},
            {"manufacturerIds", QStringList{"sega", "nonsense"}, {"b", "c"}},
            {"yearMin", "banana", {"a", "b", "c"}},
            {"inLibraryOnly", "banana", {"a", "b", "c"}},
            {"nonsense", true, {"a", "b", "c"}}};
        int i = 0;
        for (const auto &[name, value, expected] : cases)
            QTest::newRow(qPrintable(QString::number(i++))) << name << value << expected;
    }
    void facets() {
        QFETCH(QString, name);
        QFETCH(QVariant, value);
        QFETCH(QStringList, expected);
        QTemporaryDir temp;
        ac::GameListModel model(fixtures(temp.path()));
        ac::FilterSortModel proxy;
        proxy.setSourceModel(&model);
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QPersistentModelIndex persistent(model.index(1));
        proxy.setFacet(name, value);
        QCOMPARE(visible(proxy), expected);
        QCOMPARE(resets.size(), 0);
        QVERIFY(persistent.isValid());
    }
    void facetsCombinedAndScanGate() {
        QTemporaryDir temp;
        ac::GameListModel model(fixtures(temp.path()));
        ac::FilterSortModel proxy;
        proxy.setSourceModel(&model);
        proxy.setFacet("inLibraryOnly", true);
        proxy.setFacet("statePills", QStringList{"ready"});
        QCOMPARE(proxy.rowCount(), 3);
        proxy.setScanComplete(true);
        QCOMPARE(proxy.rowCount(), 0);
        proxy.setFacet("inLibraryOnly", false);
        proxy.setFacet("statePills", QStringList{"needsFiles"});
        QCOMPARE(visible(proxy), QStringList{"a"});
        ac::RuntimeState ready;
        ready.gameId = "a";
        ready.mediaFound = {"fake-media"};
        ready.toolsOk = {"fake-tool"};
        model.applyRuntimeStates({ready});
        proxy.setFacet("statePills", QStringList{"toInstall"});
        QCOMPARE(visible(proxy), QStringList{"a"});
        proxy.setFacet("inLibraryOnly", true);
        proxy.setFacet("statePills", QStringList{"ready"});
        QCOMPARE(proxy.rowCount(), 0);
        ac::RuntimeState state;
        state.gameId = "a";
        state.mediaFound = {"fake-media"};
        state.toolsOk = {"fake-tool"};
        ac::VariantRuntimeState install;
        install.id = "fake";
        install.verified = true;
        install.installedWhenExists = true;
        install.manifestExists = true;
        install.updateAvailable = true;
        state.variants << install;
        model.applyRuntimeStates({state});
        QCOMPARE(visible(proxy), QStringList{"a"});
        proxy.setFacet("statePills", QStringList{"updates"});
        QCOMPARE(visible(proxy), QStringList{"a"});
        proxy.setFacet("statePills", QStringList{"toInstall"});
        QCOMPARE(proxy.rowCount(), 0);
        proxy.clearFacets();
        proxy.setFacet("genre", "gun");
        proxy.setFacet("manufacturerIds", QStringList{"sega"});
        QCOMPARE(visible(proxy), QStringList{"c"});
    }
    void vrToolsDoNotGenerateFlatRoutes() {
        QTemporaryDir temp;
        vocab(temp.path());
        game(temp.path(), "a", "Synthetic");
        write(temp.path(), "user/overrides/games/a/game.toml",
              "[routes]\nflat='working'\nvrport='working'\n");
        write(temp.path(), "data/emulators/flat.toml",
              "id='flat'\nname='Flat emulator'\nkind='emulator'\n");
        write(temp.path(), "data/emulators/vrport.toml",
              "id='vrport'\nname='VR port'\nkind='vr-tool'\n");
        const auto data = ac::CatalogLoader().load(temp.path());
        const auto *record = data.find("a");
        QVERIFY(record);
        bool flat = false;
        for (const auto &variant : record->variants) {
            QVERIFY(variant.id != "vrport");
            flat |= variant.id == "flat" && variant.generated && variant.quality == "flat";
        }
        QVERIFY(flat);
    }
    void searchAndStableSort() {
        QTemporaryDir temp;
        ac::GameListModel model(fixtures(temp.path()));
        ac::FilterSortModel proxy;
        proxy.setSourceModel(&model);
        QSignalSpy sourceReset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy proxyReset(&proxy, &QAbstractItemModel::modelReset);
        const QList<std::pair<QString, QStringList>> cases{{"alias-b", {"b"}},
                                                           {"synthetic studio", {"a", "b", "c"}},
                                                           {"gun", {"a", "c"}},
                                                           {"planned", {"b"}},
                                                           {"true3d", {"a"}},
                                                           {"roomscale", {"a", "b", "c"}},
                                                           {"-sega", {"a"}},
                                                           {"-Equal", {"a", "b", "c"}},
                                                           {"-theatre", {"a", "c"}},
                                                           {"EQUAL -sega", {"a"}},
                                                           {"+unknown", {"a", "b", "c"}}};
        for (const auto &[query, expected] : cases) {
            proxy.setQuery(query);
            QCOMPARE(visible(proxy), expected);
        }
        proxy.setFacet("hiddenPublishers", QStringList{"namco"});
        proxy.setQuery("");
        QCOMPARE(visible(proxy), QStringList({"b", "c"}));
        proxy.setQuery("+namco");
        QCOMPARE(visible(proxy), QStringList({"a", "b", "c"}));
        proxy.clearFacets();
        proxy.setSortMode("year");
        QCOMPARE(visible(proxy), QStringList({"c", "a", "b"}));
        proxy.setSortMode("manufacturer");
        QCOMPARE(visible(proxy), QStringList({"a", "c", "b"}));
        proxy.setSortMode("hardware");
        QCOMPARE(visible(proxy), QStringList({"c", "b", "a"}));
        proxy.setSortMode("title");
        QCOMPARE(visible(proxy), QStringList({"a", "b", "c"}));
        ac::RuntimeState state;
        state.gameId = "c";
        state.lastPlayed = 200;
        state.firstSeen = QDateTime::currentSecsSinceEpoch();
        model.applyRuntimeStates({state});
        proxy.setSortMode("recent");
        QCOMPARE(visible(proxy), QStringList({"c", "a", "b"}));
        proxy.setQuery("new");
        QCOMPARE(visible(proxy), QStringList{"c"});
        QCOMPARE(sourceReset.size(), 0);
        QCOMPARE(proxyReset.size(), 0);
    }
    void duplicateAndTypeValidation() {
        QTemporaryDir temp;
        vocab(temp.path());
        game(temp.path(), "a", "Fake");
        game(temp.path(), "b", "Fake");
        write(temp.path(), "user/overrides/games/b/game.toml", "id='a'\ntitle=123\nyear=true\n");
        const auto data = ac::CatalogLoader().load(temp.path());
        QVERIFY(data.report.errors > 0);
        QVERIFY(data.find("b")->roles.value("loadError").toString().contains("duplicate"));
        QVERIFY(
            data.find("b")->roles.value("loadWarning").toString().contains("title must be text"));
        QCOMPARE(data.find("b")->roles.value("gameId").toString(), QString("b"));
        QCOMPARE(ac::folded(QString::fromUtf8("ÉQUAL")), QString("equal"));
    }
    void unknownAndBrokenRecordsFailOpen() {
        QTemporaryDir temp;
        vocab(temp.path());
        game(temp.path(), "unknown", "Unknown", "gun", "namco", 1990, "mystery-board");
        write(temp.path(), "games/broken/game.toml", "broken = [\n");
        ac::GameListModel model(ac::CatalogLoader().load(temp.path()));
        ac::FilterSortModel proxy;
        proxy.setSourceModel(&model);
        QVERIFY(!model.find("unknown")->roles.value("loadWarning").toString().isEmpty());
        QVERIFY(model.find("unknown")->roles.value("loadError").toString().isEmpty());
        proxy.setFacet("hardwareIds", QStringList{"arcade"});
        QCOMPARE(proxy.rowCount(), 2);
        proxy.setQuery("impossible");
        QCOMPARE(visible(proxy), QStringList{"broken"});
    }
    void stateGatesAndPartialUpdates() {
        QTemporaryDir temp;
        ac::GameListModel model(fixtures(temp.path()));
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        auto stateOf = [&]() { return model.find("a")->roles.value("baseState").toInt(); };
        QCOMPARE(stateOf(), static_cast<int>(ac::GameState::NeedsFiles));
        QCOMPARE(model.find("b")->roles.value("baseState").toInt(),
                 static_cast<int>(ac::GameState::NoRecipe));
        QCOMPARE(model.find("c")->roles.value("baseState").toInt(),
                 static_cast<int>(ac::GameState::PlannedOnly));
        ac::RuntimeState state;
        state.gameId = "a";
        state.mediaFound = {"fake-media"};
        model.applyRuntimeStates({state});
        QCOMPARE(stateOf(), static_cast<int>(ac::GameState::NeedsEmulator));
        state.toolsOk = {"fake-tool"};
        model.applyRuntimeStates({state});
        QCOMPARE(stateOf(), static_cast<int>(ac::GameState::ReadyToInstall));
        ac::VariantRuntimeState install;
        install.id = "fake";
        install.verified = true;
        install.installedWhenExists = true;
        state.variants = {install};
        model.applyRuntimeStates({state});
        QCOMPARE(stateOf(), static_cast<int>(ac::GameState::ReadyToInstall));
        state.variants[0].manifestExists = true;
        model.applyRuntimeStates({state});
        QCOMPARE(stateOf(), static_cast<int>(ac::GameState::Installed));
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged),
            reset(&model, &QAbstractItemModel::modelReset);
        state.jobStatus = ac::JobStatus::Running;
        state.jobProgress = "3 of 5";
        model.queueRuntimeStates({state});
        QCOMPARE(changed.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, 1000);
        QCOMPARE(reset.size(), 0);
        QCOMPARE(stateOf(), static_cast<int>(ac::GameState::Installed));
        QCOMPARE(model.find("a")->roles.value("state").toInt(),
                 static_cast<int>(ac::GameState::Installing));
        const auto roles = qvariant_cast<QList<int>>(changed[0][2]);
        QVERIFY(!roles.contains(model.roleForName("title")));
        QVERIFY(roles.contains(model.roleForName("jobProgress")));
        model.applyRuntimeStates({state});
        QCOMPARE(changed.size(), 1);
        QVERIFY(!model.find("a")->roles.value("badges").toStringList().contains("2 PLAYERS"));
        QVERIFY(model.find("b")->roles.value("badges").toStringList().contains("2 PLAYERS"));
        QVERIFY(model.roleNames().size() >= 69);
    }
};
QTEST_GUILESS_MAIN(CatalogTest)
#include "CatalogTest.moc"
