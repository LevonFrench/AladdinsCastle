// SPDX-License-Identifier: GPL-3.0-only
#include "OverlayInput.h"
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
namespace ac {
namespace {
Qt::MouseButton mouseButton(uint32_t button) {
    switch (button) {
    case vr::VRMouseButton_Left: return Qt::LeftButton;
    case vr::VRMouseButton_Right: return Qt::RightButton;
    case vr::VRMouseButton_Middle: return Qt::MiddleButton;
    default: return Qt::NoButton;
    }
}
bool valid(float value) { return std::isfinite(value); }
}
bool matchesKeyboardSession(const vr::VREvent_t &event, quint64 token, vr::VROverlayHandle_t handle) {
    const auto type = event.eventType;
    const bool keyboard = type == vr::VREvent_KeyboardCharInput || type == vr::VREvent_KeyboardDone
                       || type == vr::VREvent_KeyboardClosed || type == vr::VREvent_KeyboardClosed_Global;
    return keyboard && handle != vr::k_ulOverlayHandleInvalid
           && event.data.keyboard.uUserValue == token && event.data.keyboard.overlayHandle == handle;
}
QPointF OverlayInput::mapPosition(float x, float y) const {
    return {std::clamp(qreal(x), qreal(0), qreal(m_size.width())),
            std::clamp(m_flipY ? qreal(m_size.height()) - qreal(y) : qreal(y),
                       qreal(0), qreal(m_size.height()))};
}
bool OverlayInput::dispatch(const vr::VREvent_t &event, QObject *receiver) {
    if (!receiver) return false;
    switch (event.eventType) {
    case vr::VREvent_MouseMove:
    case vr::VREvent_MouseButtonDown:
    case vr::VREvent_MouseButtonUp: {
        if (!valid(event.data.mouse.x) || !valid(event.data.mouse.y)) return false;
        m_position = mapPosition(event.data.mouse.x, event.data.mouse.y);
        Qt::MouseButton button = Qt::NoButton;
        QEvent::Type type = QEvent::MouseMove;
        if (event.eventType != vr::VREvent_MouseMove) {
            button = mouseButton(event.data.mouse.button);
            if (button == Qt::NoButton) return false;
            if (event.eventType == vr::VREvent_MouseButtonDown) {
                type = QEvent::MouseButtonPress; m_buttons |= button;
            } else { type = QEvent::MouseButtonRelease; m_buttons &= ~Qt::MouseButtons(button); }
        }
        QMouseEvent mapped(type, m_position, m_position, m_position, button,
                           m_buttons, Qt::NoModifier);
        if (m_observer) m_observer(event, mapped.position(), {}, mapped.type(), mapped.buttons());
        QCoreApplication::sendEvent(receiver, &mapped);
        return true;
    }
    case vr::VREvent_ScrollSmooth:
    case vr::VREvent_ScrollDiscrete: {
        if (!valid(event.data.scroll.xdelta) || !valid(event.data.scroll.ydelta)) return false;
        // SteamVR reports scroll units; preserve fractional wheel units across
        // samples so slow laser scrolling is not rounded down to zero forever.
        const QPointF units(std::clamp(qreal(event.data.scroll.xdelta), qreal(-100), qreal(100)),
                            std::clamp(qreal(event.data.scroll.ydelta), qreal(-100), qreal(100)));
        const QPointF accumulated = m_wheelRemainder + units * 120;
        const QPoint angle(qRound(accumulated.x()), qRound(accumulated.y()));
        m_wheelRemainder = accumulated - QPointF(angle);
        QWheelEvent mapped(m_position, m_position, {}, angle, m_buttons,
                           Qt::NoModifier, Qt::NoScrollPhase, false);
        if (m_observer) m_observer(event, mapped.position(), mapped.angleDelta(), mapped.type(), mapped.buttons());
        QCoreApplication::sendEvent(receiver, &mapped);
        return true;
    }
    case vr::VREvent_FocusLeave:
    case vr::VREvent_OverlayHidden:
        releaseButtons(receiver); return true;
    default: return false;
    }
}
void OverlayInput::releaseButtons(QObject *receiver) {
    if (receiver) {
        for (const auto button : {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton}) {
            if (!m_buttons.testFlag(button)) continue;
            m_buttons &= ~Qt::MouseButtons(button);
            QMouseEvent released(QEvent::MouseButtonRelease, m_position, m_position,
                                 m_position, button, m_buttons, Qt::NoModifier);
            QCoreApplication::sendEvent(receiver, &released);
        }
        QEvent leave(QEvent::Leave); QCoreApplication::sendEvent(receiver, &leave);
    }
    m_buttons = Qt::NoButton;
    m_wheelRemainder = {};
}
void OverlayInput::sendText(const QString &text, QObject *receiver) {
    if (!receiver) return;
    // Minimal keyboard mode sends UTF-8 text, backspace and ANSI arrow codes.
    const QList<std::pair<QString, int>> special = {
        {"\x1b[D", Qt::Key_Left}, {"\x1b[C", Qt::Key_Right},
        {"\x1b[A", Qt::Key_Up}, {"\x1b[B", Qt::Key_Down}};
    for (const auto &[sequence, key] : special) {
        if (text != sequence) continue;
        QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier);
        QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier);
        QCoreApplication::sendEvent(receiver, &down); QCoreApplication::sendEvent(receiver, &up);
        return;
    }
    // Split by Unicode scalar, never between the two halves of a surrogate pair.
    for (const auto value : text.toUcs4()) {
        const auto scalar = static_cast<char32_t>(value);
        const QString part = QString::fromUcs4(&scalar, 1);
        const int key = scalar == 8 || scalar == 127 ? Qt::Key_Backspace
                      : scalar == 10 || scalar == 13 ? Qt::Key_Return : 0;
        QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier, key == 0 ? part : QString());
        QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier, key == 0 ? part : QString());
        QCoreApplication::sendEvent(receiver, &down); QCoreApplication::sendEvent(receiver, &up);
    }
}
bool OverlayFrameGate::shouldRender(bool visible, qint64 nowMs) {
    if (m_stopped) return false;
    if (visible && !m_visible) m_dirty = true;
    m_visible = visible;
    if (!visible || !m_dirty || nowMs - m_lastFrame < 16) return false;
    m_dirty = false; // changes signaled during rendering remain pending
    return true;
}
}
