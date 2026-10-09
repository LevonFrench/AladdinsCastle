// SPDX-License-Identifier: GPL-3.0-only
#include "overlay/OverlayInput.h"
#include "overlay/OverlayHost.h"
#include "overlay/SpikeState.h"
#include <QGuiApplication>
#include <QFocusEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickRenderControl>
#include <QQuickStyle>
#include <QtTest>
namespace {
QList<QQuickItem *> visualItems(QQuickItem *root, const QString &name) {
    QList<QQuickItem *> result;
    for (auto *child : root->childItems()) {
        if (child->objectName() == name) result.append(child);
        result.append(visualItems(child, name));
    }
    return result;
}
}
class OverlaySceneTest : public QObject {
    Q_OBJECT
private slots:
    void translatedLaserAndKeyboardReachTwoSceneInstances() {
        ac::SpikeState state;
        QQmlEngine engine;
        ac::OverlayHost host(state); // construction never initializes OpenVR
        engine.rootContext()->setContextProperty("spikeState", &state);
        engine.rootContext()->setContextProperty("overlayHost", &host);
        engine.rootContext()->setContextProperty("surfaceColor", "#10131c");
        engine.rootContext()->setContextProperty("primaryTextColor", "#ffffff");
        engine.rootContext()->setContextProperty("brandColor", "#ee892a");
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString::fromUtf8(AC_SPIKE_SCENE)));
        QQuickRenderControl renderControl;
        QQuickWindow first(&renderControl), second;
        first.setGeometry(0, 0, 1280, 900); second.setGeometry(0, 0, 1280, 900);
        auto *root = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        root->setParent(first.contentItem()); root->setParentItem(first.contentItem()); root->setSize({1280, 900});
        auto *otherRoot = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(otherRoot, qPrintable(component.errorString()));
        otherRoot->setParent(second.contentItem()); otherRoot->setParentItem(second.contentItem()); otherRoot->setSize({1280, 900});
        // Software/offscreen Qt presentation only: no graphics context or VR runtime.
        root->forceActiveFocus();
        QFocusEvent initialFocus(QEvent::FocusIn, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(&first, &initialFocus);
        second.show(); // the render-control window is never shown
        QCoreApplication::processEvents();
        QCOMPARE(visualItems(root, "syntheticCard").size(), qsizetype(50));
        QCOMPARE(visualItems(otherRoot, "syntheticCard").size(), qsizetype(50));
        auto *button = root->findChild<QQuickItem *>("spikeClickButton"); QVERIFY(button);
        auto *text = root->findChild<QQuickItem *>("spikeTextInput"); QVERIFY(text);
        auto *otherText = otherRoot->findChild<QQuickItem *>("spikeTextInput"); QVERIFY(otherText);
        const auto center = button->mapToScene({button->width() / 2, button->height() / 2});
        ac::OverlayInput input;
        vr::VREvent_t click{}; click.eventType = vr::VREvent_MouseButtonDown;
        click.data.mouse = {static_cast<float>(center.x()), static_cast<float>(900 - center.y()), vr::VRMouseButton_Left, 0};
        QVERIFY(input.dispatch(click, &first));
        click.eventType = vr::VREvent_MouseButtonUp; QVERIFY(input.dispatch(click, &first));
        QCOMPARE(state.clickCount(), 1);
        QFocusEvent focus(QEvent::FocusIn, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(&first, &focus);
        text->forceActiveFocus();
        QCOMPARE(first.activeFocusItem(), text);
        input.sendText(QString::fromUtf8("café漢😀"), &first);
        QCOMPARE(text->property("text").toString(), QString::fromUtf8("café漢😀"));
        QCOMPARE(state.text(), QString::fromUtf8("café漢😀"));
        QCOMPARE(otherText->property("text").toString(), state.text());
        input.sendText("\b", &first);
        QCOMPARE(state.text(), QString::fromUtf8("café漢"));
        QVERIFY(input.buttons() == Qt::NoButton);
    }
};
int main(int argc, char **argv) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    OverlaySceneTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "OverlaySceneTest.moc"
