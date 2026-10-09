// SPDX-License-Identifier: GPL-3.0-only
#include "OverlayRuntime.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdint>
namespace ac {
namespace {
QString overlayError(vr::EVROverlayError error) {
    return QString::fromLatin1(vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
}
class OpenVrRuntime final : public OverlayRuntime {
public:
    ~OpenVrRuntime() override { shutdown(); }
    bool initialize(QSize size, const QString &thumbnail, QString *error) override {
        vr::EVRInitError initError = vr::VRInitError_None;
        m_system = vr::VR_Init(&initError, vr::VRApplication_Overlay);
        if (initError != vr::VRInitError_None) {
            *error = QString("SteamVR overlay initialization: %1").arg(vr::VR_GetVRInitErrorAsEnglishDescription(initError));
            m_system = nullptr; return false;
        }
        m_initialized = true;
        if (!vr::VROverlay()) { *error = "SteamVR IVROverlay is unavailable."; shutdown(); return false; }
        auto result = vr::VROverlay()->CreateDashboardOverlay(overlayKey, "AladdinsCastle", &m_main, &m_thumbnail);
        if (result != vr::VROverlayError_None) { *error = "CreateDashboardOverlay: " + overlayError(result); shutdown(); return false; }
        const vr::HmdVector2_t scale{{static_cast<float>(size.width()), static_cast<float>(size.height())}};
        // Qt's GL target contains the image in GL orientation. OpenVR samples
        // flipped V bounds so the dashboard image is upright. Verify S1 in VR.
        const vr::VRTextureBounds_t bounds{0, 1, 1, 0};
        for (const auto status : {
                vr::VROverlay()->SetOverlayWidthInMeters(m_main, 2.0F),
                vr::VROverlay()->SetOverlayInputMethod(m_main, vr::VROverlayInputMethod_Mouse),
                vr::VROverlay()->SetOverlayMouseScale(m_main, &scale),
                vr::VROverlay()->SetOverlayFlag(m_main, vr::VROverlayFlags_SendVRSmoothScrollEvents, true),
                vr::VROverlay()->SetOverlayTextureBounds(m_main, &bounds),
                vr::VROverlay()->SetOverlayFromFile(m_thumbnail, thumbnail.toUtf8().constData())}) {
            if (status == vr::VROverlayError_None) continue;
            *error = "Configure dashboard overlay: " + overlayError(status); shutdown(); return false;
        }
        return true;
    }
    vr::VROverlayHandle_t handle() const override { return m_main; }
    bool isVisible() const override {
        return m_initialized && m_main != vr::k_ulOverlayHandleInvalid && vr::VROverlay()->IsOverlayVisible(m_main);
    }
    bool pollEvent(vr::VREvent_t *event) override {
        if (!m_initialized) return false;
        // Keyboard char/done events are sent twice (global and overlay). Consume
        // only the overlay copy, and route only global closes for our handle.
        if (vr::VROverlay()->PollNextOverlayEvent(m_main, event, sizeof(*event))) return true;
        while (vr::VROverlay()->PollNextOverlayEvent(m_thumbnail, event, sizeof(*event)))
            if (event->eventType == vr::VREvent_Quit) return true;
        while (m_system->PollNextEvent(event, sizeof(*event))) {
            if (event->eventType == vr::VREvent_Quit) return true;
            if (event->eventType == vr::VREvent_KeyboardClosed_Global && event->data.keyboard.overlayHandle == m_main)
                return true;
        }
        return false;
    }
    bool submit(unsigned int texture, QString *error) override {
        if (!isVisible()) return true; // guard even if hidden between poll and submit
        const vr::Texture_t submitted{reinterpret_cast<void *>(static_cast<uintptr_t>(texture)),
                                      vr::TextureType_OpenGL, vr::ColorSpace_Auto};
        const auto result = vr::VROverlay()->SetOverlayTexture(m_main, &submitted);
        if (result == vr::VROverlayError_None) return true;
        *error = "SetOverlayTexture: " + overlayError(result); return false;
    }
    bool showKeyboard(const QString &text, quint64 token, QString *error) override {
        const auto result = vr::VROverlay()->ShowKeyboardForOverlay(m_main,
            vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
            vr::KeyboardFlag_Minimal | vr::KeyboardFlag_Modal | vr::KeyboardFlag_ShowArrowKeys,
            "AladdinsCastle overlay input", 256, text.toUtf8().constData(), token);
        if (result == vr::VROverlayError_None) return true;
        *error = "SteamVR keyboard: " + overlayError(result); return false;
    }
    void hideKeyboard() override { if (m_initialized) vr::VROverlay()->HideKeyboard(); }
    void acknowledgeQuit() override { if (m_system) m_system->AcknowledgeQuit_Exiting(); }
    void shutdown() override {
        if (!m_initialized) return;
        if (m_main != vr::k_ulOverlayHandleInvalid) {
            vr::VROverlay()->ClearOverlayTexture(m_main);
            // DestroyDashboardOverlay's thumbnail is owned by its main handle.
            vr::VROverlay()->DestroyOverlay(m_main);
        }
        m_main = m_thumbnail = vr::k_ulOverlayHandleInvalid;
        vr::VR_Shutdown(); m_system = nullptr; m_initialized = false;
    }
private:
    vr::IVRSystem *m_system = nullptr;
    vr::VROverlayHandle_t m_main = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t m_thumbnail = vr::k_ulOverlayHandleInvalid;
    bool m_initialized = false;
};
}
std::unique_ptr<OverlayRuntime> makeOpenVrRuntime() { return std::make_unique<OpenVrRuntime>(); }
bool changeOverlayRegistration(const QString &manifest, bool add, QString *error) {
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly)) { *error = "Overlay manifest is missing from the portable resources folder."; return false; }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    const auto applications = doc.object().value("applications").toArray();
    if (applications.size() != 1 || applications.at(0).toObject().value("app_key").toString() != QLatin1String(overlayKey)) {
        *error = "Unexpected overlay manifest contents."; return false;
    }
    vr::EVRInitError initError = vr::VRInitError_None;
    vr::VR_Init(&initError, vr::VRApplication_Utility);
    if (initError != vr::VRInitError_None) {
        *error = QString("SteamVR manifest action: %1").arg(vr::VR_GetVRInitErrorAsEnglishDescription(initError)); return false;
    }
    if (!vr::VRApplications()) { *error = "SteamVR IVRApplications is unavailable."; vr::VR_Shutdown(); return false; }
    const auto path = QFileInfo(manifest).absoluteFilePath().toUtf8();
    const auto result = add ? vr::VRApplications()->AddApplicationManifest(path.constData(), false)
                            : vr::VRApplications()->RemoveApplicationManifest(path.constData());
    if (result != vr::VRApplicationError_None)
        *error = QString::fromLatin1(vr::VRApplications()->GetApplicationsErrorNameFromEnum(result));
    // Never enable automatic launch or launch the dashboard as part of registration.
    vr::VR_Shutdown();
    return result == vr::VRApplicationError_None;
}
}
