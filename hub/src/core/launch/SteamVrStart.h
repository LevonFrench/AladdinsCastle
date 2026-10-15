// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
namespace ac::launch {
// SteamVR's own starter, found through OpenVR's paths file (openvrpaths.vrpath); empty when unknown.
QString steamVrStarter(const QString &pathsFile = {});
// Starts SteamVR, only ever because the user pressed Play in headset: its own starter when it is
// found, otherwise through Steam. False, with the reason, when neither could be asked.
bool startSteamVr(QString *error);
} // namespace ac::launch
