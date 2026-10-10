// SPDX-License-Identifier: GPL-3.0-only
#include "LaunchOptions.h"
#include <QRegularExpression>
namespace ac {
LaunchOptions parseLaunchOptions(const QStringList &arguments) {
    LaunchOptions options;
    bool modeSeen = false;
    for (qsizetype i = 0; i < arguments.size(); ++i) {
        const auto &arg = arguments.at(i);
        if (arg == "--help" || arg == "-h") options.help = true;
        else if (arg == "--version") options.version = true;
        else if (arg == "--window") options.window = true;
        else if (arg == "--overlay" || arg == "--launch") {
            if (modeSeen) { options.error = "Choose only one of --overlay and --launch."; break; }
            modeSeen = true;
            options.mode = arg == "--overlay" ? Mode::Overlay : Mode::Launch;
            if (options.mode == Mode::Launch) {
                if (++i >= arguments.size()) { options.error = "--launch requires a game ID."; break; }
                options.gameId = arguments.at(i);
                const QRegularExpression id("^[a-z0-9]+(?:-[a-z0-9]+)*$");
                if (!id.match(options.gameId).hasMatch()) { options.error = "Invalid game ID."; break; }
            }
        } else if (arg == "--data-root" || arg == "--quit-after-ms") {
            if (++i >= arguments.size()) { options.error = arg + " requires a value."; break; }
            if (arg == "--data-root") options.dataRoot = arguments.at(i);
            else {
                bool ok = false;
                options.quitAfterMs = arguments.at(i).toInt(&ok);
                if (!ok || options.quitAfterMs <= 0) { options.error = "--quit-after-ms requires a positive integer."; break; }
            }
        } else { options.error = "Unknown argument: " + arg; break; }
    }
    if (options.error.isEmpty() && options.window && options.mode != Mode::Overlay)
        options.error = "--window requires --overlay.";
    return options;
}
}
