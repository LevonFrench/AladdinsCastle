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
// For headset launches: the universal layer on VPX's VR controller bindings. Left stick click is Start, right is Coin,
// A is the plunger button, both clicks held is VPX's "Align VR view using controllers" (scale, turn and move the table),
// the menu buttons and the stick clicks are bound to nothing else, and Y no longer exits. Exit stays on the keyboard; the Hub's own
// hold of both stick clicks for 3 s closes the table. Made again from its own output, nothing changes.
QByteArray headsetControls(const QByteArray &settings);
// The same call under its earlier name.
QByteArray exitOnChord(const QByteArray &settings);
}
