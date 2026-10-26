// SPDX-License-Identifier: GPL-3.0-only
#include "core/launch/Launch.h"
#include "OverlayRuntime.h"
#include "OverlayCapture.h"
#include <QFile>
#include <QSaveFile>
#include <QCoreApplication>
#include <QDir>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdint>
namespace ac {
bool probeOpenVrLibrary(QString *error) {
#ifdef Q_OS_WIN
    // Resolve only the portable DLL; never let the delay loader raise SEH on
    // absent DLLs or search another application's runtime installation.
    static HMODULE module = nullptr;
    if (!module) {
        const auto path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/openvr_api.dll");
        module = LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr,
                               LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (!module) {
        *error = "OpenVR runtime DLL is missing or cannot be loaded from the portable folder.";
        return false;
    }
    for (const auto *symbol : {"VR_InitInternal2", "VR_ShutdownInternal", "VR_GetGenericInterface",
                               "VR_IsInterfaceVersionValid", "VR_GetInitToken", "VR_GetVRInitErrorAsEnglishDescription"}) {
        if (!GetProcAddress(module, symbol)) {
            *error = "OpenVR runtime DLL is incompatible: required exports are missing.";
            return false;
        }
    }
#else
    Q_UNUSED(error);
#endif
    return true;
}
namespace {
class NativeImageOps final:public NativeOverlayReadbackOps {
public:
    NativeImageOps(vr::IVROverlay &overlay,vr::VROverlayHandle_t handle):overlay_(overlay),handle_(handle){}
    vr::EVROverlayError bounds(vr::VRTextureBounds_t *value)override{return overlay_.GetOverlayTextureBounds(handle_,value);}
    vr::EVROverlayError image(void *buffer,uint32_t bytes,uint32_t *width,uint32_t *height)override{return overlay_.GetOverlayImageData(handle_,buffer,bytes,width,height);}
private:
    vr::IVROverlay &overlay_;vr::VROverlayHandle_t handle_;
};
QString overlayError(vr::EVROverlayError error) {
    return QString::fromLatin1(vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
}
class OpenVrRuntime final : public OverlayRuntime {
public:
    explicit OpenVrRuntime(bool pictureFlipY):m_pictureFlipY(pictureFlipY){}
    ~OpenVrRuntime() override { shutdown(); }
    bool initialize(QSize size, const QString &thumbnail, QString *error) override {
        m_submitGate.reset();
        if (!probeOpenVrLibrary(error)) return false;
        // An overlay connecting to SteamVR would start it if it were not running: only ever join a running one.
        if (!launch::processRunning({"vrserver.exe", "vrserver"})) { if (error) *error = "SteamVR is not running."; return false; }
        vr::EVRInitError initError = vr::VRInitError_None;
        m_system = vr::VR_Init(&initError, vr::VRApplication_Overlay);
        if (initError != vr::VRInitError_None) {
            *error = QString("SteamVR overlay initialization: %1").arg(vr::VR_GetVRInitErrorAsEnglishDescription(initError));
            m_system = nullptr; return false;
        }
        m_initialized = true;
        if (!vr::VROverlay()) { *error = "SteamVR IVROverlay is unavailable."; shutdown(); return false; }
        auto result = vr::VROverlay()->CreateDashboardOverlay(overlayKey, product::display_name, &m_main, &m_thumbnail);
        if (result != vr::VROverlayError_None) { *error = "CreateDashboardOverlay: " + overlayError(result); shutdown(); return false; }
        const vr::HmdVector2_t scale{{static_cast<float>(size.width()), static_cast<float>(size.height())}};
        // The reversed V bounds inverted the owner's dashboard image.
        // Preserve the recorded M1 correction; pointer Y is separate.
        const auto bounds=overlayPictureBounds(m_pictureFlipY);
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
    bool submit(unsigned int texture, QString *error,OverlaySubmitStatus *status=nullptr) override {
        if(status)*status={};
        if(!m_initialized||m_main==vr::k_ulOverlayHandleInvalid){
            if(error)*error="Dashboard overlay is not initialized.";
            return false;
        }
        // A dashboard may need its first texture before it becomes visible.
        // Once submitted, preserve the hidden-between-poll-and-submit guard.
        if (!m_submitGate.shouldSubmit(isVisible())) return true;
        const vr::Texture_t submitted{reinterpret_cast<void *>(static_cast<uintptr_t>(texture)),
                                      vr::TextureType_OpenGL, vr::ColorSpace_Auto};
        const auto result = vr::VROverlay()->SetOverlayTexture(m_main, &submitted);
        if(status)*status={true,int(result)};
        if (result == vr::VROverlayError_None) {m_submitGate.submitted();return true;}
        *error = "SetOverlayTexture: " + overlayError(result); return false;
    }
    bool showDashboard(QString *error) override {
        if(!m_initialized||m_main==vr::k_ulOverlayHandleInvalid){if(error)*error="Dashboard overlay is not initialized.";return false;}
        vr::VROverlay()->ShowDashboard(overlayKey);return true;
    }
    bool readNativeImage(QImage *image,QJsonObject *metadata,QString *error)override{
        if(!m_initialized||m_main==vr::k_ulOverlayHandleInvalid||!vr::VROverlay()){
            if(error)*error="Native overlay handle is unavailable.";
            return false;
        }
        NativeImageOps ops(*vr::VROverlay(),m_main);
        const bool result=readNativeOverlayImage(ops,image,metadata,error);
        if(metadata)(*metadata)["overlay_handle_hex"]=QString::number(m_main,16);
        return result;
    }
    bool showKeyboard(const QString &text, quint64 token, QString *error) override {
        const auto result = vr::VROverlay()->ShowKeyboardForOverlay(m_main,
            vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
            vr::KeyboardFlag_Minimal | vr::KeyboardFlag_Modal | vr::KeyboardFlag_ShowArrowKeys,
            product::display_name, 256, text.toUtf8().constData(), token);
        if (result == vr::VROverlayError_None) return true;
        *error = "SteamVR keyboard: " + overlayError(result); return false;
    }
    void hideKeyboard() override { if (m_initialized) vr::VROverlay()->HideKeyboard(); }
    void acknowledgeQuit() override { if (m_system) m_system->AcknowledgeQuit_Exiting(); }
    HubButtons pollButtons(bool panelActive) override {
        if (!m_initialized || !vr::VRInput()) return {};
        if (!m_actionsTried) {
            m_actionsTried = true;
            const auto manifest = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/resources/hub-actions.json");
            m_actionsReady = QFileInfo(manifest).isFile()
                && vr::VRInput()->SetActionManifestPath(manifest.toUtf8().constData()) == vr::VRInputError_None
                && vr::VRInput()->GetActionSetHandle("/actions/hub", &m_actionSet) == vr::VRInputError_None
                && vr::VRInput()->GetActionHandle("/actions/hub/in/back", &m_back) == vr::VRInputError_None;
        }
        if (!m_actionsReady) return {};
        if (!panelActive) {
            // Give the buttons back to the game.
            if (m_actionsActive) { vr::VRActiveActionSet_t none{}; vr::VRInput()->UpdateActionState(&none, sizeof none, 0); m_actionsActive = false; }
            return {};
        }
        vr::VRActiveActionSet_t set{};set.ulActionSet = m_actionSet;set.nPriority = vr::k_nActionSetOverlayGlobalPriorityMin;
        if (vr::VRInput()->UpdateActionState(&set, sizeof set, 1) != vr::VRInputError_None) return {};
        m_actionsActive = true;
        // Down, and whether SteamVR has the action bound and active at all.
        const auto read = [](vr::VRActionHandle_t action, bool *active) {
            *active = false;
            if (action == vr::k_ulInvalidActionHandle) return false;
            vr::InputDigitalActionData_t data{};
            if (vr::VRInput()->GetDigitalActionData(action, &data, sizeof data, vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None) return false;
            *active = data.bActive;
            return data.bActive && data.bState;
        };
        HubButtons buttons;
        buttons.actionsReady = true;
        bool backBound = false;
        const bool back = read(m_back, &backBound);
        buttons.backDown = panelActive && back;
        buttons.backBound = backBound;
        return buttons;
    }
    void shutdown() override {
        m_submitGate.reset();
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
    OverlayTextureSubmitGate m_submitGate;
    bool m_pictureFlipY=false;
    vr::IVRSystem *m_system = nullptr;
    vr::VROverlayHandle_t m_main = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t m_thumbnail = vr::k_ulOverlayHandleInvalid;
    bool m_initialized = false;
    bool m_actionsTried = false, m_actionsReady = false, m_actionsActive = false;
    vr::VRActionSetHandle_t m_actionSet = vr::k_ulInvalidActionSetHandle;
    vr::VRActionHandle_t m_back = vr::k_ulInvalidActionHandle;
};
}
std::unique_ptr<OverlayRuntime> makeOpenVrRuntime(bool pictureFlipY) { return std::make_unique<OpenVrRuntime>(pictureFlipY); }
bool refreshOverlayManifest(const QString &manifest, QString *error) {
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly)) { *error = "Overlay manifest is missing from the portable resources folder."; return false; }
    auto doc = QJsonDocument::fromJson(file.readAll());file.close();
    auto root = doc.object();auto applications = root.value("applications").toArray();
    if (applications.size() != 1 || applications.at(0).toObject().value("app_key").toString() != QLatin1String(overlayKey)) {
        *error = "Unexpected overlay manifest contents."; return false;
    }
    auto app = applications.at(0).toObject();const auto before = app;
    const auto program = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    app["binary_path_windows"] = program;app["binary_path_linux"] = program;
    app["working_directory"] = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    // Beside the manifest, so a relative path works: SteamVR reads the Hub's actions and default bindings from it.
    app["action_manifest_path"] = QStringLiteral("hub-actions.json");
    if (app == before) return true;
    applications[0] = app;root["applications"] = applications;
    QSaveFile out(manifest);
    if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(root).toJson()) < 0 || !out.commit()) { *error = "Overlay manifest could not be updated."; return false; }
    return true;
}
bool changeOverlayRegistration(const QString &manifest, bool add, QString *error) {
    if (add && !refreshOverlayManifest(manifest, error)) return false;
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly)) { *error = "Overlay manifest is missing from the portable resources folder."; return false; }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    const auto applications = doc.object().value("applications").toArray();
    if (applications.size() != 1 || applications.at(0).toObject().value("app_key").toString() != QLatin1String(overlayKey)) {
        *error = "Unexpected overlay manifest contents."; return false;
    }
    if (!probeOpenVrLibrary(error)) return false;
    // A background application never starts SteamVR: adding or removing the Hub needs SteamVR running already.
    vr::EVRInitError initError = vr::VRInitError_None;
    vr::VR_Init(&initError, vr::VRApplication_Background);
    if (initError == vr::VRInitError_Init_NoServerForBackgroundApp) { *error = "Start SteamVR first, then add the Hub to it."; return false; }
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
