// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QPointF>
#include <QSize>
#include <openvr.h>
namespace ac {
// Keyboard packets may be queued after HideKeyboard. An old close/done must
// never dismiss a later session or accept text meant for another overlay.
bool matchesKeyboardSession(const vr::VREvent_t &event, quint64 token, vr::VROverlayHandle_t handle);
// The OpenVR header specifies GL bottom-left coordinates. A diagnostic toggle
// permits comparing runtimes against that contract without platform guesses.
class OverlayInput {
public:
    explicit OverlayInput(QSize size = {1280, 900}) : m_size(size) {}
    void setFlipY(bool flip) { m_flipY = flip; }
    QPointF mapPosition(float x, float y) const;
    bool dispatch(const vr::VREvent_t &event, QObject *receiver);
    void releaseButtons(QObject *receiver);
    void sendText(const QString &text, QObject *receiver);
    Qt::MouseButtons buttons() const { return m_buttons; }
    QPointF position() const { return m_position; }
private:
    QSize m_size;
    bool m_flipY = true;
    Qt::MouseButtons m_buttons = Qt::NoButton;
    QPointF m_position;
    QPointF m_wheelRemainder;
};
// Deterministic, hardware-free policy used by the real host. Hidden dirty work
// remains pending and visibility transitions guarantee a fresh first frame.
class OverlayFrameGate {
public:
    void markDirty() { m_dirty = true; }
    bool shouldRender(bool visible, qint64 nowMs);
    void submitted(qint64 nowMs) { m_lastFrame = nowMs; }
    void stop() { m_stopped = true; }
private:
    bool m_dirty = true;
    bool m_visible = false;
    bool m_stopped = false;
    qint64 m_lastFrame = -16;
};
}
