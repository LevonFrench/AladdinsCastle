// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
namespace ac::dreamcast {
// Dreamcast gun games through the user's own build of the hotd2-vr Flycast fork.
// Only public titles and product numbers are listed; discs and the emulator are
// found through Hub settings and never named in the repository.
QVariantList games(); // id, title, year
QVariantList views(); // id, label, help
QString viewLabel(const QString &view);
// What one disc image is: {file, game, title, product, region, verified, profile}.
// "game" is empty when it is none of the listed games. Read-only.
// A disc whose own number is listed is that game whatever its file is called.
QVariantMap identify(const QString &file,std::atomic_bool &cancel);
// The disc images directly inside a folder, identified. Subfolders are not entered.
QVariantList discs(const QString &folder,std::atomic_bool &cancel);
// executable, cwd and args for one disc and view. Throws install::Error.
QVariantMap launchPlan(const QString &file,const QString &view,const QString &executable);
}
