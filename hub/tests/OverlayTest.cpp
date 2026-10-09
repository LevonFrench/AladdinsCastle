// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "overlay/OverlayHost.h"
#include "overlay/SpikeState.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>
#include <limits>
class Recorder : public QObject {
public:
    QList<QEvent::Type> types;
    QList<Qt::MouseButtons> buttonStates;
    QList<Qt::MouseButton> changedButtons;
    QList<int> keys;
    QString typed;
    QPointF position;
    QPoint wheel;
    bool event(QEvent *event) override {
        types.append(event->type());
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            position = mouse->position(); buttonStates.append(mouse->buttons()); changedButtons.append(mouse->button());
        } else if (event->type() == QEvent::Wheel) { wheel += static_cast<QWheelEvent *>(event)->angleDelta(); }
        else if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event); keys.append(key->key()); typed += key->text();
        }
        return true;
    }
};
struct RuntimeReceipt { int acknowledgements = 0; int registrations = 0; int hides = 0; };
class FakeRuntime final : public ac::OverlayRuntime {
public:
    explicit FakeRuntime(RuntimeReceipt &receipt) : m_receipt(receipt) {}
    bool initialize(QSize, const QString &, QString *) override { return true; }
    bool isVisible() const override { return false; }
    vr::VROverlayHandle_t handle() const override { return 42; }
    bool pollEvent(vr::VREvent_t *) override { return false; }
    bool submit(unsigned int, QString *) override { return true; }
    bool showKeyboard(const QString &, quint64, QString *) override { return false; }
    void hideKeyboard() override { ++m_receipt.hides; }
    void acknowledgeQuit() override { ++m_receipt.acknowledgements; }
    void shutdown() override { }
private:
    RuntimeReceipt &m_receipt;
};
class OverlayTest : public QObject {
    Q_OBJECT
private slots:
    void coordinateContractAndDiagnosticOverride() {
        ac::OverlayInput input({1280, 900});
        QCOMPARE(input.mapPosition(0, 0), QPointF(0, 900));
        QCOMPARE(input.mapPosition(1280, 900), QPointF(1280, 0));
        QCOMPARE(input.mapPosition(640, 450), QPointF(640, 450));
        QCOMPARE(input.mapPosition(-100, 1200), QPointF(0, 0));
        input.setFlipY(false); QCOMPARE(input.mapPosition(25, 40), QPointF(25, 40));
    }
    void dragButtonStateAndHideRelease() {
        ac::OverlayInput input; Recorder receiver;
        vr::VREvent_t event{}; event.eventType = vr::VREvent_MouseButtonDown;
        event.data.mouse = {30, 850, vr::VRMouseButton_Left, 0};
        QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(receiver.position, QPointF(30, 50)); QCOMPARE(input.buttons(), Qt::LeftButton);
        event.eventType = vr::VREvent_MouseMove; event.data.mouse.x = 50;
        QVERIFY(input.dispatch(event, &receiver)); QCOMPARE(receiver.buttonStates.last(), Qt::LeftButton);
        event.eventType = vr::VREvent_MouseButtonDown; event.data.mouse.button = vr::VRMouseButton_Right;
        QVERIFY(input.dispatch(event, &receiver)); QCOMPARE(input.buttons(), Qt::LeftButton | Qt::RightButton);
        event.eventType = vr::VREvent_OverlayHidden; QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(input.buttons(), Qt::NoButton); QCOMPARE(receiver.types.last(), QEvent::Leave);
        QCOMPARE(receiver.changedButtons.last(), Qt::RightButton); QCOMPARE(receiver.buttonStates.last(), Qt::NoButton);
    }
    void releaseDoesNotPressAndUnsupportedInputIsRejected() {
        ac::OverlayInput input; Recorder receiver;
        vr::VREvent_t event{}; event.eventType = vr::VREvent_MouseButtonDown;
        event.data.mouse.button = 123; QVERIFY(!input.dispatch(event, &receiver)); QVERIFY(receiver.types.isEmpty());
        event.data.mouse.button = vr::VRMouseButton_Middle; QVERIFY(input.dispatch(event, &receiver));
        event.eventType = vr::VREvent_MouseButtonUp; QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(receiver.changedButtons.last(), Qt::MiddleButton); QCOMPARE(input.buttons(), Qt::NoButton);
        event.eventType = vr::VREvent_MouseMove; event.data.mouse.x = std::numeric_limits<float>::quiet_NaN();
        QVERIFY(!input.dispatch(event, &receiver)); QVERIFY(!input.dispatch(event, nullptr));
    }
    void smoothWheelKeepsFractionalUnits() {
        ac::OverlayInput input; Recorder receiver;
        vr::VREvent_t event{}; event.eventType = vr::VREvent_ScrollSmooth;
        event.data.scroll.ydelta = 0.001F;
        for (int i = 0; i < 1000; ++i) QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(receiver.wheel, QPoint(0, 120));
        event.eventType = vr::VREvent_ScrollDiscrete; event.data.scroll.xdelta = -1; event.data.scroll.ydelta = 0;
        QVERIFY(input.dispatch(event, &receiver)); QCOMPARE(receiver.wheel, QPoint(-120, 120));
    }
    void keyboardUnicodeAndSpecialKeys() {
        ac::OverlayInput input; Recorder receiver;
        input.sendText(QString::fromUtf8("é漢😀"), &receiver);
        QCOMPARE(receiver.typed, QString::fromUtf8("é漢😀")); QCOMPARE(receiver.keys.size(), 3);
        input.sendText("\b", &receiver); QCOMPARE(receiver.keys.last(), int(Qt::Key_Backspace));
        input.sendText("\x1b[D", &receiver); QCOMPARE(receiver.keys.last(), int(Qt::Key_Left));
        input.sendText("\r", &receiver); QCOMPARE(receiver.keys.last(), int(Qt::Key_Return));
        QCOMPARE(receiver.types.count(QEvent::KeyPress), receiver.types.count(QEvent::KeyRelease));
    }
    void delayedKeyboardCloseCannotDismissNewSession() {
        vr::VREvent_t event{};
        for (const auto type : {vr::VREvent_KeyboardCharInput, vr::VREvent_KeyboardDone,
                               vr::VREvent_KeyboardClosed, vr::VREvent_KeyboardClosed_Global}) {
            event.eventType = type; event.data.keyboard.overlayHandle = 42;
            event.data.keyboard.uUserValue = 6;
            QVERIFY(!ac::matchesKeyboardSession(event, 7, 42)); // previous session
            event.data.keyboard.uUserValue = 7;
            QVERIFY(ac::matchesKeyboardSession(event, 7, 42));
            event.data.keyboard.overlayHandle = 43;
            QVERIFY(!ac::matchesKeyboardSession(event, 7, 42)); // another overlay
        }
        QVERIFY(!ac::matchesKeyboardSession(event, 7, vr::k_ulOverlayHandleInvalid));
    }
    void dirtyDuringRenderIsPreserved() {
        ac::OverlayFrameGate gate;
        QVERIFY(gate.shouldRender(true, 0));
        gate.markDirty(); gate.submitted(0); // renderRequested during sync/render
        QVERIFY(gate.shouldRender(true, 16)); gate.submitted(16);
        QVERIFY(!gate.shouldRender(true, 32));
    }
    void hiddenFramesRetainDirtyWorkAndQuitStopsFrames() {
        ac::OverlayFrameGate gate;
        QVERIFY(!gate.shouldRender(false, 0)); QVERIFY(gate.shouldRender(true, 0)); gate.submitted(0);
        QVERIFY(!gate.shouldRender(true, 20)); gate.markDirty();
        QVERIFY(!gate.shouldRender(true, 8)); QVERIFY(gate.shouldRender(true, 16)); gate.submitted(16);
        gate.markDirty(); QVERIFY(!gate.shouldRender(false, 32)); QVERIFY(gate.shouldRender(true, 48)); gate.submitted(48);
        QVERIFY(!gate.shouldRender(false, 64)); QVERIFY(gate.shouldRender(true, 80)); gate.submitted(80);
        gate.stop(); gate.markDirty(); QVERIFY(!gate.shouldRender(true, 1000));
    }
    void quitAcknowledgedOnceBeforeExitSignal() {
        ac::SpikeState state; RuntimeReceipt receipt;
        ac::OverlayHost host(state, std::make_unique<FakeRuntime>(receipt));
        bool signaled = false;
        connect(&host, &ac::OverlayHost::quitRequested, this, [&] { QCOMPARE(receipt.acknowledgements, 1); signaled = true; });
        vr::VREvent_t event{}; event.eventType = vr::VREvent_Quit;
        host.handleRuntimeEvent(event); QVERIFY(signaled);
        host.handleRuntimeEvent(event); QCOMPARE(receipt.acknowledgements, 1); QCOMPARE(receipt.hides, 0);
    }
    void explicitRegistrationModesCannotBeCombined() {
        QCOMPARE(ac::parseLaunchOptions({"--register-overlay"}).mode, ac::Mode::RegisterOverlay);
        QCOMPARE(ac::parseLaunchOptions({"--unregister-overlay"}).mode, ac::Mode::UnregisterOverlay);
        QVERIFY(ac::parseLaunchOptions({"--register-overlay", "--overlay"}).error.size() > 0);
        QVERIFY(ac::parseLaunchOptions({"--unregister-overlay", "--window"}).error.size() > 0);
        QVERIFY(ac::parseLaunchOptions({"--register-overlay", "--spike"}).error.size() > 0);
        QVERIFY(ac::parseLaunchOptions({"--spike"}).spike);
    }
    void sceneLoadsWithoutSteamVrOrGameContent() {
        QTemporaryDir dir; QVERIFY(dir.isValid()); QVERIFY(QDir(dir.path()).mkpath("games"));
        QString executable = QDir(QCoreApplication::applicationDirPath()).filePath("aladdinscastle-hub");
#ifdef Q_OS_WIN
        executable += ".exe";
#endif
        QProcess process; auto env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM", "offscreen"); env.insert("QT_QUICK_BACKEND", "software");
        process.setProcessEnvironment(env);
        process.start(executable, {"--spike", "--data-root", dir.path(), "--quit-after-ms", "250"});
        QVERIFY(process.waitForStarted()); QVERIFY(process.waitForFinished(15000));
        const auto errors = process.readAllStandardError();
        QVERIFY2(process.exitCode() == 0, errors.constData());
        QVERIFY2(!errors.contains("Error") && !errors.contains("is not installed") && !errors.contains("is not a type"), errors.constData());
        QVERIFY(process.readAllStandardOutput().contains("AladdinsCastle: 0 games"));
        // This verifies QML loading and portable fallback, never GL or VR pixels.
    }
};
QTEST_GUILESS_MAIN(OverlayTest)
#include "OverlayTest.moc"
