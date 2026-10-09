// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogPaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto args = app.arguments(); args.removeFirst();
    QTextStream out(stdout), err(stderr);
    QString overrideRoot;
    const auto pos = args.indexOf("--data-root");
    if (pos >= 0) {
        if (pos + 1 >= args.size()) { err << "--data-root requires a folder.\n"; return 2; }
        overrideRoot = args.at(pos + 1); args.removeAt(pos + 1); args.removeAt(pos);
    }
    if (args == QStringList{"--help"}) { out << "Usage: hubtool [--data-root <folder>] count\n"; return 0; }
    if (args != QStringList{"count"}) { err << "Usage: hubtool [--data-root <folder>] count\n"; return 2; }
    const auto root = ac::findCatalogRoot(app.applicationDirPath(), QDir::currentPath(), overrideRoot);
    if (root.isEmpty()) { err << "Catalog missing.\n"; return 2; }
    out << ac::countGames(root) << '\n'; return 0;
}
