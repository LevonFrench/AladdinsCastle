// SPDX-License-Identifier: GPL-3.0-only
#include "GameListModel.h"
#include <QThread>
namespace ac {
GameListModel::GameListModel(CatalogData catalog, QObject *parent)
    : QAbstractListModel(parent), m_catalog(std::move(catalog)) {
    const QList<QByteArray> names{"gameId",
                                  "title",
                                  "altTitles",
                                  "series",
                                  "developer",
                                  "developerShown",
                                  "blurb",
                                  "pill",
                                  "colourBase",
                                  "accent",
                                  "neonAccent",
                                  "loadWarning",
                                  "loadError",
                                  "genreId",
                                  "genreLabel",
                                  "genreColour",
                                  "subgenreIds",
                                  "subgenreLabels",
                                  "bucketMask",
                                  "year",
                                  "decade",
                                  "manufacturerId",
                                  "manufacturerLabel",
                                  "manufacturerShort",
                                  "hardwareId",
                                  "hardwareLabel",
                                  "hardwareFamily",
                                  "hardwareKind",
                                  "hardwareOrder",
                                  "graphicsId",
                                  "graphicsLabel",
                                  "players",
                                  "playersBucket",
                                  "controlsType",
                                  "controlsLabel",
                                  "vrBest",
                                  "vrBadge",
                                  "vrPlanned",
                                  "badges",
                                  "featuredEligible",
                                  "originalId",
                                  "hasRecipe",
                                  "variantCount",
                                  "quest",
                                  "roomscale",
                                  "seated",
                                  "state",
                                  "baseState",
                                  "stateLabel",
                                  "stateColour",
                                  "statePill",
                                  "inLibrary",
                                  "mediaStatus",
                                  "toolStatus",
                                  "installedVariantId",
                                  "updateAvailable",
                                  "updateVersion",
                                  "isWip",
                                  "reinstallVisible",
                                  "jobStatus",
                                  "jobProgress",
                                  "lastPlayed",
                                  "recentIndex",
                                  "firstSeen",
                                  "artBanner",
                                  "artTile",
                                  "artPortrait",
                                  "artSource",
                                  "searchIndex", "playing", "preferredVariantId"};
    int role = Qt::UserRole + 1;
    for (const auto &name : names)
        m_roles.insert(role++, name);
    qRegisterMetaType<QVector<RuntimeState>>();
    m_batchTimer.setSingleShot(true);
    m_batchTimer.setInterval(80);
    connect(&m_batchTimer, &QTimer::timeout, this, [this] {
        const auto values = m_pending.values();
        m_pending.clear();
        applyRuntimeStates(values);
    });
}
int GameListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : count();
}
QVariant GameListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return {};
    if (role == Qt::DisplayRole)
        return m_catalog.games[index.row()].roles.value("title");
    return m_catalog.games[index.row()].roles.value(QString::fromUtf8(m_roles.value(role)));
}
QHash<int, QByteArray> GameListModel::roleNames() const { return m_roles; }
int GameListModel::roleForName(const QByteArray &name) const { return m_roles.key(name, -1); }
void GameListModel::applyRuntimeStates(const QVector<RuntimeState> &states) {
    Q_ASSERT(QThread::currentThread() == thread());
    for (const auto &state : states)
        for (int row = 0; row < count(); ++row) {
            auto &game = m_catalog.games[row];
            if (game.id != state.gameId)
                continue;
            const auto before = game.roles;
            game.runtime = state;
            resolveState(game);
            QList<int> changed;
            for (auto it = m_roles.begin(); it != m_roles.end(); ++it)
                if (before.value(QString::fromUtf8(it.value())) !=
                    game.roles.value(QString::fromUtf8(it.value())))
                    changed << it.key();
            if (!changed.isEmpty())
                emit dataChanged(index(row), index(row), changed);
            break;
        }
}
void GameListModel::queueRuntimeStates(const QVector<RuntimeState> &states) {
    Q_ASSERT(QThread::currentThread() == thread());
    for (const auto &state : states)
        m_pending[state.gameId] = state;
    if (!m_batchTimer.isActive())
        m_batchTimer.start();
}
} // namespace ac
