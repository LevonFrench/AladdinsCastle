// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "OverlayInput.h"
#include "OverlayKeyboard.h"
#include "OverlayRuntime.h"
#include "OverlayCapture.h"
#include <QElapsedTimer>
#include <QFile>
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
    bool showDashboard(QString *error){return m_running&&m_runtime->showDashboard(error);}
    bool configureCapture(const QString &catalogRoot,const QString &directory,bool pictureFlipY,QString *error){return m_capture.configure(catalogRoot,directory,pictureFlipY,error);}
    // Appends one line per pointer packet of interest to a fresh file (positions and outcomes only).
    void setInputLog(const QString &path);
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
    OverlayCapture m_capture;
    void logInput(const vr::VREvent_t &event,const OverlayObservation &value);
    QFile m_inputLog;
    int m_inputLines = 0, m_inputMoves = 0;
    bool m_loggedVisible = false;
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
