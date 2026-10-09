// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <nlohmann/json.hpp>
namespace ac {
using Json = nlohmann::ordered_json;
// Every declared media row has a requirement, even before its serial is known.
QString mediaRequirementId(const Json &media, const QString &gameId, qsizetype index);
enum class GameState {
    DataError,
    Installing,
    InstallFailed,
    Update,
    Installed,
    NeedsFiles,
    NeedsEmulator,
    ReadyToInstall,
    PlannedOnly,
    NoRecipe
};
enum class VrBadge { None, True3D, Theatre, Planned };
enum class MediaStatus { Missing, Partial, Found };
enum class ToolStatus { Ok, Missing, Older };
enum class JobStatus { None, Running, Failed };
enum class ArtSource { User, Pack, Generated };
struct Variant {
    QString id, title, quality, status, installedWhen;
    QStringList media, tools;
    Json raw = Json::object();
    bool generated = false;
};
// Worker-to-repository interface. verified installs require all three gates.
struct VariantRuntimeState {
    QString id;
    bool verified = false, installedWhenExists = false, manifestExists = false;
    bool updateAvailable = false;
    QString updateVersion;
};
struct RuntimeState {
    QString gameId;
    QStringList mediaFound, toolsOk, toolsOlder;
    QVector<VariantRuntimeState> variants;
    JobStatus jobStatus = JobStatus::None;
    QString jobProgress, selectedVariantId;
    qint64 lastPlayed = 0, firstSeen = 0;
    int recentIndex = -1;
    ArtSource artSource = ArtSource::Generated;
};
struct GameRecord {
    QString id, folder;
    Json raw = Json::object(), install = Json::object(), setup = Json::object();
    // JSON pointers to leaf values; arrays keyed by current numeric index.
    Json provenance = Json::object(), installProvenance = Json::object(),
         setupProvenance = Json::object();
    QVector<Variant> variants;
    QVariantMap roles;
    RuntimeState runtime;
    QStringList warnings, errors, validationErrors;
    bool hasRecipe = false;
};
void resolveState(GameRecord &game);
QString folded(const QString &value);
QVariant jsonVariant(const Json &value);
} // namespace ac
