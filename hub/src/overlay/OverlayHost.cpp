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
    : QObject(parent), m_state(state), m_runtime(std::move(runtime)) {
    m_timer.setTimerType(Qt::PreciseTimer); m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &OverlayHost::tick);
    connect(&state, &SpikeState::flipYChanged, this, [this] {
        m_input.setFlipY(m_state.flipY()); m_gate.markDirty();
    });
}
OverlayHost::~OverlayHost() { shutdown(); }
bool OverlayHost::initialize(QQmlEngine &engine, const QString &thumbnail, QString *error) {
    m_renderer = std::make_unique<QuickTextureRenderer>();
    connect(m_renderer.get(), &QuickTextureRenderer::dirty, this, [this] { m_gate.markDirty(); });
    if (!m_renderer->initialize(engine, {1280, 900}, error)) return false;
    if (!m_runtime->initialize(m_renderer->size(), thumbnail, error)) return false;
    m_running = true;
    connect(m_renderer->window(), &QQuickWindow::activeFocusItemChanged, this, [this] {
        if (m_keyboardOpen && m_renderer->window()->activeFocusItem() != m_keyboardTarget)
            closeKeyboard(false, true);
    });
    m_clock.start(); m_timer.start();
    QTextStream(stdout) << "Overlay GL: " << m_renderer->graphicsDescription()
        << "\nOverlay target: 1280x900 RGBA8, visible dirty frames capped at 62.5 Hz.\n"
        << "Render telemetry is CPU submission time, not GPU frame time or VR acceptance.\n";
    return true;
}
void OverlayHost::requestKeyboard(QQuickItem *item) {
    if (!m_running || !item || item->window() != m_renderer->window()) return;
    if (m_keyboardOpen && m_keyboardTarget == item) return;
    closeKeyboard(false, true);
    m_keyboardTarget = item;
    item->forceActiveFocus(Qt::OtherFocusReason);
    ++m_keyboardToken;
    QString error;
    if (!m_runtime->showKeyboard(item->property("text").toString(), m_keyboardToken, &error)) {
        m_state.setStatus(error + ". Use the in-scene keys below."); m_keyboardTarget.clear(); return;
    }
    m_keyboardOpen = true;
    m_state.setStatus("SteamVR minimal keyboard opened; type and press Done.");
}
void OverlayHost::dismissKeyboard() { closeKeyboard(true, true); }
void OverlayHost::closeKeyboard(bool clearFocus, bool hide) {
    if (!m_keyboardOpen) { m_keyboardTarget.clear(); return; }
    m_keyboardOpen = false;
    auto target = m_keyboardTarget; m_keyboardTarget.clear();
    if (hide) m_runtime->hideKeyboard();
    if (clearFocus && target) target->setFocus(false);
}
void OverlayHost::handleRuntimeEvent(const vr::VREvent_t &event) {
    if (m_quitSeen) return;
    if (event.eventType == vr::VREvent_Quit) {
        // Acknowledge before requesting Qt exit, including overlay-only mode.
        m_quitSeen = true; m_gate.stop(); m_timer.stop();
        closeKeyboard(false, true); m_runtime->acknowledgeQuit();
        emit quitRequested(); return;
    }
    if (!m_renderer) return;
    if (event.eventType == vr::VREvent_KeyboardCharInput || event.eventType == vr::VREvent_KeyboardDone) {
        if (!m_keyboardOpen || !m_keyboardTarget || !matchesKeyboardSession(event, m_keyboardToken, m_runtime->handle())) return;
        if (event.eventType == vr::VREvent_KeyboardDone) {
            m_state.setStatus("SteamVR keyboard Done received; confirm the text in VR.");
            closeKeyboard(true, true); return;
        }
        const auto &bytes = event.data.keyboard.cNewInput;
        const auto end = std::find(std::begin(bytes), std::end(bytes), '\0');
        const QString text = QString::fromUtf8(bytes, end - std::begin(bytes));
        m_keyboardTarget->forceActiveFocus(Qt::OtherFocusReason);
        m_input.sendText(text, m_renderer->window());
        m_gate.markDirty(); return;
    }
    if (event.eventType == vr::VREvent_KeyboardClosed || event.eventType == vr::VREvent_KeyboardClosed_Global) {
        if (m_keyboardOpen && matchesKeyboardSession(event, m_keyboardToken, m_runtime->handle())) { closeKeyboard(true, false); m_state.setStatus("SteamVR keyboard closed; in-scene keys remain available."); }
        return;
    }
    if (event.eventType == vr::VREvent_OverlayShown) m_gate.markDirty();
    if (event.eventType == vr::VREvent_OverlayHidden) closeKeyboard(false, true);
    if (m_input.dispatch(event, m_renderer->window())) m_gate.markDirty();
}
void OverlayHost::tick() {
    vr::VREvent_t event{};
    // Bound work per tick; an input flood must not starve Qt's event loop.
    for (int count = 0; count < 256 && m_runtime->pollEvent(&event); ++count) {
        handleRuntimeEvent(event); if (m_quitSeen) return;
    }
    const bool visible = m_runtime->isVisible();
    m_timer.setInterval(visible ? 16 : 50);
    m_renderer->setVisible(visible);
    if (!visible) { m_input.releaseButtons(m_renderer->window()); closeKeyboard(false, true); }
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
    m_timer.stop(); m_gate.stop(); closeKeyboard(false, true);
    // Clear compositor references before deleting the GL texture/context.
    if (m_running) { m_runtime->shutdown(); m_running = false; }
    m_renderer.reset();
}
}
