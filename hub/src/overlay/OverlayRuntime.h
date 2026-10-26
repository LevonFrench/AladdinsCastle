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
    // The Hub's button read. panelActive: the Hub's dashboard panel is on show. Otherwise the button goes back to the game.
    virtual HubButtons pollButtons(bool panelActive){Q_UNUSED(panelActive);return {};}
    virtual void shutdown() = 0;
};
// Loads and checks exports only; never calls VR_Init or contacts SteamVR.
bool probeOpenVrLibrary(QString *error);
std::unique_ptr<OverlayRuntime> makeOpenVrRuntime(bool pictureFlipY=false);
// Only the two explicit command-line actions call this function.
bool changeOverlayRegistration(const QString &manifest, bool add, QString *error);
// SteamVR cannot follow ".." in a manifest: it looked for resources\<program>.exe, called the path invalid and
// skipped the Hub, which then ran as an anonymous app. This writes the running program's absolute path, folder and
// action manifest into the Hub's own manifest (only when they differ). No OpenVR call; safe in every mode.
bool refreshOverlayManifest(const QString &manifest, QString *error);
}
