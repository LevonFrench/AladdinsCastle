// SPDX-License-Identifier: GPL-3.0-only
#include "OverlayKeyboard.h"
#include "OverlayInput.h"
#include "OverlayRuntime.h"
#include "SpikeState.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <algorithm>
namespace ac {
OverlayKeyboard::OverlayKeyboard(SpikeState &state, OverlayInput &input, OverlayRuntime &runtime)
    :m_state(state),m_input(input),m_runtime(runtime) {}
void OverlayKeyboard::setWindow(QQuickWindow *window) {
    close(false,true);disconnect(m_focusConnection);m_window=window;
    if(window)m_focusConnection=connect(window,&QQuickWindow::activeFocusItemChanged,this,[this]{
        if(m_open&&m_window->activeFocusItem()!=m_target)close(false,true);
    });
}
void OverlayKeyboard::request(QQuickItem *item) {
    if(!m_window||!item||item->window()!=m_window)return;
    if(m_open&&m_target==item)return;
    close(false,true);m_target=item;item->forceActiveFocus(Qt::OtherFocusReason);++m_token;
    QString error;
    if(!m_runtime.showKeyboard(item->property("text").toString(),m_token,&error)){
        m_state.setStatus(error+". Use the in-scene keys below.");m_target.clear();return;
    }
    m_open=true;m_state.setStatus("SteamVR minimal keyboard opened; type and press Done.");
}
void OverlayKeyboard::close(bool clearFocus,bool hide) {
    if(!m_open){m_target.clear();return;}
    m_open=false;auto target=m_target;m_target.clear();
    if(hide)m_runtime.hideKeyboard();
    if(clearFocus&&target)target->setFocus(false);
}
bool OverlayKeyboard::handle(const vr::VREvent_t &event) {
    if(event.eventType==vr::VREvent_KeyboardCharInput||event.eventType==vr::VREvent_KeyboardDone){
        if(!m_open||!m_target||!matchesKeyboardSession(event,m_token,m_runtime.handle()))return true;
        if(event.eventType==vr::VREvent_KeyboardDone){
            m_state.setStatus("SteamVR keyboard Done received; confirm the text in VR.");close(true,true);return true;
        }
        const auto &bytes=event.data.keyboard.cNewInput;
        const auto end=std::find(std::begin(bytes),std::end(bytes),'\0');
        const auto text=QString::fromUtf8(bytes,end-std::begin(bytes));
        m_target->forceActiveFocus(Qt::OtherFocusReason);m_input.sendText(text,m_window);emit dirty();return true;
    }
    if(event.eventType==vr::VREvent_KeyboardClosed||event.eventType==vr::VREvent_KeyboardClosed_Global){
        if(m_open&&matchesKeyboardSession(event,m_token,m_runtime.handle())){
            close(true,false);m_state.setStatus("SteamVR keyboard closed; in-scene keys remain available.");
        }
        return true;
    }
    return false;
}
}
