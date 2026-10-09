// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QSize>
#include <openvr.h>
#include <memory>
namespace ac {
inline constexpr char overlayKey[] = "org.aladdinscastle.hub";
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
std::unique_ptr<OverlayRuntime> makeOpenVrRuntime();
// Only the two explicit command-line actions call this function.
bool changeOverlayRegistration(const QString &manifest, bool add, QString *error);
}
