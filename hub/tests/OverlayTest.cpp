// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "core/app/PanelAttacher.h"
#include "overlay/OverlayHost.h"
#include "overlay/SpikeState.h"
#include <QKeyEvent>
#include <QSignalSpy>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ProductIdentity.h"
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
    bool submit(unsigned int, QString *,ac::OverlaySubmitStatus *status=nullptr) override { if(status)*status={};return true; }
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
    void textureAndInputCornerContracts() {
        const auto bounds=ac::overlayTextureBounds;
        ac::OverlayInput input({1280,800});
        for(const QPointF corner:QList<QPointF>{{0,0},{1,0},{0,1},{1,1}}){
            QPointF sampled(bounds.uMin+corner.x()*(bounds.uMax-bounds.uMin),
                            bounds.vMin+corner.y()*(bounds.vMax-bounds.vMin));
            if(ac::overlayMirrorVertically)sampled.setY(1-sampled.y());
            QCOMPARE(sampled,corner);
            const auto flipped=ac::overlayPictureBounds(true);
            QCOMPARE(QPointF(flipped.uMin+corner.x()*(flipped.uMax-flipped.uMin),
                             flipped.vMin+corner.y()*(flipped.vMax-flipped.vMin)),QPointF(corner.x(),1-corner.y()));
            QCOMPARE(input.mapPosition(float(corner.x()*1280),float((1-corner.y())*800)),
                     QPointF(corner.x()*1280,corner.y()*800));
        }
    }
    void desktopHubIsAPanelWheneverSteamVrRuns(){
        // The desktop Hub attaches when SteamVR runs; it never changes its mode or opens the dashboard.
        auto options=ac::parseLaunchOptions({});
        QVERIFY(ac::attachesPanelWhenSteamVrRuns(options,false));QCOMPARE(options.mode,ac::Mode::Desktop);QVERIFY(!options.showDashboard);
        QVERIFY(!ac::attachesPanelWhenSteamVrRuns(options,true)); // No display to share.
        auto desktop=ac::parseLaunchOptions({"--desktop"});QVERIFY(desktop.error.isEmpty());QVERIFY(!ac::attachesPanelWhenSteamVrRuns(desktop,false));
        // A Hub that SteamVR itself starts, or a launch, is left as it was asked for.
        for(const auto &arguments:{QStringList{"--overlay","--steamvr-started"},QStringList{"--launch","synthetic"},QStringList{"--register-overlay"}})
            QVERIFY(!ac::attachesPanelWhenSteamVrRuns(ac::parseLaunchOptions(arguments),false));
    }
    void panelAttachesWhenSteamVrStartsLaterAndLetsGoWhenItQuits(){
        // A stand-in for SteamVR: the attach step accepts once SteamVR is ready.
        int attaches=0,detaches=0;bool ready=false;
        ac::PanelAttacher panel([&](QString *error){++attaches;if(!ready&&error)*error="Synthetic SteamVR not ready";return ready;},[&]{++detaches;});
        panel.setRetry(10,3);
        QSignalSpy changes(&panel,&ac::PanelAttacher::attachedChanged),gaveUp(&panel,&ac::PanelAttacher::gaveUp);
        panel.setSteamVrRunning(false);QCOMPARE(attaches,0); // The window alone, until SteamVR runs.
        panel.setSteamVrRunning(true);QCOMPARE(attaches,1);QVERIFY(!panel.attached());
        ready=true;QTRY_VERIFY(panel.attached());QCOMPARE(attaches,2);QCOMPARE(changes.size(),1);
        panel.setSteamVrRunning(true);QCOMPARE(attaches,2);
        // SteamVR quits: the panel goes, the window stays, and it does not rejoin the closing SteamVR.
        panel.steamVrQuit();QVERIFY(!panel.attached());QCOMPARE(detaches,1);
        panel.setSteamVrRunning(true);QTest::qWait(60);QCOMPARE(attaches,2);
        // SteamVR started again: the panel comes back.
        panel.setSteamVrRunning(false);panel.setSteamVrRunning(true);QVERIFY(panel.attached());QCOMPARE(attaches,3);
        // SteamVR gone without a goodbye: let go too.
        panel.setSteamVrRunning(false);QVERIFY(!panel.attached());QCOMPARE(detaches,2);
        // A SteamVR that never accepts: a few tries, then a reason, and the window carries on.
        ready=false;panel.setSteamVrRunning(true);QTRY_COMPARE(gaveUp.size(),1);QCOMPARE(attaches,6);
        QVERIFY(gaveUp.first()[0].toString().contains("not ready"));
    }
    void independentOrientationCommandLine(){
        const auto defaults=ac::parseLaunchOptions({"--overlay"});
        QVERIFY(!defaults.pictureFlipY);
#ifdef Q_OS_WIN
        QVERIFY(!defaults.pointerFlipY);
#else
        QVERIFY(defaults.pointerFlipY);
#endif
        const auto options=ac::parseLaunchOptions({"--overlay","--window","--spike","--overlay-picture-flip-y","on","--overlay-pointer-flip-y","off"});
        QVERIFY2(options.error.isEmpty(),qPrintable(options.error));
        QVERIFY(options.pictureFlipY);QVERIFY(!options.pointerFlipY);
        QVERIFY(!ac::parseLaunchOptions({"--overlay","--overlay-picture-flip-y"}).error.isEmpty());
        QVERIFY(!ac::parseLaunchOptions({"--overlay","--overlay-pointer-flip-y","maybe"}).error.isEmpty());
        QVERIFY(!ac::parseLaunchOptions({"--launch","synthetic","--overlay-picture-flip-y","on"}).error.isEmpty());
    }
    void hiddenSubmitSuccessDoesNotClaimAnApiCall(){
        RuntimeReceipt receipt;FakeRuntime runtime(receipt);ac::OverlaySubmitStatus status{true,0};QString error;
        QVERIFY(runtime.submit(41,&error,&status));QVERIFY(!status.apiCalled);QCOMPARE(status.apiResult,-1);
    }
    void coordinateContractAndDiagnosticOverride() {
        ac::OverlayInput input({1280, 800});input.setTuning({0,0,0,120,true});
        QCOMPARE(input.mapPosition(0, 0), QPointF(0, 800));
        QCOMPARE(input.mapPosition(1280, 800), QPointF(1280, 0));
        QCOMPARE(input.mapPosition(640, 400), QPointF(640, 400));
        QCOMPARE(input.mapPosition(-100, 1200), QPointF(0, 0));
        input.setFlipY(false); QCOMPARE(input.mapPosition(25, 40), QPointF(25, 40));
    }
    void dragButtonStateAndHideRelease() {
        ac::OverlayInput input;input.setTuning({0,0,0,120,true}); Recorder receiver;
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
        ac::OverlayInput input;input.setTuning({0,0,0,120,true}); Recorder receiver;
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
        ac::OverlayInput input;input.setTuning({0,0,0,120,true}); Recorder receiver;
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,700,0,0};QVERIFY(input.dispatch(event,&receiver));event={};event.eventType = vr::VREvent_ScrollSmooth;
        event.data.scroll.ydelta = 0.001F;
        for (int i = 0; i < 1000; ++i) QVERIFY(input.dispatch(event, &receiver));
        QCOMPARE(receiver.wheel, QPoint(0, 120));
        event.eventType = vr::VREvent_ScrollDiscrete; event.data.scroll.xdelta = -1; event.data.scroll.ydelta = 0;
        QVERIFY(input.dispatch(event, &receiver)); QCOMPARE(receiver.wheel, QPoint(-120, 120));
    }
    void numericObservationsRemainBoundedAndDoNotChangeDispatch() {
        ac::SpikeState state;ac::OverlayInput plain,observed;plain.setTuning({0,0,0,120,true});observed.setTuning({0,0,0,120,true});Recorder first,second;
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
        ac::OverlayInput input;input.setTuning({0,0,0,120,true});input.setFlipY(false);Recorder receiver;vr::VREvent_t event{};
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
        ac::OverlayInput input;input.setTuning({0,0,0,120,true});Recorder receiver;vr::VREvent_t event{};
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse={100,700,vr::VRMouseButton_Left,0};QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseMove;QVERIFY(input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseMove;event.data.mouse.x=-1;QVERIFY(!input.dispatch(event,&receiver));
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.x=100;QVERIFY(!input.dispatch(event,&receiver));
    }
    void wheelRemainderAndPositionBelongToCursor() {
        ac::OverlayInput input;input.setTuning({0,0,0,120,true});input.setFlipY(false);Recorder receiver;vr::VREvent_t event{};
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
        ac::OverlayInput input;input.setTuning({0,0,0,120,true});input.setFlipY(false);Recorder receiver;vr::VREvent_t event{};
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
        ac::OverlayInput input;input.setTuning({0,0,0,120,true});Recorder receiver;vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,700,0,0};QVERIFY(input.dispatch(event,&receiver));event.data.mouse.cursorIndex=1;QVERIFY(input.dispatch(event,&receiver));
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
        ac::OverlayInput input;input.setTuning({0,0,0,120,true}); Recorder receiver;
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
    void coldStartRequestsOneFrameWhileStillHidden(){
        ac::OverlayFrameGate gate;gate.requestInitialFrame();
        QVERIFY(gate.initialFramePending());QVERIFY(gate.shouldRender(false,0));gate.submitted(0);
        QVERIFY(!gate.initialFramePending());gate.markDirty();QVERIFY(!gate.shouldRender(false,16));
        QVERIFY(gate.shouldRender(true,32));gate.submitted(32);QVERIFY(!gate.shouldRender(false,48));
        gate.stop();gate.requestInitialFrame();QVERIFY(!gate.shouldRender(false,1000));
    }
    void coldStartSubmissionDoesNotBecomeContinuousHiddenRendering(){
        ac::OverlayTextureSubmitGate gate;QVERIFY(gate.shouldSubmit(false));gate.submitted();
        QVERIFY(!gate.shouldSubmit(false));QVERIFY(gate.shouldSubmit(true));
        gate.reset();QVERIFY(gate.shouldSubmit(false));
    }
    void initialFrameDoesNotGrantHiddenInput(){
        ac::OverlayFrameGate gate;gate.requestInitialFrame();QVERIFY(gate.shouldRender(false,0));
        ac::OverlayInput input;Recorder receiver;input.setVisible(false);
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={30,750,vr::VRMouseButton_Left,0};
        QVERIFY(!input.dispatch(event,&receiver));event.eventType=vr::VREvent_MouseButtonDown;
        QVERIFY(!input.dispatch(event,&receiver));QCOMPARE(input.buttons(),Qt::NoButton);
        QVERIFY(!receiver.types.contains(QEvent::MouseButtonPress));QVERIFY(!receiver.types.contains(QEvent::MouseMove));
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
    // Holding the close button: once at 3 seconds of one unbroken hold, not before and not again until released.
    void closeGestureNeedsAnUnbrokenThreeSecondHold() {
        ac::HoldGesture hold;
        QVERIFY(!hold.update(true,0));QVERIFY(!hold.update(true,2999));QVERIFY(hold.update(true,3000));QVERIFY(!hold.update(true,9000));
        QVERIFY(!hold.update(false,9050));QVERIFY(!hold.update(true,9100));QVERIFY(!hold.update(true,12000));QVERIFY(hold.update(true,12100));
        // Let go before 3 seconds: the count starts again.
        ac::HoldGesture tap;QVERIFY(!tap.update(true,0));QVERIFY(!tap.update(false,2500));QVERIFY(!tap.update(true,2600));QVERIFY(!tap.update(true,5000));QVERIFY(tap.update(true,5600));
        // A runtime without SteamVR input never reports the button.
        RuntimeReceipt receipt;FakeRuntime runtime(receipt);QVERIFY(!runtime.closeGameHeld(true));
    }
    // B on the controller: one step back per press, never repeated while held, never while the panel is hidden.
    // SteamVR cannot follow "../" in a manifest: the Hub writes its own absolute path, folder and action manifest
    // into it, once, and leaves an up-to-date manifest untouched.
    void manifestPointsAtThisProgram() {
        const auto built=QCoreApplication::applicationDirPath()+"/resources/"+QString::fromLatin1(ac::product::short_id)+".vrmanifest";
        if(!QFileInfo::exists(built))QSKIP("Runtime resources are not beside this test binary.");
        QTemporaryDir temp;QVERIFY(temp.isValid());const auto manifest=temp.filePath("hub.vrmanifest");QVERIFY(QFile::copy(built,manifest));
        QFile::setPermissions(manifest,QFile::ReadOwner|QFile::WriteOwner);
        const auto app=[&]{QFile file(manifest);if(!file.open(QIODevice::ReadOnly))return QJsonObject{};return QJsonDocument::fromJson(file.readAll()).object().value("applications").toArray().at(0).toObject();};
        QCOMPARE(app().value("action_manifest_path").toString(),QString("hub-actions.json")); // In the template already.
        QString error;QVERIFY2(ac::refreshOverlayManifest(manifest,&error),qPrintable(error));
        QCOMPARE(app().value("binary_path_windows").toString(),QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
        QCOMPARE(app().value("working_directory").toString(),QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));
        QVERIFY(!app().value("binary_path_windows").toString().contains(".."));
        const auto written=QFileInfo(manifest).lastModified();QTest::qWait(1100);
        QVERIFY(ac::refreshOverlayManifest(manifest,&error));QCOMPARE(QFileInfo(manifest).lastModified(),written); // Unchanged: not rewritten.
        // Another program's manifest is refused, never rewritten.
        QFile other(temp.filePath("other.vrmanifest"));QVERIFY(other.open(QIODevice::WriteOnly));other.write(R"({"applications":[{"app_key":"someone.else"}]})");other.close();
        QVERIFY(!ac::refreshOverlayManifest(other.fileName(),&error));
    }
    void backButtonOncePerPressWhilePanelShown() {
        ac::PressEdge edge;
        QVERIFY(edge.update(true));QVERIFY(!edge.update(true));QVERIFY(!edge.update(false));QVERIFY(edge.update(true));
        ac::SpikeState state;RuntimeReceipt receipt;ac::OverlayHost host(state,std::make_unique<FakeRuntime>(receipt));
        QSignalSpy back(&host,&ac::OverlayHost::backRequested),close(&host,&ac::OverlayHost::closeGameRequested);
        const ac::HubButtons up{false,false},down{false,true};
        host.handleButtons(down,true,0);QCOMPARE(back.size(),1);
        // Held for many frames: still one.
        for(int frame=1;frame<120;++frame)host.handleButtons(down,true,frame*16);
        QCOMPARE(back.size(),1);
        // Spammed: one per press, however fast.
        for(int press=0;press<10;++press){host.handleButtons(up,true,2000+press*32);host.handleButtons(down,true,2016+press*32);}
        QCOMPARE(back.size(),11);
        // Hidden: nothing, and a press held while the panel opens is not a new press.
        host.handleButtons(up,false,3000);host.handleButtons(down,false,3016);QCOMPARE(back.size(),11);
        host.handleButtons(down,true,3032);QCOMPARE(back.size(),11);
        host.handleButtons(up,true,3048);host.handleButtons(down,true,3064);QCOMPARE(back.size(),12);
        // B never closes the game, and the close hold still needs its own button and three seconds.
        QCOMPARE(close.size(),0);host.setCloseGesture(true);
        host.handleButtons({true,false},true,4000);host.handleButtons({true,false},true,7000);QCOMPARE(close.size(),1);QCOMPARE(back.size(),12);
        // A runtime without SteamVR input reports neither button.
        FakeRuntime runtime(receipt);const auto none=runtime.pollButtons(true,true);QVERIFY(!none.closeHeld);QVERIFY(!none.backDown);
        // The shipped action manifest names B, and each controller binds it to the right hand's B, apart from the close button.
        const auto resources=QCoreApplication::applicationDirPath()+"/resources/";
        if(!QFileInfo::exists(resources+"hub-actions.json"))QSKIP("Runtime resources are not beside this test binary.");
        const auto read=[&](const QString &name){QFile file(resources+name);return file.open(QIODevice::ReadOnly)?QString::fromUtf8(file.readAll()):QString();};
        QVERIFY(read("hub-actions.json").contains("/actions/hub/in/back"));
        for(const auto &name:{"hub-bindings-knuckles.json","hub-bindings-oculus_touch.json"}){
            const auto text=read(name);QVERIFY2(text.contains("/user/hand/right/input/b"),name);QVERIFY2(text.contains("/actions/hub/in/back"),name);QVERIFY2(text.contains("/actions/hub/in/close_game"),name);
        }
    }
    void quitAcknowledgedOnceBeforeExitSignal() {
        ac::SpikeState state; RuntimeReceipt receipt;
        ac::OverlayHost host(state, std::make_unique<FakeRuntime>(receipt));
        bool signaled = false;
        connect(&host, &ac::OverlayHost::quitRequested, this, [&] { QCOMPARE(receipt.acknowledgements, 1); signaled = true; });
        vr::VREvent_t event{}; event.eventType = vr::VREvent_Quit;
        host.handleRuntimeEvent(event); QVERIFY(signaled);
        host.handleRuntimeEvent(event); QCOMPARE(receipt.acknowledgements, 1); QCOMPARE(receipt.hides, 0);
        // After the panel let go, a later SteamVR session can quit again.
        host.shutdown(); signaled = false; receipt.acknowledgements = 0;
        host.handleRuntimeEvent(event); QVERIFY(signaled); QCOMPARE(receipt.acknowledgements, 1);
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
