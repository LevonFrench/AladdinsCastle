// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QSize>
#include <openvr.h>
#include <memory>
#include "ProductIdentity.h"
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
// A narrow adapter keeps lifecycle tests independent of the SteamVR service.
class OverlayRuntime {
public:
    virtual ~OverlayRuntime() = default;
    virtual bool initialize(QSize size, const QString &thumbnail, QString *error) = 0;
    virtual bool isVisible() const = 0;
    virtual vr::VROverlayHandle_t handle() const = 0;
    virtual bool pollEvent(vr::VREvent_t *event) = 0;
    virtual bool submit(unsigned int texture, QString *error) = 0;
    virtual bool showKeyboard(const QString &text, quint64 token, QString *error) = 0;
    virtual void hideKeyboard() = 0;
    virtual void acknowledgeQuit() = 0;
    virtual void shutdown() = 0;
};
// Loads and checks exports only; never calls VR_Init or contacts SteamVR.
bool probeOpenVrLibrary(QString *error);
std::unique_ptr<OverlayRuntime> makeOpenVrRuntime(bool pictureFlipY=false);
// Only the two explicit command-line actions call this function.
bool changeOverlayRegistration(const QString &manifest, bool add, QString *error);
}
