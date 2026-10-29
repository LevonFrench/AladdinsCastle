// SPDX-License-Identifier: GPL-3.0-only
#include "core/catalog/CatalogLoader.h"
#include "core/catalog/CatalogPaths.h"
#include "core/scan/Scan.h"
#include "core/install/ErrorText.h"
#include "core/install/Install.h"
#include "core/install/Support.h"
#include "core/launch/Launch.h"
#include "core/pinball/RomList.h"
#include "core/pinball/Pinball.h"
#include "core/library/Library.h"
#include "core/model2/Model2.h"
#include "core/model3/Model3.h"
#include "core/n64/N64.h"
#include "core/ps1/Ps1.h"
#include "core/ps2/Ps2.h"
#include "core/saturn/Saturn.h"
#include "core/scripted/Scripted.h"
#include "core/system22/System22.h"
#include <QFile>
#include <QJsonArray>
#include <functional>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <algorithm>
namespace {
// What the install engine says an E_* code means to a person, one indented line after the technical text. Nothing for a
// code the engine does not know, and the technical text and the code are always printed as before.
void plainLine(QTextStream &stream, const QString &code, const QString &plain) {
  if (!code.isEmpty() && !plain.isEmpty())
    stream << "  plain: " << plain << "  [" << code << "]\n";
}
QString plainFor(const QString &code) { return ac::install::hasPlainError(code) ? ac::install::plainErrorText(code) : QString(); }
// The most unrecognised files and duplicate groups the library summary names; the counts are always whole.
constexpr int kLibraryListed = 20;
} // namespace
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
  QString installRoot, bindingsPath, recipePath, handover, settingsPath;
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
      !take("--handover", handover) || !take("--settings", settingsPath)) {
    err << "Missing option value\n";
    return 64;
  }
  const bool consent = args.removeOne("--consent");
  const QString usage =
      "Usage: hubtool [--data-root <folder>] [--install-root <folder>] "
      "[--bindings <JSON>] [--settings <JSON>] [--recipe <TOML>] [--handover <path>] count | "
      "explain <game> | plan-launch <game> <variant> | plan/install/repair/update/uninstall/recover <game> "
      "<variant> | plan-tool/install-tool/uninstall-tool <tool> | locate-tool "
      "<tool> <exe> | identify-disc <file> | dreamcast-product <file> | scan-pinball <folder> [<database> [<roms> [<media> [<pinmame>]]]] | pinmame-roms <pinmame-folder> <set>... | plan-pinball <table> <exe> <owned-session> <vr|desktop> | "
      "plan-views --settings <JSON>\n";
  if (args == QStringList{"--help"}) {
    out << usage;
    return 0;
  }
  if (args.isEmpty()) {
    err << usage;
    return 2;
  }
  // The Game library over folders, read-only: counts per system and the time it took, nothing written.
  // Also the files not recognised (the first 20, each with its plain reason) and the games found more than once.
  if(args[0]=="library" && args.size()>=2) {
    std::atomic_bool cancel=false;
    const auto result=ac::library::scan(args.mid(1),{},cancel);
    const auto unread=result.value("unrecognised").toMap();
    const auto unreadFiles=unread.value("files").toList();
    const int shown=std::min<int>(int(unreadFiles.size()),kLibraryListed);
    const int unreadTotal=unread.contains("total")?unread.value("total").toInt():int(unreadFiles.size());
    QVariantMap unrecognised=unread;
    unrecognised.insert("files",unreadFiles.mid(0,shown));
    unrecognised.insert("total",unreadTotal);
    unrecognised.insert("shown",shown);
    unrecognised.insert("moreNotShown",unreadTotal-shown);
    if(unreadTotal>shown)unrecognised.insert("note",QString("and %1 more").arg(unreadTotal-shown));
    QJsonObject summary{{"files",result.value("files").toInt()},{"seconds",result.value("seconds").toDouble()},{"depth",result.value("depth").toInt()},
                        {"unrecognised",QJsonObject::fromVariantMap(unrecognised)}};
    QJsonObject systems;
    const auto all=result.value("systems").toMap();
    for(auto it=all.cbegin();it!=all.cend();++it){
      const auto system=it.value().toMap();QJsonObject games;
      const auto copies=system.value("copies").toMap();
      for(auto game=copies.cbegin();game!=copies.cend();++game)games.insert(game.key(),game.value().toList().size());
      systems.insert(it.key(),QJsonObject{{"games",system.value("games").toInt()},{"notInList",system.value("other").toInt()},{"copies",games},{"duplicateGames",system.value("duplicateGames").toInt()}});
    }
    summary.insert("systems",systems);
    const auto groups=result.value("duplicates").toList();
    const int groupsShown=std::min<int>(int(groups.size()),kLibraryListed);
    summary.insert("duplicates",QJsonArray::fromVariantList(groups.mid(0,groupsShown)));
    summary.insert("duplicateGroups",int(groups.size()));
    summary.insert("duplicatesMoreNotShown",int(groups.size())-groupsShown);
    if(int(groups.size())>groupsShown)summary.insert("duplicatesNote",QString("and %1 more").arg(int(groups.size())-groupsShown));
    summary.insert("duplicateRule",ac::library::duplicateRule());
    const auto folders=result.value("pinballFolders").toList();
    summary.insert("pinballTables",folders.isEmpty()?0:folders.first().toMap().value("tables").toInt());
    out<<QJsonDocument(summary).toJson();return 0;
  }
  // PinMAME's file lists for ROM sets, from the VPinMAME registered from <pinmame-folder> (Windows). The scan
  // runs this in a hidden child process so VPinMAME never loads into the Hub.
  if(args[0]=="pinmame-roms" && args.size()>=3) {
    QStringList unknown;QString error;const auto sets=ac::pinball::vpinmameRomSets(args.mid(2),args[1],unknown,error);
    if(!error.isEmpty()){err<<error<<"\n";return 2;}
    out<<ac::pinball::expectedSetsToJson(sets,unknown)<<"\n";return 0;
  }
  if(args[0]=="scan-pinball" && args.size()>=2 && args.size()<=6) {
    try{out<<QJsonDocument::fromVariant(ac::pinball::scan(args[1],args.value(2),args.value(3),args.value(4),args.value(5))).toJson();return 0;}catch(const std::exception &e){err<<e.what()<<"\n";return 2;}
  }
  if(args[0]=="identify-disc"&&args.size()==2){std::atomic_bool cancel=false;const auto id=ac::scan::Scanner::inspect(args[1],cancel);const bool valid=id.kind=="disc-id"&&!id.identity.isEmpty()&&id.error.isEmpty();out<<QJsonDocument::fromVariant(QVariantMap{{"kind",id.kind},{"identity",id.identity},{"error",id.error},{"valid",valid}}).toJson();return valid?0:2;}
  if(args[0]=="dreamcast-product"&&args.size()==2){std::atomic_bool cancel=false;const auto product=ac::scan::Scanner::dreamcastProduct(args[1],cancel);out<<product<<'\n';return product.isEmpty()?2:0;}
  // Every game and view of the systems the settings set up, each with the launch plan the Hub would start
  // (read-only: nothing is started or written). Settings: the Hub's own keys (ps1Launcher, model3Executable ...).
  if(args==QStringList{"plan-views"}) {
    QFile file(settingsPath);
    if(settingsPath.isEmpty()||!file.open(QIODevice::ReadOnly)){err<<"plan-views needs --settings <JSON>.\n";return 2;}
    const auto settings=QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    const auto get=[&](const char *key){return settings.value(QString::fromLatin1(key)).toString();};
    QJsonArray rows;
    const auto add=[&](const QString &system,const QString &game,const QString &title,const QVariantList &views,const std::function<QVariantMap(const QString &)> &plan){
      for(const auto &value:views){
        const auto view=value.toMap();QVariantMap row{{"system",system},{"game",game},{"title",title},{"view",view.value("id")},{"label",view.value("label")},
          {"controls",view.value("controls")},{"monitor",view.value("monitor").toBool()}};
        try{row["plan"]=plan(view.value("id").toString());}catch(const ac::install::Error &e){row["error"]=QString(e.code)+": "+QString::fromUtf8(e.what());if(const auto plain=plainFor(e.code);!plain.isEmpty())row["plain"]=plain;}
        rows.append(QJsonObject::fromVariantMap(row));
      }
    };
    const QStringList others{get("ps1Launcher"),get("n64Launcher"),get("saturnLauncher"),get("ps2Launcher")};
    if(!get("ps1Launcher").isEmpty())for(const auto &value:ac::ps1::games()){const auto g=value.toMap();const auto id=g.value("id").toString();
      add("ps1",id,g.value("title").toString(),ac::ps1::views(id,get("ps1Launcher")),[&](const QString &v){return ac::ps1::launchPlan(id,get("ps1Launcher"),v);});}
    if(!get("n64Launcher").isEmpty())for(const auto &value:ac::n64::games()){const auto g=value.toMap();const auto id=g.value("id").toString();
      add("n64",id,g.value("title").toString(),ac::n64::views(id,get("n64Launcher")),[&](const QString &v){return ac::n64::launchPlan(id,get("n64Launcher"),v);});}
    const auto saturnLauncher=ac::scripted::locate({"saturn","SATURN","saturnLauncher","saturn","launch-saturn.ps1"},get("saturnLauncher"),others);
    if(!saturnLauncher.isEmpty())for(const auto &value:ac::saturn::games()){const auto g=value.toMap();const auto id=g.value("id").toString();
      add("saturn",id,g.value("title").toString(),{QVariantMap{{"id","theatre"},{"label","Theatre screen"},{"controls",g.value("controls")}}},[&](const QString &){return ac::saturn::launchPlan(id,saturnLauncher);});}
    if(!get("ps2Launcher").isEmpty())for(const auto &value:ac::ps2::games()){const auto g=value.toMap();const auto id=g.value("id").toString();
      add("ps2",id,g.value("title").toString(),ac::ps2::views(id,get("ps2Launcher")),[&](const QString &v){return ac::ps2::launchPlan(id,get("ps2Launcher"),get("ps2Python"),get("ps2StopFile"),v);});}
    if(!get("model2Executable").isEmpty())for(const auto &value:ac::model2::games()){const auto g=value.toMap();const auto id=g.value("id").toString();
      add("model2",id,g.value("title").toString(),ac::model2::views(id),[&](const QString &v){return ac::model2::launchPlan(id,get("model2Executable"),get("model2GameFolder"),v);});}
    if(!get("model3Executable").isEmpty())for(const auto &value:ac::model3::games()){const auto g=value.toMap();const auto id=g.value("id").toString();
      add("model3",id,g.value("title").toString(),ac::model3::views(id),[&](const QString &v){return ac::model3::launchPlan(id,v,get("model3Executable"),get("model3GameFolder"));});}
    if(!get("system22Executable").isEmpty())
      add("system22","timecris","Time Crisis",ac::system22::views(get("system22Executable")),[&](const QString &v){return ac::system22::launchPlan(get("system22Executable"),v);});
    for(const auto &system:ac::scripted::systems()){
      const auto launcher=ac::scripted::locate(system,settings.value(system.settingKey).toString(),others);if(launcher.isEmpty())continue;
      for(const auto &value:ac::scripted::games(launcher)){const auto g=value.toMap();const auto key=g.value("id").toString();
        const auto shelf=system.id=="dreamcast"&&g.value("group")=="atomiswave"?QString("atomiswave"):system.id;
        add(shelf,key,g.value("title").toString(),g.value("views").toList(),[&](const QString &v){return ac::scripted::launchPlan(launcher,key,v);});}
    }
    out<<QJsonDocument(rows).toJson();return 0;
  }
  if(args[0]=="plan-pinball" && args.size()==5 && QStringList{"vr","desktop"}.contains(args[4])) {
    try{out<<QJsonDocument::fromVariant(ac::pinball::launchPlan(args[1],args[2],args[3],args[4]=="vr")).toJson();return 0;}catch(const std::exception &e){err<<e.what()<<"\n";return 2;}
  }
  const auto root = ac::findCatalogRoot(app.applicationDirPath(),
                                        QDir::currentPath(), overrideRoot);
  if (root.isEmpty()) {
    err << "Catalog missing.\n";
    return 2;
  }
  const auto catalog = ac::CatalogLoader().load(root);
  const bool scanMode = args.size() == 3 && (args[0] == "scan" || args[0] == "locate") && args[1] == "--request";
  if (scanMode) {
    QFile request(args[2]);
    if (!request.open(QIODevice::ReadOnly)) {
      err << "Cannot read private scan request.\n";
      return 2;
    }
    try {
      auto options = ac::scan::Scanner::optionsFromJson(
          ac::Json::parse(request.readAll().toStdString()));
      options.locateOnly = args[0] == "locate";
      if (options.userRoot.isEmpty())
        options.userRoot = QDir(root).filePath("user");
      std::atomic_bool cancel = false;
      const auto result = ac::scan::Scanner::run(catalog, options, cancel);
      out << QString::fromStdString(result.toJson().dump(2)) << '\n';
      return result.cancelled ? 4 : 0;
    } catch (const std::exception &e) {
      err << "Invalid scan request: " << e.what() << '\n';
      return 2;
    }
  }

  if (installRoot.isEmpty())
    installRoot = root;
  installRoot = QDir(installRoot).absolutePath();
  if (args.size()==3 && args[0]=="plan-launch") {
    try {
      const auto bindings=ac::Json::parse(ac::install::readBytes(bindingsPath).toStdString());
      const auto request=ac::launch::gameRequest(catalog,args[1],installRoot,bindings,args[2]);
      ac::Json receipt{{"exe",request.plan.executable.toStdString()},{"cwd",request.plan.cwd.toStdString()},
        {"runtime",request.runtime.toStdString()},{"args",ac::Json::array()}};
      for(const auto &arg:request.plan.args)receipt["args"].push_back(arg.toStdString());
      out<<QString::fromStdString(receipt.dump(2))<<'\n';return 0;
    } catch(const ac::install::Error &e){err<<e.code<<": "<<e.what()<<'\n';plainLine(err,e.code,plainFor(e.code));return 64;}
      catch(const std::exception &e){err<<e.what()<<'\n';return 64;}
  }
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
      if (!settingsPath.isEmpty()) {
        request.settings = ac::Json::parse(readBytes(settingsPath).toStdString());
        if (!request.settings.is_object()) throw Error("E_PLAN_INVALID", "Settings must be a JSON object");
      }
      Options options;
      options.event = [&](const QVariantMap &value) {
        out << value.value("kind").toString() << "  "
            << value.value("text").toString() << '\n';
        const auto code = value.value("code").toString();
        if (!code.isEmpty())
          plainLine(out, code, value.value("plain").toString().isEmpty() ? plainFor(code) : value.value("plain").toString());
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
      plainLine(out, result.code, result.plain());
      return result.exitCode;
    } catch (const ac::install::Error &e) {
      err << e.code << ": " << e.what() << '\n';
      plainLine(err, e.code, plainFor(e.code));
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
