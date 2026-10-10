// SPDX-License-Identifier: GPL-3.0-only
#include "OverlayInput.h"
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QScopedValueRollback>
#include <QTimer>
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
OverlayInput::OverlayInput(QSize size,Clock clock)
    :m_size(size),m_clock(std::move(clock)),m_device("AladdinsCastle overlay",qint64(reinterpret_cast<quintptr>(this)),
      QInputDevice::DeviceType::Mouse,QPointingDevice::PointerType::Generic,QInputDevice::Capability::Position,1,3) {
    m_elapsed.start();if(!m_clock)m_clock=[this]{return m_elapsed.elapsed();};
}
OverlayInput::~OverlayInput(){finishDeferred(std::max(qint64(0),m_lastReceipt));cancel(std::max(qint64(0),m_lastReceipt),"shutdown");}
qint64 OverlayInput::receipt(){
    const auto value=std::max(qint64(0),std::max(m_lastReceipt,m_inPacket?m_packetReceipt:m_clock()));
    if(!m_inPacket)m_lastReceipt=value;
    return value;
}
void OverlayInput::clearTargets(){for(const auto &connection:m_targetDeaths)disconnect(connection);m_targetDeaths.clear();}
void OverlayInput::watchTargets(){
    clearTargets();if(!m_lastMouse||!m_owner)return;
    QList<QPointer<QObject>> targets=m_lastMouse->passiveGrabbers(m_lastMouse->point(0));
    if(auto target=m_lastMouse->exclusivePointGrabber())targets.append(target);
    const auto direct=targets;for(const auto &target:direct)if(target&&qobject_cast<QQuickItem *>(target->parent()))targets.append(target->parent());
    for(const auto &target:targets)if(target)m_targetDeaths.append(connect(target,&QObject::destroyed,this,[this]{cancel(std::max(qint64(0),receipt()),"target-destroyed",nullptr,true);}));
}
void OverlayInput::mouse(QEvent::Type type,Qt::MouseButton button,qint64 now){
    if(!m_receiver)return;
    QMouseEvent event(type,m_position,m_position,m_position,button,m_buttons,Qt::NoModifier,Qt::MouseEventSynthesizedByApplication,&m_device);
    event.setTimestamp(quint64(now));QCoreApplication::sendEvent(m_receiver,&event);
    if(m_owner){m_lastMouse.reset(event.clone());watchTargets();}
}
void OverlayInput::observe(const vr::VREvent_t &event,quint32 id,qint64 now,const QString &reason,int qtType,QPoint angle,bool canceled){
    OverlayObservation observation;observation.cursor=id;observation.receiptMs=m_inPacket?m_packetReceipt:now;observation.position=m_position;
    observation.angle=angle;observation.qtType=qtType;observation.buttons=m_buttons;observation.dispatched=qtType!=0;
    observation.canceled=canceled;observation.hasOwner=m_owner.has_value();observation.owner=m_owner.value_or(0);
    observation.pressMs=m_pressMs;observation.pressPosition=m_pressPosition;observation.reason=reason;
    const auto cursor=m_cursors.constFind(id);if(cursor!=m_cursors.cend()){
        observation.cachedRaw=cursor->raw;observation.cachedPosition=cursor->position;observation.valid=cursor->epoch==m_epoch;
    }
    if(m_diagnosticObserver)m_diagnosticObserver(event,observation);
    if(qtType&&m_observer)m_observer(event,m_position,angle,qtType,m_buttons);
}
void OverlayInput::cancel(qint64 now,const QString &reason,const vr::VREvent_t *packet,bool deferQt){
    if(m_canceling)return;
    QScopedValueRollback<bool> canceling(m_canceling,true);
    const auto sample=m_inPacket?m_packetReceipt:now;
    QScopedValueRollback<bool> active(m_inPacket,true);QScopedValueRollback<qint64> time(m_packetReceipt,sample);
    const auto id=m_owner.value_or(m_lastCursor);
    bool hadState=m_owner.has_value();for(auto &cursor:m_cursors){hadState|=cursor.epoch==m_epoch;cursor.suppressed|=cursor.held;cursor.epoch=0;cursor.remainder={};}
    ++m_epoch;clearTargets();
    if(deferQt){
        const bool scheduled=m_pendingQt;
        if(m_lastMouse)m_pendingMouse=std::move(m_lastMouse);
        if(m_receiver)m_pendingReceiver=m_receiver;
        m_pendingButtons|=m_buttons;m_pendingStamp=std::max(m_pendingStamp,now);m_pendingDevice=&m_device;m_pendingQt=true;
        if(!scheduled){const auto generation=++m_cleanupGeneration;QTimer::singleShot(0,this,[this,generation]{if(m_pendingQt&&generation==m_cleanupGeneration)finishDeferred(receipt());});}
        m_owner.reset();m_position={-1,-1};m_buttons=Qt::NoButton;
    }else{
        // Cancel actual Qt grabbers before releasing this dedicated device off-panel.
        if(m_lastMouse){m_lastMouse->setExclusivePointGrabber(nullptr);m_lastMouse->clearPassiveGrabbers(m_lastMouse->point(0));m_lastMouse.reset();}
        m_owner.reset();m_position={-1,-1};
        if(m_receiver&&m_buttons){
            for(const auto button:{Qt::LeftButton,Qt::RightButton,Qt::MiddleButton})if(m_buttons.testFlag(button)){
                m_buttons&=~Qt::MouseButtons(button);mouse(QEvent::MouseButtonRelease,button,now);
            }
            mouse(QEvent::MouseMove,Qt::NoButton,now);
        }
        m_buttons=Qt::NoButton;
        if(m_receiver&&hadState){QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_receiver,&leave);}
    }
    if(hadState||packet){vr::VREvent_t event{};if(packet)event=*packet;observe(event,id,now,reason,0,{},true);}
}
void OverlayInput::finishDeferred(qint64 now){
    if(!m_pendingQt)return;
    // QObject child destruction can occur after QQuickWindow's delivery agent
    // has begun teardown. Never send input from that destruction callback.
    const auto stamp=std::max(now,m_pendingStamp);m_lastReceipt=std::max(m_lastReceipt,stamp);
    auto mouseEvent=std::move(m_pendingMouse);const auto receiver=m_pendingReceiver;const auto device=m_pendingDevice;auto buttons=m_pendingButtons;
    m_pendingReceiver.clear();m_pendingDevice.clear();m_pendingButtons=Qt::NoButton;m_pendingQt=false;++m_cleanupGeneration;
    QScopedValueRollback<bool> canceling(m_canceling,true);QScopedValueRollback<bool> active(m_inPacket,true);QScopedValueRollback<qint64> time(m_packetReceipt,stamp);
    if(!device)return;
    if(mouseEvent){mouseEvent->setExclusivePointGrabber(nullptr);mouseEvent->clearPassiveGrabbers(mouseEvent->point(0));}
    if(!receiver)return;
    for(const auto button:{Qt::LeftButton,Qt::RightButton,Qt::MiddleButton})if(buttons.testFlag(button)){
        buttons&=~Qt::MouseButtons(button);
        QMouseEvent event(QEvent::MouseButtonRelease,{-1,-1},{-1,-1},{-1,-1},button,buttons,Qt::NoModifier,Qt::MouseEventSynthesizedByApplication,device.data());
        event.setTimestamp(quint64(stamp));QCoreApplication::sendEvent(receiver,&event);
    }
    QMouseEvent move(QEvent::MouseMove,{-1,-1},{-1,-1},{-1,-1},Qt::NoButton,Qt::NoButton,Qt::NoModifier,Qt::MouseEventSynthesizedByApplication,device.data());move.setTimestamp(quint64(stamp));QCoreApplication::sendEvent(receiver,&move);
    QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(receiver,&leave);
}
void OverlayInput::bind(QObject *receiver,qint64 now){
    if(m_receiver==receiver)return;
    finishDeferred(now);
    cancel(now,"receiver-changed");if(m_receiver)m_receiver->removeEventFilter(this);disconnect(m_receiverDeath);
    m_receiver=receiver;m_visible=true;m_focused=true;
    if(receiver){receiver->installEventFilter(this);m_receiverDeath=connect(receiver,&QObject::destroyed,this,[this]{m_receiver.clear();cancel(std::max(qint64(0),receipt()),"receiver-destroyed",nullptr,true);});}
}
void OverlayInput::bindReceiver(QObject *receiver){bind(receiver,std::max(qint64(0),receipt()));}
void OverlayInput::setVisible(bool visible){
    if(m_hostVisible==visible)return;
    m_hostVisible=visible;
    if(visible){m_visible=true;m_focused=true;}
    cancel(receipt(),visible?"host-visible-restored":"host-hidden");
}
void OverlayInput::setFlipY(bool flip){if(flip==m_flipY)return;cancel(std::max(qint64(0),receipt()),"orientation-changed");m_flipY=flip;}
bool OverlayInput::eventFilter(QObject *receiver,QEvent *event){
    if(receiver!=m_receiver)return false;
    if(event->type()==QEvent::Hide){m_visible=false;cancel(receipt(),"window-hidden",nullptr,qobject_cast<QQuickWindow *>(receiver)!=nullptr);}
    else if(event->type()==QEvent::FocusOut||event->type()==QEvent::WindowDeactivate){m_focused=false;cancel(receipt(),"window-focus-lost",nullptr,qobject_cast<QQuickWindow *>(receiver)!=nullptr);}
    else if(event->type()==QEvent::Show&&!m_visible){m_visible=true;cancel(receipt(),"window-shown");}
    else if((event->type()==QEvent::FocusIn||event->type()==QEvent::WindowActivate)&&!m_focused){m_focused=true;cancel(receipt(),"window-focus-restored");}
    return false;
}
void OverlayInput::releaseButtons(QObject *receiver){const auto now=receipt();bind(receiver,now);cancel(now,"host-cancel");}
void OverlayInput::suppress(const vr::VREvent_t &event){
    if(event.eventType!=vr::VREvent_MouseButtonDown&&event.eventType!=vr::VREvent_MouseButtonUp)return;
    const auto id=event.data.mouse.cursorIndex,raw=event.data.mouse.button;const auto button=mouseButton(raw);
    if(button==Qt::NoButton||(!m_cursors.contains(id)&&m_cursors.size()>=64))return;
    auto &cursor=m_cursors[id];
    if(event.eventType==vr::VREvent_MouseButtonDown){cursor.held|=button;cursor.suppressed|=button;}
    else{cursor.held&=~Qt::MouseButtons(button);cursor.suppressed&=~Qt::MouseButtons(button);}
}
bool OverlayInput::dispatch(const vr::VREvent_t &event,QObject *receiver){
    const auto now=m_clock();QScopedValueRollback<bool> active(m_inPacket,true);QScopedValueRollback<qint64> time(m_packetReceipt,now);
    finishDeferred(std::max(qint64(0),std::max(now,m_lastReceipt)));
    bind(receiver,std::max(qint64(0),std::max(now,m_lastReceipt)));
    const bool pointer=event.eventType==vr::VREvent_MouseMove||event.eventType==vr::VREvent_MouseButtonDown||event.eventType==vr::VREvent_MouseButtonUp;
    const bool wheel=event.eventType==vr::VREvent_ScrollSmooth||event.eventType==vr::VREvent_ScrollDiscrete;
    const auto id=wheel?event.data.scroll.cursorIndex:pointer?event.data.mouse.cursorIndex:m_lastCursor;
    if(now<0||now<m_lastReceipt){
        cancel(std::max(qint64(0),m_lastReceipt),"clock-regressed",&event);
        suppress(event);
        return false;
    }
    m_lastReceipt=now;
    if(!receiver){cancel(now,"missing-receiver");suppress(event);return false;}
    if(event.eventType==vr::VREvent_FocusLeave||event.eventType==vr::VREvent_OverlayHidden){
        if(event.eventType==vr::VREvent_FocusLeave)m_focused=false;else m_visible=false;
        cancel(now,event.eventType==vr::VREvent_FocusLeave?"focus-leave":"overlay-hidden",&event);return true;
    }
    if(event.eventType==vr::VREvent_FocusEnter||event.eventType==vr::VREvent_OverlayShown){
        if(event.eventType==vr::VREvent_OverlayShown){
            m_visible=true;
        }
        m_focused=true;
        cancel(now,event.eventType==vr::VREvent_FocusEnter?"focus-enter":"overlay-shown",&event);return true;
    }
    if(!pointer&&!wheel)return false;
    if(!m_hostVisible||!m_visible||!m_focused){suppress(event);observe(event,id,now,!m_hostVisible?"inactive-host-hidden":!m_visible?"inactive-hidden":"inactive-focus");return false;}
    // Keep admitted IDs stable for this translator lifetime; over-budget IDs
    // never become a late gesture after another ID releases its buttons.
    if(!m_cursors.contains(id)&&m_cursors.size()>=64){observe(event,id,now,"cursor-limit");return false;}
    auto &cursor=m_cursors[id];m_lastCursor=id;
    auto reject=[&](const QString &reason){observe(event,id,now,reason);return false;};
    if(event.eventType==vr::VREvent_MouseMove){
        const auto x=event.data.mouse.x,y=event.data.mouse.y;
        if(!valid(x)||!valid(y)||x<0||y<0||x>=m_size.width()||y>=m_size.height()){
            cursor.epoch=0;cursor.remainder={};if(m_owner==id)cancel(now,"invalid-owner-move",&event);else observe(event,id,now,"invalid-move");return false;
        }
        cursor.raw={x,y};cursor.position=mapPosition(x,y);cursor.epoch=m_epoch;
        if(m_owner&&m_owner!=id)return reject("owned-by-other-cursor");
        m_position=cursor.position;observe(event,id,now,"move",QEvent::MouseMove);mouse(QEvent::MouseMove,Qt::NoButton,now);return true;
    }
    if(pointer){
        const auto button=mouseButton(event.data.mouse.button);if(button==Qt::NoButton)return reject("unknown-button");
        if(event.eventType==vr::VREvent_MouseButtonDown){
            if(cursor.held.testFlag(button)){
                return reject("duplicate-down");
            }
            cursor.held|=button;
            if((m_owner&&m_owner!=id)||cursor.suppressed!=Qt::NoButton||cursor.epoch!=m_epoch){
                const auto reason=m_owner&&m_owner!=id?"owned-by-other-cursor":cursor.suppressed!=Qt::NoButton?"suppressed-chord":"missing-current-move";
                cursor.suppressed|=button;return reject(reason);
            }
            if(!m_owner){m_owner=id;m_pressPosition=cursor.position;m_pressMs=now;}
            m_buttons|=button;m_position=cursor.position;observe(event,id,now,"press",QEvent::MouseButtonPress);mouse(QEvent::MouseButtonPress,button,now);return true;
        }
        const bool held=cursor.held.testFlag(button),suppressed=cursor.suppressed.testFlag(button);
        cursor.held&=~Qt::MouseButtons(button);cursor.suppressed&=~Qt::MouseButtons(button);
        if(suppressed)return reject("suppressed-up");
        if(!held||m_owner!=id||!m_buttons.testFlag(button))return reject("unowned-up");
        if(cursor.epoch!=m_epoch){cancel(now,"invalid-release-position",&event);return false;}
        m_position=cursor.position;m_buttons&=~Qt::MouseButtons(button);
        if(!m_buttons){m_owner.reset();clearTargets();m_lastMouse.reset();}
        observe(event,id,now,"release",QEvent::MouseButtonRelease);mouse(QEvent::MouseButtonRelease,button,now);return true;
    }
    if(m_owner&&m_owner!=id)return reject("owned-by-other-cursor");
    if(cursor.suppressed!=Qt::NoButton)return reject("suppressed-chord");
    if(cursor.epoch!=m_epoch)return reject("missing-current-move");
    if(!valid(event.data.scroll.xdelta)||!valid(event.data.scroll.ydelta))return reject("invalid-scroll");
    const QPointF units(std::clamp(qreal(event.data.scroll.xdelta),qreal(-100),qreal(100)),std::clamp(qreal(event.data.scroll.ydelta),qreal(-100),qreal(100)));
    const auto accumulated=cursor.remainder+units*120;const QPoint angle(qRound(accumulated.x()),qRound(accumulated.y()));cursor.remainder=accumulated-QPointF(angle);m_position=cursor.position;
    QWheelEvent mapped(m_position,m_position,{},angle,m_buttons,Qt::NoModifier,Qt::NoScrollPhase,false,Qt::MouseEventSynthesizedByApplication,&m_device);
    mapped.setTimestamp(quint64(now));observe(event,id,now,"scroll",QEvent::Wheel,angle);QCoreApplication::sendEvent(receiver,&mapped);return true;
}
void OverlayInput::sendText(const QString &text, QObject *receiver) {
    if (!receiver) return;
    const auto now=m_clock();
    if(now<0||now<m_lastReceipt){cancel(std::max(qint64(0),m_lastReceipt),"clock-regressed");return;}
    m_lastReceipt=now;
    // Minimal keyboard mode sends UTF-8 text, backspace and ANSI arrow codes.
    const QList<std::pair<QString, int>> special = {
        {"\x1b[D", Qt::Key_Left}, {"\x1b[C", Qt::Key_Right},
        {"\x1b[A", Qt::Key_Up}, {"\x1b[B", Qt::Key_Down}};
    for (const auto &[sequence, key] : special) {
        if (text != sequence) continue;
        QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier);
        QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier);
        down.setTimestamp(quint64(now));up.setTimestamp(quint64(now));
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
        down.setTimestamp(quint64(now));up.setTimestamp(quint64(now));
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
