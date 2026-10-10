// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "OverlayInput.h"
#include "OverlayKeyboard.h"
#include "OverlayRuntime.h"
#include <QElapsedTimer>
#include <QTimer>
class QQmlEngine;
class QQuickItem;
namespace ac {
class QuickTextureRenderer;
class SpikeState;
class OverlayHost : public QObject {
    Q_OBJECT
public:
    OverlayHost(SpikeState &state, std::unique_ptr<OverlayRuntime> runtime = makeOpenVrRuntime(), QObject *parent = nullptr);
    ~OverlayHost() override;
    bool initialize(QQmlEngine &engine, const QString &thumbnail, QString *error);
    Q_INVOKABLE void requestKeyboard(QQuickItem *item);
    Q_INVOKABLE void dismissKeyboard();
    // Public input boundary: tests inject synthetic events through the same path.
    void handleRuntimeEvent(const vr::VREvent_t &event);
    void shutdown();
signals:
    void quitRequested();
    void failed(const QString &error);
private:
    void tick();
    SpikeState &m_state;
    std::unique_ptr<OverlayRuntime> m_runtime;
    std::unique_ptr<QuickTextureRenderer> m_renderer;
    OverlayInput m_input;
    OverlayFrameGate m_gate;
    OverlayKeyboard m_keyboard;
    bool m_quitSeen = false;
    bool m_running = false;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_reportTime = 0;
    quint64 m_frames = 0;
    double m_cpuSum = 0;
    double m_cpuMax = 0;
};
}
