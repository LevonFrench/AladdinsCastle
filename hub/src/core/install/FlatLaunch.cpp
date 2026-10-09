// SPDX-License-Identifier: GPL-3.0-only
#include "Support.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
namespace ac::install {
LaunchPlan makeFlatLaunchPlan(const GameRecord &game, const Json &emulator,
                              const QString &root, const Json &bindings) {
  const auto tool = string(emulator, "id");
  const auto located = bindings.value("tools", Json::object())
                           .value(tool.toStdString(), Json::object());
  LaunchPlan plan;
  plan.executable = string(located, "path");
  if (!QFileInfo(plan.executable).isFile())
    throw Error("E_TOOL_MISSING", "Locate the emulator first");
  scopedPath(plan.executable, QFileInfo(plan.executable).absolutePath());
  const auto profile =
      scopedPath("user/emulator-profiles/" + tool + "/" + game.id, root);
  plan.cwd = profile;
  plan.writableDirs << profile;
  QMap<QString, QString> vars{
      {"tool_dir", QFileInfo(plan.executable).absolutePath()},
      {"profile_dir", profile},
      {"logs", profile + "/logs"},
      {"game.id", game.id},
      {"display.width", "1280"},
      {"display.height", "720"}};
  Json chosen = Json::object();
  Json bios = Json::object();
  Json aliases = Json::object();
  QStringList rompaths;
  int index = 0;
  for (const auto &m : game.raw.value("media", Json::array())) {
    const auto key = mediaRequirementId(m, game.id, index++);
    const auto media = bindings.value("media", Json::object())
                           .value(key.toStdString(), Json::object());
    const auto path = string(media, "path");
    if (!QFileInfo(path).isFile() || !media.value("verified", false))
      throw Error("E_MEDIA_MISSING", "Required media is not verified");
    const auto dir = QFileInfo(path).absolutePath();
    if (string(m, "kind").contains("bios")) {
      vars["bios.dir"] = dir;
      bios = media;
      continue;
    }
    if (chosen.empty())
      chosen = Json{
          {"path", path.toStdString()},
          {"dir", dir.toStdString()},
          {"set",
           string(m, "set", QFileInfo(path).completeBaseName()).toStdString()}};
    if (!rompaths.contains(dir))
      rompaths << dir;
    for (const auto &support :
         media.value("supportRequirements", Json::array())) {
      const auto source = string(support, "sourcePath"),
                 set = string(support, "set");
      validateRelative(set);
      if (set.contains('/'))
        throw Error("E_PLAN_INVALID", "Invalid device set ID");
      const auto supportDir = QFileInfo(source).absolutePath();
      if (QFileInfo(source).isFile() && !rompaths.contains(supportDir))
        rompaths << supportDir;
      aliases[set.toStdString()].push_back(support);
    }
    for (const auto &support : media.value("supportPaths", Json::array())) {
      const auto supportPath =
          support.is_string()
              ? QString::fromStdString(support.get<std::string>())
              : string(support, "path");
      if (QFileInfo(supportPath).exists()) {
        const auto supportDir = QFileInfo(supportPath).absolutePath();
        if (!rompaths.contains(supportDir))
          rompaths << supportDir;
      }
    }
    vars["media." + key + ".path"] = path;
  }
  if (chosen.empty())
    throw Error("E_MEDIA_MISSING", "No game media binding");
  vars["rom.set"] = string(chosen, "set");
  vars["set"] = vars["rom.set"];
  vars["rom.zip"] = string(chosen, "path");
  vars["rom.basename"] = QFileInfo(string(chosen, "path")).completeBaseName();
  for (const auto *key : {"disc", "iso", "file", "eboot", "xbe_or_xiso"})
    vars["media." + QString(key)] = string(chosen, "path");
  vars["media.dir"] = rompaths.join(';');
  vars["rompath"] = vars["media.dir"];
  if (!vars.contains("bios.dir"))
    vars["bios.dir"] = vars["media.dir"];
  if (emulator.contains("launch")) {
    for (const auto &arg : emulator["launch"].value("args", Json::array()))
      plan.args << expand(QString::fromStdString(arg.get<std::string>()), vars);
    if (string(game.raw.value("controls", Json::object()), "type") == "gun")
      for (const auto &arg :
           emulator["launch"].value("gun_args", Json::array()))
        plan.args << expand(QString::fromStdString(arg.get<std::string>()),
                            vars);
    plan.cwd =
        expand(string(emulator["launch"], "cwd", "${profile_dir}"), vars, true);
  }
  if (plan.args.isEmpty())
    throw Error("E_PLAN_INVALID", "Manifest launch arguments missing");
  if (tool == "mame")
    for (const auto *folder : {"cfg", "nvram", "input", "sta", "snap", "diff",
                               "comments", "home", "ini"})
      plan.writableDirs << profile + "/" + folder;
  else if (tool == "pcsx2")
    for (const auto *folder : {"inis", "bios", "memcards", "sstates", "snaps",
                               "logs", "textures", "cache"})
      plan.writableDirs << profile + "/" + folder;
  else if (tool == "supermodel")
    for (const auto *folder : {"Config", "NVRAM", "Saves", "logs"})
      plan.writableDirs << profile + "/" + folder;
  plan.data = Json{{"game", game.id.toStdString()},
                   {"variant", tool.toStdString()},
                   {"quality", "flat"},
                   {"exe", plan.executable.toStdString()},
                   {"cwd", plan.cwd.toStdString()},
                   {"args", Json::array()},
                   {"writable_dirs", Json::array()}};
  for (const auto &arg : plan.args)
    plan.data["args"].push_back(arg.toStdString());
  for (const auto &dir : plan.writableDirs)
    plan.data["writable_dirs"].push_back(dir.toStdString());
  plan.data["initial_config"] = Json::array();
  plan.data["copy_resources"] = Json::array();
  plan.data["initial_files"] = Json::array();
  plan.data["media_aliases"] = aliases;
  if (tool == "pcsx2") {
    if (bios.empty())
      throw Error("E_MEDIA_MISSING",
                  "Choose a verified BIOS for the isolated PCSX2 profile");
    const auto dataRoot = profile + "/PCSX2";
    plan.writableDirs << dataRoot << dataRoot + "/inis";
    plan.data["initial_config"].push_back(Json{
        {"file", (dataRoot + "/inis/PCSX2.ini").toStdString()},
        {"format", "ini"},
        {"set",
         Json::array(
             {Json{{"section", "UI"}, {"key", "SettingsVersion"}, {"value", 1}},
              Json{{"section", "UI"},
                   {"key", "SetupWizardIncomplete"},
                   {"value", false}},
              Json{{"section", "UI"},
                   {"key", "StartFullscreen"},
                   {"value", false}},
              Json{{"section", "Folders"},
                   {"key", "Bios"},
                   {"value", QFileInfo(string(bios, "path"))
                                 .absolutePath()
                                 .toStdString()}},
              Json{{"section", "Filenames"},
                   {"key", "BIOS"},
                   {"value", QFileInfo(string(bios, "path"))
                                 .fileName()
                                 .toStdString()}}})}});
    const auto originalDir = QFileInfo(plan.executable).absolutePath();
    {
      const auto cloneDir = profile + "/application";
      const auto cloned =
          cloneDir + "/" + QFileInfo(plan.executable).fileName();
      plan.data["copy_resources"].push_back(
          Json{{"from", plan.executable.toStdString()},
               {"to", cloned.toStdString()},
               {"kind", "file"}});
      plan.data["copy_resources"].push_back(
          Json{{"from", originalDir.toStdString()},
               {"to", cloneDir.toStdString()},
               {"kind", "directory"},
               {"include", Json::array({"*.dll"})}});
      for (const auto *name : {"resources", "translations", "plugins",
                               "platforms", "styles", "imageformats", "tls"})
        if (QFileInfo(originalDir + "/" + name).isDir())
          plan.data["copy_resources"].push_back(
              Json{{"from", (originalDir + "/" + name).toStdString()},
                   {"to", (cloneDir + "/" + name).toStdString()},
                   {"kind", "directory"}});
      plan.executable = cloned;
      plan.data["exe"] = cloned.toStdString();
      plan.writableDirs << cloneDir;
      plan.data["initial_files"].push_back(
          Json{{"file", (cloneDir + "/portable.txt").toStdString()},
               {"text", (dataRoot + "\n").toStdString()},
               {"origin", "profile-routing"}});
      const auto datapath = plan.args.indexOf("-datapath");
      if (datapath >= 0) {
        plan.args.removeAt(datapath);
        if (datapath < plan.args.size())
          plan.args.removeAt(datapath);
      }
      if (!plan.args.contains("-portable"))
        plan.args.prepend("-portable");
    }
  }
  if (tool == "supermodel") {
    const auto originalDir = QFileInfo(plan.executable).absolutePath();
    if (QFileInfo(originalDir + "/Config/Music.xml").isFile())
      plan.data["copy_resources"].push_back(
          Json{{"from", (originalDir + "/Config/Music.xml").toStdString()},
               {"to", (profile + "/Config/Music.xml").toStdString()},
               {"kind", "file"}});
    if (QFileInfo(originalDir + "/Assets").isDir())
      plan.data["copy_resources"].push_back(
          Json{{"from", (originalDir + "/Assets").toStdString()},
               {"to", (profile + "/Assets").toStdString()},
               {"kind", "directory"}});
  }
  plan.data["writable_dirs"] = Json::array();
  plan.data["args"] = Json::array();
  for (const auto &arg : plan.args)
    plan.data["args"].push_back(arg.toStdString());
  for (const auto &dir : plan.writableDirs)
    plan.data["writable_dirs"].push_back(dir.toStdString());
  return plan;
}
Json prepareFlatLaunch(const LaunchPlan &plan, const QString &root,
                       bool materializeAliases) {
  const auto profileRoot = scopedPath("user/emulator-profiles", root);
  const auto manifest =
      scopedPath(string(plan.data, "variant") + "/" +
                     string(plan.data, "game") + "/preparation.manifest.toml",
                 profileRoot);
  auto receipt = readEnvelope(manifest);
  if (receipt.empty())
    receipt = Json{{"files", Json::array()}, {"directories", Json::array()}};
  const auto journal =
      QFileInfo(manifest).absolutePath() + "/preparation.journal.jsonl";
  QDir().mkpath(QFileInfo(journal).absolutePath());
  if (!QFileInfo::exists(journal))
    atomicWrite(journal, {});
  for (auto &row : receipt["files"]) {
    if (!row.value("pending", false))
      continue;
    const auto path = scopedPath(string(row, "path"), root);
    if (QFileInfo::exists(path)) {
      if (string(row, "origin") == "fresh-profile")
        throw Error("E_PREP_RECOVERY_REQUIRED",
                    "Interrupted profile creation needs review; existing INI "
                    "was not read");
      if (hashFile(path) != string(row, "sha256"))
        throw Error("E_FILE_CHANGED",
                    "Interrupted resource differs from planned bytes");
      row["pending"] = false;
      durableAppend(journal, Json{{"kind", "done"},
                                  {"path", string(row, "path").toStdString()},
                                  {"recovered", true}});
      writeEnvelope(manifest, receipt);
    }
  }
  auto writePrepared = [&](const QString &path, const QByteArray &bytes,
                           const QString &origin) {
    const auto safe = scopedPath(path, profileRoot),
               relative = QDir(root).relativeFilePath(safe);
    if (QFileInfo::exists(safe))
      return;
    durableAppend(journal, Json{{"kind", "intent"},
                                {"path", relative.toStdString()},
                                {"sha256", sha256(bytes).toStdString()},
                                {"origin", origin.toStdString()}});
    bool existing = false;
    for (auto &row : receipt["files"])
      if (string(row, "path") == relative) {
        row["sha256"] = sha256(bytes).toStdString();
        row["pending"] = true;
        existing = true;
      }
    if (!existing)
      receipt["files"].push_back(Json{{"path", relative.toStdString()},
                                      {"sha256", sha256(bytes).toStdString()},
                                      {"origin", origin.toStdString()},
                                      {"pending", true}});
    writeEnvelope(manifest, receipt);
    atomicWrite(safe, bytes);
    durableAppend(journal,
                  Json{{"kind", "done"}, {"path", relative.toStdString()}});
    for (auto &row : receipt["files"])
      if (string(row, "path") == relative)
        row["pending"] = false;
    writeEnvelope(manifest, receipt);
  };
  for (const auto &path : plan.writableDirs) {
    const auto safe = scopedPath(path, profileRoot);
    if (!QFileInfo::exists(safe)) {
      durableAppend(
          journal,
          Json{{"kind", "intent"},
               {"op", "mkdir"},
               {"path", QDir(root).relativeFilePath(safe).toStdString()}});
      if (!QDir().mkpath(safe))
        throw Error("E_WRITE_DENIED", "Cannot create isolated profile");
      receipt["directories"].push_back(
          QDir(root).relativeFilePath(safe).toStdString());
      durableAppend(
          journal,
          Json{{"kind", "done"},
               {"op", "mkdir"},
               {"path", QDir(root).relativeFilePath(safe).toStdString()}});
      writeEnvelope(manifest, receipt);
    }
  }
  auto copyOne = [&](const QString &source, const QString &destination) {
    scopedPath(source, QFileInfo(source).absolutePath());
    const auto safe = scopedPath(destination, profileRoot);
    const auto suffix = QFileInfo(source).suffix().toLower();
    if (!QSet<QString>{"exe", "dll",  "xml",  "bmp",  "png", "svg", "ttf",
                       "otf", "qm",   "json", "yaml", "zip", "txt", "metallib",
                       "bin", "dxil", "spv",  "glsl", "ini", "dat"}
             .contains(suffix))
      throw Error("E_SOURCE_OUT_OF_SCOPE",
                  "Unknown public application resource type");
    // No INI or account databases from a located tool, even under resource
    // directories.
    if (suffix == "ini" ||
        QFileInfo(source).fileName().contains("account", Qt::CaseInsensitive) ||
        QFileInfo(source).fileName().contains("token", Qt::CaseInsensitive) ||
        QFileInfo(source).fileName().contains("credential",
                                              Qt::CaseInsensitive))
      throw Error("E_SOURCE_OUT_OF_SCOPE", "Account/config source forbidden");
    if (QFileInfo::exists(safe)) {
      const auto relative = QDir(root).relativeFilePath(safe);
      const auto actual = hashFile(safe);
      bool owned = false;
      for (const auto &row : receipt["files"])
        if (string(row, "path") == relative && string(row, "sha256") == actual)
          owned = true;
      if (!owned || actual != hashFile(source))
        throw Error("E_FILE_CHANGED",
                    "Prepared public resource is edited or stale");
      return;
    }
    const auto bytes = readBytes(source);
    QDir().mkpath(QFileInfo(safe).absolutePath());
    writePrepared(safe, bytes, "public-emulator-resource");
  };
  for (const auto &item : plan.data.value("copy_resources", Json::array())) {
    const auto from = string(item, "from"), to = string(item, "to");
    scopedPath(from, QFileInfo(from).absolutePath());
    if (string(item, "kind") == "file")
      copyOne(from, to);
    else {
      QDirIterator it(
          from, item.contains("include") ? QStringList{"*.dll"} : QStringList{},
          QDir::Files | QDir::NoSymLinks,
          item.contains("include") ? QDirIterator::NoIteratorFlags
                                   : QDirIterator::Subdirectories);
      while (it.hasNext()) {
        const auto source = it.next(),
                   relative = QDir(from).relativeFilePath(source);
        copyOne(source, to + "/" + relative);
      }
    }
  }
  for (const auto &config : plan.data.value("initial_config", Json::array())) {
    const auto file = scopedPath(string(config, "file"), profileRoot);
    if (QFileInfo::exists(file))
      continue;
    const auto edit =
        editConfig({}, string(config, "format"), config["set"], {}, {});
    writePrepared(file, edit.bytes, "fresh-profile");
  }
  for (const auto &item : plan.data.value("initial_files", Json::array())) {
    const auto file = scopedPath(string(item, "file"), profileRoot);
    const auto bytes = string(item, "text").toUtf8();
    if (QFileInfo::exists(QFileInfo(file).absolutePath() + "/portable.ini"))
      throw Error("E_FILE_CHANGED",
                  "Unexpected portable.ini overrides routing");
    if (QFileInfo::exists(file)) {
      if (hashFile(file) != sha256(bytes))
        throw Error("E_FILE_CHANGED", "Profile routing marker was edited");
      continue;
    }
    writePrepared(file, bytes, string(item, "origin"));
  }
  if (materializeAliases) {
    const auto aliases = plan.data.value("media_aliases", Json::object());
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
      const auto destination = scopedPath(
          string(plan.data, "variant") + "/" + string(plan.data, "game") +
              "/roms/" + QString::fromStdString(it.key()) + ".zip",
          profileRoot);
      const auto relative = QDir(root).relativeFilePath(destination);
      if (QFileInfo::exists(destination)) {
        bool owned = false;
        for (const auto &row : receipt["files"])
          if (string(row, "path") == relative &&
              string(row, "sha256") == hashFile(destination))
            owned = true;
        if (!owned)
          throw Error("E_FILE_CHANGED",
                      "Existing media alias is unowned or edited");
        continue;
      }
      const auto staged = scopedPath(relative + ".pending", root);
      Archive::makeUserMediaAlias(it.value(), staged);
      writePrepared(destination, readBytes(staged), "user-media-alias");
      QFile::remove(staged);
    }
  }
  writeEnvelope(manifest, receipt);
  return receipt;
}
} // namespace ac::install
