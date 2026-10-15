// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QStringList>
namespace ac {
enum class Mode { Desktop, Overlay, Launch, RegisterOverlay, UnregisterOverlay, CompositorCapture };
struct LaunchOptions {
    Mode mode = Mode::Desktop;
    QString gameId;
    QString dataRoot;
    QString installRoot, bindingsFile, variantId;
    QString overlayCaptureDir;
    QString compositorCaptureDir,compositorDashboard="any";
    QString error;
    bool steamVrStarted = false;
    bool window = false;
    bool spike = false;
    bool showDashboard = false;
    bool desktopOnly = false; // --desktop: window only, never SteamVR's dashboard panel
    bool pictureFlipY = false;
    // SteamVR on Windows reports overlay pointer positions from the top-left of
    // an OpenGL overlay as shown, like Valve's own Qt sample; the header's
    // bottom-left convention is kept elsewhere until a headset there says otherwise.
#ifdef _WIN32
    bool pointerFlipY = false;
#else
    bool pointerFlipY = true;
#endif
    bool help = false;
    bool version = false;
    int quitAfterMs = 0;
};
// Arguments exclude argv[0]. Parsing never launches a process or changes state.
LaunchOptions parseLaunchOptions(const QStringList &arguments);
bool wantsParentConsole(const LaunchOptions &options);
bool shouldOpenDesktop(const LaunchOptions &options, bool overlayInitialized);
// A Hub started on the desktop is also SteamVR's dashboard panel whenever SteamVR runs, attached
// when SteamVR starts, whichever started first. It never opens the dashboard by itself. Not with
// --desktop, any other mode, or a window-less (offscreen) display.
bool attachesPanelWhenSteamVrRuns(const LaunchOptions &options, bool offscreenDisplay);
bool shouldQuitAfterLastWindow(bool launchBusy);
bool shouldQuitAfterLaunch(bool overlayInitialized, bool launchBusy, bool desktopVisible);
}
