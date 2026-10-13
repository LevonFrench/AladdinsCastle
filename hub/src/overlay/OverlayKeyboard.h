// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QPointer>
#include <openvr.h>
class QQuickWindow;
class QQuickItem;
namespace ac {
class SpikeState;
class OverlayInput;
class OverlayRuntime;
// The real host binds this only after runtime/renderer initialization. Synthetic
// scene tests supply a fake keyboard adapter; this class never initializes XR.
class OverlayKeyboard : public QObject {
    Q_OBJECT
public:
    OverlayKeyboard(SpikeState &state, OverlayInput &input, OverlayRuntime &runtime);
    void setWindow(QQuickWindow *window);
    void request(QQuickItem *item);
    void close(bool clearFocus, bool hide);
    bool handle(const vr::VREvent_t &event);
signals:
    void dirty();
private:
    SpikeState &m_state;
    OverlayInput &m_input;
    OverlayRuntime &m_runtime;
    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_target;
    QMetaObject::Connection m_focusConnection;
    quint64 m_token=0;
    bool m_open=false;
};
}
