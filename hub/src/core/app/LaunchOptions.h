// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QStringList>
namespace ac {
enum class Mode { Desktop, Overlay, Launch, RegisterOverlay, UnregisterOverlay };
struct LaunchOptions {
    Mode mode = Mode::Desktop;
    QString gameId;
    QString dataRoot;
    QString installRoot, bindingsFile, variantId;
    QString error;
    bool steamVrStarted = false;
    bool window = false;
    bool spike = false;
    bool help = false;
    bool version = false;
    int quitAfterMs = 0;
};
// Arguments exclude argv[0]. Parsing never launches a process or changes state.
LaunchOptions parseLaunchOptions(const QStringList &arguments);
bool wantsParentConsole(const LaunchOptions &options);
bool shouldOpenDesktop(const LaunchOptions &options, bool overlayInitialized);
bool shouldQuitAfterLastWindow(bool launchBusy);
bool shouldQuitAfterLaunch(bool overlayInitialized, bool launchBusy, bool desktopVisible);
}
