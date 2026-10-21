// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QVariantList>
#include <QVariantMap>
namespace ac::system22 {
// Time Crisis through the user's own native VR port (system22Executable), in the views the old numbered
// launchers offered: "vr" (in the headset), "frame" (with the arcade screen's outline), "inbetween" (in-between
// frames, a test), "own-gun" (the build with our own pistol, when it is there) and "monitor" (flat, the mouse
// is the gun). Each {id, label, help, controls, monitor}; "own-gun" only when that build is found beside it.
QVariantList views(const QString &executable);
QString viewLabel(const QString &view);
// The build with our own pistol: own-gun85/build/pc beside the port's port/build/pc folder.
QString ownGunExecutable(const QString &executable);
// executable, cwd, args and env for one view. Throws install::Error.
QVariantMap launchPlan(const QString &executable, const QString &view);
} // namespace ac::system22
