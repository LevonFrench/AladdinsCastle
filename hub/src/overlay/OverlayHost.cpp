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
    m_input.setDiagnosticObserver([this](const auto &event,const auto &value){m_state.recordInput(event,value);});
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
    vr::VREvent_t event{};
    // Bound work per tick; an input flood must not starve Qt's event loop.
    for (int count = 0; count < 256 && m_runtime->pollEvent(&event); ++count) {
        handleRuntimeEvent(event); if (m_quitSeen) return;
    }
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
