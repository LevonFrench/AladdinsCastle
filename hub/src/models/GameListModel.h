// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/catalog/CatalogLoader.h"
#include <QAbstractListModel>
#include <QTimer>
namespace ac {
class GameListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count CONSTANT)
    Q_PROPERTY(QStringList warnings READ warnings CONSTANT)
  public:
    explicit GameListModel(CatalogData catalog, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return static_cast<int>(m_catalog.games.size()); }
    QStringList warnings() const { return m_catalog.report.messages; }
    const QVector<GameRecord> &records() const { return m_catalog.games; }
    const GameRecord *find(const QString &id) const { return m_catalog.find(id); }
    const CatalogData &catalog() const { return m_catalog; }
    int roleForName(const QByteArray &name) const;
    // Call on GUI thread. queueRuntimeStates coalesces worker values for >=80ms.
    void applyRuntimeStates(const QVector<RuntimeState> &states);
    void queueRuntimeStates(const QVector<RuntimeState> &states);

  private:
    CatalogData m_catalog;
    QHash<int, QByteArray> m_roles;
    QMap<QString, RuntimeState> m_pending;
    QTimer m_batchTimer;
};
} // namespace ac
