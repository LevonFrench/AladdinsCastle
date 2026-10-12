// SPDX-License-Identifier: GPL-3.0-only
#include "LaunchOptions.h"
#include <QRegularExpression>
#include <QDir>
namespace ac {
bool wantsParentConsole(const LaunchOptions &options){
    return options.help||options.version||options.quitAfterMs>0||!options.overlayCaptureDir.isEmpty()||options.mode==Mode::Launch||options.mode==Mode::RegisterOverlay||options.mode==Mode::UnregisterOverlay;
}
bool shouldOpenDesktop(const LaunchOptions &options, bool overlayInitialized) {
    if (options.mode == Mode::Overlay && options.steamVrStarted && !overlayInitialized) return false;
    return options.mode == Mode::Desktop || options.window || (options.mode == Mode::Overlay && !overlayInitialized);
}
bool shouldQuitAfterLastWindow(bool launchBusy) { return !launchBusy; }
bool shouldQuitAfterLaunch(bool overlayInitialized,bool launchBusy,bool desktopVisible){return !overlayInitialized&&!launchBusy&&!desktopVisible;}
LaunchOptions parseLaunchOptions(const QStringList &arguments) {
    LaunchOptions options;
    bool modeSeen = false;
    bool orientationSeen = false;
    for (qsizetype i = 0; i < arguments.size(); ++i) {
        const auto &arg = arguments.at(i);
        if (arg == "--help" || arg == "-h") options.help = true;
        else if (arg == "--version") options.version = true;
        else if (arg == "--steamvr-started") options.steamVrStarted = true;
        else if (arg == "--window") options.window = true;
        else if (arg == "--spike") options.spike = true;
        else if(arg=="--overlay-capture-dir"){
            if(!options.overlayCaptureDir.isEmpty()){options.error="Only one source texture capture directory may be requested.";break;}
            if(++i>=arguments.size()||!QDir::isAbsolutePath(arguments.at(i))){options.error="--overlay-capture-dir requires an absolute fresh directory under the selected catalog's .local.";break;}
            options.overlayCaptureDir=arguments.at(i);
        }
        else if(arg=="--overlay-picture-flip-y"||arg=="--overlay-pointer-flip-y"){
            orientationSeen=true;
            if(++i>=arguments.size()||!QStringList{"on","off"}.contains(arguments.at(i))){options.error=arg+" requires on or off.";break;}
            if(arg=="--overlay-picture-flip-y")options.pictureFlipY=arguments.at(i)=="on";
            else options.pointerFlipY=arguments.at(i)=="on";
        }
        else if (arg == "--overlay" || arg == "--launch" || arg == "--register-overlay" || arg == "--unregister-overlay") {
            if (modeSeen) { options.error = "Choose one execution mode (--overlay, --launch, --register-overlay, --unregister-overlay)."; break; }
            modeSeen = true;
            options.mode = arg == "--overlay" ? Mode::Overlay : arg == "--launch" ? Mode::Launch
                        : arg == "--register-overlay" ? Mode::RegisterOverlay : Mode::UnregisterOverlay;
            if (options.mode == Mode::Launch) {
                if (++i >= arguments.size()) { options.error = "--launch requires a game ID."; break; }
                options.gameId = arguments.at(i);
                const QRegularExpression id("^[a-z0-9]+(?:-[a-z0-9]+)*$");
                if (!id.match(options.gameId).hasMatch()) { options.error = "Invalid game ID."; break; }
            }
        } else if (arg == "--data-root" || arg == "--quit-after-ms" || arg == "--install-root" || arg == "--bindings" || arg == "--variant") {
            if (++i >= arguments.size()) { options.error = arg + " requires a value."; break; }
            if (arg == "--data-root") options.dataRoot = arguments.at(i);
            else if (arg == "--install-root") options.installRoot = arguments.at(i);
            else if (arg == "--bindings") options.bindingsFile = arguments.at(i);
            else if (arg == "--variant") options.variantId = arguments.at(i);
            else {
                bool ok = false;
                options.quitAfterMs = arguments.at(i).toInt(&ok);
                if (!ok || options.quitAfterMs <= 0) { options.error = "--quit-after-ms requires a positive integer."; break; }
            }
        } else { options.error = "Unknown argument: " + arg; break; }
    }
    if(options.error.isEmpty()&&orientationSeen&&options.mode!=Mode::Overlay)
        options.error="Overlay orientation options require --overlay.";
    if(options.error.isEmpty()&&!options.overlayCaptureDir.isEmpty()&&options.mode!=Mode::Overlay)
        options.error="Source texture capture requires --overlay.";
    if (options.error.isEmpty() && options.steamVrStarted && options.mode != Mode::Overlay)
        options.error = "--steamvr-started requires --overlay.";
    if (options.error.isEmpty() && options.window && options.mode != Mode::Overlay)
        options.error = "--window requires --overlay.";
    if (options.error.isEmpty() && options.spike && options.mode != Mode::Desktop && options.mode != Mode::Overlay)
        options.error = "--spike requires desktop or overlay mode.";
    if(options.error.isEmpty()&&!options.variantId.isEmpty()&&!QRegularExpression("^[a-z0-9]+(?:-[a-z0-9]+)*$").match(options.variantId).hasMatch())options.error="Invalid variant ID.";
    return options;
}
}
