// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <optional>
#include <functional>

namespace ac {
struct ControlsResolveOptions {
    // Caller registry order; only these confined packs are read.
    QStringList packIds;
    bool useUserOverrides = true;
    QString userOverrideDirectory = "user/overrides";
    // Profile defaults are supplied by the native profile service, not inferred
    // from a new persistence format. Game/pack/user-specific keys win.
    QVariantMap profileDefaults;
    QString profileModel;
    std::optional<QVariantMap> backend;
    // Only a successful runtime asset decoder supplies these names. No GLB I/O.
    QMap<QString,QSet<QString>> decodedModelNodes;
    // Preferred production seam: names for this exact layered model metadata,
    // obtained from the lead's successful asset decoder/cache.
    std::function<std::optional<QSet<QString>>(const QVariantMap &)> decodedNodesForModel;
};

class ControlsResolver final {
public:
    // Reads configuration TOMLs only; no Python, renderer, XR or input emission.
    bool loadLibrary(const QString &root,QString &error);
    bool resolveGame(const QString &id,const ControlsResolveOptions &options,QVariantMap &out,QString &error) const;
    static bool mergeLayers(const QVariant &base,const QVariant &later,QVariant &out,QString &error);
    static bool readConfinedToml(const QString &root,const QString &relative,QVariantMap &out,QString &error,bool optional=false);
private:
    QString m_root;
    QVariantMap m_games,m_models,m_sets,m_gunDefaults,m_defaults,m_hardware;
};
}
