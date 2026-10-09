// SPDX-License-Identifier: GPL-3.0-only
#include "LaunchOptions.h"
#include <QRegularExpression>
namespace ac {
bool wantsParentConsole(const LaunchOptions &options){
    return options.help||options.version||options.quitAfterMs>0||options.mode==Mode::Launch||options.mode==Mode::RegisterOverlay||options.mode==Mode::UnregisterOverlay;
}
bool shouldOpenDesktop(const LaunchOptions &options, bool overlayInitialized) {
    return options.mode == Mode::Desktop || options.window || (options.mode == Mode::Overlay && !overlayInitialized);
}
LaunchOptions parseLaunchOptions(const QStringList &arguments) {
    LaunchOptions options;
    bool modeSeen = false;
    for (qsizetype i = 0; i < arguments.size(); ++i) {
        const auto &arg = arguments.at(i);
        if (arg == "--help" || arg == "-h") options.help = true;
        else if (arg == "--version") options.version = true;
        else if (arg == "--window") options.window = true;
        else if (arg == "--spike") options.spike = true;
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
    if (options.error.isEmpty() && options.window && options.mode != Mode::Overlay)
        options.error = "--window requires --overlay.";
    if (options.error.isEmpty() && options.spike && options.mode != Mode::Desktop && options.mode != Mode::Overlay)
        options.error = "--spike requires desktop or overlay mode.";
    if(options.error.isEmpty()&&!options.variantId.isEmpty()&&!QRegularExpression("^[a-z0-9]+(?:-[a-z0-9]+)*$").match(options.variantId).hasMatch())options.error="Invalid variant ID.";
    return options;
}
}
