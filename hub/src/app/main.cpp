// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "core/art/Art.h"
#include "core/catalog/CatalogLoader.h"
#include "core/catalog/CatalogPaths.h"
#include "core/scan/ScanController.h"
#include "models/FilterSortModel.h"
#include "models/GameListModel.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTextStream>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <toml++/toml.hpp>
namespace {
QString token(const toml::table &theme, const char *section, const char *key,
              const char *fallback) {
  return QString::fromStdString(
      theme["color"][section][key].value_or(std::string(fallback)));
}
} // namespace
int main(int argc, char **argv) {
  ac::LaunchOptions options;
  {
    // Qt reads the native Unicode command line on Windows. Keep headless
    // options free of a GUI application, then create the selected app below.
    QCoreApplication argumentApp(argc, argv);
    options = ac::parseLaunchOptions(argumentApp.arguments().mid(1));
  }
  QTextStream out(stdout), err(stderr);
  if (!options.error.isEmpty()) {
    err << options.error << '\n';
    return 2;
  }
  if (options.help) {
    out << "AladdinsCastle Hub\nUsage: aladdinscastle-hub [--overlay "
           "[--window] | --launch "
           "<game-id>]\n"
           "  --data-root <folder>   Folder containing games/ (portable data "
           "by default)\n"
           "  --quit-after-ms <ms>   Exit after a bounded desktop smoke run\n";
    return 0;
  }
  if (options.version) {
    out << "AladdinsCastle Hub 0.1.0\n";
    return 0;
  }
  if (options.mode != ac::Mode::Desktop) {
    QCoreApplication app(argc, argv);
    err << (options.mode == ac::Mode::Overlay
                ? "Overlay rendering is implemented in lane B."
                : "Game launch is implemented in lane G.")
        << '\n';
    return 3;
  }
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
  QGuiApplication app(argc, argv);
  QCoreApplication::setApplicationName("AladdinsCastle");
  QCoreApplication::setApplicationVersion("0.1.0");
  const auto root = ac::findCatalogRoot(app.applicationDirPath(),
                                        QDir::currentPath(), options.dataRoot);
  if (root.isEmpty()) {
    err << "Catalog missing: supply --data-root <folder containing games/>.\n";
    return 2;
  }
  auto catalog = QtConcurrent::run([root] {
                   return ac::CatalogLoader().load(root);
                 }).result();
  const auto count = catalog.report.records;
  ac::GameListModel gameModel(std::move(catalog));
  ac::scan::ScanOptions scanOptions;
  scanOptions.userRoot = QDir(root).filePath("user");
  ac::scan::ScanController scanner(&gameModel, scanOptions);
  auto art = std::make_shared<ac::art::Resolver>(
      gameModel.catalog(), ac::art::ArtOptions{scanOptions.userRoot, {}});
  ac::FilterSortModel filterModel;
  filterModel.setSourceModel(&gameModel);
  QFile themeFile(":/resources/theme.toml");
  if (!themeFile.open(QIODevice::ReadOnly)) {
    err << "Embedded theme missing.\n";
    return 2;
  }
  toml::table theme;
  try {
    theme = toml::parse(themeFile.readAll().toStdString());
  } catch (const toml::parse_error &error) {
    err << "Theme parse failed: " << error.what() << '\n';
    return 2;
  }
  QQmlApplicationEngine engine;
  engine.addImageProvider("art", new ac::art::Provider(art));
  engine.rootContext()->setContextProperty("scanController", &scanner);
  QObject::connect(&scanner, &ac::scan::ScanController::resultsReady, &app,
                   [&](const ac::scan::ScanResult &result) {
                     art->setBindings(result.bindings);
                     auto roots = scanner.options().artRoots +
                                  scanner.options().mediaRoots;
                     for (const auto &tool : result.tools)
                       roots << QFileInfo(tool.path).absolutePath();
                     art->setRoots(roots);
                     filterModel.setScanComplete(true);
                   });
  engine.rootContext()->setContextProperty("catalogGameCount", count);
  engine.rootContext()->setContextProperty("gameModel", &gameModel);
  engine.rootContext()->setContextProperty("gameFilter", &filterModel);
  engine.rootContext()->setContextProperty(
      "surfaceColor", token(theme, "surface", "window", "#0f0f12"));
  engine.rootContext()->setContextProperty(
      "primaryTextColor", token(theme, "text", "primary", "#ffffff"));
  engine.rootContext()->setContextProperty(
      "brandColor", token(theme, "brand", "orange", "#dd6600"));
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
  engine.loadFromModule("AladdinsCastle.Hub", "DesktopShell");
  if (engine.rootObjects().isEmpty())
    return 2;
  out << "AladdinsCastle: " << count << " games\n";
  out.flush();
  if (options.quitAfterMs > 0)
    QTimer::singleShot(options.quitAfterMs, &app, &QCoreApplication::quit);
  return app.exec();
}
