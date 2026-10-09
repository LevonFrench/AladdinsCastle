// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogLoader.h"
#include "core/catalog/CatalogPaths.h"
#include "core/install/Install.h"
#include "core/install/Support.h"
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
  QString installRoot, bindingsPath, recipePath, handover;
  auto take = [&](const QString &name, QString &value) {
    const auto p = args.indexOf(name);
    if (p < 0)
      return true;
    if (p + 1 >= args.size())
      return false;
    value = args[p + 1];
    args.removeAt(p + 1);
    args.removeAt(p);
    return true;
  };
  if (!take("--install-root", installRoot) ||
      !take("--bindings", bindingsPath) || !take("--recipe", recipePath) ||
      !take("--handover", handover)) {
    err << "Missing option value\n";
    return 64;
  }
  const bool consent = args.removeOne("--consent");
  const QString usage =
      "Usage: hubtool [--data-root <folder>] [--install-root <folder>] "
      "[--bindings <JSON>] [--recipe <TOML>] [--handover <path>] count | "
      "explain <game> | plan/install/repair/update/uninstall/recover <game> "
      "<variant> | plan-tool/install-tool/uninstall-tool <tool> | locate-tool "
      "<tool> <exe>\n";
  if (args == QStringList{"--help"}) {
    out << usage;
    return 0;
  }
  if (args.isEmpty()) {
    err << usage;
    return 2;
  }
  const auto root = ac::findCatalogRoot(app.applicationDirPath(),
                                        QDir::currentPath(), overrideRoot);
  if (root.isEmpty()) {
    err << "Catalog missing.\n";
    return 2;
  }
  const auto catalog = ac::CatalogLoader().load(root);
  if (installRoot.isEmpty())
    installRoot = root;
  installRoot = QDir(installRoot).absolutePath();
  if (args[0] != "count" && args[0] != "explain") {
    try {
      using namespace ac::install;
      Request request;
      request.root = installRoot;
      request.handover = handover;
      request.operation = args[0];
      if (args[0].endsWith("-tool")) {
        if (args.size() < 2 ||
            !catalog.emulators.contains(args[1].toStdString())) {
          err << usage;
          return 64;
        }
        const auto manifest = catalog.emulators[args[1].toStdString()];
        if (args[0] == "locate-tool") {
          if (args.size() != 3) {
            err << usage;
            return 64;
          }
          out << QString::fromStdString(
                     Engine::locateTool(installRoot, manifest, args[2]).dump(2))
              << '\n';
          return 0;
        }
        if (args[0] == "uninstall-tool") {
          request.gameId = "tool-" + args[1];
          request.variantId = "windows-x64";
        } else
          request = Engine::emulatorRequest(installRoot, manifest, consent);
      } else {
        if (args.size() != 3) {
          err << usage;
          return 64;
        }
        request.gameId = args[1];
        request.variantId = args[2];
        if (!recipePath.isEmpty())
          request.recipe = ac::CatalogLoader::parseToml(recipePath);
        else if (const auto *g = catalog.find(request.gameId))
          request.recipe = g->install;
      }
      request.catalogRoot = root;
      if (!bindingsPath.isEmpty())
        request.bindings =
            ac::Json::parse(readBytes(bindingsPath).toStdString());
      Options options;
      options.event = [&](const QVariantMap &value) {
        out << value.value("kind").toString() << "  "
            << value.value("text").toString() << '\n';
        out.flush();
      };
      Engine engine(options);
      if (args[0] == "plan" || args[0] == "plan-tool") {
        out << engine.plan(request).text;
        return 0;
      }
      if (args[0] == "uninstall-preview") {
        out << QString::fromStdString(engine.uninstallPreview(request).dump(2))
            << '\n';
        return 0;
      }
      Result result;
      if (args[0] == "recover")
        result = engine.recover(request);
      else if (args[0] == "uninstall" || args[0] == "uninstall-tool")
        result = engine.uninstall(request);
      else if (QStringList{"install", "install-tool", "repair", "update"}
                   .contains(args[0]))
        result = engine.install(request);
      else {
        err << usage;
        return 64;
      }
      out << result.state << ": " << result.message << '\n';
      return result.exitCode;
    } catch (const ac::install::Error &e) {
      err << e.code << ": " << e.what() << '\n';
      return 64;
    } catch (const std::exception &e) {
      err << e.what() << '\n';
      return 64;
    }
  }
  if (args[0] == "count") {
    if (args.size() != 1) { err << usage; return 64; }
    out << catalog.report.records << '\n';
    return 0;
  }
  if (args[0] != "explain" || args.size() != 2) {
    err << usage;
    return 64;
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
  const auto result =
      ac::Json{{"game", game->raw},
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
