// SPDX-License-Identifier: GPL-3.0-only
#include "CatalogPaths.h"
#include <QDir>
#include <QFileInfo>
namespace ac {
namespace {
QString catalogAt(const QString &root) {
    const QDir dir(root);
    if (QFileInfo(dir.filePath("games")).isDir()) return dir.absolutePath();
    return {};
}
QString searchParents(const QString &start) {
    QDir dir(start);
    if (!dir.exists()) return {};
    do {
        const auto found = catalogAt(dir.absolutePath());
        if (!found.isEmpty()) return found;
    } while (dir.cdUp());
    return {};
}
}
QString findCatalogRoot(const QString &executableDir, const QString &workingDir,
                        const QString &overrideRoot) {
    if (!overrideRoot.isEmpty()) return catalogAt(overrideRoot);
    auto root = searchParents(executableDir);
    if (root.isEmpty()) root = searchParents(workingDir);
    return root;
}
qsizetype countGames(const QString &catalogRoot) {
    if (catalogRoot.isEmpty()) return 0;
    const QDir games(QDir(catalogRoot).filePath("games"));
    qsizetype count = 0;
    for (const auto &entry : games.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (QFileInfo(QDir(entry.absoluteFilePath()).filePath("game.toml")).isFile()) ++count;
    return count;
}
QString portableUserRoot(const QString &executableDir) {
    return QDir(executableDir).absoluteFilePath("user");
}
}
