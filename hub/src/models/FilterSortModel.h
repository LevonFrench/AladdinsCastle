// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "GameListModel.h"
#include <QSortFilterProxyModel>
namespace ac {
class FilterSortModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY visibleCountChanged)
    Q_PROPERTY(bool scanComplete READ scanComplete WRITE setScanComplete NOTIFY facetsChanged)
  public:
    explicit FilterSortModel(QObject *parent = nullptr);
    void setSourceModel(QAbstractItemModel *model) override;
    QString query() const { return m_query; }
    QString sortMode() const { return m_sort; }
    int visibleCount() const { return rowCount(); }
    bool scanComplete() const { return m_scanComplete; }
    void setQuery(const QString &query);
    void setSortMode(const QString &mode);
    void setScanComplete(bool value);
    Q_INVOKABLE void setFacet(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant facet(const QString &name) const { return m_facets.value(name); }
    Q_INVOKABLE void clearFacets();
    Q_INVOKABLE QVariantList choices(const QString &facetName) const;
  signals:
    void filterWarning(const QString &message);
    void queryChanged();
    void sortModeChanged();
    void visibleCountChanged();
    void facetsChanged();

  protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

  private:
    bool acceptsRow(int row, const QVariantMap &facets) const;
    void buildSortKeys(QAbstractItemModel *model);
    struct SortKeys {QString title,manufacturer,id;};
    QVector<SortKeys> m_sortKeys;
    mutable QMap<QString, QVariantList> m_choicesCache;
    QString m_query, m_sort = "title";
    QVariantMap m_facets;
    bool m_scanComplete = false;
};
using GameFilterModel = FilterSortModel;
} // namespace ac
