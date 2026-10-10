// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QElapsedTimer>
#include <QMap>
#include <QPointer>
#include <QPointingDevice>
#include <memory>
#include <optional>
#include <openvr.h>
#include <functional>
#include <utility>
class QMouseEvent;
namespace ac {
// Keyboard packets may be queued after HideKeyboard. An old close/done must
// never dismiss a later session or accept text meant for another overlay.
bool matchesKeyboardSession(const vr::VREvent_t &event, quint64 token, vr::VROverlayHandle_t handle);
// The OpenVR header specifies GL bottom-left coordinates. A diagnostic toggle
// permits comparing runtimes against that contract without platform guesses.
struct OverlayObservation {
    quint32 cursor=0;
    QPointF cachedRaw, cachedPosition, position;
    QPoint angle;
    int qtType=0;
    Qt::MouseButtons buttons=Qt::NoButton;
    bool valid=false, dispatched=false, canceled=false, hasOwner=false;
    quint32 owner=0;
    qint64 receiptMs=0, pressMs=0;
    QPointF pressPosition;
    QString reason;
};
class OverlayInput : public QObject {
public:
    using Clock = std::function<qint64()>;
    explicit OverlayInput(QSize size = {1280,800}, Clock clock = {});
    ~OverlayInput() override;
    void setFlipY(bool flip);
    void bindReceiver(QObject *receiver);
    void setVisible(bool visible);
    QPointF mapPosition(float x, float y) const;
    bool dispatch(const vr::VREvent_t &event, QObject *receiver);
    void releaseButtons(QObject *receiver);
    void sendText(const QString &text, QObject *receiver);
    Qt::MouseButtons buttons() const { return m_buttons; }
    QPointF position() const { return m_position; }
    // Observes accepted packets and the Qt event submitted by this translator.
    // It does not supply position, button state or routing decisions.
    using Observer = std::function<void(const vr::VREvent_t &, QPointF, QPoint, int, Qt::MouseButtons)>;
    void setObserver(Observer observer) { m_observer = std::move(observer); }
    using DiagnosticObserver = std::function<void(const vr::VREvent_t &, const OverlayObservation &)>;
    void setDiagnosticObserver(DiagnosticObserver observer) { m_diagnosticObserver=std::move(observer); }
protected:
    bool eventFilter(QObject *receiver,QEvent *event) override;
private:
    struct Cursor {
        QPointF raw,position,remainder;
        quint64 epoch=0;
        Qt::MouseButtons held=Qt::NoButton,suppressed=Qt::NoButton;
    };
    qint64 receipt();
    void suppress(const vr::VREvent_t &event);
    void bind(QObject *receiver,qint64 now);
    void cancel(qint64 now,const QString &reason,const vr::VREvent_t *packet=nullptr,bool deferQt=false);
    void finishDeferred(qint64 now);
    void observe(const vr::VREvent_t &event,quint32 cursor,qint64 now,const QString &reason,int qtType=0,QPoint angle={},bool canceled=false);
    void mouse(QEvent::Type type,Qt::MouseButton button,qint64 now);
    void watchTargets();
    void clearTargets();
    QSize m_size;
    bool m_flipY=true;
    QElapsedTimer m_elapsed;
    Clock m_clock;
    qint64 m_lastReceipt=-1,m_packetReceipt=0,m_pressMs=0;
    bool m_inPacket=false,m_canceling=false,m_visible=true,m_focused=true,m_hostVisible=true;
    QPointF m_pressPosition;
    quint64 m_epoch=1;
    QMap<quint32,Cursor> m_cursors;
    std::optional<quint32> m_owner;
    quint32 m_lastCursor=0;
    Qt::MouseButtons m_buttons=Qt::NoButton;
    QPointF m_position{-1,-1};
    QPointer<QObject> m_receiver;
    QMetaObject::Connection m_receiverDeath;
    QList<QMetaObject::Connection> m_targetDeaths;
    std::unique_ptr<QMouseEvent> m_lastMouse,m_pendingMouse;
    QPointer<QObject> m_pendingReceiver;
    QPointer<QPointingDevice> m_pendingDevice;
    quint64 m_cleanupGeneration=0;
    Qt::MouseButtons m_pendingButtons=Qt::NoButton;
    qint64 m_pendingStamp=0;
    bool m_pendingQt=false;
    QPointingDevice m_device;
    Observer m_observer;
    DiagnosticObserver m_diagnosticObserver;
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
