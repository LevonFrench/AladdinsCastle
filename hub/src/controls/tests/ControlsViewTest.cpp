// SPDX-License-Identifier: GPL-3.0-only
#include "ControlsViewModel.h"
#include <QtTest>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QImage>
#include <memory>

namespace {
QStringList warnings;
void messages(QtMsgType type,const QMessageLogContext &,const QString &message) {
    if (type==QtWarningMsg || type==QtCriticalMsg) warnings.append(message);
    fprintf(stderr,"%s\n",qPrintable(message));
}
QList<QQuickItem *> named(QQuickItem *root,const QString &name) {
    QList<QQuickItem *> result;
    if (root->objectName()==name) result.append(root);
    for (auto *child:root->childItems()) result.append(named(child,name));
    return result;
}
QVariantMap rowWithId(const ac::ControlsViewModel &model,const QString &id) {
    for (const auto &value:model.rows()) if (value.toMap().value("id").toString()==id) return value.toMap();
    return {};
}
}

class ControlsViewTest final: public QObject {
    Q_OBJECT
    QMap<QString,QVariantMap> samples;
    QString fixtureRoot;
private slots:
    void initTestCase() {
        fixtureRoot=QDir(AC_CONTROLS_ROOT).filePath(".local/controls-view-tests");
        QVERIFY(QDir().mkpath(fixtureRoot));
        QProcess process;
        process.start(AC_CONTROLS_PYTHON,{QDir(AC_CONTROLS_ROOT).filePath("tools/control_sets.py"),"--allow-unmapped","--proposed-overrides","--json"});
        QVERIFY(process.waitForFinished(30000));
        QCOMPARE(process.exitCode(),0);
        QJsonParseError parseError;
        const auto output=QJsonDocument::fromJson(process.readAllStandardOutput(),&parseError).object().toVariantMap();
        QVERIFY2(parseError.error==QJsonParseError::NoError,qPrintable(parseError.errorString()));
        QVERIFY(output.value("errors").toList().isEmpty());
        for (const auto &value:output.value("games").toList()) {
            const auto map=value.toMap(); samples[map.value("game_id").toString()]=map;
        }
        QCOMPARE(samples.size(),152);
        qInstallMessageHandler(messages);
    }
    void allResolvedGamesHaveCalloutsAndExplicitBindingState() {
        ac::ControlsViewModel model;
        for (const auto &sample:samples) {
            QVERIFY(model.loadResolved(sample));
            QVERIFY(!model.previewAvailable());
            const auto data=sample.value("data").toMap();
            QCOMPARE(model.rows().size(),data.value("element").toList().size()+data.value("unmapped_part").toList().size());
            for (const auto &value:model.rows()) {
                const auto row=value.toMap();
                QVERIFY(!row.value("part").toString().isEmpty());
                QVERIFY(!row.value("action").toString().isEmpty());
                QVERIFY(!row.value("bindingLabel").toString().isEmpty());
                QVERIFY(row.value("availability").toString()!="available");
                QVERIFY(!row.value("pressed").toBool());
            }
        }
    }
    void timeCrisisAndMirroredControllerLabels() {
        ac::ControlsViewModel model;
        QVERIFY(model.loadResolved(samples["timecris"]));
        QVERIFY(rowWithId(model,"p1-cover").value("bindingLabel").toString().contains("Right grip"));
        QVERIFY(rowWithId(model,"p1-coin").value("bindingLabel").toString().contains("B (right)"));
        QVERIFY(rowWithId(model,"p1-start").isEmpty());
        model.setPrimaryHand("left");
        QVERIFY(rowWithId(model,"p1-cover").value("bindingLabel").toString().contains("Left grip"));
        QVERIFY(rowWithId(model,"p1-coin").value("bindingLabel").toString().contains("Y (left)"));
        QVERIFY(model.controllerBindings("right").isEmpty());
        model.setTwoGunsActive(true); QVERIFY(!model.twoGunsActive());
    }
    void replacementDefaultsHandAndClearsHostPreviewOverride() {
        ac::ControlsViewModel model;
        auto sample=samples["timecris"],data=sample.value("data").toMap();
        auto policy=data.value("policy").toMap(); policy["p1_hand"]="left";
        data["policy"]=policy; sample["data"]=data;
        QVERIFY(model.loadResolved(sample)); QCOMPARE(model.primaryHand(),QString("left"));
        model.setBindingState("left","trigger",true);
        QVERIFY(rowWithId(model,"p1-trigger").value("pressed").toBool());
        // A resolved !delete removes the key. No state from the preceding game
        // or a host's transient preview override may substitute for the default.
        policy.remove("p1_hand"); data["policy"]=policy; sample["data"]=data;
        QVERIFY(model.loadResolved(sample)); QCOMPARE(model.primaryHand(),QString("right"));
        QVERIFY(!rowWithId(model,"p1-trigger").value("pressed").toBool());
        model.setPrimaryHand("left");
        data.remove("policy"); sample["data"]=data;
        QVERIFY(model.loadResolved(sample)); QCOMPARE(model.primaryHand(),QString("right"));
        model.setPrimaryHand("left"); QVERIFY(!model.loadResolvedJson("invalid JSON"));
        QCOMPARE(model.primaryHand(),QString("right"));
    }
    void nativeCatalogLoaderNeedsNoPythonAtRuntime() {
        const QVariantMap catalog{{"version","0.1"},{"baseline_only",true},{"games",QVariantList{samples["timecris"],samples["hotd2"]}}};
        ac::ControlsViewModel model;
        QVERIFY(model.loadCatalogGame(QJsonDocument::fromVariant(catalog).toJson(),"timecris"));
        QCOMPARE(model.gameId(),QString("timecris"));
        QVERIFY(model.loadCatalogGame(QJsonDocument::fromVariant(catalog).toJson(),"hotd2"));
        QCOMPARE(model.gameId(),QString("hotd2"));
        QVERIFY(!model.loadCatalogGame(QJsonDocument::fromVariant(catalog).toJson(),"absent"));
        QVERIFY(model.rows().isEmpty());
    }
    void boundInputHighlightsAndClearsWithoutPromotingAvailability() {
        ac::ControlsViewModel model;
        QVERIFY(model.loadResolved(samples["timecris"]));
        model.setBindingState("right","trigger",true);
        QVERIFY(rowWithId(model,"p1-trigger").value("pressed").toBool());
        QCOMPARE(rowWithId(model,"p1-trigger").value("availability").toString(),QString("unverified"));
        QVERIFY(!rowWithId(model,"p1-cover").value("pressed").toBool());
        model.setBindingState("right","trigger",false);
        QVERIFY(!rowWithId(model,"p1-trigger").value("pressed").toBool());
        model.setBindingState("right","grip",true);
        QVERIFY(rowWithId(model,"p1-cover").value("pressed").toBool());
        model.clearBindingStates(); QVERIFY(!rowWithId(model,"p1-cover").value("pressed").toBool());
        model.setBindingState("right","grip",true);
        QVERIFY(model.loadResolved(samples["hotd2"]));
        QVERIFY(!rowWithId(model,"p1-trigger").value("pressed").toBool());
    }
    void twoGunsUseFlickFallbackWithoutCrossHandHighlight() {
        ac::ControlsViewModel model;
        QVERIFY(model.loadResolved(samples["hotd3"]));
        QVERIFY(rowWithId(model,"p1-reload").value("bindingLabel").toString().contains("Pump"));
        model.setTwoGunsActive(true);
        QVERIFY(rowWithId(model,"p1-reload").value("bindingLabel").toString().contains("Right wrist flick"));
        QVERIFY(rowWithId(model,"p2-reload").value("bindingLabel").toString().contains("Left wrist flick"));
        model.setBindingState("left","flick_up",true);
        QVERIFY(rowWithId(model,"p2-reload").value("pressed").toBool());
        QVERIFY(!rowWithId(model,"p1-reload").value("pressed").toBool());
    }
    void unavailableUnmappedAndMalformedInputRemainVisible() {
        ac::ControlsViewModel model;
        auto sample=samples["ps1-time-crisis"];
        auto data=sample.value("data").toMap();
        auto elements=data.value("element").toList();
        auto element=elements[0].toMap();
        element["availability"]=QVariantMap{{"state","unavailable"},{"reason","Synthetic backend omits this input"}};
        elements[0]=element; data["element"]=elements; sample["data"]=data;
        QVERIFY(model.loadResolved(sample));
        QCOMPARE(rowWithId(model,"p1-trigger").value("availability").toString(),QString("unavailable"));
        QCOMPARE(rowWithId(model,"p1-a").value("availability").toString(),QString("unmapped"));
        QVERIFY(rowWithId(model,"p1-a").value("callout").toString().contains("Binding pending"));
        QVERIFY(!model.loadResolvedJson("invalid JSON"));
        QVERIFY(model.rows().isEmpty()); QVERIFY(!model.lastError().isEmpty());
    }
    void previewsUseActualImageAndProjectionAndRejectTraversal() {
        QTemporaryDir directory(fixtureRoot+"/preview-XXXXXX"); QVERIFY(directory.isValid());
        QImage image(512,512,QImage::Format_ARGB32); image.fill(QColor("#884455"));
        QVERIFY(image.save(directory.filePath("synthetic-preview.png")));
        auto view=QVariantMap{{"image","synthetic-preview.png"},{"nodes",QVariantMap{
            {"pivot_trigger",QVariantList{.42,.61}},{"muzzle",QVariantList{.2,.3}},{"fx_laser",QVariantList{.2,.3}}}}};
        auto manifest=QVariantMap{{"version","0.1"},{"model","arc-pistol-slide"},{"size",QVariantList{512,512}},
                                 {"views",QVariantMap{{"threequarter",view},{"front",view}}}};
        auto write=[&] {
            QFile file(directory.filePath("arc-pistol-slide.json"));
            if (!file.open(QIODevice::WriteOnly)) return false;
            return file.write(QJsonDocument::fromVariant(manifest).toJson())>0;
        };
        QVERIFY(write()); ac::ControlsViewModel model;
        QVERIFY(model.loadResolved(samples["timecris"],directory.path()));
        QVERIFY(model.previewAvailable());
        QVERIFY(rowWithId(model,"p1-trigger").value("anchorAvailable").toBool());
        QCOMPARE(rowWithId(model,"p1-trigger").value("anchorX").toDouble(),.42);
        model.setBindingState("right","trigger",true);
        bool highlighted=false;
        for (const auto &marker:model.markers()) highlighted|=marker.toMap().value("pressed").toBool();
        QVERIFY(highlighted);
        view["image"]="../outside.png";
        manifest["views"]=QVariantMap{{"threequarter",view}}; QVERIFY(write());
        QVERIFY(model.loadResolved(samples["timecris"],directory.path())); QVERIFY(!model.previewAvailable());
        view["image"]="synthetic-preview.png";
        view["nodes"]=QVariantMap{{"pivot_trigger",QVariantList{1.5,.2}}};
        manifest["views"]=QVariantMap{{"threequarter",view}}; QVERIFY(write());
        QVERIFY(model.loadResolved(samples["timecris"],directory.path())); QVERIFY(!model.previewAvailable());
    }
    void offscreenComponentsShowPlaceholderAndEveryCallout() {
        QQmlEngine engine;
        QQmlComponent component(&engine,QUrl("qrc:/controls/ControlsView.qml"));
        QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        for (const QString &gid : {QString("timecris"),QString("ps1-time-crisis"),QString("hotd3"),QString("gunblade"),
                                  QString("sscope"),QString("ps3-time-crisis-4"),QString("brave-firefighters"),QString("vcop")}) {
            ac::ControlsViewModel model; QVERIFY(model.loadResolved(samples[gid]));
            std::unique_ptr<QObject> object(component.createWithInitialProperties({{"controlsModel",QVariant::fromValue(&model)},{"width",1000}}));
            QVERIFY2(object,qPrintable(component.errorString()));
            auto *item=qobject_cast<QQuickItem *>(object.get()); QVERIFY(item);
            QCOMPARE(named(item,"controlsCallout").size(),model.rows().size());
            const auto placeholders=named(item,"gunPreviewPlaceholder"); QCOMPARE(placeholders.size(),1);
            QVERIFY(placeholders[0]->isVisible());
            model.setBindingState("right","trigger",true);
            QCoreApplication::processEvents();
            QVERIFY(rowWithId(model,"p1-trigger").value("pressed").toBool());
        }
    }
    void softwareLayoutCapture() {
        QQmlEngine engine; ac::ControlsViewModel model;
        QVERIFY(model.loadResolved(samples["timecris"]));
        QQmlComponent component(&engine,QUrl("qrc:/controls/ControlsView.qml"));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{"controlsModel",QVariant::fromValue(&model)},{"width",1000}}));
        QVERIFY2(object,qPrintable(component.errorString()));
        auto *item=qobject_cast<QQuickItem *>(object.get()); QVERIFY(item);
        QQuickWindow window; window.setColor(QColor("#0f0f12")); window.resize(1040,1300);
        item->setParentItem(window.contentItem()); item->setPosition({20,20}); window.show();
        QTest::qWait(100);
        const QImage desktop=window.grabWindow(); QVERIFY(!desktop.isNull());
        QVERIFY(desktop.save(QDir(fixtureRoot).filePath("timecris-desktop-source-only.png")));
        item->setWidth(460); window.resize(500,1650); QTest::qWait(60);
        const QImage narrow=window.grabWindow(); QVERIFY(!narrow.isNull());
        QVERIFY(narrow.save(QDir(fixtureRoot).filePath("timecris-narrow-source-only.png")));
        item->setParentItem(nullptr);
    }
    void cleanupTestCase() {
        qInstallMessageHandler(nullptr);
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
};

int main(int argc,char **argv) {
    qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_BACKEND","software"); qputenv("QT_OPENGL","software");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software); QQuickStyle::setStyle("Basic");
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR")) {
        const QString fonts=QDir(qEnvironmentVariable("WINDIR")).filePath("Fonts");
        if (QDir(fonts).exists()) qputenv("QT_QPA_FONTDIR",fonts.toUtf8());
    }
#endif
    QGuiApplication application(argc,argv);
    ControlsViewTest tests; return QTest::qExec(&tests,argc,argv);
}
#include "ControlsViewTest.moc"
