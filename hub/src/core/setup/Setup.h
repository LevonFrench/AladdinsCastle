// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
namespace ac::setup {
// The systems the Hub can start today, for the first-run set-up. Each names the
// Hub setting that holds the user's game folder and the one that holds the
// user's emulator or game program; no location is ever part of the repository.
QVariantList systems(); // id, kind (arcade|pinball|consoles), title, detail, folderKey, programKey, programLabel
// folderKey is empty for a system whose program keeps its game files beside itself.
// A folder that probably holds this system's files, found beside or under the
// folders the user already chose. Read-only; empty when nothing fits.
QString guessFolder(const QString &system,const QStringList &knownFolders);
// Whether SteamVR's own application list (its appconfig.json) names this manifest.
// Read-only; false when the file is missing or unreadable.
bool steamVrLists(const QString &appConfig,const QString &manifest);
}
