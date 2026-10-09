// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "core/catalog/CatalogLoader.h"
#include "core/catalog/CatalogPaths.h"
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
#include "ui/Theme.h"
#include "ui/UiController.h"
#include "ui/UiSettings.h"
#include <QQuickStyle>
#include <qqml.h>
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
        out << "AladdinsCastle Hub\nUsage: aladdinscastle-hub [--overlay [--window] | --launch "
               "<game-id>]\n"
               "  --data-root <folder>   Folder containing games/ (portable data by default)\n"
               "  --quit-after-ms <ms>   Exit after a bounded desktop smoke run\n";
        return 0;
    }
    if (options.version) {
        out << "AladdinsCastle Hub 0.1.0\n";
        return 0;
    }
    if (options.mode != ac::Mode::Desktop) {
        QCoreApplication app(argc, argv);
        err << (options.mode == ac::Mode::Overlay ? "Overlay rendering is implemented in lane B."
                                                  : "Game launch is implemented in lane G.")
            << '\n';
        return 3;
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("AladdinsCastle");
    QCoreApplication::setApplicationVersion("0.1.0");
    const auto root =
        ac::findCatalogRoot(app.applicationDirPath(), QDir::currentPath(), options.dataRoot);
    if (root.isEmpty()) {
        err << "Catalog missing: supply --data-root <folder containing games/>.\n";
        return 2;
    }
    auto catalog = QtConcurrent::run([root] { return ac::CatalogLoader().load(root); }).result();
    const auto count = catalog.report.records;
    ac::GameListModel gameModel(std::move(catalog));
    ac::FilterSortModel filterModel;
    filterModel.setSourceModel(&gameModel);
    QQuickStyle::setStyle("Basic");
    ac::Theme theme(gameModel.catalog().theme);
    ac::UiSettings settings(QDir(app.applicationDirPath()).filePath("user"));
    ac::UiController controller(&gameModel, &filterModel, &settings);
    qmlRegisterSingletonInstance("AladdinsCastle.Hub", 1, 0, "Theme", &theme);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("catalogGameCount", count);
    engine.rootContext()->setContextProperty("gameModel", &gameModel);
    engine.rootContext()->setContextProperty("gameFilter", &filterModel);
    engine.rootContext()->setContextProperty("uiSettings", &settings);
    engine.rootContext()->setContextProperty("uiController", &controller);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    engine.load(QUrl("qrc:/qt/qml/AladdinsCastle/Hub/DesktopShell.qml"));
    if (engine.rootObjects().isEmpty())
        return 2;
    out << "AladdinsCastle: " << count << " games\n";
    out.flush();
    if (options.quitAfterMs > 0)
        QTimer::singleShot(options.quitAfterMs, &app, &QCoreApplication::quit);
    return app.exec();
}
