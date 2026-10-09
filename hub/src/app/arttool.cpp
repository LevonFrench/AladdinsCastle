// SPDX-License-Identifier: GPL-3.0-only
#include "core/art/Art.h"
#include "core/catalog/CatalogPaths.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  const auto args = app.arguments();
  if (args.size() != 3 || args[1] != "--request") {
    QTextStream(stderr) << "Usage: arttool --request <private JSON file>\n";
    return 2;
  }
  QFile f(args[2]);
  if (!f.open(QIODevice::ReadOnly))
    return 2;
  try {
    const auto request = ac::Json::parse(f.readAll().toStdString());
    const auto options = ac::scan::Scanner::optionsFromJson(request);
    const auto root =
        QString::fromStdString(request.at("dataRoot").get<std::string>());
    const auto output = QString::fromStdString(
        request.at("outputDirectory").get<std::string>());
    ac::art::Resolver resolver(ac::CatalogLoader().load(root),
                               {options.userRoot, options.artRoots});
    QVector<ac::scan::Binding> bindings;
    const auto bindingFile =
        QString::fromStdString(request.value("bindingsFile", std::string()));
    QFile b(bindingFile);
    if (b.open(QIODevice::ReadOnly)) {
      const auto json = ac::Json::parse(b.readAll().toStdString());
      for (const auto &v : json.at("bindings")) {
        ac::scan::Binding value;
        value.gameId = QString::fromStdString(v.value("gameId", std::string()));
        value.path = QString::fromStdString(v.value("path", std::string()));
        value.identity =
            QString::fromStdString(v.value("identity", std::string()));
        value.proof = QString::fromStdString(v.value("proof", std::string()));
        value.verified = v.value("verified", false);
        bindings << value;
      }
    }
    resolver.setBindings(bindings);
    QDir().mkpath(output);
    ac::Json receipt = ac::Json::array();
    const auto kind =
        QString::fromStdString(request.value("kind", std::string("banner")));
    for (const auto &game : request.at("gameIds")) {
      const auto id = QString::fromStdString(game.get<std::string>());
      if (!QRegularExpression("^[A-Za-z0-9_-]+$").match(id).hasMatch())
        return 2;
      const auto image = resolver.resolve(id, kind, {920, 430});
      const auto path = QDir(output).filePath(id + ".png");
      if (!image.image.save(path))
        return 3;
      receipt.push_back({{"gameId", id.toStdString()},
                         {"source", image.source.toStdString()},
                         {"sourcePath", image.path.toStdString()},
                         {"preview", path.toStdString()},
                         {"width", image.image.width()},
                         {"height", image.image.height()}});
    }
    QSaveFile out(QDir(output).filePath("receipt.json"));
    if (!out.open(QIODevice::WriteOnly))
      return 3;
    out.write(QByteArray::fromStdString(receipt.dump(2)));
    out.commit();
    return 0;
  } catch (const std::exception &e) {
    QTextStream(stderr) << e.what() << '\n';
    return 2;
  }
}
