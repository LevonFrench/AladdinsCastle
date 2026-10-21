// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QSize>
#include <openvr.h>
#include <memory>
#include "ProductIdentity.h"
class QImage;
class QJsonObject;
namespace ac {
inline constexpr bool overlayMirrorVertically=false;
// Preserve the Qt target's recorded corner ordering. Pointer coordinates use
// their separate OpenVR bottom-left conversion and must not invert the image.
inline constexpr vr::VRTextureBounds_t overlayTextureBounds{0,0,1,1};
inline vr::VRTextureBounds_t overlayPictureBounds(bool flipY){
    auto bounds=overlayTextureBounds;
    if(flipY){bounds.vMin=1;bounds.vMax=0;}
    return bounds;
}
inline constexpr auto &overlayKey = product::overlay_key;
struct OverlaySubmitStatus {
    bool apiCalled=false;
    int apiResult=-1;
};
// Held for the whole time, once: true on the update that reaches it, then not again until released.
class HoldGesture {
public:
    explicit HoldGesture(qint64 holdMs=3000):m_holdMs(holdMs){}
    bool update(bool pressed,qint64 nowMs){
        if(!pressed){m_since=-1;m_fired=false;return false;}
        if(m_since<0)m_since=nowMs;
        if(m_fired||nowMs-m_since<m_holdMs)return false;
        m_fired=true;return true;
    }
private:
    qint64 m_holdMs,m_since=-1;bool m_fired=false;
};
// A button's press, once: true on the update where it goes down, then not again until it is let go.
// A press the runtime reports while it was already down never counts twice.
class PressEdge {
public:
    bool update(bool down){const bool pressed=down&&!m_down;m_down=down;return pressed;}
    void reset(){m_down=false;}
private:
    bool m_down=false;
};
// The Hub's own controller buttons, read once per frame.
struct HubButtons {
    bool closeHeld=false; // "close the game" (held 3 s) while a game runs in the headset
    bool backDown=false;  // B: back one page, while the panel is shown
    // For the input log: SteamVR accepted the Hub's action manifest, and B has an active binding now.
    bool actionsReady=false;
    bool backBound=false;
};
class OverlayTextureSubmitGate {
public:
    bool shouldSubmit(bool visible)const{return visible||!m_hasTexture;}
    void submitted(){m_hasTexture=true;}
    void reset(){m_hasTexture=false;}
private:
    bool m_hasTexture=false;
};
// A narrow adapter keeps lifecycle tests independent of the SteamVR service.
class OverlayRuntime {
public:
    virtual ~OverlayRuntime() = default;
    virtual bool initialize(QSize size, const QString &thumbnail, QString *error) = 0;
    virtual bool isVisible() const = 0;
    virtual vr::VROverlayHandle_t handle() const = 0;
    virtual bool pollEvent(vr::VREvent_t *event) = 0;
    virtual bool submit(unsigned int texture, QString *error,OverlaySubmitStatus *status=nullptr) = 0;
    // An explicit request, not proof that the dashboard has rendered.
    virtual bool showDashboard(QString *error){if(error)*error="Dashboard presentation is unavailable.";return false;}
    virtual bool readNativeImage(QImage *,QJsonObject *,QString *error){if(error)*error="Native overlay image readback is unavailable.";return false;}
    virtual bool showKeyboard(const QString &text, quint64 token, QString *error) = 0;
    virtual void hideKeyboard() = 0;
    virtual void acknowledgeQuit() = 0;
    // Whether the "close the game" button is down now. While active, the Hub's action set
    // runs above the game's own input, so the game does not see that button.
    virtual bool closeGameHeld(bool active){Q_UNUSED(active);return false;}
    // Both Hub buttons in one read. closeActive: a game the Hub started runs in the headset;
    // panelActive: the Hub's dashboard panel is on show. With neither, the buttons go back to the game.
    virtual HubButtons pollButtons(bool closeActive,bool panelActive){Q_UNUSED(panelActive);return {closeGameHeld(closeActive),false};}
    virtual void shutdown() = 0;
};
// Loads and checks exports only; never calls VR_Init or contacts SteamVR.
bool probeOpenVrLibrary(QString *error);
std::unique_ptr<OverlayRuntime> makeOpenVrRuntime(bool pictureFlipY=false);
// Only the two explicit command-line actions call this function.
bool changeOverlayRegistration(const QString &manifest, bool add, QString *error);
}
