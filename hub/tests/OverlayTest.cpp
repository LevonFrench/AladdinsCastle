// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "overlay/OverlayHost.h"
#include "overlay/SpikeState.h"
#include <QKeyEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QtTest>
#include <QQmlEngine>
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
    QList<quint64> timestamps;
    bool event(QEvent *event) override {
        types.append(event->type());
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            timestamps.append(mouse->timestamp());position = mouse->position(); buttonStates.append(mouse->buttons()); changedButtons.append(mouse->button());
        } else if (event->type() == QEvent::Wheel) { const auto *mapped=static_cast<QWheelEvent *>(event);timestamps.append(mapped->timestamp());wheel += mapped->angleDelta();position=mapped->position(); }
        else if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);timestamps.append(key->timestamp()); keys.append(key->key()); typed += key->text();
        }
        return true;
    }
};
struct RuntimeReceipt { int acknowledgements = 0; int registrations = 0; int hides = 0; QSize initializedSize; bool rejectInit = false; };
class FakeRuntime final : public ac::OverlayRuntime {
public:
    explicit FakeRuntime(RuntimeReceipt &receipt) : m_receipt(receipt) {}
    bool initialize(QSize size, const QString &, QString *error) override { m_receipt.initializedSize=size; if(m_receipt.rejectInit) { if(error)*error="Synthetic runtime unavailable"; return false; } return true; }
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
    void presentDllProbeDoesNotInitializeRuntime() {
#ifdef Q_OS_WIN
        QVERIFY(QFileInfo::exists(QCoreApplication::applicationDirPath()+"/openvr_api.dll"));
        QString error;QVERIFY2(ac::probeOpenVrLibrary(&error),qPrintable(error));QVERIFY(error.isEmpty());
        QVERIFY2(ac::probeOpenVrLibrary(&error),qPrintable(error));
#else
        QSKIP("Portable DLL probe is Windows-specific; no runtime initialization is permitted.");
#endif
    }
    void coordinateContractAndDiagnosticOverride() {
        ac::OverlayInput input({1280, 800});
        QCOMPARE(input.mapPosition(0, 0), QPointF(0, 800));
        QCOMPARE(input.mapPosition(1280, 800), QPointF(1280, 0));
        QCOMPARE(input.mapPosition(640, 400), QPointF(640, 400));
        QCOMPARE(input.mapPosition(-100, 1200), QPointF(0, 0));
        input.setFlipY(false); QCOMPARE(input.mapPosition(25, 40), QPointF(25, 40));
    }
    void dragButtonStateAndHideRelease() {
        ac::OverlayInput input; Recorder receiver;
        vr::VREvent_t event{}; event.eventType = vr::VREvent_MouseButtonDown;
        event.data.mouse = {30, 750, vr::VRMouseButton_Left, 0};
        event.eventType=vr::VREvent_MouseMove;QVERIFY(input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonDown;
        QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(receiver.position, QPointF(30, 50)); QCOMPARE(input.buttons(), Qt::LeftButton);
        event.eventType = vr::VREvent_MouseMove; event.data.mouse.x = 50;
        QVERIFY(input.dispatch(event, &receiver)); QCOMPARE(receiver.buttonStates.last(), Qt::LeftButton);
        event.eventType = vr::VREvent_MouseButtonDown; event.data.mouse.button = vr::VRMouseButton_Right;
        QVERIFY(input.dispatch(event, &receiver)); QCOMPARE(input.buttons(), Qt::LeftButton | Qt::RightButton);
        event.eventType = vr::VREvent_OverlayHidden; QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(input.buttons(), Qt::NoButton); QCOMPARE(receiver.types.last(), QEvent::Leave);
        QCOMPARE(receiver.changedButtons.at(receiver.changedButtons.size()-2), Qt::RightButton); QCOMPARE(receiver.buttonStates.last(), Qt::NoButton);QCOMPARE(receiver.position,QPointF(-1,-1));
    }
    void releaseDoesNotPressAndUnsupportedInputIsRejected() {
        ac::OverlayInput input; Recorder receiver;
        vr::VREvent_t event{}; event.eventType = vr::VREvent_MouseButtonDown;
        event.data.mouse.button = 123; QVERIFY(!input.dispatch(event, &receiver)); QVERIFY(receiver.types.isEmpty());
        event.eventType=vr::VREvent_MouseMove;QVERIFY(input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonDown;
        event.data.mouse.button = vr::VRMouseButton_Middle; QVERIFY(input.dispatch(event, &receiver));
        event.eventType = vr::VREvent_MouseButtonUp; QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(receiver.changedButtons.last(), Qt::MiddleButton); QCOMPARE(input.buttons(), Qt::NoButton);
        event.eventType = vr::VREvent_MouseMove; event.data.mouse.x = std::numeric_limits<float>::quiet_NaN();
        QVERIFY(!input.dispatch(event, &receiver)); QVERIFY(!input.dispatch(event, nullptr));
    }
    void smoothWheelKeepsFractionalUnits() {
        ac::OverlayInput input; Recorder receiver;
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,700,0,0};QVERIFY(input.dispatch(event,&receiver));event={};event.eventType = vr::VREvent_ScrollSmooth;
        event.data.scroll.ydelta = 0.001F;
        for (int i = 0; i < 1000; ++i) QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(receiver.wheel, QPoint(0, 120));
        event.eventType = vr::VREvent_ScrollDiscrete; event.data.scroll.xdelta = -1; event.data.scroll.ydelta = 0;
        QVERIFY(input.dispatch(event, &receiver)); QCOMPARE(receiver.wheel, QPoint(-120, 120));
    }
    void numericObservationsRemainBoundedAndDoNotChangeDispatch() {
        ac::SpikeState state;ac::OverlayInput plain,observed;Recorder first,second;
        observed.setObserver([&](const auto &event,auto position,auto angle,int type,auto buttons){state.recordInput(event,position,angle,type,buttons);});
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;
        for(quint32 cursor=0;cursor<20;++cursor){event.data.mouse={float(cursor*10),float(600+cursor),0,cursor};QVERIFY(plain.dispatch(event,&first));QVERIFY(observed.dispatch(event,&second));}
        QCOMPARE(first.types,second.types);QCOMPARE(first.position,second.position);QCOMPARE(plain.position(),observed.position());
        QCOMPARE(state.cursors().size(),16);QCOMPARE(state.droppedPackets(),4);QCOMPARE(state.pointerPosition(),observed.position());
        event.data.mouse.x=std::numeric_limits<float>::quiet_NaN();QVERIFY(!observed.dispatch(event,&second));QCOMPARE(state.droppedPackets(),4);
        event={};event.eventType=vr::VREvent_KeyboardCharInput;event.data.keyboard.cNewInput[0]='q';
        state.recordInput(event,{1,2},{},int(QEvent::KeyPress),Qt::NoButton);QCOMPARE(state.pointerPosition(),observed.position());QCOMPARE(state.cursors().size(),16);
    }
    void buttonAuthorityOwnershipAndSuppressedChord() {
        ac::OverlayInput input;input.setFlipY(false);Recorder receiver;vr::VREvent_t event{};
        event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,100,0,0};QVERIFY(input.dispatch(event,&receiver));
        event.data.mouse={900,600,0,1};QVERIFY(input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse={999,700,vr::VRMouseButton_Left,0};QVERIFY(input.dispatch(event,&receiver));
        QCOMPARE(receiver.position,QPointF(100,100));
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.x=std::numeric_limits<float>::quiet_NaN();event.data.mouse.button=vr::VRMouseButton_Right;QVERIFY(input.dispatch(event,&receiver));QCOMPARE(receiver.position,QPointF(100,100));event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(input.dispatch(event,&receiver));event.data.mouse.button=vr::VRMouseButton_Left;event.eventType=vr::VREvent_MouseButtonDown;
        event.data.mouse.cursorIndex=1;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&receiver));QCOMPARE(input.buttons(),Qt::LeftButton);
        event.data.mouse.cursorIndex=0;QVERIFY(input.dispatch(event,&receiver));QCOMPARE(input.buttons(),Qt::NoButton);
    }
    void missingMoveAndOffPanelInvalidateBeforeClamp() {
        ac::OverlayInput input;Recorder receiver;vr::VREvent_t event{};
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse={100,700,vr::VRMouseButton_Left,0};QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseMove;QVERIFY(input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseMove;event.data.mouse.x=-1;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.x=100;QVERIFY(!input.dispatch(event,&receiver));
    }
    void wheelRemainderAndPositionBelongToCursor() {
        ac::OverlayInput input;input.setFlipY(false);Recorder receiver;vr::VREvent_t event{};
        event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,100,0,0};QVERIFY(input.dispatch(event,&receiver));
        event.data.mouse={900,600,0,1};QVERIFY(input.dispatch(event,&receiver));
        event={};event.eventType=vr::VREvent_ScrollSmooth;event.data.scroll.ydelta=0.002F;event.data.scroll.cursorIndex=0;
        QVERIFY(input.dispatch(event,&receiver));QCOMPARE(receiver.position,QPointF(100,100));
        event.data.scroll.cursorIndex=1;QVERIFY(input.dispatch(event,&receiver));
        event.data.scroll.cursorIndex=0;QVERIFY(input.dispatch(event,&receiver));QCOMPARE(receiver.wheel,QPoint(0,0));
    }
    void receiptClockEpochAndZeroTuningBoundaries() {
        qint64 now=1000;int samples=0;QList<ac::OverlayObservation> observed;ac::OverlayInput input({1280,800},[&]{++samples;return now;});input.setFlipY(false);samples=0;Recorder first,second;
        input.setDiagnosticObserver([&](const auto &,const auto &value){observed<<value;});
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,100,0,0};QVERIFY(input.dispatch(event,&first));QCOMPARE(samples,1);QCOMPARE(first.timestamps.last(),quint64(1000));
        now=1000000;event.eventAgeSeconds=std::numeric_limits<float>::quiet_NaN();event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.button=vr::VRMouseButton_Left;QVERIFY(input.dispatch(event,&first));QCOMPARE(samples,2);QCOMPARE(observed.last().pressMs,now);
        for(const auto step:{0,99,100,101}){now=1000000+step;event.eventType=vr::VREvent_MouseMove;event.data.mouse.x=float(100+step*0.4);QVERIFY(input.dispatch(event,&first));QCOMPARE(first.position,QPointF(event.data.mouse.x,100));QCOMPARE(first.timestamps.last(),quint64(now));QCOMPARE(observed.last().pressPosition,QPointF(100,100));}
        now=1000000+100;QVERIFY(!input.dispatch(event,&first));QCOMPARE(input.buttons(),Qt::NoButton);QCOMPARE(observed.last().reason,QString("clock-regressed"));
        now=1000000+101;event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&first));
        event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(!input.dispatch(event,&first));event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&first));
        event.eventType=vr::VREvent_MouseMove;QVERIFY(input.dispatch(event,&first));event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(!input.dispatch(event,&second));
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&second));event.eventType=vr::VREvent_MouseMove;QVERIFY(input.dispatch(event,&second));event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(input.dispatch(event,&second));
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(input.dispatch(event,&second));
        const auto before=samples;input.sendText("abc",&second);QCOMPARE(samples,before+1);for(const auto stamp:second.timestamps)QVERIFY(stamp<=quint64(now));
    }
    void rejectedAndCanceledButtonsStaySuppressedUntilUp() {
        ac::OverlayInput input;input.setFlipY(false);Recorder receiver;vr::VREvent_t event{};
        event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,100,0,0};QVERIFY(input.dispatch(event,&receiver));event.data.mouse.cursorIndex=1;QVERIFY(input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse={100,100,vr::VRMouseButton_Left,0};QVERIFY(input.dispatch(event,&receiver));event.data.mouse.cursorIndex=1;QVERIFY(!input.dispatch(event,&receiver));
        event.data.mouse.cursorIndex=0;event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(input.dispatch(event,&receiver));event.data.mouse.cursorIndex=1;event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(input.dispatch(event,&receiver));
        event={};event.eventType=vr::VREvent_FocusLeave;QVERIFY(input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_FocusEnter;QVERIFY(input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseMove;event.data.mouse={200,200,0,1};QVERIFY(input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.button=vr::VRMouseButton_Left;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(input.dispatch(event,&receiver));
    }
    void aSuppressedChordCannotRestartOnAnotherButton() {
        ac::OverlayInput input;Recorder receiver;vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,700,0,0};QVERIFY(input.dispatch(event,&receiver));event.data.mouse.cursorIndex=1;QVERIFY(input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse={100,700,vr::VRMouseButton_Left,0};QVERIFY(input.dispatch(event,&receiver));event.data.mouse.cursorIndex=1;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonUp;event.data.mouse.cursorIndex=0;QVERIFY(input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse={100,700,vr::VRMouseButton_Right,1};QVERIFY(!input.dispatch(event,&receiver));QCOMPARE(input.buttons(),Qt::NoButton);
        event.eventType=vr::VREvent_MouseButtonUp;event.data.mouse.button=vr::VRMouseButton_Left;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(!input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&receiver));event.data.mouse.button=vr::VRMouseButton_Right;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.button=vr::VRMouseButton_Left;QVERIFY(input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(input.dispatch(event,&receiver));
    }
    void externalCancellationAdvancesClockHighWater_data(){QTest::addColumn<int>("kind");QTest::newRow("host cancel")<<0;QTest::newRow("orientation")<<1;QTest::newRow("receiver bind")<<2;QTest::newRow("focus out")<<3;}
    void externalCancellationAdvancesClockHighWater(){
        QFETCH(int,kind);qint64 now=100;int samples=0;ac::OverlayObservation last;ac::OverlayInput input({1280,800},[&]{++samples;return now;});input.setFlipY(false);input.setDiagnosticObserver([&](const auto &,const auto &value){last=value;});Recorder first,second;
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,100,0,0};QVERIFY(input.dispatch(event,&first));now=101;event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.button=vr::VRMouseButton_Left;QVERIFY(input.dispatch(event,&first));
        now=200;if(kind==0)input.releaseButtons(&first);else if(kind==1)input.setFlipY(true);else if(kind==2)input.bindReceiver(&second);else{QFocusEvent focus(QEvent::FocusOut,Qt::OtherFocusReason);QCoreApplication::sendEvent(&first,&focus);}
        QCOMPARE(first.timestamps.last(),quint64(200));now=150;event.eventType=vr::VREvent_MouseMove;const auto before=samples;QVERIFY(!input.dispatch(event,&first));QCOMPARE(samples,before+1);QCOMPARE(last.reason,QString("clock-regressed"));
        now=201;event.eventType=vr::VREvent_FocusEnter;QVERIFY(input.dispatch(event,&first));event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&first));event.eventType=vr::VREvent_MouseMove;QVERIFY(input.dispatch(event,&first));now=202;event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(input.dispatch(event,&first));event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(input.dispatch(event,&first));
        quint64 previous=0;for(const auto stamp:first.timestamps){QVERIFY(stamp>=previous);previous=stamp;}
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
    void failedRuntimeDoesNotAllocateRenderer() {
        ac::SpikeState state;RuntimeReceipt receipt;receipt.rejectInit=true;QQmlEngine engine;ac::OverlayHost host(state,std::make_unique<FakeRuntime>(receipt));QString error;
        QVERIFY(!host.initialize(engine,{},&error));QCOMPARE(receipt.initializedSize,QSize(1280,800));QCOMPARE(error,QString("Synthetic runtime unavailable"));host.shutdown();
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
