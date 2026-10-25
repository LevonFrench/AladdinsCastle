// SPDX-License-Identifier: GPL-3.0-only
// The games the Hub starts straight through acvr-ps1 and a profile (the 3DS route): the exact command line, the
// refusals, and that the public id has a catalog entry and a profile in the repo. Synthetic files only; nothing
// here starts acvr-ps1, an emulator or SteamVR.
#include "core/install/Install.h"
#include "core/libretro/Libretro.h"
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>
namespace {
void write(const QString &path, const QByteArray &bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(bytes);
}
QString root() { return QStringLiteral(AC_CATALOG_ROOT); }
QString native(const QString &path) { return QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()); }
QString plainPath(const QString &path) { return QFileInfo(path).absoluteFilePath(); } // As the Hub keeps folders: forward slashes.
const QString game = "3ds-star-fox-64-3d";
} // namespace
class LibretroTest : public QObject {
  Q_OBJECT
  // Synthetic host, core, profile folder and dumps folder.
  struct Setup {
    QString host, core, profiles, dumps, data;
    explicit Setup(const QString &base) {
      host = base + "/bin/acvr-ps1.exe";
      core = base + "/cores/azahar_libretro.dll";
      profiles = base + "/profiles";
      dumps = base + "/dumps";
      data = base + "/user/libretro/" + game;
      write(host, "synthetic");
      write(core, "synthetic");
      write(profiles + "/" + game + ".toml", "schema = 1\n");
      write(dumps + "/Star Fox 64 3D.3ds", "synthetic");
    }
  };
private slots:
  void theGameIsListedUnderItsCatalogId() {
    const auto rows = ac::libretro::games();
    QCOMPARE(rows.size(), 1);
    const auto row = rows.first().toMap();
    QCOMPARE(row.value("id").toString(), game);
    QCOMPARE(row.value("system").toString(), QString("3ds"));
    QCOMPARE(row.value("profile").toString(), game + ".toml");
    QVERIFY(!row.value("controls").toString().isEmpty());
    // The id is the catalog's own, and the repo carries the profile it names; neither names a path of the owner's.
    QVERIFY(QFileInfo(root() + "/games/" + game + "/game.toml").isFile());
    const auto profiles = ac::libretro::profilesFolder({}, root() + "/hub");
    QVERIFY2(QFileInfo(profiles + "/" + game + ".toml").isFile(), qPrintable(profiles));
    QCOMPARE(ac::libretro::profileName("point-blank"), QString());
  }
  void exactCommandLine() {
    QTemporaryDir t;
    const Setup s(t.path());
    const auto plan = ac::libretro::launchPlan(game, s.host, s.core, s.dumps + "/Star Fox 64 3D.3ds", s.data, s.profiles, "theatre");
    QCOMPARE(plan.value("executable").toString(), native(s.host));
    QCOMPARE(plan.value("cwd").toString(), native(QFileInfo(s.host).absolutePath()));
    // The usual view passes no --view: the profile's default_view is the theatre. No game, core or path is in the repo.
    QCOMPARE(plan.value("args").toStringList(),
             QStringList({"--core", native(s.core), "--game", native(s.dumps + "/Star Fox 64 3D.3ds"), "--system", native(s.data + "/system"), "--saves", native(s.data + "/saves"),
                          "--profile", native(s.profiles + "/" + game + ".toml")}));
    const auto swapped = ac::libretro::launchPlan(game, s.host, s.core, s.dumps + "/Star Fox 64 3D.3ds", s.data, s.profiles, "swap-eyes");
    QCOMPARE(swapped.value("args").toStringList().mid(swapped.value("args").toStringList().size() - 2), QStringList({"--view", "swap-eyes"}));
    QCOMPARE(swapped.value("args").toStringList().size(), plan.value("args").toStringList().size() + 2);
  }
  void refusesWhatIsMissing() {
    QTemporaryDir t;
    const Setup s(t.path());
    const auto file = s.dumps + "/Star Fox 64 3D.3ds";
    const auto code = [&](const QString &host, const QString &core, const QString &dump, const QString &profiles, const QString &view) {
      try {
        ac::libretro::launchPlan(game, host, core, dump, s.data, profiles, view);
      } catch (const ac::install::Error &error) {
        return error.code;
      }
      return QString();
    };
    QCOMPARE(code(s.host, s.core, file, s.profiles, "theatre"), QString());
    QCOMPARE(code({}, s.core, file, s.profiles, "theatre"), QString("E_LIBRETRO_SETUP"));
    QCOMPARE(code(s.host, t.filePath("none.dll"), file, s.profiles, "theatre"), QString("E_LIBRETRO_CORE"));
    QCOMPARE(code(s.host, s.core, t.filePath("none.3ds"), s.profiles, "theatre"), QString("E_LIBRETRO_GAME_FILE"));
    QCOMPARE(code(s.host, s.core, file, t.filePath("nowhere"), "theatre"), QString("E_LIBRETRO_PROFILE"));
    QCOMPARE(code(s.host, s.core, file, s.profiles, "monitor"), QString("E_LIBRETRO_VIEW"));
    try {
      ac::libretro::launchPlan("point-blank", s.host, s.core, file, s.data, s.profiles, "theatre");
      QFAIL("an unlisted game started");
    } catch (const ac::install::Error &error) {
      QCOMPARE(error.code, QString("E_LIBRETRO_GAME"));
    }
  }
  void problemSaysWhatToDo() {
    QTemporaryDir t;
    const Setup s(t.path());
    QVERIFY(ac::libretro::problem(game, s.host, s.core, s.dumps, s.profiles).isEmpty());
    QVERIFY(ac::libretro::problem(game, {}, s.core, s.dumps, s.profiles).contains("libretroHost"));
    QVERIFY(ac::libretro::problem(game, s.host, {}, s.dumps, s.profiles).contains("threedsCore"));
    QVERIFY(ac::libretro::problem(game, s.host, s.core, s.dumps, {}).contains("libretroProfiles"));
    QVERIFY(ac::libretro::problem(game, s.host, s.core, t.filePath("empty"), s.profiles).contains("threedsGameFolder"));
    QVERIFY(!ac::libretro::problem("point-blank", s.host, s.core, s.dumps, s.profiles).isEmpty());
  }
  void findsTheDumpByTitle() {
    QTemporaryDir t;
    write(t.filePath("Other Game.3ds"), "synthetic");
    write(t.filePath("notes.txt"), "synthetic");
    QVERIFY(ac::libretro::gameFile(game, t.path()).isEmpty());
    write(t.filePath("starfox64-3d prepared.cci"), "synthetic");
    QCOMPARE(ac::libretro::gameFile(game, t.path()), QFileInfo(t.filePath("starfox64-3d prepared.cci")).absoluteFilePath());
    write(t.filePath("STAR FOX 64 3D.3ds"), "synthetic"); // The exact name wins over a longer one.
    QCOMPARE(QFileInfo(ac::libretro::gameFile(game, t.path())).fileName().toLower(), QString("star fox 64 3d.3ds"));
    QVERIFY(ac::libretro::gameFile(game, {}).isEmpty());
    QVERIFY(ac::libretro::gameFile("point-blank", t.path()).isEmpty());
  }
  void profileFolderIsChosenOrFoundAbove() {
    QTemporaryDir t;
    write(t.filePath("repo/setups/retro-ps1-vr/profiles/a.toml"), "schema = 1\n");
    QDir().mkpath(t.filePath("repo/hub/build/bin"));
    QCOMPARE(ac::libretro::profilesFolder({}, t.filePath("repo/hub/build/bin")), plainPath(t.filePath("repo/setups/retro-ps1-vr/profiles")));
    QCOMPARE(ac::libretro::profilesFolder(t.filePath("repo/hub"), t.filePath("repo/hub/build/bin")), plainPath(t.filePath("repo/hub")));
    QVERIFY(ac::libretro::profilesFolder(t.filePath("nowhere"), t.filePath("repo")).isEmpty()); // A chosen folder that is gone is not replaced.
    QVERIFY(ac::libretro::profilesFolder({}, {}).isEmpty());
    QTemporaryDir other;
    QVERIFY(ac::libretro::profilesFolder({}, other.path()).isEmpty());
  }
  void viewsAreTheProfilesTwo() {
    const auto views = ac::libretro::views();
    QCOMPARE(views.size(), 2);
    QCOMPARE(views.first().toMap().value("id").toString(), QString("theatre"));
    QCOMPARE(views.last().toMap().value("id").toString(), QString("swap-eyes"));
    QVERIFY(ac::libretro::viewLabel("monitor").isEmpty());
  }
};
QTEST_GUILESS_MAIN(LibretroTest)
#include "LibretroTest.moc"
