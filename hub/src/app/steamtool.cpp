// SPDX-License-Identifier: GPL-3.0-only
#include "core/install/Support.h"
#include "core/steam/Steam.h"
#include <QGuiApplication>
#include <QTextStream>
int main(int argc, char **argv) {
  // Image generation requires Qt GUI fonts, but no window or Steam launch.
#ifndef Q_OS_WIN
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
  QGuiApplication app(argc, argv);
  QTextStream out(stdout), err(stderr);
  const auto args = app.arguments().mid(1);
  if (args.size() < 2 || !QStringList{"preview", "apply"}.contains(args[0]) ||
      (args[0] == "preview" && args.size() != 2) ||
      (args[0] == "apply" &&
       (args.size() != 4 || args[3] != "--owner-approved"))) {
    err << "Usage: steamtool preview <request.json> | apply <request.json> "
           "<approved-preview.json> --owner-approved\n";
    return 64;
  }
  try {
    const auto j =
        ac::Json::parse(ac::install::readBytes(args[1]).toStdString());
    using ac::install::string;
    ac::steam::WriteRequest r;
    r.steamRoot = string(j, "steamRoot");
    r.userRoot = string(j, "userRoot");
    r.accountId = string(j, "accountId");
    r.shortcut = {string(j, "gameId"),
                  string(j, "title"),
                  string(j, "executable"),
                  string(j, "startDir"),
                  j.value("vr", false),
                  j.value("storedAppId", quint32(0)),
                  j.value("lastPlayed", qint64(0)),
                  string(j, "variantId")};
    r.remove = j.value("remove", false);
    r.overwriteCustomArt = j.value("overwriteCustomArt", false);
    if (!r.remove) {
      r.art =
          ac::steam::fallbackArt(r.shortcut.title, string(j, "manufacturer"));
      for (const auto *role : {"header", "capsule", "hero", "logo"})
        if (j.contains("art") && j["art"].contains(role)) {
          QImage image(
              QString::fromStdString(j["art"][role].get<std::string>()));
          if (image.isNull())
            throw ac::install::Error(
                "E_ART", "Configured local art could not be decoded");
          r.art[role] = image;
        }
    }
    auto p = ac::steam::preview(r);
    if (args[0] == "preview") {
      out << QString::fromStdString(p.json.dump(2)) << '\n';
      return 0;
    }
    const auto approved =
        ac::Json::parse(ac::install::readBytes(args[2]).toStdString());
    if (approved != p.json)
      throw ac::install::Error("E_PREVIEW_CHANGED",
                               "Request differs from the approved preview");
    const auto result = ac::steam::apply(r, p, true);
    out << QString::fromStdString(
               ac::Json{{"appid", result.id},
                        {"changed", result.changed},
                        {"warnings", result.warnings.join('\n').toStdString()}}
                   .dump(2))
        << '\n';
    return 0;
  } catch (const std::exception &e) {
    err << e.what() << '\n';
    return 1;
  }
}
