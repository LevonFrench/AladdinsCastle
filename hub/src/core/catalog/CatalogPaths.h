// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
namespace ac {
QString findCatalogRoot(const QString &executableDir, const QString &workingDir,
                        const QString &overrideRoot = {});
qsizetype countGames(const QString &catalogRoot);
QString portableUserRoot(const QString &executableDir);
}
