// SPDX-License-Identifier: GPL-3.0-only
#include "ControlsResolver.h"
#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

namespace {
using Map=QVariantMap;
using List=QVariantList;
bool write(const QString &root,const QString &relative,const QByteArray &bytes) {
    const QString path=QDir(root).filePath(relative);
    if (!QDir().mkpath(QFileInfo(path).dir().absolutePath())) return false;
    QFile file(path); return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();
}
}
class ControlsResolverTest final: public QObject {
    Q_OBJECT
    ac::ControlsResolver resolver;
    Map expected;
    QString scratch;
private slots:
    void initTestCase() {
        QString error; QVERIFY2(resolver.loadLibrary(AC_CONTROLS_ROOT,error),qPrintable(error));
        scratch=QDir(AC_CONTROLS_ROOT).filePath(".local/native-control-tests"); QVERIFY(QDir().mkpath(scratch));
        QProcess process; process.start(AC_CONTROLS_PYTHON,{QDir(AC_CONTROLS_ROOT).filePath("tools/control_sets.py"),"--allow-unmapped","--json"});
        QVERIFY(process.waitForFinished(30000)); QCOMPARE(process.exitCode(),0);
        QJsonParseError parseError;
        const auto output=QJsonDocument::fromJson(process.readAllStandardOutput(),&parseError).object().toVariantMap();
        QVERIFY2(parseError.error==QJsonParseError::NoError,qPrintable(parseError.errorString()));
        for (const auto &game:output.value("games").toList()) expected[game.toMap().value("game_id").toString()]=game;
        QCOMPARE(expected.size(),152);
    }
    void all152NativeResultsMatchPython() {
        ac::ControlsResolveOptions options; options.useUserOverrides=false;
        QString error;
        for (auto it=expected.begin();it!=expected.end();++it) {
            Map actual; QVERIFY2(resolver.resolveGame(it.key(),options,actual,error),qPrintable(it.key()+": "+error));
            const auto native=QJsonDocument::fromVariant(actual),python=QJsonDocument::fromVariant(it.value());
            if (native!=python) {
                QVERIFY(write(scratch,"native-mismatch.json",native.toJson()));
                QVERIFY(write(scratch,"python-mismatch.json",python.toJson()));
            }
            QVERIFY2(native==python,qPrintable(it.key()+" differs from Python"));
        }
    }
    void mergePreservesExtensionsDeletionAndArrayOrder() {
        const Map base{{"future",Map{{"keep",1},{"remove",2}}},{"element",List{Map{{"id","fire"},{"binding",Map{{"hand","slot"},{"control","trigger"}}}}}}};
        const Map later{{"future",Map{{"remove","!delete"},{"add",3}}},{"element",List{Map{{"id","fire"},{"binding",Map{{"mode","hold"}}}},Map{{"id","coin"},{"extra",4}}}}};
        QVariant merged; QString error; QVERIFY2(ac::ControlsResolver::mergeLayers(base,later,merged,error),qPrintable(error));
        QCOMPARE(merged.toMap().value("future").toMap(),Map({{"keep",1},{"add",3}}));
        const auto elements=merged.toMap().value("element").toList(); QCOMPARE(elements.size(),2);
        QCOMPARE(elements[0].toMap().value("binding").toMap(),Map({{"hand","slot"},{"control","trigger"},{"mode","hold"}}));
        QCOMPARE(elements[1].toMap().value("id").toString(),QString("coin"));
        QVERIFY(ac::ControlsResolver::mergeLayers(List{1,2},List{},merged,error)); QVERIFY(merged.toList().isEmpty());
        merged="unchanged";
        QVERIFY(!ac::ControlsResolver::mergeLayers(List{Map{{"id","fire"}}},List{Map{{"id","fire"}},Map{{"id","fire"}}},merged,error));
        QCOMPARE(merged.toString(),QString("unchanged"));
        const qlonglong large=9007199254740993LL;
        QVERIFY(ac::ControlsResolver::mergeLayers(Map{{"big",large}},Map{{"other",true}},merged,error));
        QCOMPARE(merged.toMap().value("big").toLongLong(),large);
        QVERIFY(ac::ControlsResolver::mergeLayers(List{Map{{"id","Plugin key / 1"},{"opaque",7}}},
            List{Map{{"id","Plugin key / 1"},{"extra",8}}},merged,error));
        QCOMPARE(merged.toList()[0].toMap().value("opaque").toInt(),7);
        QCOMPARE(merged.toList()[0].toMap().value("extra").toInt(),8);
    }
    void pathEscapesAreRejectedBeforeReading() {
        QTemporaryDir directory(scratch+"/paths-XXXXXX"); QVERIFY(directory.isValid());
        QVERIFY(write(directory.path(),"safe.toml","value = 7\n"));
        Map out{{"sentinel",1}}; QString error;
        for (const QString &path:{QString("../safe.toml"),QString("/absolute.toml"),QString("bad:drive/file.toml"),QString("nested\\escape.toml")}) {
            QVERIFY(!ac::ControlsResolver::readConfinedToml(directory.path(),path,out,error,true)); QCOMPARE(out,Map({{"sentinel",1}}));
        }
        QVERIFY(ac::ControlsResolver::readConfinedToml(directory.path(),"safe.toml",out,error)); QCOMPARE(out.value("value").toInt(),7);
        QVERIFY(ac::ControlsResolver::readConfinedToml(directory.path(),"absent/nested.toml",out,error,true)); QVERIFY(out.isEmpty());
        ac::ControlsResolveOptions options; options.packIds={"../escape"};
        QVERIFY(!resolver.resolveGame("timecris",options,out,error));
        options.packIds.clear(); options.userOverrideDirectory="../outside";
        QVERIFY(!resolver.resolveGame("timecris",options,out,error));
        options.useUserOverrides=false;
        QVERIFY(!resolver.resolveGame("../timecris",options,out,error));
    }
    void callerModelDefaultsAreProvisionalAndDoNotOverrideGameModel() {
        ac::ControlsResolveOptions options; options.useUserOverrides=false; options.profileModel="generic-pistol";
        options.profileDefaults={{"policy",Map{{"p1_hand","left"},{"two_guns","off"}}},{"future",Map{{"key","retained"}}}};
        Map out; QString error; QVERIFY2(resolver.resolveGame("hotd2",options,out,error),qPrintable(error));
        QCOMPARE(out.value("model").toString(),QString("generic-pistol"));
        QCOMPARE(out.value("configured_slots").toInt(),1);
        QCOMPARE(out.value("data").toMap().value("policy").toMap().value("p1_hand").toString(),QString("left"));
        QCOMPARE(out.value("data").toMap().value("future").toMap().value("key").toString(),QString("retained"));
        options.profileModel="unknown"; QVERIFY(!resolver.resolveGame("hotd2",options,out,error));
    }
    void actualPackUserFilesMatchPythonAndPreserveUserPrecedence() {
        QTemporaryDir directory(scratch+"/layers-XXXXXX"); QVERIFY(directory.isValid());
        for (const QString &folder:{QString("data/guns"),QString("data/controls"),QString("data/vocab")}) {
            const QDir source(QDir(AC_CONTROLS_ROOT).filePath(folder));
            for (const auto &name:source.entryList({"*.toml"},QDir::Files)) {
                QFile file(source.filePath(name)); QVERIFY(file.open(QIODevice::ReadOnly));
                QVERIFY(write(directory.path(),folder+'/'+name,file.readAll()));
            }
        }
        QFile header(QDir(AC_CONTROLS_ROOT).filePath("libacvr/include/acvr.h")); QVERIFY(header.open(QIODevice::ReadOnly));
        QVERIFY(write(directory.path(),"libacvr/include/acvr.h",header.readAll()));
        QVERIFY(write(directory.path(),"games/fixture-gun/game.toml",
            "id='fixture-gun'\ngenre='gun'\nplayers=2\nmanufacturer='sega'\nhardware='sega-naomi'\n[controls]\nguns=2\n"));
        QVERIFY(write(directory.path(),"games/fixture-gun/setup/controls.toml",
            "version='0.1'\n[extension]\nkeep='canonical'\nerase='remove-me'\n[[element]]\nid='p1-trigger'\nlabel='Canonical fire'\n"));
        QVERIFY(write(directory.path(),"packs/first/games/fixture-gun/game.toml","[controls]\ngun_model='generic-pistol'\n"));
        QVERIFY(write(directory.path(),"packs/second/games/fixture-gun/game.toml","[controls]\ngun_model='arc-pistol-slide'\n"));
        QVERIFY(write(directory.path(),"user/overrides/games/fixture-gun/game.toml","[controls]\ngun_model='con-pistol-slim'\ntwo_guns='on_join'\n"));
        QVERIFY(write(directory.path(),"packs/first/data/controls/gun-con-pistol-slim.toml",
            "[extension]\npack='first'\nvalues=[1,2]\n"));
        QVERIFY(write(directory.path(),"packs/first/games/fixture-gun/setup/controls.toml",
            "[[element]]\nid='p1-trigger'\nbinding={ threshold=0.3 }\nfuture={ first=true }\n"));
        QVERIFY(write(directory.path(),"packs/second/data/controls/gun-con-pistol-slim.toml",
            "[extension]\npack='second'\nvalues=[3]\n"));
        QVERIFY(write(directory.path(),"user/overrides/data/controls/gun-con-pistol-slim.toml",
            "[extension]\nshared='user-shared'\n"));
        QVERIFY(write(directory.path(),"packs/first/data/guns/con-pistol-slim.toml",
            "[extension]\nmodel_pack='preserved'\n"));
        QVERIFY(write(directory.path(),"user/overrides/data/guns/con-pistol-slim.toml",
            "notes='Synthetic user model metadata'\n[extension]\nmodel_user=true\n"));
        QVERIFY(write(directory.path(),"user/overrides/games/fixture-gun/setup/controls.toml",
            "[extension]\nerase='!delete'\nuser='last'\n[policy]\np1_hand='right'\n[[element]]\nid='p1-trigger'\nbinding={ threshold=0.8 }\nfuture={ second=true }\n"));
        ac::ControlsResolver fixture; QString error; QVERIFY2(fixture.loadLibrary(directory.path(),error),qPrintable(error));
        ac::ControlsResolveOptions options; options.packIds={"first","second"}; options.profileModel="generic-pistol";
        options.profileDefaults={{"policy",Map{{"p1_hand","left"},{"two_guns","off"}}},{"extension",Map{{"caller","kept"}}}};
        Map actual; QVERIFY2(fixture.resolveGame("fixture-gun",options,actual,error),qPrintable(error));
        QCOMPARE(actual.value("model").toString(),QString("con-pistol-slim"));
        QCOMPARE(actual.value("model_metadata").toMap().value("extension").toMap(),Map({{"model_pack","preserved"},{"model_user",true}}));
        QCOMPARE(actual.value("configured_slots").toInt(),2);
        const auto data=actual.value("data").toMap(),extension=data.value("extension").toMap();
        QVERIFY(!extension.contains("erase")); QCOMPARE(extension.value("pack").toString(),QString("second"));
        QCOMPARE(extension.value("values").toList().size(),1);
        QCOMPARE(extension.value("values").toList()[0].toLongLong(),qlonglong(3));
        QCOMPARE(data.value("policy").toMap().value("p1_hand").toString(),QString("right"));
        const auto trigger=data.value("element").toList()[0].toMap();
        QCOMPARE(trigger.value("binding").toMap().value("threshold").toDouble(),.8);
        QCOMPARE(trigger.value("future").toMap(),Map({{"first",true},{"second",true}}));
        QProcess process; process.start(AC_CONTROLS_PYTHON,{QDir(AC_CONTROLS_ROOT).filePath("hub/src/controls/tests/native_parity.py"),
            "--source-root",AC_CONTROLS_ROOT,"--fixture-root",directory.path()});
        QVERIFY(process.waitForFinished(30000)); QCOMPARE(process.exitCode(),0);
        const auto expectedFixture=QJsonDocument::fromJson(process.readAllStandardOutput());
        if (QJsonDocument::fromVariant(actual)!=expectedFixture) {
            QVERIFY(write(scratch,"layer-native.json",QJsonDocument::fromVariant(actual).toJson()));
            QVERIFY(write(scratch,"layer-python.json",expectedFixture.toJson()));
        }
        QCOMPARE(QJsonDocument::fromVariant(actual),expectedFixture);
        QVERIFY(write(directory.path(),"user/overrides/games/fixture-gun/setup/controls.toml",
            "[[element]]\nid='p2-trigger'\nslot=4294967296\nplayer=4294967296\n"));
        const Map previous=actual;
        QVERIFY(!fixture.resolveGame("fixture-gun",options,actual,error)); QCOMPARE(actual,previous);
        QVERIFY(write(directory.path(),"user/overrides/games/fixture-gun/setup/controls.toml",
            "[[element]]\nid='p1-coin'\nbinding={control='trigger', mode='press'}\n"));
        QVERIFY(!fixture.resolveGame("fixture-gun",options,actual,error)); QVERIFY(error.contains("collision"));
        QVERIFY(write(directory.path(),"user/overrides/games/fixture-gun/setup/controls.toml",
            "[[element]]\nid='p1-trigger'\nslot=0.0\n"));
        QVERIFY(!fixture.resolveGame("fixture-gun",options,actual,error));
        QVERIFY(write(directory.path(),"user/overrides/games/fixture-gun/setup/controls.toml","[policy]\np1_hand='!delete'\n"));
        QVERIFY(fixture.resolveGame("fixture-gun",options,actual,error));
        QVERIFY(!actual.value("data").toMap().value("policy").toMap().contains("p1_hand"));
        QVERIFY(write(directory.path(),"user/overrides/data/guns/con-pistol-slim.toml","model='../outside.glb'\n"));
        QVERIFY(!fixture.resolveGame("fixture-gun",options,actual,error)); QVERIFY(error.contains("GLB"));
    }
    void handednessPrimaryAndFallbackCollisionsMatchPython() {
        QProcess process; process.start(AC_CONTROLS_PYTHON,{QDir(AC_CONTROLS_ROOT).filePath("hub/src/controls/tests/handedness_parity.py"),
            "--source-root",AC_CONTROLS_ROOT});
        QVERIFY(process.waitForFinished(30000)); QCOMPARE(process.exitCode(),0);
        QJsonParseError parseError;
        const auto cases=QJsonDocument::fromJson(process.readAllStandardOutput(),&parseError).array();
        QVERIFY(parseError.error==QJsonParseError::NoError); QCOMPARE(cases.size(),8);
        for (const auto &value:cases) {
            const auto test=value.toObject().toVariantMap();
            ac::ControlsResolveOptions options; options.useUserOverrides=false;
            options.profileDefaults=test.value("defaults").toMap();
            Map out{{"sentinel",1}}; QString error;
            const bool ok=resolver.resolveGame(test.value("game_id").toString(),options,out,error);
            if (test.value("collides").toBool()) {
                QVERIFY(!ok); QVERIFY2(error.contains("collision"),qPrintable(error));
                QVERIFY(test.value("error").toString().contains("collision"));
                QCOMPARE(out,Map({{"sentinel",1}}));
            } else {
                QVERIFY2(ok,qPrintable(error)); QVERIFY(!test.contains("error"));
                QCOMPARE(QJsonDocument::fromVariant(out),QJsonDocument::fromVariant(test.value("result")));
            }
        }
    }
    void backendSupportAndDecodedNodesAreSeparateGates() {
        ac::ControlsResolveOptions options; options.useUserOverrides=false;
        options.backend=Map{{"guns",1},{"players",1},{"shared_view",true},{"runtime_actions",List{"laser_toggle"}},
            {"control",List{Map{{"kind","gun"},{"semantic","trigger"},{"player",0}},Map{{"kind","gun"},{"semantic","reload"},{"player",0}},Map{{"kind","button"},{"semantic","coin"},{"player",0}}}}};
        Map out; QString error; QVERIFY2(resolver.resolveGame("timecris",options,out,error),qPrintable(error));
        QCOMPARE(out.value("declared_active_slots").toInt(),1);
        QVERIFY(out.value("gaps").toList().size()>=2); // missing model + canonical pedal
        options.decodedModelNodes["arc-pistol-slide"]={"pivot_trigger","muzzle","fx_laser"};
        QVERIFY(resolver.resolveGame("timecris",options,out,error));
        QCOMPARE(out.value("node_validation").toString(),QString("built-references-checked"));
        options.decodedModelNodes["arc-pistol-slide"].remove("pivot_trigger");
        QVERIFY(!resolver.resolveGame("timecris",options,out,error));
        bool suppliedExactMetadata=false;
        options.decodedNodesForModel=[&](const Map &model)->std::optional<QSet<QString>> {
            suppliedExactMetadata=model.value("id").toString()=="arc-pistol-slide" && model.value("model").toString().endsWith("arc-pistol-slide.glb");
            return QSet<QString>{"pivot_trigger","muzzle","fx_laser"};
        };
        QVERIFY(resolver.resolveGame("timecris",options,out,error)); QVERIFY(suppliedExactMetadata);
        QCOMPARE(out.value("model_metadata").toMap().value("id").toString(),QString("arc-pistol-slide"));
        auto backend=*options.backend; backend["guns"]=true; options.backend=backend;
        QVERIFY(!resolver.resolveGame("timecris",options,out,error));
    }
};
QTEST_GUILESS_MAIN(ControlsResolverTest)
#include "ControlsResolverTest.moc"
