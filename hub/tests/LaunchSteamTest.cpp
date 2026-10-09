// SPDX-License-Identifier: GPL-3.0-only
#include "core/install/Support.h"
#include "core/launch/Launch.h"
#include "core/steam/Steam.h"
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QtEndian>
using namespace ac;
namespace {
void write(const QString &path, const QByteArray &bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  install::atomicWrite(path, bytes);
}
steam::Shortcut shortcut() {
  return {"test-game",
          "Synthetic Game",
          "C:/Synthetic/hub.exe",
          "C:/Synthetic",
          false,
          0,
          0};
}
const steam::Node *find(const steam::Node &n, const QByteArray &key) {
  for (const auto &c : n.children)
    if (c.key == key)
      return &c;
  return nullptr;
}
} // namespace
class LaunchSteamTest : public QObject {
  Q_OBJECT
private slots:
  void binaryRoundTrip();
  void malformed_data();
  void malformed();
  void appIds();
  void ownedEditPreservesUnownedAndUnknown();
  void collisionRefused();
  void previewApprovalClosedAndChanged();
  void backupsAndOwnedRemoval();
  void customArtAndReceiptChanges();
  void supportChangesRefused();
  void runtimePrecedenceAndLibraries();
  void bindingChangesRefused();
  void toolChangesRefused();
  void flatSelectionAndPlayingRole();
  void generatedFlatReadyOverridesLegacyRecipe();
  void childLaunchAndLogs_data();
  void childLaunchAndLogs();
  void sameGameLock();
  void immediateCancel_data();
  void immediateCancel();
};
void LaunchSteamTest::binaryRoundTrip() {
  QCOMPARE(steam::serialize(steam::parse({})), QByteArray());
  // Includes duplicate keys, non-text payload, nested tags and alternative
  // ends.
  const auto b =
      QByteArray::fromHex("0073686f7274637574730000300001756e6b6e6f776e00610001"
                          "756e6b6e6f776e00620003666c6f6174000000803f056f646400"
                          "fffe4100000007776964650000010203040506070b0b0b");
  QCOMPARE(steam::serialize(steam::parse(b)), b);
}
void LaunchSteamTest::malformed_data() {
  QTest::addColumn<QByteArray>("bytes");
  QTest::newRow("truncated-key") << QByteArray::fromHex("006162");
  QTest::newRow("truncated-int") << QByteArray::fromHex("026100010208");
  QTest::newRow("unknown-type") << QByteArray::fromHex("09610008");
  QTest::newRow("trailing") << QByteArray::fromHex("0800");
  QTest::newRow("truncated-wide") << QByteArray::fromHex("056100ffff08");
  QByteArray deep;
  for (int i = 0; i < 34; ++i)
    deep += QByteArray::fromHex("006100");
  deep += QByteArray(35, '\x08');
  QTest::newRow("depth") << deep;
}
void LaunchSteamTest::malformed() {
  QFETCH(QByteArray, bytes);
  QVERIFY_THROWS_EXCEPTION(install::Error, steam::parse(bytes));
}
void LaunchSteamTest::appIds() {
  // Independently generated with Python zlib.crc32 UTF-8 vectors.
  QCOMPARE(steam::appId("123456789", {}), quint32(0xcbf43926));
  QCOMPARE(steam::appId({}, {}), quint32(0x80000000));
  QCOMPARE(steam::appId("\"C:/Synthetic/hub.exe\"", "Synthetic Game"),
           quint32(0xc17a01bd));
  QCOMPARE(steam::appId("\"/tmp/hub\"", QString::fromUtf8("遊戲 Café")),
           quint32(0x9c6c4176));
  QCOMPARE(steam::launchId(0xcbf43926), quint64(0xcbf4392602000000ull));
  QCOMPARE(steam::artNames(4294967295u),
           QStringList({"4294967295.png", "4294967295p.png",
                        "4294967295_hero.png", "4294967295_logo.png"}));
}
void LaunchSteamTest::ownedEditPreservesUnownedAndUnknown() {
  auto first = steam::edit({}, shortcut());
  QVERIFY(first.changed);
  QCOMPARE(steam::edit(first.bytes, shortcut()).bytes, first.bytes);
  auto d = steam::parse(first.bytes);
  steam::Node unowned;
  unowned.key = "9";
  unowned.children << steam::Node{1, "AppName", "Other", {}, {}, 8};
  d.roots[0].raw.clear();
  d.roots[0].children.prepend(unowned);
  auto &owned = d.roots[0].children[1];
  owned.raw.clear();
  owned.children << steam::Node{
      3, "NewSteamField", QByteArray::fromHex("0000803f"), {}, {}, 8};
  const auto original = steam::serialize(d);
  const auto parsed = steam::parse(original);
  auto s = shortcut();
  s.title = "Renamed synthetic";
  s.executable = "C:/Moved/hub.exe";
  const auto update = steam::edit(original, s);
  QCOMPARE(update.id, first.id);
  QVERIFY(update.changed);
  const auto after = steam::parse(update.bytes);
  QCOMPARE(after.roots[0].children[0].raw, parsed.roots[0].children[0].raw);
  QVERIFY(find(after.roots[0].children[1], "NewSteamField"));
  const auto *vr = find(after.roots[0].children[1], "OpenVR");
  QVERIFY(vr);
  QCOMPARE(qFromLittleEndian<quint32>(vr->payload.constData()), quint32(0));
  const auto remove = steam::edit(update.bytes, s, true);
  QCOMPARE(steam::parse(remove.bytes).roots[0].children.size(), 1);
  QCOMPARE(steam::parse(remove.bytes).roots[0].children[0].raw,
           parsed.roots[0].children[0].raw);
}
void LaunchSteamTest::collisionRefused() {
  auto a = shortcut();
  auto first = steam::edit({}, a);
  auto b = a;
  b.gameId = "other-game";
  b.storedAppId = first.id;
  QVERIFY_THROWS_EXCEPTION(install::Error, steam::edit(first.bytes, b));
  QVERIFY(!steam::edit(first.bytes, b, true).changed);
}
void LaunchSteamTest::customArtAndReceiptChanges() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/steam/userdata/123/config/grid");
  steam::WriteRequest r;
  r.steamRoot = temp.path() + "/steam";
  r.accountId = "123";
  r.userRoot = temp.path() + "/user";
  r.shortcut = shortcut();
  r.art = steam::fallbackArt("Synthetic", "Test");
  auto p = steam::preview(r);
  write(p.artPaths[0], "custom-original");
  p = steam::preview(r);
  QCOMPARE(p.json["art"][0]["action"], Json("keep-custom"));
  steam::apply(r, p, true, [] { return false; });
  QCOMPARE(install::readBytes(p.artPaths[0]), QByteArray("custom-original"));
  write(p.artPaths[1], "owner-edited");
  p = steam::preview(r);
  steam::apply(r, p, true, [] { return false; });
  QCOMPARE(install::readBytes(p.artPaths[1]), QByteArray("owner-edited"));
  r.remove = true;
  const auto removal = steam::preview(r);
  const auto receipt = r.userRoot + "/state/steam/123/" +
                       QString::number(removal.edit.id) + ".json";
  auto own = Json::parse(install::readBytes(receipt).toStdString());
  own[QFileInfo(p.artPaths[0]).fileName().toStdString()] =
      install::hashFile(p.artPaths[0]).toStdString();
  write(receipt, QByteArray::fromStdString(own.dump(2)));
  QVERIFY_THROWS_EXCEPTION(
      install::Error, steam::apply(r, removal, true, [] { return false; }));
  QVERIFY(QFileInfo::exists(p.artPaths[0]));
}
void LaunchSteamTest::supportChangesRefused() {
  QTemporaryDir temp;
  const auto primary = temp.path() + "/primary.zip",
             support = temp.path() + "/support.zip";
  write(primary, "primary");
  write(support, "support");
  Json files = Json::array();
  for (const auto &path : QStringList{primary, support}) {
    const QFileInfo f(path);
    files.push_back({{"path", path.toStdString()},
                     {"size", f.size()},
                     {"mtime", f.lastModified().toMSecsSinceEpoch()}});
  }
  Json b{{"gameId", "test-game"},
         {"requirementId", "disc"},
         {"path", primary.toStdString()},
         {"verified", true},
         {"supportPaths", Json::array({support.toStdString()})},
         {"supportRequirements",
          Json::array({{{"sourcePath", support.toStdString()}}})}};
  Json j{{"bindings", Json::array({b})}, {"files", files}};
  launch::normalizeBindings(j, "test-game");
  write(support, "changed-support-size");
  QVERIFY_THROWS_EXCEPTION(install::Error,
                           launch::normalizeBindings(j, "test-game"));
}
void LaunchSteamTest::previewApprovalClosedAndChanged() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/steam/userdata/123/config");
  steam::WriteRequest r;
  r.steamRoot = temp.path() + "/steam";
  r.accountId = "123";
  r.userRoot = temp.path() + "/user";
  r.shortcut = shortcut();
  auto p = steam::preview(r);
  QVERIFY(p.json.contains("StartDir"));
  QVERIFY(p.json.contains("launchId"));
  QCOMPARE(p.json["art"][0]["action"], Json("none"));
  auto moved = r;
  moved.shortcut.startDir = temp.path() + "/different";
  QVERIFY_THROWS_EXCEPTION(install::Error,
                           steam::apply(moved, p, true, [] { return false; }));
  QVERIFY(!QFileInfo::exists(p.target));
  QVERIFY_THROWS_EXCEPTION(install::Error,
                           steam::apply(r, p, false, [] { return false; }));
  QVERIFY_THROWS_EXCEPTION(install::Error,
                           steam::apply(r, p, true, [] { return true; }));
  QVERIFY(!QFileInfo::exists(p.target));
  write(p.target, steam::edit({}, shortcut()).bytes);
  QVERIFY_THROWS_EXCEPTION(install::Error,
                           steam::apply(r, p, true, [] { return false; }));
  auto invalid = r;
  invalid.accountId = "../escape";
  QVERIFY_THROWS_EXCEPTION(install::Error, steam::preview(invalid));
  QCOMPARE(steam::accounts(r.steamRoot), QStringList{"123"});
}
void LaunchSteamTest::backupsAndOwnedRemoval() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/steam/userdata/123/config/grid");
  steam::WriteRequest r;
  r.steamRoot = temp.path() + "/steam";
  r.accountId = "123";
  r.userRoot = temp.path() + "/user";
  r.shortcut = shortcut();
  r.art = steam::fallbackArt("Synthetic", "Test");
  for (int i = 0; i < 12; ++i) {
    r.shortcut.title = "Synthetic " + QString::number(i);
    const auto p = steam::preview(r);
    steam::apply(r, p, true, [] { return false; });
  }
  const auto p = steam::preview(r);
  QCOMPARE(
      QDir(p.backupFolder).entryList({"shortcuts-*.bak"}, QDir::Files).size(),
      10);
  for (const auto &path : p.artPaths)
    QVERIFY(QFileInfo::exists(path));
  const QImage logo(p.artPaths.last());
  QVERIFY(logo.hasAlphaChannel());
  QCOMPARE(qAlpha(logo.pixel(0, 0)), 0);
  r.remove = true;
  steam::apply(r, steam::preview(r), true, [] { return false; });
  QVERIFY(
      steam::parse(install::readBytes(p.target)).roots[0].children.isEmpty());
  for (const auto &path : p.artPaths)
    QVERIFY(!QFileInfo::exists(path));
}
void LaunchSteamTest::runtimePrecedenceAndLibraries() {
  QTemporaryDir temp;
  launch::RuntimeInputs in;
  in.environment = QProcessEnvironment();
  in.home = temp.path();
  in.registryRuntime = "registry.json";
  in.environment.insert("XR_RUNTIME_JSON", "override.json");
  QCOMPARE(launch::detectRuntime(in).active, QString("override.json"));
  in.environment.remove("XR_RUNTIME_JSON");
  QCOMPARE(launch::detectRuntime(in).source, QString("registry"));
  in.registryRuntime.clear();
  in.environment.insert("XDG_CONFIG_HOME", temp.path() + "/xdg");
  write(temp.path() + "/xdg/openxr/1/active_runtime.json", "{}");
  QCOMPARE(launch::detectRuntime(in).source, QString("xdg"));
  in.steamPath = temp.path() + "/steam";
  const auto library = temp.path() + "/library";
  write(in.steamPath + "/steamapps/libraryfolders.vdf",
        "\"libraryfolders\" { \"1\" { \"path\" \"" + library.toUtf8() +
            "\" } }");
#ifdef Q_OS_WIN
  const auto manifest =
      library + "/steamapps/common/SteamVR/steamxr_win64.json";
#else
  const auto manifest =
      library + "/steamapps/common/SteamVR/steamxr_linux64.json";
#endif
  write(manifest, "{}");
  QCOMPARE(launch::detectRuntime(in).steamVrManifest, manifest);
}
void LaunchSteamTest::bindingChangesRefused() {
  QTemporaryDir temp;
  const auto path = temp.path() + "/synthetic.iso";
  write(path, "synthetic");
  const QFileInfo info(path);
  Json j{{"bindings", Json::array({{{"gameId", "test-game"},
                                    {"requirementId", "disc"},
                                    {"path", path.toStdString()},
                                    {"verified", true}}})},
         {"files",
          Json::array({{{"path", path.toStdString()},
                        {"size", info.size()},
                        {"mtime", info.lastModified().toMSecsSinceEpoch()}}})}};
  QVERIFY(launch::normalizeBindings(j, "test-game")["media"].contains("disc"));
  write(path, "changed-size");
  QVERIFY_THROWS_EXCEPTION(install::Error,
                           launch::normalizeBindings(j, "test-game"));
}
void LaunchSteamTest::toolChangesRefused() {
  QTemporaryDir temp;
  const auto path = temp.path() + "/synthetic.exe";
  write(path, "synthetic");
  const QFileInfo info(path);
  Json j{{"tools",
          Json::array({{{"id", "synthetic"},
                        {"path", path.toStdString()},
                        {"verified", true},
                        {"size", info.size()},
                        {"mtime", info.lastModified().toMSecsSinceEpoch()}}})}};
  QVERIFY(
      launch::normalizeBindings(j, "test-game")["tools"].contains("synthetic"));
  write(path, "changed-size");
  QVERIFY(!launch::normalizeBindings(
               j, "test-game")["tools"]["synthetic"]["verified"]
               .get<bool>());
}
void LaunchSteamTest::flatSelectionAndPlayingRole() {
  QTemporaryDir temp;
  const auto path = temp.path() + "/synthetic.iso";
  write(path, "synthetic");
  CatalogData catalog;
  GameRecord game;
  game.id = "test-game";
  game.raw = {
      {"media", Json::array({{{"kind", "disc"}, {"serial", "SYNTH-00001"}}})}};
  Variant vr;
  vr.id = "legacy-vr";
  vr.quality = "true-3d";
  vr.tools = {"synthetic"};
  Variant flat;
  flat.id = "flat-synthetic";
  flat.quality = "flat";
  flat.tools = {"synthetic"};
  game.variants = {vr, flat};
  catalog.games = {game};
  catalog.emulators = {
      {"synthetic",
       {{"id", "synthetic"},
        {"launch", {{"args", Json::array({"${media.file}"})}}}}}};
  Json bindings{
      {"media",
       {{"SYNTH-00001", {{"path", path.toStdString()}, {"verified", true}}}}},
      {"tools",
       {{"synthetic",
         {{"path", QCoreApplication::applicationFilePath().toStdString()},
          {"verified", true}}}}}};
  const auto request =
      launch::flatRequest(catalog, game.id, temp.path(), bindings);
  QCOMPARE(request.variantId, flat.id);
  QCOMPARE(request.runtime, QString("none"));
  bindings["tools"]["unrelated"] = {
      {"path", (temp.path() + "/missing-unrelated.exe").toStdString()},
      {"verified", true}};
  QCOMPARE(
      launch::flatRequest(catalog, game.id, temp.path(), bindings).variantId,
      flat.id);
  QVERIFY_THROWS_EXCEPTION(
      install::Error,
      launch::flatRequest(catalog, game.id, temp.path(), bindings, vr.id));
  game.runtime.playing = true;
  resolveState(game);
  QCOMPARE(game.roles["stateLabel"].toString(), QString("Playing"));
  QVERIFY(game.roles["playing"].toBool());
}
void LaunchSteamTest::generatedFlatReadyOverridesLegacyRecipe() {
  GameRecord game; game.id="test-game"; game.hasRecipe=true;
  Variant vr; vr.id="legacy-vr"; vr.quality="true-3d"; vr.status="stable"; vr.media={"disc"}; vr.tools={"vr-wrapper"};
  Variant flat; flat.id="flat-mame"; flat.quality="flat"; flat.status="stable"; flat.generated=true; flat.media={"disc"}; flat.tools={"mame"};
  game.variants={vr,flat}; game.runtime.mediaFound={"disc"}; game.runtime.toolsOk={"mame"};
  resolveState(game); QCOMPARE(game.roles["baseState"].toInt(),int(GameState::Installed));
  QCOMPARE(game.roles["preferredVariantId"].toString(),flat.id); QVERIFY(!game.roles["reinstallVisible"].toBool());
  game.runtime.selectedVariantId=vr.id; resolveState(game);
  QCOMPARE(game.roles["preferredVariantId"].toString(),vr.id); QCOMPARE(game.roles["baseState"].toInt(),int(GameState::NeedsEmulator));
  game.runtime.toolsOk << "vr-wrapper"; resolveState(game); QCOMPARE(game.roles["baseState"].toInt(),int(GameState::ReadyToInstall));
  game.runtime.variants={{vr.id,true,true,true,false,{}}}; resolveState(game); QCOMPARE(game.roles["baseState"].toInt(),int(GameState::Installed));
  game.runtime.variants[0].manifestExists=false; resolveState(game); QCOMPARE(game.roles["baseState"].toInt(),int(GameState::ReadyToInstall));
}
void LaunchSteamTest::childLaunchAndLogs_data() {
  QTest::addColumn<int>("exitCode");
  QTest::addColumn<bool>("prepare");
  QTest::newRow("normal") << 0 << false;
  QTest::newRow("nonzero") << 7 << false;
  QTest::newRow("prepared-profile") << 0 << true;
}
void LaunchSteamTest::childLaunchAndLogs() {
  QFETCH(int, exitCode);
  QFETCH(bool, prepare);
  QTemporaryDir temp;
  launch::Request r;
  r.root = temp.path();
  r.gameId = "test-game";
  r.variantId = "flat-test";
  r.prepareProfile = prepare;
  r.plan.executable = QCoreApplication::applicationFilePath();
  r.plan.cwd = temp.path();
  r.plan.args = {"--synthetic-child", QString::number(exitCode)};
  if (prepare) {
    r.plan.cwd = temp.path() + "/user/emulator-profiles/synthetic/test-game";
    r.plan.writableDirs = {r.plan.cwd};
    r.plan.data = {{"variant", "synthetic"}, {"game", "test-game"}};
  }
  r.runtimeInputs.environment.insert("XR_RUNTIME_JSON",
                                     "inherited-synthetic-runtime");
  launch::LaunchService service;
  RuntimeState current;
  current.mediaFound = {"preserved"};
  service.setRuntimeStateSource([&](const QString &) { return current; });
  connect(&service, &launch::LaunchService::runtimeStateReady, this,
          [&](const RuntimeState &s) { current = s; });
  bool sawPlaying = false;
  connect(&service, &launch::LaunchService::started, this,
          [&](const QString &) { sawPlaying = current.playing; });
  QSignalSpy done(&service, &launch::LaunchService::finished),
      raised(&service, &launch::LaunchService::raiseHubRequested);
  QVERIFY(service.start(r));
  QVERIFY(service.playing());
  QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
  QVERIFY(!service.playing());
  QCOMPARE(done[0][1].toInt(), exitCode);
  QCOMPARE(raised.count(), 1);
  QVERIFY(sawPlaying);
  QCOMPARE(current.mediaFound, QStringList{"preserved"});
  QVERIFY(current.lastPlayed > 0);
  QVERIFY(!current.playing);
  const auto log = done[0][3].toString();
  QVERIFY(install::readBytes(log).contains("XR=inherited-synthetic-runtime"));
  QCOMPARE(launch::lastLines(log).split('\n').size(), 40);
  if (exitCode)
    QVERIFY(done[0][2].toString().contains("line 49"));
}
void LaunchSteamTest::sameGameLock() {
  QTemporaryDir temp;
  launch::Request r;
  r.root = temp.path();
  r.gameId = "test-game";
  r.variantId = "flat-test";
  r.prepareProfile = false;
  r.plan.executable = QCoreApplication::applicationFilePath();
  r.plan.cwd = temp.path();
  r.plan.args = {"--synthetic-child", "0", "wait"};
  launch::LaunchService one, two;
  QSignalSpy done(&one, &launch::LaunchService::finished),
      rejected(&two, &launch::LaunchService::finished);
  QVERIFY(one.start(r));
  QVERIFY(!two.start(r));
  QCOMPARE(rejected.count(), 1);
  QVERIFY(rejected[0][2].toString().contains("already running"));
  QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
}
void LaunchSteamTest::immediateCancel_data() {
  QTest::addColumn<bool>("prepare");
  QTest::newRow("direct") << false;
  QTest::newRow("preparation") << true;
}
void LaunchSteamTest::immediateCancel() {
  QFETCH(bool, prepare);
  QTemporaryDir temp;
  launch::Request r;
  r.root = temp.path();
  r.gameId = "test-game";
  r.variantId = "flat-test";
  r.prepareProfile = prepare;
  r.plan.executable = QCoreApplication::applicationFilePath();
  r.plan.cwd = temp.path();
  r.plan.args = {"--synthetic-child", "0"};
  launch::LaunchService service;
  QSignalSpy started(&service, &launch::LaunchService::started),
      done(&service, &launch::LaunchService::finished);
  connect(&service, &launch::LaunchService::playingChanged, &service, [&] {
    if (service.playing())
      service.stop();
  });
  QVERIFY(!service.start(r));
  QTest::qWait(100);
  QCOMPARE(started.count(), 0);
  QCOMPARE(done.count(), 1);
  QVERIFY(!service.playing());
}
int main(int argc, char **argv) {
  if (argc > 1 && QByteArray(argv[1]) == "--synthetic-child") {
    QTextStream out(stdout);
    out << "XR=" << qEnvironmentVariable("XR_RUNTIME_JSON") << '\n';
    for (int i = 0; i < 50; ++i)
      out << "line " << i << '\n';
    out.flush();
    if (argc > 3)
      QThread::msleep(1000);
    return QByteArray(argv[2]).toInt();
  }
  QGuiApplication app(argc, argv);
  LaunchSteamTest tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "LaunchSteamTest.moc"
