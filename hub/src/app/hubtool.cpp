// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogLoader.h"
#include "core/catalog/CatalogPaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto args = app.arguments();
    args.removeFirst();
    QTextStream out(stdout), err(stderr);
    QString overrideRoot;
    const auto pos = args.indexOf("--data-root");
    if (pos >= 0) {
        if (pos + 1 >= args.size()) {
            err << "--data-root requires a folder.\n";
            return 2;
        }
        overrideRoot = args[pos + 1];
        args.removeAt(pos + 1);
        args.removeAt(pos);
    }
    const QString usage = "Usage: hubtool [--data-root <folder>] count | explain <game-id>\n";
    if (args == QStringList{"--help"}) {
        out << usage;
        return 0;
    }
    if (args != QStringList{"count"} && !(args.size() == 2 && args[0] == "explain")) {
        err << usage;
        return 2;
    }
    const auto root =
        ac::findCatalogRoot(app.applicationDirPath(), QDir::currentPath(), overrideRoot);
    if (root.isEmpty()) {
        err << "Catalog missing.\n";
        return 2;
    }
    const auto catalog = ac::CatalogLoader().load(root);
    if (args[0] == "count") {
        out << catalog.report.records << '\n';
        return 0;
    }
    const auto *game = catalog.find(args[1]);
    if (!game) {
        err << "Unknown game id.\n";
        return 2;
    }
    ac::Json variants = ac::Json::array();
    for (const auto &v : game->variants) {
        auto raw = v.raw;
        raw["id"] = v.id.toStdString();
        variants.push_back(raw);
    }
    const auto result = ac::Json{{"game", game->raw},
                                 {"install", game->install},
                                 {"setup", game->setup},
                                 {"variants", variants},
                                 {"provenance",
                                  {{"game", game->provenance},
                                   {"install", game->installProvenance},
                                   {"setup", game->setupProvenance}}},
                                 {"warnings", game->warnings.join("\n").toStdString()},
                                 {"loadError", game->errors.join("\n").toStdString()}};
    out << QString::fromStdString(result.dump(2)) << '\n';
    return 0;
}
