// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QByteArray>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
namespace ac::pinball {
// A gamepad as Visual Pinball names it: {"uid": "SDLJoy_<GUID>_<n>", "name": ...,
// "elements": {"A": id, "LeftShoulder": id, "LeftTrigger": id, ...}} where each id is
// VPX's own element number (a button, 0x0100 + hat, or 0x0200 + axis).
// Asks the SDL library beside Visual Pinball, read only and with VPX's own defaults,
// so the names and numbers are the ones VPX itself will see. Empty when no gamepad
// is connected or the library cannot be used.
QVariantList probeGamepads(const QString &sdlLibrary);
// The layout written for each gamepad, for the Pinball page and the docs: {"control","action"}.
QVariantList gamepadLayout();
// The user's VPX settings with each gamepad's mappings added to the [Input] section.
// The user's own key, VR controller and other bindings stay; any earlier binding of
// the same gamepad is replaced. Every other line is kept as it is.
QByteArray gamepadSettings(const QByteArray &settings,const QVariantList &pads);
// Where VPX keeps the user's own settings: its preferences folder, else beside the program.
QString userSettingsFile(const QString &executable);
// For headset launches: a single tap of the left VR controller's Y no longer exits the table;
// Exit stays on Y with the left stick click held together. Holding Y in the Hub closes it gracefully.
QByteArray exitOnChord(const QByteArray &settings);
}
