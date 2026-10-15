// SPDX-License-Identifier: GPL-3.0-only
#include "OverlayHost.h"
#include "QuickTextureRenderer.h"
#include "SpikeState.h"
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>
#include <QScopeGuard>
#include <algorithm>
namespace ac {
OverlayHost::OverlayHost(SpikeState &state, std::unique_ptr<OverlayRuntime> runtime, QObject *parent)
    : QObject(parent), m_state(state), m_runtime(std::move(runtime)), m_keyboard(state,m_input,*m_runtime) {
    m_input.setDiagnosticObserver([this](const auto &event,const auto &value){m_state.recordInput(event,value);logInput(event,value);});
    connect(&m_keyboard,&OverlayKeyboard::dirty,this,[this]{m_gate.markDirty();});
    m_timer.setTimerType(Qt::PreciseTimer); m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &OverlayHost::tick);
    m_input.setTuning(state.tuning());
    m_input.setFlipY(state.flipY());
    connect(&state,&SpikeState::tuningChanged,this,[this]{m_input.setTuning(m_state.tuning());m_gate.markDirty();});
    connect(&state, &SpikeState::flipYChanged, this, [this] {
        m_input.setFlipY(m_state.flipY()); m_gate.markDirty();
    });
}
OverlayHost::~OverlayHost() { shutdown(); }
void OverlayHost::setInputLog(const QString &path) {
    m_inputLog.close();m_inputLog.setFileName(path);m_inputLines=m_inputMoves=0;
    if(m_inputLog.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text))
        m_inputLog.write(QByteArray("ms type rawX rawY panelX panelY outcome | pointer flip ")+(m_state.flipY()?"on":"off")+", panel 1280x800\n");
}
void OverlayHost::logInput(const vr::VREvent_t &event,const OverlayObservation &value) {
    if(!m_inputLog.isOpen()||m_inputLines>=4000)return;
    // Plain moves are sampled; every press, release, scroll and refusal is kept.
    const bool move=event.eventType==vr::VREvent_MouseMove&&value.reason=="move";
    if(move&&(m_inputMoves++%15)!=0)return;
    const bool mouse=event.eventType==vr::VREvent_MouseMove||event.eventType==vr::VREvent_MouseButtonDown||event.eventType==vr::VREvent_MouseButtonUp;
    const auto line=QString("%1 %2 %3 %4 %5 %6 %7\n").arg(value.receiptMs).arg(event.eventType)
        .arg(mouse?double(event.data.mouse.x):-1.0,0,'f',0).arg(mouse?double(event.data.mouse.y):-1.0,0,'f',0)
        .arg(value.position.x(),0,'f',0).arg(value.position.y(),0,'f',0).arg(value.reason);
    m_inputLog.write(line.toUtf8());m_inputLog.flush();++m_inputLines;
}
bool OverlayHost::initialize(QQmlEngine &engine, const QString &thumbnail, QString *error) {
    // Runtime failure is handled before allocating a rendering context.
    if (!m_runtime->initialize({1280, 800}, thumbnail, error)) return false;
    m_renderer = std::make_unique<QuickTextureRenderer>();
    connect(m_renderer.get(), &QuickTextureRenderer::dirty, this, [this] { m_gate.markDirty(); });
    if (!m_renderer->initialize(engine, {1280, 800}, error,m_capture.requested())) { m_runtime->shutdown(); return false; }
    m_running = true;
    m_gate.requestInitialFrame();
    m_input.bindReceiver(m_renderer->window());
    m_keyboard.setWindow(m_renderer->window());
    m_clock.start(); m_timer.start();
    QTextStream(stdout) << "Overlay GL: " << m_renderer->graphicsDescription()
        << "\nOverlay target: 1280x800 RGBA8, visible dirty frames capped at 62.5 Hz.\n"
        << "Render telemetry is CPU submission time, not GPU frame time or VR acceptance.\n";
    return true;
}
void OverlayHost::requestKeyboard(QQuickItem *item) { if(m_running)m_keyboard.request(item); }
void OverlayHost::dismissKeyboard() { m_keyboard.close(true,true); }
void OverlayHost::handleRuntimeEvent(const vr::VREvent_t &event) {
    if (m_quitSeen) return;
    if (event.eventType == vr::VREvent_Quit) {
        // Acknowledge before requesting Qt exit, including overlay-only mode.
        m_quitSeen = true; m_gate.stop(); m_timer.stop();
        m_keyboard.close(false, true); m_runtime->acknowledgeQuit();
        emit quitRequested(); return;
    }
    if (!m_renderer) return;
    if (m_keyboard.handle(event)) return;
    if (event.eventType == vr::VREvent_OverlayShown) m_gate.markDirty();
    if (event.eventType == vr::VREvent_OverlayHidden) m_keyboard.close(false, true);
    if (m_input.dispatch(event, m_renderer->window())) m_gate.markDirty();
}
void OverlayHost::tick() {
    const bool visible = m_runtime->isVisible();
    const bool initialFrame=m_gate.initialFramePending();
    // Only the offscreen QML item is made renderable for bootstrap. Native
    // visibility remains authoritative for input and keyboard cancellation.
    m_renderer->setVisible(visible||initialFrame);m_input.setVisible(visible);
    if(m_inputLog.isOpen()&&visible!=m_loggedVisible&&m_inputLines<4000){
        m_loggedVisible=visible;m_inputLog.write(QByteArray::number(m_clock.elapsed())+(visible?" panel-visible\n":" panel-hidden\n"));m_inputLog.flush();++m_inputLines;
    }
    vr::VREvent_t event{};
    // Bound work per tick; an input flood must not starve Qt's event loop.
    for (int count = 0; count < 256 && m_runtime->pollEvent(&event); ++count) {
        handleRuntimeEvent(event); if (m_quitSeen) return;
    }
    if (m_closeHold.update(m_runtime->closeGameHeld(m_gestureActive) && m_gestureActive, m_clock.elapsed())) emit closeGameRequested();
    m_timer.setInterval(visible ? 16 : 50);
    if (!visible) { m_input.releaseButtons(m_renderer->window()); m_keyboard.close(false, true); }
    if (!m_gate.shouldRender(visible, m_clock.elapsed())) return;
    QString error;
    if (!m_renderer->render(&error)) { m_timer.stop(); emit failed(error); return; }
    OverlaySubmitStatus submission;
    const bool submitted = m_runtime->submit(m_renderer->texture(), &error,(m_capture.requested()||initialFrame)?&submission:nullptr);
    {
        const auto release=qScopeGuard([this]{m_renderer->doneCurrent();});
        if(m_capture.requested()){
            QString captureError;
            const auto captured=m_capture.attempt([this](QImage *image,QString *message){return m_renderer->readbackCurrentTexture(image,message);},m_renderer->texture(),submission,m_state.flipY(),&captureError,
                [this](QImage *image,QJsonObject *metadata,QString *message){
                    const bool result=m_runtime->readNativeImage(image,metadata,message);
                    if(result)QTextStream(stdout)<<"Native overlay image read; source orientation requires review, compositor/headset unverified.\n";
                    else QTextStream(stderr)<<"Native overlay image read failed: "<<(message?*message:QString())<<'\n';
                    return result;
                });
            if(captured==CaptureAttempt::Captured)QTextStream(stdout)<<"Source texture capture written; compositor display and pointer acceptance remain unverified.\n";
            else if(captured==CaptureAttempt::Failed)QTextStream(stderr)<<"Source texture capture failed: "<<captureError<<'\n';
        }
    }
    if (!submitted) { m_timer.stop(); emit failed(error); return; }
    if(initialFrame&&(!submission.apiCalled||submission.apiResult!=int(vr::VROverlayError_None))){
        m_timer.stop();emit failed("Initial overlay texture was not submitted.");return;
    }
    m_gate.submitted(m_clock.elapsed());
    ++m_frames; m_cpuSum += m_renderer->lastCpuMs(); m_cpuMax = std::max(m_cpuMax, m_renderer->lastCpuMs());
    if (m_clock.elapsed() - m_reportTime >= 5000) {
        QTextStream(stdout) << "Overlay interval: " << m_frames << " submitted dirty frames; CPU render mean "
            << m_cpuSum / static_cast<double>(m_frames) << " ms, max " << m_cpuMax
            << " ms; wall interval " << m_clock.elapsed() - m_reportTime << " ms.\n";
        m_frames = 0; m_cpuSum = m_cpuMax = 0; m_reportTime = m_clock.elapsed();
    }
}
void OverlayHost::shutdown() {
    m_timer.stop(); m_gate.stop();
    if(m_capture.requested()){
        QTextStream(stderr)<<"Source texture capture was not attempted: no rendered overlay frame.\n";
        m_capture.cancel();
    }
    if(m_renderer)m_input.releaseButtons(m_renderer->window());
    m_keyboard.close(false, true);
    // Clear compositor references before deleting the GL texture/context.
    if (m_running) { m_runtime->shutdown(); m_running = false; }
    m_keyboard.setWindow(nullptr);
    m_renderer.reset();
}
}
