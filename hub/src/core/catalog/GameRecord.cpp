// SPDX-License-Identifier: GPL-3.0-only
#include "GameRecord.h"
#include <QColor>
#include <QJsonDocument>
#include <QRegularExpression>
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
    if (!installedId.isEmpty())
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
            for (const auto &id : best->media)
                if (!runtime.mediaFound.contains(id))
                    base = GameState::NeedsFiles;
        }
    }
    int found = 0;
    if (best)
        for (const auto &id : best->media)
            if (runtime.mediaFound.contains(id))
                ++found;
    const auto mediaStatus = !best || best->media.isEmpty() || found == best->media.size()
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
    r["stateLabel"] = labels.at(static_cast<int>(display)) +
                      (display == GameState::Installing && !runtime.jobProgress.isEmpty()
                           ? " " + runtime.jobProgress
                           : "");
    r["stateColour"] =
        display == GameState::ReadyToInstall
            ? r.value("accent")
            : stateColours.value(colourKeys.at(static_cast<int>(display)), QColor("#8a93a6"));
    r["statePill"] = base == GameState::ReadyToInstall ? "toInstall"
                     : base == GameState::Update       ? "updates"
                     : base == GameState::Installed    ? "ready"
                     : base == GameState::NeedsFiles   ? "needsFiles"
                                                       : "";
    r["inLibrary"] = !runtime.mediaFound.isEmpty();
    r["mediaStatus"] = static_cast<int>(mediaStatus);
    r["toolStatus"] = static_cast<int>(tools);
    r["installedVariantId"] = installedId;
    r["updateAvailable"] = update;
    r["updateVersion"] = version;
    r["reinstallVisible"] = base == GameState::Installed || base == GameState::Update;
    r["jobStatus"] = static_cast<int>(runtime.jobStatus);
    r["jobProgress"] = runtime.jobProgress;
    r["lastPlayed"] = runtime.lastPlayed;
    r["firstSeen"] = runtime.firstSeen;
    r["recentIndex"] = runtime.recentIndex;
    r["artSource"] = static_cast<int>(runtime.artSource);
}
} // namespace ac
