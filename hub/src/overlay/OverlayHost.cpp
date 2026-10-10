// SPDX-License-Identifier: GPL-3.0-only
#include "OverlayHost.h"
#include "QuickTextureRenderer.h"
#include "SpikeState.h"
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>
#include <algorithm>
namespace ac {
OverlayHost::OverlayHost(SpikeState &state, std::unique_ptr<OverlayRuntime> runtime, QObject *parent)
    : QObject(parent), m_state(state), m_runtime(std::move(runtime)), m_keyboard(state,m_input,*m_runtime) {
    m_input.setDiagnosticObserver([this](const auto &event,const auto &value){m_state.recordInput(event,value);});
    connect(&m_keyboard,&OverlayKeyboard::dirty,this,[this]{m_gate.markDirty();});
    m_timer.setTimerType(Qt::PreciseTimer); m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &OverlayHost::tick);
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
    if (!m_renderer->initialize(engine, {1280, 800}, error)) { m_runtime->shutdown(); return false; }
    m_running = true;
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
    m_renderer->setVisible(visible);m_input.setVisible(visible);
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
    const bool submitted = m_runtime->submit(m_renderer->texture(), &error);
    m_renderer->doneCurrent();
    if (!submitted) { m_timer.stop(); emit failed(error); return; }
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
    if(m_renderer)m_input.releaseButtons(m_renderer->window());
    m_keyboard.close(false, true);
    // Clear compositor references before deleting the GL texture/context.
    if (m_running) { m_runtime->shutdown(); m_running = false; }
    m_keyboard.setWindow(nullptr);
    m_renderer.reset();
}
}
