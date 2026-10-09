// SPDX-License-Identifier: GPL-3.0-only
#include "core/app/LaunchOptions.h"
#include "core/catalog/CatalogPaths.h"
#include "overlay/OverlayHost.h"
#include "overlay/SpikeState.h"
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
#include <toml++/toml.hpp>
namespace {
QString token(const toml::table &theme, const char *section, const char *key,
              const char *fallback) {
    return QString::fromStdString(theme["color"][section][key].value_or(std::string(fallback)));
}
}
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    ac::LaunchOptions options;
    {
        // Qt reads the native Unicode command line on Windows. Keep headless
        // options free of a GUI application, then create the selected app below.
        QCoreApplication argumentApp(argc, argv);
        options = ac::parseLaunchOptions(argumentApp.arguments().mid(1));
    }
    QTextStream out(stdout), err(stderr);
    if (!options.error.isEmpty()) { err << options.error << '\n'; return 2; }
    if (options.help) {
        out << "AladdinsCastle Hub\nUsage: aladdinscastle-hub [--overlay [--window] | --launch <game-id>]\n"
               "  --data-root <folder>   Folder containing games/ (portable data by default)\n"
               "  --quit-after-ms <ms>   Exit after a bounded diagnostic run\n"
               "  --spike                Synthetic S1-S3 scene on the desktop\n"
               "  --register-overlay     Explicitly register portable SteamVR manifest\n"
               "  --unregister-overlay   Remove that manifest; no auto-launch is enabled\n";
        return 0;
    }
    if (options.version) { out << "AladdinsCastle Hub 0.1.0\n"; return 0; }
    if (options.mode == ac::Mode::RegisterOverlay || options.mode == ac::Mode::UnregisterOverlay) {
        QCoreApplication app(argc, argv);
        QString error;
        if (!ac::changeOverlayRegistration(QDir(app.applicationDirPath()).filePath("resources/aladdinscastle.vrmanifest"),
                options.mode == ac::Mode::RegisterOverlay, &error)) {
            err << "Overlay manifest action failed: " << error << '\n'; return 3;
        }
        out << (options.mode == ac::Mode::RegisterOverlay ? "Overlay manifest registered. Auto-launch remains unchanged.\n"
                                                        : "Overlay manifest removed.\n");
        return 0;
    }
    if (options.mode == ac::Mode::Launch) {
        QCoreApplication app(argc, argv);
        err << "Game launch is implemented in lane G." << '\n';
        return 3;
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("AladdinsCastle");
    QCoreApplication::setApplicationVersion("0.1.0");
    const auto root = ac::findCatalogRoot(app.applicationDirPath(), QDir::currentPath(), options.dataRoot);
    if (root.isEmpty()) { err << "Catalog missing: supply --data-root <folder containing games/>.\n"; return 2; }
    const auto count = ac::countGames(root);
    QFile themeFile(":/resources/theme.toml");
    if (!themeFile.open(QIODevice::ReadOnly)) { err << "Embedded theme missing.\n"; return 2; }
    toml::table theme;
    try { theme = toml::parse(themeFile.readAll().toStdString()); }
    catch (const toml::parse_error &error) { err << "Theme parse failed: " << error.what() << '\n'; return 2; }
    ac::SpikeState spikeState;
    QQmlApplicationEngine engine;
    ac::OverlayHost overlayHost(spikeState);
    engine.rootContext()->setContextProperty("overlaySpikeEnabled", options.spike || options.mode == ac::Mode::Overlay);
    engine.rootContext()->setContextProperty("spikeState", &spikeState);
    engine.rootContext()->setContextProperty("overlayHost", &overlayHost);
    engine.rootContext()->setContextProperty("catalogGameCount", count);
    engine.rootContext()->setContextProperty("surfaceColor", token(theme, "surface", "window", "#0f0f12"));
    engine.rootContext()->setContextProperty("primaryTextColor", token(theme, "text", "primary", "#ffffff"));
    engine.rootContext()->setContextProperty("brandColor", token(theme, "brand", "orange", "#dd6600"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    if (options.mode == ac::Mode::Overlay) {
        app.setQuitOnLastWindowClosed(false);
        QObject::connect(&overlayHost, &ac::OverlayHost::quitRequested, &app, &QCoreApplication::quit);
        QObject::connect(&overlayHost, &ac::OverlayHost::failed, &app, [&err](const QString &error) {
            err << error << '\n'; err.flush(); QCoreApplication::exit(3);
        });
        QString error;
        if (!overlayHost.initialize(engine, QDir(app.applicationDirPath()).filePath("resources/overlay-thumbnail.png"), &error)) {
            err << error << '\n'; return 3;
        }
    }
    if (options.mode == ac::Mode::Desktop || options.window) {
        engine.loadFromModule("AladdinsCastle.Hub", "DesktopShell");
        if (engine.rootObjects().isEmpty()) return 2;
    }
    out << "AladdinsCastle: " << count << " games\n";
    out.flush();
    if (options.quitAfterMs > 0) QTimer::singleShot(options.quitAfterMs, &app, &QCoreApplication::quit);
    return app.exec();
}
