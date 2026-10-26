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
    // The page's answer to one B press, for the input log: "back: <what happened>" or "ignored: <why>".
    Q_INVOKABLE void noteBack(const QString &outcome);
    // Public input boundary: tests inject synthetic events through the same path.
    void handleRuntimeEvent(const vr::VREvent_t &event);
    // Public button boundary: tests feed the per-frame button state through the same path as tick().
    void handleButtons(const HubButtons &buttons,bool panelVisible,qint64 nowMs);
    void shutdown();
signals:
    void quitRequested();
    // The controller's B button, pressed once while the panel is shown: the page goes back one level.
    void backRequested();
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
    void logLine(const QByteArray &text);
    // Input-log state: what SteamVR reported for the actions last time, the last raw B, and the last tick.
    int m_loggedActions = -1;
    bool m_rawBack = false;
    qint64 m_lastTick = -1;
    QFile m_inputLog;
    int m_inputLines = 0, m_inputMoves = 0;
    bool m_loggedVisible = false;
    bool m_quitSeen = false;
    bool m_running = false;
    PressEdge m_backPress;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_reportTime = 0;
    quint64 m_frames = 0;
    double m_cpuSum = 0;
    double m_cpuMax = 0;
};
}
