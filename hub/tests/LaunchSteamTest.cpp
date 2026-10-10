// SPDX-License-Identifier: GPL-3.0-only
#include "core/install/Support.h"
#include "core/launch/Launch.h"
#include "core/steam/Steam.h"
#include <QGuiApplication>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QSemaphore>
#include <QScopeGuard>
#include <QtConcurrent/QtConcurrentRun>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QtEndian>
#include <csignal>
#include <cstdlib>
#include <limits>
using namespace ac;
namespace {
void write(const QString &path, const QByteArray &bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  install::atomicWrite(path, bytes);
}
steam::Shortcut shortcut() {
  return {"test-game",
          "Synthetic Game",
          "/Synthetic/hub.exe",
          "/Synthetic",
          false,
          0,
          0,
          "flat-synthetic"};
}
steam::WriteRequest syntheticSteamRequest(const QString &root, const QString &copy,
                                          const QString &game, const QString &account = "123") {
  steam::WriteRequest request;
  request.steamRoot = root + "/synthetic-steam";
  request.userRoot = root + "/copy-" + copy;
  request.accountId = account;
  request.shortcut = shortcut(); request.shortcut.gameId = game;
  request.shortcut.title = "Synthetic " + game;
  return request;
}
install::Request syntheticManagedTool(const QString &root) {
  write(root + "/data/content-guard.toml", install::readBytes(QString(AC_CATALOG_ROOT) + "/data/content-guard.toml"));
  install::Request request; request.root=root; request.gameId="tool-synthetic"; request.variantId="windows-x64";
  request.runtime.gameId=request.gameId;
  const auto source=root+"/games/tool-synthetic/setup/synthetic-tool.exe";
  install::atomicCopy(QCoreApplication::applicationFilePath(),source);
  request.recipe={{"format",1},{"variant",{{"windows-x64",{
      {"status","stable"},{"version","v1"},{"install_dir","emulators/synthetic"},
      {"installed_when","file:${install_dir}/synthetic-tool.exe"},
      {"step",Json::array({{{"id","copy"},{"do","copy"},{"from",source.toStdString()},
                           {"to","${install_dir}/synthetic-tool.exe"}},
                          {{"id","config"},{"do","write-config"},{"file","${install_dir}/settings.ini"},
                           {"format","ini"},{"create",true},{"set",Json::array({{{"section","Synthetic"},{"key","enabled"},{"value",1}}})}}})}
  }}}}};
  return request;
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
  void steamCopiesShareTargetLock_data();
  void steamCopiesShareTargetLock();
  void managedToolUsersExcludeMutation();
  void flatPlanRetainsOriginalPayload();
  void sharedResourceUseMutationRaceAndCrash();
  void sortedResourcesRefuseWithoutPartialReservation();
  void orphanedChildKeepsUsageLease_data();
  void orphanedChildKeepsUsageLease();
  void binaryRoundTrip();
  void malformed_data();
  void malformed();
  void appIds();
  void ownedEditPreservesUnownedAndUnknown();
  void userFieldsPreservedAndPreviewed();
  void pinnedVariants();
  void variantRemovalRefused_data();
  void variantRemovalRefused();
  void sameTitlesDisambiguated();
  void rotationFailureAdvisory();
  void steamtoolVariantRequest();
  void finalWriteGuards();
  void finalArtGuards_data();
  void finalArtGuards();
  void noOpAndFirstBackup();
  void multipleAccountsAndUnicodePaths();
  void collisionRefused();
  void previewApprovalClosedAndChanged();
  void backupsAndOwnedRemoval();
  void customArtAndReceiptChanges();
  void supportChangesRefused();
  void runtimePrecedenceAndLibraries();
  void bindingChangesRefused();
  void chdReceiptNeedsValidatedBounds_data();
  void chdReceiptNeedsValidatedBounds();
  void toolChangesRefused();
  void flatSelectionAndPlayingRole();
  void generatedFlatReadyOverridesLegacyRecipe();
  void childLaunchAndLogs_data();
  void childLaunchAndLogs();
  void sameGameLock();
  void boundedLogsAndConcurrentLastPlayed();
  void lastPlayedLockRefusal();
  void transientLastPlayedLockHandoff();
  void completionIndependentOfGlobalPool();
  void pendingPersistenceTeardownBounded();
  void completionBlocksReentrantStart();
  void finishedCanStartReplacementWithoutStaleRaise();
  void hungChildCanBeStopped();
  void immediateCancel_data();
  void immediateCancel();
};
void LaunchSteamTest::steamCopiesShareTargetLock_data() {
  QTest::addColumn<bool>("distinctTarget"); QTest::addColumn<bool>("caseAlias");
  QTest::newRow("same-target-two-copies") << false << false;
  QTest::newRow("distinct-account") << true << false;
#ifdef Q_OS_WIN
  QTest::newRow("same-target-case-alias") << false << true;
#endif
}
void LaunchSteamTest::steamCopiesShareTargetLock() {
  QFETCH(bool, distinctTarget); QFETCH(bool, caseAlias);
  QTemporaryDir temp;
  const auto root=temp.path();
  QVERIFY(QDir().mkpath(root+"/synthetic-steam/userdata/123/config"));
  QVERIFY(QDir().mkpath(root+"/synthetic-steam/userdata/456/config"));
  const auto firstRequest=syntheticSteamRequest(root,"one","first-game");
  const auto secondRoot=caseAlias?root.toUpper():root;
  const auto secondRequest=syntheticSteamRequest(secondRoot,"two","second-game",distinctTarget?"456":"123");
  const auto readyA=root+"/ready-a", goA=root+"/go-a", readyB=root+"/ready-b", goB=root+"/go-b";
  QProcess first,second;
  // Copies can intentionally override every process temporary location.
  const auto tempA=root+"/temp-a",tempB=root+"/temp-b";
  QVERIFY(QDir().mkpath(tempA)); QVERIFY(QDir().mkpath(tempB));
  auto envA=QProcessEnvironment::systemEnvironment(),envB=envA;
  for(const auto &key:{"TEMP","TMP","TMPDIR"}) {envA.insert(key,tempA);envB.insert(key,tempB);}
  first.setProcessEnvironment(envA); second.setProcessEnvironment(envB);
  auto cleanup=qScopeGuard([&]{for(auto *process:{&first,&second})if(process->state()!=QProcess::NotRunning){process->kill();process->waitForFinished(3000);}});
  const auto executable=QCoreApplication::applicationFilePath();
  first.start(executable,{"--synthetic-steam-writer",root,"one","first-game","123",readyA,goA});
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(readyA),10000);
  second.start(executable,{"--synthetic-steam-writer",secondRoot,"two","second-game",distinctTarget?"456":"123",readyB,goB});
  QTRY_VERIFY_WITH_TIMEOUT(second.state()==QProcess::NotRunning || QFileInfo::exists(readyB),10000);
  const bool excluded=second.state()==QProcess::NotRunning && second.exitCode()!=0 &&
      second.readAllStandardOutput().contains("E_BUSY") && !QFileInfo::exists(readyB);
  if(distinctTarget) {
    QVERIFY(QFileInfo::exists(readyB)); write(goB,"go"); QVERIFY(second.waitForFinished(10000)); QCOMPARE(second.exitCode(),0);
    QCOMPARE(first.state(),QProcess::Running); // Different target did not wait for the first writer.
  }
  write(goA,"go"); QVERIFY(first.waitForFinished(10000)); QCOMPARE(first.exitCode(),0);
  if(!distinctTarget && QFileInfo::exists(readyB)) {write(goB,"go"); QVERIFY(second.waitForFinished(10000));}
  if(!distinctTarget) {
    QVERIFY2(excluded,"Separate Hub copies both reached the same Steam replacement boundary");
    const auto target=steam::preview(firstRequest).target;
    QVERIFY(steam::edit(install::readBytes(target),firstRequest.shortcut,true).ownedFound);
    // Retry uses a fresh owner preview, preserving the first writer's addition.
    steam::apply(secondRequest,steam::preview(secondRequest),true,[]{return false;});
    const auto bytes=install::readBytes(target);
    QVERIFY(steam::edit(bytes,firstRequest.shortcut,true).ownedFound);
    QVERIFY(steam::edit(bytes,secondRequest.shortcut,true).ownedFound);
  } else {
    QVERIFY(steam::edit(install::readBytes(steam::preview(firstRequest).target),firstRequest.shortcut,true).ownedFound);
    QVERIFY(steam::edit(install::readBytes(steam::preview(secondRequest).target),secondRequest.shortcut,true).ownedFound);
  }
}
void LaunchSteamTest::managedToolUsersExcludeMutation() {
  QTemporaryDir owner, portable;
  auto request=syntheticManagedTool(owner.path()); install::Options options; options.survivalMs=0;
  const auto installed=install::Engine(options).install(request); QVERIFY2(installed.success,qPrintable(installed.code+": "+installed.message));
  const auto payload=owner.path()+"/emulators/synthetic";
#ifndef Q_OS_WIN
  // The Windows-recipe fixture is copied atomically, which does not retain Unix
  // execute bits. Grant only this owned synthetic child's owner execute bit.
  const auto child=payload+"/synthetic-tool.exe";
  QVERIFY(QFile::setPermissions(child,QFile::permissions(child)|QFileDevice::ExeOwner));
#endif
  launch::Request launch; launch.root=portable.path(); launch.gameId="first-dependent"; launch.variantId="flat-synthetic";
  launch.prepareProfile=false; launch.plan.executable=payload+"/synthetic-tool.exe"; launch.plan.cwd=portable.path();
  launch.plan.args={"--synthetic-child","0","hang"}; launch.plan.payloadRoots={payload};
  launch.runtimeInputs.environment.insert("XR_RUNTIME_JSON","synthetic-unused");
  auto other=launch; other.gameId="second-dependent";
  launch::LaunchService first,second;
  QSignalSpy firstStarted(&first,&launch::LaunchService::started),secondStarted(&second,&launch::LaunchService::started);
  QVERIFY(first.start(launch)); QVERIFY(second.start(other));
  QTRY_COMPARE_WITH_TIMEOUT(firstStarted.count(),1,10000); QTRY_COMPARE_WITH_TIMEOUT(secondStarted.count(),1,10000);
  const auto journal=install::stateBase(request)+".journal.jsonl";
  const auto before=install::readBytes(journal);
  QProcess remover;
  remover.start(QCoreApplication::applicationDirPath()+"/hubtool",
      {"--data-root",QString(AC_CATALOG_ROOT),"--install-root",owner.path(),"uninstall","tool-synthetic","windows-x64"});
  QVERIFY(remover.waitForFinished(10000));
  QVERIFY2(remover.readAllStandardOutput().contains("Another Hub"),"Removal did not acquire the same shared payload guard as launch");
  QCOMPARE(install::readBytes(journal),before);
  QVERIFY(QFileInfo(payload+"/settings.ini").isFile());
  first.stop(); QTRY_VERIFY_WITH_TIMEOUT(!first.busy(),7000);
  const auto stillUsed=install::Engine(options).uninstall(request); QVERIFY(!stillUsed.success);
  QCOMPARE(stillUsed.code,QString("E_LOCKED")); QCOMPARE(install::readBytes(journal),before);
  // A distinct managed resource remains independently usable.
  QTemporaryDir unrelated; auto independent=syntheticManagedTool(unrelated.path());
  QVERIFY(install::Engine(options).install(independent).success); QVERIFY(install::Engine(options).uninstall(independent).success);
  second.stop(); QTRY_VERIFY_WITH_TIMEOUT(!second.busy(),7000);
  QVERIFY(install::Engine(options).uninstall(request).success);
  QVERIFY(!QFileInfo(payload+"/synthetic-tool.exe").exists());
}
void LaunchSteamTest::flatPlanRetainsOriginalPayload() {
  QTemporaryDir temp;
  const auto original=temp.path()+"/emulators/pcsx2/bin";
  write(original+"/pcsx2.exe","synthetic executable; never run");
  const auto media=temp.path()+"/synthetic.iso", bios=temp.path()+"/synthetic.rom";
  write(media,"synthetic medium"); write(bios,"synthetic bios");
  GameRecord game; game.id="synthetic"; game.raw={{"hardware","sony-ps2"},{"media",Json::array({{{"kind","disc"},{"id","disc"}},{{"kind","bios"},{"id","bios"}}})}};
  Json emulator{{"id","pcsx2"},{"launch",{{"args",Json::array({"${media.disc}"})}}}};
  Json bindings{{"tools",{{"pcsx2",{{"path",(original+"/pcsx2.exe").toStdString()}}}}},
                {"media",{{"disc",{{"path",media.toStdString()},{"verified",true}}},
                           {"bios",{{"path",bios.toStdString()},{"verified",true}}}}}};
  install::LaunchPlan plan; try {plan=install::makeFlatLaunchPlan(game,emulator,temp.path(),bindings);} catch(const install::Error &error){QFAIL(qPrintable(error.code+": "+QString::fromUtf8(error.what())));}
  QVERIFY(plan.executable.contains("/user/emulator-profiles/pcsx2/"));
  QVERIFY2(plan.payloadRoots.contains(temp.path()+"/emulators/pcsx2"),"Clone plan lost its original shared tool resource");
}
void LaunchSteamTest::sharedResourceUseMutationRaceAndCrash() {
  QTemporaryDir temp;
  const auto payload=temp.path()+"/payload",ready=temp.path()+"/reader-ready",go=temp.path()+"/reader-go";
  QVERIFY(QDir().mkpath(payload));
  QProcess reader;
  auto cleanup=qScopeGuard([&]{if(reader.state()!=QProcess::NotRunning){reader.kill();reader.waitForFinished(3000);}});
  const auto arguments=QStringList{"--synthetic-resource-user",payload,ready,go,"crash"};
  {
    install::ResourceLocks mutation({payload},install::ResourceAccess::Mutation);
    reader.start(QCoreApplication::applicationFilePath(),arguments);
    QVERIFY(reader.waitForFinished(10000)); QCOMPARE(reader.exitCode(),1);
    QVERIFY(reader.readAllStandardOutput().contains("E_LOCKED")); QVERIFY(!QFileInfo::exists(ready));
  }
  reader.start(QCoreApplication::applicationFilePath(),arguments);
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(ready),10000);
  const auto directory=install::resourceLockDirectory(payload);
  QVERIFY(!QDir(directory).entryList({"use-*.lock"}).isEmpty());
  try {install::ResourceLocks denied({payload},install::ResourceAccess::Mutation);QFAIL("Mutation entered while a reader held use");}
  catch(const install::Error &error){QCOMPARE(error.code,QString("E_LOCKED"));}
  write(go,"go"); QVERIFY(reader.waitForFinished(10000)); QCOMPARE(reader.exitCode(),0);
  QVERIFY(!QDir(directory).entryList({"use-*.lock"}).isEmpty()); // Abrupt exit bypassed destructor.
  install::ResourceLocks recovered({payload},install::ResourceAccess::Mutation);
  QVERIFY(QDir(directory).entryList({"use-*.lock"}).isEmpty());
}
void LaunchSteamTest::sortedResourcesRefuseWithoutPartialReservation() {
  QTemporaryDir temp;
  const auto a=temp.path()+"/a",b=temp.path()+"/b";
  QVERIFY(QDir().mkpath(a)); QVERIFY(QDir().mkpath(b));
  install::ResourceLocks use({b},install::ResourceAccess::Use);
  QElapsedTimer elapsed; elapsed.start();
  try {install::ResourceLocks denied({b,a},install::ResourceAccess::Mutation);QFAIL("Mutation entered in-use resource");}
  catch(const install::Error &error){QCOMPARE(error.code,QString("E_LOCKED"));}
  QVERIFY(elapsed.elapsed()<1000);
  install::ResourceLocks aStillFree({a},install::ResourceAccess::Mutation); // Earlier sorted reservation unwound.
}
void LaunchSteamTest::orphanedChildKeepsUsageLease_data() {
  QTest::addColumn<QString>("evidence");
  QTest::newRow("surviving-original-child") << QString("original");
  QTest::newRow("unknown-child-creation") << QString("unknown");
  QTest::newRow("recycled-pid-creation") << QString("recycled");
}
void LaunchSteamTest::orphanedChildKeepsUsageLease() {
  QFETCH(QString,evidence);
  QTemporaryDir temp;
  const auto payload=temp.path()+"/payload",ready=temp.path()+"/ready",go=temp.path()+"/go";
  QVERIFY(QDir().mkpath(payload));
  QProcess child,holder;
  auto cleanup=qScopeGuard([&]{for(auto *process:{&child,&holder})if(process->state()!=QProcess::NotRunning){process->kill();process->waitForFinished(3000);}});
  child.start(QCoreApplication::applicationFilePath(),{"--synthetic-child","0","hang"});
  QVERIFY(child.waitForStarted(10000));
  holder.start(QCoreApplication::applicationFilePath(),{"--synthetic-resource-user",payload,ready,go,"crash",QString::number(child.processId())});
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(ready),10000);
  write(go,"go"); QVERIFY(holder.waitForFinished(10000)); QCOMPARE(holder.exitCode(),0);
  QCOMPARE(child.state(),QProcess::Running);
  const auto directory=install::resourceLockDirectory(payload);
  const auto records=QDir(directory).entryList({"*.child.json"}); QCOMPARE(records.size(),1);
  if(evidence!="original") {
    const auto path=directory+"/"+records[0];
    auto record=Json::parse(install::readBytes(path).toStdString());
    // Simulate an unavailable identity query or a previous PID creation, without
    // depending on OS PID allocation/reuse timing.
    record["child_identity"]=evidence=="unknown"?"":"0";
    write(path,QByteArray::fromStdString(record.dump()));
  }
  bool refused=false;
  try {install::ResourceLocks mutation({payload},install::ResourceAccess::Mutation);}
  catch(const install::Error &error){QCOMPARE(error.code,QString("E_LOCKED"));refused=true;}
  if(evidence=="recycled") {
    QVERIFY2(!refused,"A reused PID with a different creation incorrectly preserved the old child lease");
    QVERIFY(QDir(directory).entryList({"use-*"}).isEmpty());
  } else QVERIFY2(refused,"Stale Hub cleanup permitted mutation under its surviving tracked child");
  child.kill(); QVERIFY(child.waitForFinished(10000));
  install::ResourceLocks reclaimed({payload},install::ResourceAccess::Mutation);
  QVERIFY(QDir(directory).entryList({"use-*"}).isEmpty());
}
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
  QCOMPARE(steam::appId("\"/Synthetic/hub.exe\"", "Synthetic Game"),
           quint32(0xb7400df6));
  QCOMPARE(steam::appId("\"/tmp/hub\"", QString::fromUtf8("遊戲 Café")),
           quint32(0x9c6c4176));
  QCOMPARE(steam::launchId(0xcbf43926), quint64(0xcbf4392602000000ull));
  const auto maximumId = std::numeric_limits<quint32>::max();
  const auto stem = QString::number(maximumId);
  QCOMPARE(steam::artNames(maximumId),
           QStringList({stem + ".png", stem + "p.png",
                        stem + "_hero.png", stem + "_logo.png"}));
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
  s.executable = "/Moved/hub.exe";
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
void LaunchSteamTest::userFieldsPreservedAndPreviewed() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/steam/userdata/123/config");
  steam::WriteRequest r;
  r.steamRoot = temp.path() + "/steam";
  r.accountId = "123";
  r.userRoot = temp.path() + "/user";
  r.shortcut = shortcut();
  const auto defaults = steam::preview(r);
  QCOMPARE(defaults.json["userFields"]["icon"]["after"], Json(""));
  QCOMPARE(defaults.json["userFields"]["IsHidden"]["after"], Json(0));
  QCOMPARE(defaults.json["userFields"]["AllowOverlay"]["after"], Json(1));
  QCOMPARE(defaults.json["userFields"]["AllowDesktopConfig"]["after"], Json(1));
  QCOMPARE(defaults.json["userFields"]["LastPlayTime"]["after"], Json(0));
  auto d = steam::parse(steam::edit({}, r.shortcut).bytes);
  d.roots[0].raw.clear();
  auto &owned = d.roots[0].children[0];
  owned.raw.clear();
  for (auto &n : owned.children) {
    if (n.key == "icon") { n.payload = "/Synthetic/custom icon.png"; n.raw.clear(); }
    if (n.key == "IsHidden" || n.key == "LastPlayTime") {
      qToLittleEndian(quint32(42), n.payload.data()); n.raw.clear();
    }
    if (n.key == "AllowOverlay" || n.key == "AllowDesktopConfig") {
      qToLittleEndian(quint32(0), n.payload.data()); n.raw.clear();
    }
  }
  owned.children << steam::Node{1, "FlatpakAppID", "org.synthetic.Game", {}, {}, 8};
  const auto before = steam::serialize(d);
  const auto target = r.steamRoot + "/userdata/123/config/shortcuts.vdf";
  write(target, before);
  r.shortcut.title = "Updated synthetic";
  r.shortcut.lastPlayed = 999;
  const auto p = steam::preview(r);
  const auto prior = steam::parse(before), after = steam::parse(p.edit.bytes);
  for (const auto *key : {"icon", "IsHidden", "AllowOverlay", "AllowDesktopConfig", "LastPlayTime", "FlatpakAppID"})
    QCOMPARE(find(after.roots[0].children[0], key)->raw,
             find(prior.roots[0].children[0], key)->raw);
  QCOMPARE(p.json["userFields"]["icon"]["after"], Json("/Synthetic/custom icon.png"));
  QCOMPARE(p.json["userFields"]["IsHidden"]["after"], Json(42));
  QCOMPARE(p.json["userFields"]["AllowOverlay"]["after"], Json(0));
  QCOMPARE(p.json["userFields"]["LastPlayTime"]["after"], Json(42));
  for (const auto *key : {"icon", "IsHidden", "AllowOverlay", "AllowDesktopConfig", "LastPlayTime"}) {
    QCOMPARE(p.json["userFields"][key]["before"], p.json["userFields"][key]["after"]);
    QCOMPARE(p.json["userFields"][key]["action"], Json("preserve"));
  }
  // Missing user fields in legacy entries stay absent, too.
  d.roots[0].children[0].children.removeIf([](const steam::Node &n) { return n.key == "icon"; });
  const auto missing = steam::edit(steam::serialize(d), r.shortcut);
  QVERIFY(!find(steam::parse(missing.bytes).roots[0].children[0], "icon"));
  d.roots[0].children[0].children << steam::Node{1, "icon", "one", {}, {}, 8}
                                << steam::Node{1, "icon", "two", {}, {}, 8};
  QVERIFY_THROWS_EXCEPTION(install::Error, steam::edit(steam::serialize(d), r.shortcut));
}
void LaunchSteamTest::pinnedVariants() {
  auto s = shortcut();
  auto first = steam::edit({}, s);
  QCOMPARE(find(steam::parse(first.bytes).roots[0].children[0], "LaunchOptions")->payload,
           QByteArray("--launch test-game --variant flat-synthetic"));
  s.variantId = "legacy-vr"; s.vr = true;
  const auto vr = steam::edit(first.bytes, s);
  QCOMPARE(find(steam::parse(vr.bytes).roots[0].children[0], "LaunchOptions")->payload,
           QByteArray("--launch test-game --variant legacy-vr"));
  for (const auto &id : QStringList{"", "bad id", "flat\n", "--flat", "../escape", "flat\" --other", QString::fromUtf8("遊戲")}) {
    s.variantId = id;
    QVERIFY_THROWS_EXCEPTION(install::Error, steam::edit(vr.bytes, s));
  }
  // Removal of a legacy shortcut does not invent a flat variant.
  s.variantId.clear();
  QVERIFY_THROWS_EXCEPTION(install::Error, steam::edit(vr.bytes, s, true));
}
void LaunchSteamTest::variantRemovalRefused_data() {
  QTest::addColumn<QByteArray>("options");
  QTest::newRow("different-variant") << QByteArray("--launch test-game --variant other-flat");
  QTest::newRow("legacy-unpinned") << QByteArray("--launch test-game");
  QTest::newRow("wrong-game") << QByteArray("--launch other-game --variant flat-synthetic");
  QTest::newRow("extra-options") << QByteArray("--launch test-game --variant flat-synthetic --variant other-flat");
  QTest::newRow("absent") << QByteArray();
}
void LaunchSteamTest::variantRemovalRefused() {
  QFETCH(QByteArray, options);
  QTemporaryDir temp;
  steam::WriteRequest request;
  request.steamRoot = temp.path() + "/steam";
  request.userRoot = temp.path() + "/user";
  request.accountId = "123";
  request.shortcut = shortcut();
  request.remove = true;
  auto document = steam::parse(steam::edit({}, request.shortcut).bytes);
  document.roots[0].raw.clear();
  auto &owned = document.roots[0].children[0];
  owned.raw.clear();
  for (auto &node : owned.children)
    if (node.key == "LaunchOptions") { node.raw.clear(); node.payload = options; }
  const auto before = steam::serialize(document);
  const auto target = request.steamRoot + "/userdata/123/config/shortcuts.vdf";
  write(target, before);
  QVERIFY_THROWS_EXCEPTION(install::Error, steam::preview(request));
  QCOMPARE(install::readBytes(target), before);
  QVERIFY(!QFileInfo::exists(request.userRoot));
  request.remove = false;
  const auto repaired = steam::edit(before, request.shortcut);
  QVERIFY(steam::edit(repaired.bytes, request.shortcut, true).changed);
}
void LaunchSteamTest::sameTitlesDisambiguated() {
  auto a = shortcut(), b = a;
  b.gameId = "other-game";
  const auto first = steam::edit({}, a);
  const auto second = steam::edit(first.bytes, b);
  QVERIFY(first.id != second.id);
  const auto document = steam::parse(second.bytes);
  QCOMPARE(document.roots[0].children.size(), 2);
  QVERIFY(!find(document.roots[0].children[0], "AladdinsCastleDisambiguator"));
  QVERIFY(!find(document.roots[0].children[1], "AladdinsCastleDisambiguator"));
  const auto quoted = '\"' + QDir::toNativeSeparators(a.executable) + '\"';
  QCOMPARE(first.id, steam::appId(quoted, a.title + "\nAladdinsCastle:" + a.gameId));
  QCOMPARE(second.id, steam::appId(quoted, b.title + "\nAladdinsCastle:" + b.gameId));
  // Stored identity survives relocation/renaming, and removal targets one game.
  b.title = "Renamed synthetic";
  b.executable = "/Synthetic/moved-hub.exe";
  const auto updated = steam::edit(second.bytes, b);
  QCOMPARE(updated.id, second.id);
  const auto removed = steam::edit(updated.bytes, a, true);
  QCOMPARE(steam::parse(removed.bytes).roots[0].children.size(), 1);
  QCOMPARE(steam::edit(removed.bytes, b).id, second.id);
  // Legacy owned AppIds retain their ID without a newly invented disambiguator.
  auto legacy = steam::parse(first.bytes);
  legacy.roots[0].raw.clear();
  auto &entry = legacy.roots[0].children[0]; entry.raw.clear();
  entry.children << steam::Node{1, "AladdinsCastleDisambiguator", "obsolete", {}, {}, 8};
  for (auto &node : entry.children)
    if (node.key == "appid") { node.raw.clear(); qToLittleEndian(quint32(0x81234567), node.payload.data()); }
  const auto preserved = steam::edit(steam::serialize(legacy), a);
  QCOMPARE(preserved.id, quint32(0x81234567));
  QVERIFY(!find(steam::parse(preserved.bytes).roots[0].children[0], "AladdinsCastleDisambiguator"));
}
void LaunchSteamTest::rotationFailureAdvisory() {
  QTemporaryDir temp;
  const auto logs = temp.path() + "/user/logs/test-game";
  for (int i = 0; i < 21; ++i)
    write(logs + "/old-" + QString::number(i) + ".log", "synthetic old log");
  int attempts = 0;
  const auto warnings = launch::rotateLaunchLogs(temp.path(), logs, [&](const QString &) { ++attempts; return false; });
  QCOMPARE(attempts, 2);
  QCOMPARE(warnings.size(), 2);
  QVERIFY(warnings.first().contains("launch will continue"));
  QCOMPARE(QDir(logs).entryList({"*.log"}, QDir::Files).size(), 21);
  QCOMPARE(launch::rotateLaunchLogs(temp.path(), logs).size(), 0);
  QCOMPARE(QDir(logs).entryList({"*.log"}, QDir::Files).size(), 19);
}
void LaunchSteamTest::steamtoolVariantRequest() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/steam/userdata/123/config");
  const auto requestPath = temp.path() + "/request.json";
  Json request{{"steamRoot", (temp.path() + "/steam").toStdString()},
               {"userRoot", (temp.path() + "/user").toStdString()},
               {"accountId", "123"}, {"gameId", "test-game"},
               {"title", "Synthetic"}, {"executable", "/Synthetic/hub"},
               {"startDir", "/Synthetic"}, {"variantId", "flat-synthetic"}};
  write(requestPath, QByteArray::fromStdString(request.dump()));
