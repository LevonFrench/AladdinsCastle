// SPDX-License-Identifier: GPL-3.0-only
#include "GameRecord.h"
#include <QColor>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
namespace ac {
QString mediaRequirementId(const Json &media, const QString &gameId, qsizetype index) {
    if (media.is_object()) {
        for (const char *key : {"set", "serial", "id"}) {
            const auto it = media.find(key);
            if (it != media.end() && it->is_string() && !it->get_ref<const std::string &>().empty())
                return QString::fromStdString(it->get<std::string>());
        }
    }
    return gameId + "-media-" + QString::number(index);
}
bool optionalMedia(const Json &media) {
    return media.is_object() && media.contains("optional") &&
           media["optional"].is_boolean() && media["optional"].get<bool>();
}
bool mediaAppliesToRoute(const Json &media, const QString &routeId, const QString &hardware) {
    if (!media.is_object() || !media.contains("kind") || !media["kind"].is_string()) return false;
    const auto kind = QString::fromStdString(media["kind"].get<std::string>());
    const bool arcade = QStringList{"mame", "supermodel", "model2emu", "lindbergh-loader"}.contains(routeId) ||
        (QStringList{"flycast", "demul"}.contains(routeId) && hardware != "sega-dreamcast");
    const bool pc = QStringList{"teknoparrot", "native", "demulshooter"}.contains(routeId) || hardware == "pc-windows";
    if (arcade)
        return kind == "mame-romset" || (kind == "bios" && media.contains("set")) ||
               (routeId == "lindbergh-loader" && kind == "other");
    if (pc)
        return kind == "pc-game" || kind == "pc" || kind == "other";
    return kind == "disc" || (kind == "bios" && !media.contains("set"));
}
QString folded(const QString &value) {
    auto text = value.normalized(QString::NormalizationForm_D).toCaseFolded();
    text.remove(QRegularExpression("[\\p{Mn}\\p{Mc}]"));
    return text;
}
QVariant jsonVariant(const Json &value) {
    const auto bytes = QByteArray::fromStdString(Json{{"value", value}}.dump());
    return QJsonDocument::fromJson(bytes).toVariant().toMap().value("value");
}
void resolveState(GameRecord &game) {
    auto &r = game.roles;
    const auto &runtime = game.runtime;
    QSet<QString> optionalIds, biosIds;
    qsizetype mediaIndex = 0;
    for (const auto &m : (game.raw.contains("media") && game.raw["media"].is_array()
                             ? game.raw["media"] : Json::array())) {
        const auto id = mediaRequirementId(m, game.id, mediaIndex++);
        if (optionalMedia(m)) optionalIds.insert(id);
        if (m.is_object() && m.contains("kind") && m["kind"] == "bios") biosIds.insert(id);
    }
    const auto requiredMedia = [&](const Variant &v) {
        auto ids = v.media;
        ids.removeIf([&](const QString &id) { return optionalIds.contains(id); });
        return ids;
    };
    GameState base = GameState::NoRecipe;
    const Variant *best = nullptr;
    QString installedId, version;
    bool update = false;
    for (const auto &v : game.variants) {
        for (const auto &state : runtime.variants) {
            if (state.id == v.id && state.verified && state.installedWhenExists &&
                state.manifestExists) {
                if (installedId.isEmpty()) {
                    installedId = v.id;
                    best = &v;
                }
                if (state.updateAvailable) {
                    update = true;
                    version = state.updateVersion;
                }
            }
        }
    }
    const auto requirementsReady = [&](const Variant &v) {
        if (v.generated && v.quality == "flat" && (requiredMedia(v).isEmpty() || v.tools.isEmpty()))
            return false;
        for (const auto &id : requiredMedia(v)) if (!runtime.mediaFound.contains(id)) return false;
        for (const auto &id : v.tools) if (!runtime.toolsOk.contains(id)) return false;
        return true;
    };
    const Variant *preferred = nullptr;
    for (const auto &v : game.variants)
        if (v.id == runtime.selectedVariantId && v.status != "planned") { preferred = &v; break; }
    if (!preferred)
        for (const auto &v : game.variants)
            if (v.generated && v.quality == "flat" && v.status != "planned" && requirementsReady(v)) { preferred = &v; break; }
    if (!preferred)
        for (const auto &v : game.variants)
            if (v.generated && v.quality == "flat" && v.status != "planned") { preferred = &v; break; }
    if (preferred) {
        best = preferred;
        installedId.clear(); update = false; version.clear();
        for (const auto &state : runtime.variants)
            if (state.id == best->id && state.verified && state.installedWhenExists && state.manifestExists) {
                installedId = state.id; update = state.updateAvailable; version = state.updateVersion;
            }
        if (!installedId.isEmpty()) base = update ? GameState::Update : GameState::Installed;
        else if (best->generated && best->quality == "flat" && requirementsReady(*best)) base = GameState::Installed;
        else {
            base = GameState::ReadyToInstall;
            for (const auto &id : best->tools) if (!runtime.toolsOk.contains(id)) base = GameState::NeedsEmulator;
            for (const auto &id : requiredMedia(*best)) if (!runtime.mediaFound.contains(id)) base = GameState::NeedsFiles;
        }
    }
    else if (!installedId.isEmpty())
        base = update ? GameState::Update : GameState::Installed;
    else if (game.hasRecipe || !game.variants.isEmpty()) {
        for (const auto &v : game.variants) {
            if (v.id == runtime.selectedVariantId && v.status != "planned") {
                best = &v;
                break;
            }
            if (v.status != "planned" &&
                (!best || (best->status != "stable" && v.status == "stable")))
                best = &v;
        }
        if (!best)
            base = GameState::PlannedOnly;
        else {
            base = GameState::ReadyToInstall;
            for (const auto &id : best->tools)
                if (!runtime.toolsOk.contains(id))
                    base = GameState::NeedsEmulator;
            for (const auto &id : requiredMedia(*best))
                if (!runtime.mediaFound.contains(id))
                    base = GameState::NeedsFiles;
        }
    }
    int found = 0;
    if (best)
        for (const auto &id : requiredMedia(*best))
            if (runtime.mediaFound.contains(id))
                ++found;
    const auto mediaStatus = !best || requiredMedia(*best).isEmpty() || found == requiredMedia(*best).size()
                                 ? MediaStatus::Found
                             : found > 0 ? MediaStatus::Partial
                                         : MediaStatus::Missing;
    ToolStatus tools = ToolStatus::Ok;
    if (best)
        for (const auto &id : best->tools) {
            if (runtime.toolsOlder.contains(id))
                tools = ToolStatus::Older;
            else if (!runtime.toolsOk.contains(id)) {
                tools = ToolStatus::Missing;
                break;
            }
        }
    if (!game.errors.isEmpty())
        base = GameState::DataError;
    auto display = base;
    if (base != GameState::DataError) {
        if (runtime.jobStatus == JobStatus::Running)
            display = GameState::Installing;
        else if (runtime.jobStatus == JobStatus::Failed)
            display = GameState::InstallFailed;
    }
    const QStringList labels{"Data error",  "Installing",    "Retry install",    "↓ Update",
                             "Ready",       "Find my files", "Install emulator", "Install",
                             "Coming soon", "No setup yet"};
    const QStringList colourKeys{"planned_neon", "installed_neon", "retry_line",     "update_neon",
                                 "ready_neon",   "needs_neon",     "needs_emulator", "",
                                 "planned_neon", "planned_neon"};
    const auto stateColours = r.value("_stateColours").toMap();
    r["baseState"] = static_cast<int>(base);
    r["state"] = static_cast<int>(display);
    r["playing"] = runtime.playing;
    const auto label=display==GameState::Installed
        ? (installedId.isEmpty()?QString("Flat launch ready"):QString("Setup installed"))
        : labels.at(static_cast<int>(display));
    r["stateLabel"] = runtime.playing ? QString("Playing") : label +
                      (display == GameState::Installing && !runtime.jobProgress.isEmpty()
                           ? " " + runtime.jobProgress
                           : "");
    // Evidence wording does not change admission, filter states or acceptance.
    QString reason;
    if(display==GameState::DataError)reason="Catalog or recipe data needs correction. Review its diagnostics before installing.";
    else if(display==GameState::Installing)reason="Installation is running; completion will refresh its recorded checks.";
    else if(display==GameState::InstallFailed)reason="An installation attempt needs attention. Review the recovery details before retrying.";
    else if(!installedId.isEmpty())reason="Owned files passed the recorded installation checks. Runtime and gameplay acceptance are separate.";
    else if(base==GameState::Installed && best && best->generated)
        reason="Required media and emulator were detected for this flat route. Launch preflight runs when you play.";
    else if(best && best->generated)reason="A catalog flat route is available. Locate its required media and emulator to prepare a launch.";
    else if(game.hasRecipe)reason="Authored recipe metadata is present. An owned installation still needs its recorded checks.";
    else reason="Catalog capabilities describe recorded routes. No owned installation has been established for this entry.";
    r["stateReason"]=reason;
    r["stateColour"] =
        display == GameState::ReadyToInstall
            ? r.value("accent")
            : stateColours.value(colourKeys.at(static_cast<int>(display)), QColor("#8a93a6"));
    r["statePill"] = base == GameState::ReadyToInstall ? "toInstall"
                     : base == GameState::Update       ? "updates"
                     : base == GameState::Installed    ? "ready"
                     : base == GameState::NeedsFiles   ? "needsFiles"
                                                       : "";
    r["inLibrary"] = std::any_of(runtime.mediaFound.cbegin(), runtime.mediaFound.cend(),
                                  [&](const QString &id) { return !biosIds.contains(id); });
    r["mediaStatus"] = static_cast<int>(mediaStatus);
    r["toolStatus"] = static_cast<int>(tools);
    r["installedVariantId"] = installedId;
    r["preferredVariantId"] = best ? best->id : QString();
    r["updateAvailable"] = update;
    r["updateVersion"] = version;
    r["reinstallVisible"] = best && !best->generated && (base == GameState::Installed || base == GameState::Update);
    r["jobStatus"] = static_cast<int>(runtime.jobStatus);
    r["jobProgress"] = runtime.jobProgress;
    r["lastPlayed"] = runtime.lastPlayed;
    r["firstSeen"] = runtime.firstSeen;
    r["recentIndex"] = runtime.recentIndex;
    r["artSource"] = static_cast<int>(runtime.artSource);
}
} // namespace ac
