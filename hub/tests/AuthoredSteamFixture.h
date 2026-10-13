// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/catalog/CatalogLoader.h"
#include "core/install/Support.h"
#include <QDir>
#include <QTemporaryDir>
namespace ac::test {
// Owned synthetic bytes only. No executable or runtime is started.
struct AuthoredSteamFixture {
  QTemporaryDir temp;
  CatalogData catalog;
  Json bindings;
  QString executable, disc, hubExecutable, stateBase;
  AuthoredSteamFixture() {
    const auto root=temp.path();
    install::atomicWrite(root+"/data/content-guard.toml",install::readBytes(QString(AC_CATALOG_ROOT)+"/data/content-guard.toml"));
    catalog=CatalogLoader().load(AC_CATALOG_ROOT);
    auto game=*catalog.find("wii-resident-evil-umbrella-chronicles");
    game.id="wii-synthetic";game.folder=root+"/games/wii-synthetic";
    game.errors.clear();game.validationErrors.clear();game.runtime={};game.runtime.gameId=game.id;
    game.raw["id"]=game.id.toStdString();game.raw["hardware"]="nintendo-wii";
    game.raw["media"]=Json::array({{{"kind","disc"},{"id","disc"}}});
    game.roles["gameId"]=game.id;game.roles["id"]=game.id;
    game.roles["title"]="Synthetic Wii game";
    const auto dir=root+"/installed/wii-synthetic/dolphinxr";
    executable=dir+"/Dolphin.exe";disc=root+"/synthetic.iso";hubExecutable=root+"/hub.exe";
    install::atomicWrite(executable,"synthetic authored executable; never run");
    install::atomicWrite(disc,"synthetic disc; no game data");
    QDir().mkpath(dir+"/User");
    const auto hash=install::hashFile(executable);
    Variant authored;authored.id="dolphinxr";authored.title="DolphinXR (experimental VR)";
    authored.status="wip";authored.quality="true3d";authored.media={"disc"};
    authored.raw={{"status","wip"},{"quality","true3d"},{"version","v0.4"},
      {"needs",{{"media",Json::array({"disc"})},{"tools",Json::array()}}},
      {"installed_when","file:${install_dir}/Dolphin.exe"},
      {"step",Json::array({{{"id","disc"},{"do","require-media"},{"media","disc"},{"verify","none"}}})},
      {"launch",{{"exe","${install_dir}/Dolphin.exe"},{"exe_sha256",hash.toStdString()},
        {"args",Json::array({"--user","${install_dir}/User","--exec","${media.disc.path}"})},
        {"cwd","${install_dir}"},{"runtime","steamvr"}}}};
    Variant flat;flat.id="flat-dolphin";flat.title="Play in ordinary Dolphin";
    flat.status="stable";flat.quality="flat";flat.generated=true;flat.media={"disc"};flat.tools={"dolphin"};
    game.variants={flat,authored};game.install={{"format",1},{"variant",{{"dolphinxr",authored.raw}}}};game.hasRecipe=true;
    game.runtime.mediaFound={"disc"};game.runtime.toolsOk={"dolphin"};game.runtime.lastPlayed=12345;
    VariantRuntimeState flatRuntime;flatRuntime.id=flat.id;
    flatRuntime.verified=true;flatRuntime.installedWhenExists=true;flatRuntime.manifestExists=true;
    auto authoredRuntime=flatRuntime;authoredRuntime.id=authored.id;
    game.runtime.variants={flatRuntime,authoredRuntime};
    resolveState(game);catalog.games={game};
    catalog.emulators["dolphin"]={{"id","dolphin"},{"launch",{{"args",Json::array({"--exec","${media.iso}"})},{"cwd","${tool_dir}"}}}};
    const auto ordinary=root+"/ordinary/Dolphin.exe";
    install::atomicWrite(ordinary,"synthetic flat executable; never run");
    bindings={{"media",{{"disc",{{"path",disc.toStdString()},{"verified",true}}}}},
      {"tools",{{"dolphin",{{"path",ordinary.toStdString()},{"verified",true}}},
                {"unrelated",{{"path",(root+"/absent-unrelated.exe").toStdString()},{"verified",false}}}}}};
    install::Request r;r.root=root;r.gameId=game.id;r.variantId=authored.id;
    stateBase=install::stateBase(r);
    const auto recipeHash=install::sha256(QByteArray::fromStdString(game.install.dump()));
    install::writeEnvelope(stateBase+".toml",{{"game",game.id.toStdString()},{"variant",authored.id.toStdString()},
      {"state","installed"},{"installed_version","v0.4"},{"recipe_sha256",recipeHash.toStdString()}});
    install::writeEnvelope(stateBase+".manifest.toml",{{"game",game.id.toStdString()},{"variant",authored.id.toStdString()},
      {"install_dir","installed/wii-synthetic/dolphinxr"},{"installed_when","file:${install_dir}/Dolphin.exe"},
      {"recipe_sha256",recipeHash.toStdString()},
      {"file",Json::array({{{"path","Dolphin.exe"},{"sha256",hash.toStdString()}}})}});
  }
};
}