#ifdef Q_OS_WIN
  const auto tool = QCoreApplication::applicationDirPath() + "/steamtool.exe";
#else
  const auto tool = QCoreApplication::applicationDirPath() + "/steamtool";
#endif
  QProcess process;
  process.start(tool, {"preview", requestPath});
  QVERIFY(process.waitForFinished(10000));
  QCOMPARE(process.exitCode(), 0);
  const auto p = Json::parse(process.readAllStandardOutput().toStdString());
  QCOMPARE(p["variantId"], Json("flat-synthetic"));
  QCOMPARE(p["launchOptions"], Json("--launch test-game --variant flat-synthetic"));
  QVERIFY(!QFileInfo::exists(temp.path() + "/steam/userdata/123/config/shortcuts.vdf"));
  request.erase("variantId");
  write(requestPath, QByteArray::fromStdString(request.dump()));
  process.start(tool, {"preview", requestPath});
  QVERIFY(process.waitForFinished(10000));
  QCOMPARE(process.exitCode(), 1);
  QVERIFY(process.readAllStandardError().contains("variant ID"));
}
void LaunchSteamTest::finalWriteGuards() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/steam/userdata/123/config");
  steam::WriteRequest r;
  r.steamRoot = temp.path() + "/steam"; r.accountId = "123";
  r.userRoot = temp.path() + "/user"; r.shortcut = shortcut();
  auto initial = steam::edit({}, r.shortcut).bytes;
  const auto target = r.steamRoot + "/userdata/123/config/shortcuts.vdf";
  write(target, initial);
  r.shortcut.title = "Updated synthetic";
  auto p = steam::preview(r);
  int checks = 0;
  try {
    steam::apply(r, p, true, [&] { return ++checks == 2; });
    QFAIL("Steam starting immediately before replacement was not rejected");
  } catch (const install::Error &e) { QCOMPARE(e.code, QString("E_STEAM_RUNNING")); }
  QCOMPARE(install::readBytes(target), initial);
  auto external = r.shortcut; external.title = "External editor";
  const auto externalBytes = steam::edit(initial, external).bytes;
  checks = 0;
  try {
    steam::apply(r, p, true, [&] {
      if (++checks == 2) write(target, externalBytes);
      return false;
    });
    QFAIL("Last-moment external write was not rejected");
  } catch (const install::Error &e) { QCOMPARE(e.code, QString("E_PREVIEW_CHANGED")); }
  QCOMPARE(install::readBytes(target), externalBytes);
}
void LaunchSteamTest::finalArtGuards_data() {
  QTest::addColumn<bool>("remove");
  QTest::newRow("write") << false;
  QTest::newRow("remove") << true;
}
void LaunchSteamTest::finalArtGuards() {
  QFETCH(bool, remove);
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/steam/userdata/123/config");
  steam::WriteRequest r;
  r.steamRoot = temp.path() + "/steam"; r.accountId = "123";
  r.userRoot = temp.path() + "/user"; r.shortcut = shortcut();
  r.art = steam::fallbackArt("Synthetic", "Test");
  steam::apply(r, steam::preview(r), true, [] { return false; });
  r.shortcut.title = "Updated synthetic";
  r.remove = remove;
  if (!remove) {
    QImage changed(10, 10, QImage::Format_ARGB32); changed.fill(Qt::red);
    r.art.clear(); r.art["header"] = changed;
  }
  const auto p = steam::preview(r);
  int checks = 0;
  const auto result = steam::apply(r, p, true, [&] {
    if (++checks == 4) write(p.artPaths[0], "owner-edit-during-apply");
    return false;
  });
  QCOMPARE(install::readBytes(p.artPaths[0]), QByteArray("owner-edit-during-apply"));
  QVERIFY(result.warnings.join('\n').contains("changed after preview"));
}
void LaunchSteamTest::noOpAndFirstBackup() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/steam/userdata/123/config");
  steam::WriteRequest r;
  r.steamRoot = temp.path() + "/steam"; r.accountId = "123";
  r.userRoot = temp.path() + "/user"; r.shortcut = shortcut();
  const auto initial = steam::edit({}, r.shortcut).bytes;
  auto p = steam::preview(r);
  write(p.target, initial);
  p = steam::preview(r);
  QVERIFY(!p.edit.changed);
  const auto modified = QFileInfo(p.target).lastModified();
  steam::apply(r, p, true, [] { return false; });
  QCOMPARE(install::readBytes(p.target), initial);
  QCOMPARE(QFileInfo(p.target).lastModified(), modified);
  QVERIFY(!QFileInfo::exists(p.backupFolder));
  for (int i = 0; i < 15; ++i) {
    r.shortcut.title = "Update " + QString::number(i);
    steam::apply(r, steam::preview(r), true, [] { return false; });
  }
  QCOMPARE(install::readBytes(p.backupFolder + "/shortcuts-first.bak"), initial);
  QCOMPARE(QDir(p.backupFolder).entryList({"shortcuts-recent-*.bak"}, QDir::Files).size(), 10);
  const auto count = QDir(p.backupFolder).entryList(QDir::Files).size();
  const auto latestModified = QFileInfo(p.target).lastModified();
  QVERIFY(!steam::apply(r, steam::preview(r), true, [] { return false; }).changed);
  QCOMPARE(QDir(p.backupFolder).entryList(QDir::Files).size(), count);
  QCOMPARE(QFileInfo(p.target).lastModified(), latestModified);
  QCOMPARE(install::readBytes(p.backupFolder + "/shortcuts-first.bak"), initial);
  r.art = steam::fallbackArt("Synthetic", "Test");
  steam::apply(r, steam::preview(r), true, [] { return false; });
  p = steam::preview(r);
  QList<QDateTime> artModified;
  for (const auto &path : p.artPaths) artModified << QFileInfo(path).lastModified();
  const auto artBackupCount = QDir(p.backupFolder).entryList(QDir::Files).size();
  QVERIFY(!steam::apply(r, p, true, [] { return false; }).changed);
  QCOMPARE(QDir(p.backupFolder).entryList(QDir::Files).size(), artBackupCount);
  for (qsizetype i = 0; i < p.artPaths.size(); ++i)
    QCOMPARE(QFileInfo(p.artPaths[i]).lastModified(), artModified[i]);
}
void LaunchSteamTest::multipleAccountsAndUnicodePaths() {
  QTemporaryDir temp;
  const auto root = temp.path() + QString::fromUtf8("/遊戲 Café folder");
  for (const auto &id : QStringList{"123", "456", "0", "invalid"})
    QDir().mkpath(root + "/userdata/" + id + "/config");
  QCOMPARE(steam::accounts(root), QStringList({"123", "456"}));
  steam::WriteRequest r;
  r.steamRoot = root; r.accountId = "456"; r.userRoot = temp.path() + "/user";
  r.shortcut = shortcut(); r.shortcut.executable = root + "/hub executable.exe";
  r.shortcut.startDir = root;
  const auto p = steam::preview(r);
  QCOMPARE(p.json["variantId"], Json("flat-synthetic"));
  steam::apply(r, p, true, [] { return false; });
  const auto stored = steam::parse(install::readBytes(p.target));
  QCOMPARE(QString::fromUtf8(find(stored.roots[0].children[0], "Exe")->payload),
           '"' + QDir::toNativeSeparators(r.shortcut.executable) + '"');
  QVERIFY(!QFileInfo::exists(root + "/userdata/123/config/shortcuts.vdf"));
  r.remove = true;
  r.shortcut.variantId = "another-flat";
  QVERIFY_THROWS_EXCEPTION(install::Error, steam::preview(r));
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
      QDir(p.backupFolder).entryList({"shortcuts-recent-*.bak"}, QDir::Files).size(),
      10);
  QCOMPARE(install::readBytes(p.backupFolder + "/shortcuts-first.bak"), QByteArray());
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
void LaunchSteamTest::chdReceiptNeedsValidatedBounds_data() {
  QTest::addColumn<int>("reference");
  QTest::newRow("primary-chd") << 0;
  QTest::newRow("support-path-chd") << 1;
  QTest::newRow("support-object-chd") << 2;
  QTest::newRow("support-requirement-chd") << 3;
  QTest::newRow("renamed-primary-chd") << 4;
  QTest::newRow("renamed-support-chd") << 5;
}
void LaunchSteamTest::chdReceiptNeedsValidatedBounds() {
  QFETCH(int, reference);
  QTemporaryDir temp;
  const auto chd = temp.path() + (reference >= 4 ? "/synthetic.dat" : "/synthetic.chd");
  const bool primaryChd = reference == 0 || reference == 4;
  const auto primary = primaryChd ? chd : temp.path() + "/synthetic.zip";
  write(primary, "synthetic saved-receipt primary");
  if (!primaryChd) write(chd, "synthetic saved-receipt support");
  Json binding{{"gameId", "test-game"}, {"requirementId", "TEST-00002"},
               {"path", primary.toStdString()}, {"verified", true}};
  if (reference == 1 || reference == 5) binding["supportPaths"] = Json::array({chd.toStdString()});
  if (reference == 2) binding["supportPaths"] = Json::array({{{"path", chd.toStdString()}}});
  if (reference == 3) binding["supportRequirements"] = Json::array({{{"set", "synthetic-support"}, {"sourcePath", chd.toStdString()}, {"entries", Json::array()}}});
  Json files = Json::array();
  for (const auto &path : QStringList{primary, chd}) {
    const QFileInfo file(path);
    files.push_back({{"path", path.toStdString()}, {"size", file.size()},
                     {"mtime", file.lastModified().toMSecsSinceEpoch()},
                     {"kind", path == chd ? "chd-synthetic" : "zip"}});
  }
  Json source{{"bindings", Json::array({binding})}, {"files", files},
              {"tools", {{"synthetic", {{"path", QCoreApplication::applicationFilePath().toStdString()},
                                           {"verified", true}}}}}};
  CatalogData catalog;
  GameRecord game; game.id = "test-game";
  game.raw = {{"media", Json::array({{{"kind", "disc"}, {"serial", "TEST-00002"}}})}};
  Variant route; route.id = "flat-synthetic"; route.quality = "flat"; route.tools = {"synthetic"};
  game.variants = {route}; catalog.games = {game};
  catalog.emulators = {{"synthetic", {{"id", "synthetic"}, {"launch", {{"args", Json::array({"${media.file}"})}}}}}};
  auto normalized = launch::normalizeBindings(source, game.id);
  QVERIFY(!normalized["media"]["TEST-00002"]["verified"].get<bool>());
  QVERIFY(normalized["media"]["TEST-00002"]["verificationError"].get<std::string>().find("scan again") != std::string::npos);
  QVERIFY_THROWS_EXCEPTION(install::Error, launch::flatRequest(catalog, game.id, temp.path(), source));
  // Current scanners emit this marker only for all physically checked CHDs.
  // Here synthetic bytes exercise receipt policy without opening CHD payloads.
  source["bindings"][0]["chdBounds"] = {{"version", 1}, {"paths", Json::array({chd.toStdString()})}};
  // A marker alone cannot upgrade old unchecked file-identity metadata.
  QVERIFY(!launch::normalizeBindings(source, game.id)["media"]["TEST-00002"]["verified"].get<bool>());
  for (auto &file : source["files"])
    if (file["path"] == chd.toStdString()) file["chdHeaderBoundsOk"] = true;
  normalized = launch::normalizeBindings(source, game.id);
  QVERIFY(normalized["media"]["TEST-00002"]["verified"].get<bool>());
  try {
    QCOMPARE(launch::flatRequest(catalog, game.id, temp.path(), source).variantId, route.id);
  } catch (const std::exception &error) {
    QFAIL(qPrintable(QString::fromUtf8(error.what())));
  }
  source["bindings"][0]["chdBounds"]["paths"] = Json::array();
  QVERIFY(!launch::normalizeBindings(source, game.id)["media"]["TEST-00002"]["verified"].get<bool>());
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
      {"media", Json::array({{{"kind", "disc"}, {"serial", "TEST-00002"}}})}};
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
       {{"TEST-00002", {{"path", path.toStdString()}, {"verified", true}}}}},
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
void LaunchSteamTest::boundedLogsAndConcurrentLastPlayed() {
  QTemporaryDir temp; QVERIFY(temp.isValid());
  for(int i=0;i<35;++i)write(temp.path()+"/user/logs/test-game/old-"+QString::number(i)+".log","old synthetic log");
  write(temp.path()+"/user/logs/test-game/preserve.txt","unrelated");
  launch::Request r;r.root=temp.path();r.gameId="test-game";r.variantId="flat-test";r.prepareProfile=false;r.plan.executable=QCoreApplication::applicationFilePath();r.plan.cwd=temp.path();r.plan.args={"--synthetic-child","0","wait"};
  launch::LaunchService one,two;QSignalSpy first(&one,&launch::LaunchService::finished),second(&two,&launch::LaunchService::finished), firstWarnings(&one,&launch::LaunchService::warning), secondWarnings(&two,&launch::LaunchService::warning);
  QVERIFY(one.start(r));r.gameId="other-game";QVERIFY(two.start(r));
  QTRY_COMPARE_WITH_TIMEOUT(first.count(),1,10000);QTRY_COMPARE_WITH_TIMEOUT(second.count(),1,10000);
  QCOMPARE(QDir(temp.path()+"/user/logs/test-game").entryList({"*.log"},QDir::Files).size(),20);QVERIFY(QFileInfo::exists(temp.path()+"/user/logs/test-game/preserve.txt"));
  QTRY_VERIFY_WITH_TIMEOUT(([&] {
    const auto path = temp.path() + "/user/last-played.json";
    if (!QFileInfo::exists(path)) return false;
    const auto times = Json::parse(install::readBytes(path).toStdString());
    return times.contains("test-game") && times.contains("other-game");
  })(), 5000);
  QCOMPARE(firstWarnings.count(), 0); QCOMPARE(secondWarnings.count(), 0);
}
void LaunchSteamTest::lastPlayedLockRefusal() {
  QTemporaryDir temp;QDir().mkpath(temp.path()+"/user/locks");QLockFile lock(temp.path()+"/user/locks/last-played.lock");QVERIFY(lock.tryLock());
  launch::Request r;r.root=temp.path();r.gameId="test-game";r.variantId="flat-test";r.prepareProfile=false;r.plan.executable=QCoreApplication::applicationFilePath();r.plan.cwd=temp.path();r.plan.args={"--synthetic-child","0"};
  launch::LaunchService service;
  QSignalSpy done(&service, &launch::LaunchService::finished), warnings(&service, &launch::LaunchService::warning);
  int heartbeats = 0;
  QTimer heartbeat; heartbeat.setInterval(10);
  connect(&heartbeat, &QTimer::timeout, this, [&] { ++heartbeats; });
  heartbeat.start();
  QElapsedTimer elapsed; elapsed.start();
  QVERIFY(service.start(r));
  QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
  QCOMPARE(done[0][1].toInt(), 0);
  QVERIFY(done[0][2].toString().isEmpty());
  QVERIFY(elapsed.elapsed() >= 1200);
  QVERIFY(elapsed.elapsed() < 3500); // Bounded worker lock wait, including process startup.
  QTRY_COMPARE_WITH_TIMEOUT(warnings.count(), 1, 5000);
  QVERIFY(warnings[0][1].toString().contains("Last-played state is locked"));
  QVERIFY(heartbeats > 30); // GUI event processing continues throughout the worker wait.
  QVERIFY(!service.busy());
  QVERIFY(!QFileInfo::exists(temp.path() + "/user/last-played.json"));
}
void LaunchSteamTest::transientLastPlayedLockHandoff() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/user/locks");
  QLockFile held(temp.path() + "/user/locks/last-played.lock");
  QVERIFY(held.tryLock());
  launch::Request request;
  request.root = temp.path(); request.gameId = "test-game"; request.variantId = "flat-test";
  request.prepareProfile = false; request.plan.executable = QCoreApplication::applicationFilePath();
  request.plan.cwd = temp.path(); request.plan.args = {"--synthetic-child", "0"};
  launch::LaunchService service;
  QSignalSpy done(&service, &launch::LaunchService::finished), warnings(&service, &launch::LaunchService::warning);
  connect(&service, &launch::LaunchService::playingChanged, this, [&] {
    if (!service.playing()) QTimer::singleShot(150, this, [&] { held.unlock(); });
  });
  QVERIFY(service.start(request));
  QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
  QCOMPARE(done[0][1].toInt(), 0); QVERIFY(done[0][2].toString().isEmpty());
  QCOMPARE(warnings.count(), 0);
  const auto times = Json::parse(install::readBytes(temp.path() + "/user/last-played.json").toStdString());
  QVERIFY(times.contains(request.gameId.toStdString())); QVERIFY(!service.busy());
}
void LaunchSteamTest::completionIndependentOfGlobalPool() {
  auto *global = QThreadPool::globalInstance();
  const auto previousLimit = global->maxThreadCount();
  global->setMaxThreadCount(1);
  QSemaphore entered, release;
  auto blocker = QtConcurrent::run(global, [&] { entered.release(); release.acquire(); });
  auto cleanup = qScopeGuard([&] { release.release(); blocker.waitForFinished(); global->setMaxThreadCount(previousLimit); });
  QVERIFY(entered.tryAcquire(1, 5000));
  QTemporaryDir temp;
  launch::Request request;
  request.root = temp.path(); request.gameId = "test-game"; request.variantId = "flat-test";
  request.prepareProfile = false; request.plan.executable = QCoreApplication::applicationFilePath();
  request.plan.cwd = temp.path(); request.plan.args = {"--synthetic-child", "0"};
  launch::LaunchService service;
  QSignalSpy done(&service, &launch::LaunchService::finished);
  QVERIFY(service.start(request));
  QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
  QVERIFY(!blocker.isFinished()); // Completion succeeded while the global worker remained occupied.
  QCOMPARE(done[0][1].toInt(), 0); QVERIFY(done[0][2].toString().isEmpty());
  QVERIFY(QFileInfo::exists(temp.path() + "/user/last-played.json"));
}
void LaunchSteamTest::pendingPersistenceTeardownBounded() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/user/locks");
  QLockFile held(temp.path() + "/user/locks/last-played.lock");
  QVERIFY(held.tryLock());
  launch::Request request;
  request.root = temp.path(); request.gameId = "test-game"; request.variantId = "flat-test";
  request.prepareProfile = false; request.plan.executable = QCoreApplication::applicationFilePath();
  request.plan.cwd = temp.path(); request.plan.args = {"--synthetic-child", "0"};
  auto service = std::make_unique<launch::LaunchService>();
  QVERIFY(service->start(request));
  QTRY_VERIFY_WITH_TIMEOUT(!service->playing() && service->busy(), 5000);
  QElapsedTimer elapsed; elapsed.start();
  service.reset(); // Drains its owned pool; no detached task retains session state.
  QVERIFY(elapsed.elapsed() < 2500);
  QVERIFY(!QFileInfo::exists(temp.path() + "/user/last-played.json"));
}
void LaunchSteamTest::completionBlocksReentrantStart() {
  QTemporaryDir temp;
  QDir().mkpath(temp.path() + "/user/locks");
  QLockFile held(temp.path() + "/user/locks/last-played.lock");
  QVERIFY(held.tryLock());
  launch::Request first;
  first.root = temp.path(); first.gameId = "test-game"; first.variantId = "flat-test";
  first.prepareProfile = false; first.plan.executable = QCoreApplication::applicationFilePath();
  first.plan.cwd = temp.path(); first.plan.args = {"--synthetic-child", "0"};
  auto next = first; next.gameId = "other-game";
  launch::LaunchService service;
  QSignalSpy done(&service, &launch::LaunchService::finished), started(&service, &launch::LaunchService::started);
  int attempts = 0; bool refused = true, pendingBusy = true;
  const auto attempt = [&] {
    ++attempts; pendingBusy = pendingBusy && service.busy();
    refused = !service.start(next) && refused;
  };
  connect(&service, &launch::LaunchService::runtimeStateReady, this, [&](const RuntimeState &state) {
    if (!state.playing) attempt();
  });
  connect(&service, &launch::LaunchService::playingChanged, this, [&] {
    if (!service.playing()) attempt();
  });
  connect(&service, &launch::LaunchService::warning, this, [&](const QString &, const QString &) { attempt(); });
  QVERIFY(service.start(first));
  QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
  QCOMPARE(attempts, 3); QVERIFY(refused); QVERIFY(pendingBusy);
  QCOMPARE(started.count(), 1); QCOMPARE(done[0][0].toString(), first.gameId);
  QCOMPARE(done[0][1].toInt(), 0); QVERIFY(done[0][2].toString().isEmpty());
  QVERIFY(!service.busy());
  QVERIFY(!QFileInfo::exists(temp.path() + "/user/logs/other-game"));
}
void LaunchSteamTest::finishedCanStartReplacementWithoutStaleRaise() {
  QTemporaryDir temp;
  launch::Request first;
  first.root = temp.path(); first.gameId = "test-game"; first.variantId = "flat-test";
  first.prepareProfile = false; first.plan.executable = QCoreApplication::applicationFilePath();
  first.plan.cwd = temp.path(); first.plan.args = {"--synthetic-child", "0"};
  auto next = first; next.gameId = "other-game"; next.plan.args << "wait";
  launch::LaunchService service;
  QSignalSpy done(&service, &launch::LaunchService::finished), raised(&service, &launch::LaunchService::raiseHubRequested);
  bool replacementStarted = false;
  connect(&service, &launch::LaunchService::finished, this,
          [&](const QString &gameId, int, const QString &, const QString &) {
    if (gameId == first.gameId) replacementStarted = service.start(next);
  });
  QVERIFY(service.start(first));
  QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
  QVERIFY(replacementStarted); QVERIFY(service.busy());
  QCOMPARE(raised.count(), 0); // Old session cannot raise over its replacement.
  QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 10000);
  QCOMPARE(done[1][0].toString(), next.gameId);
  QCOMPARE(raised.count(), 1); QVERIFY(!service.busy());
}
void LaunchSteamTest::hungChildCanBeStopped() {
  QTemporaryDir temp;launch::Request r;r.root=temp.path();r.gameId="test-game";r.variantId="flat-test";r.prepareProfile=false;r.plan.executable=QCoreApplication::applicationFilePath();r.plan.cwd=temp.path();r.plan.args={"--synthetic-child","0","hang"};
  launch::LaunchService service;QSignalSpy started(&service,&launch::LaunchService::started),done(&service,&launch::LaunchService::finished);QVERIFY(service.start(r));QTRY_COMPARE(started.count(),1);
  QTRY_VERIFY_WITH_TIMEOUT(([&]{const auto logs=QDir(temp.path()+"/user/logs/test-game").entryList({"*.log"},QDir::Files);return !logs.isEmpty()&&install::readBytes(temp.path()+"/user/logs/test-game/"+logs.first()).contains("synthetic hung child ready");})(),5000);
  service.stop();QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,7000);QVERIFY(!service.playing());QVERIFY(done[0][1].toInt()!=0);
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
  if((argc==6 || argc==7) && QByteArray(argv[1])=="--synthetic-resource-user") {
    QCoreApplication app(argc,argv);const auto args=app.arguments();QTextStream out(stdout);
    try {
      install::ResourceLocks use({args[2]},install::ResourceAccess::Use);
      if(args.size()==7)use.trackChild(args[6].toLongLong());
      write(args[3],"ready");QElapsedTimer timeout;timeout.start();
      while(!QFileInfo::exists(args[4]) && timeout.elapsed()<15000)QThread::msleep(10);
      if(!QFileInfo::exists(args[4]))throw install::Error("E_TEST_TIMEOUT","Synthetic reader barrier timed out");
      if(args[5]=="crash")std::_Exit(0);
      return 0;
    } catch(const install::Error &error){out << error.code << '\n';out.flush();return 1;}
  }
  if (argc == 8 && QByteArray(argv[1]) == "--synthetic-steam-writer") {
    QCoreApplication app(argc,argv); const auto args=app.arguments(); QTextStream out(stdout);
    try {
      auto request=syntheticSteamRequest(args[2],args[3],args[4],args[5]);
      request.beforeReplace=[&] {
        write(args[6],"ready"); QElapsedTimer timeout; timeout.start();
        while(!QFileInfo::exists(args[7]) && timeout.elapsed()<15000) QThread::msleep(10);
        if(!QFileInfo::exists(args[7])) throw install::Error("E_TEST_TIMEOUT","Synthetic writer barrier timed out");
      };
      steam::apply(request,steam::preview(request),true,[]{return false;});
      out << "ok\n"; out.flush(); return 0;
    } catch(const install::Error &error) {out << error.code << '\n'; out.flush(); return 1;}
  }
  if (argc > 1 && QByteArray(argv[1]) == "--synthetic-child") {
    QTextStream out(stdout);
    out << "XR=" << qEnvironmentVariable("XR_RUNTIME_JSON") << '\n';
    for (int i = 0; i < 50; ++i)
      out << "line " << i << '\n';
    out.flush();
    if (argc > 3 && QByteArray(argv[3]) == "hang") {
#ifndef Q_OS_WIN
      std::signal(SIGTERM, SIG_IGN);
#endif
      out << "synthetic hung child ready\n";out.flush();
      QThread::sleep(60);
    } else if(argc > 3) QThread::msleep(1000);
    return QByteArray(argv[2]).toInt();
  }
  QGuiApplication app(argc, argv);
  LaunchSteamTest tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "LaunchSteamTest.moc"
