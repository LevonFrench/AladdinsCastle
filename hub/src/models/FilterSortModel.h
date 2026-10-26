// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "GameListModel.h"
#include "core/match/TitleMatch.h"
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
    // How a game answers the search words: -1 not at all, 0 a title is what was typed, 1 a title
    // starts with it, 2 every word is found as typed, 3 found only with a slip or a closed-up space.
    int queryClass(int row, const QVariantMap &roles) const;
    int relevance(int row) const;
    struct RowSearch {bool ready=false;match::SearchKeys names,series;qint8 rank=-2;};
    RowSearch &rowSearch(int row, const QVariantMap &roles) const;
    void forgetSearch();
    void forgetRanks();
    void buildSortKeys(QAbstractItemModel *model);
    struct SortKeys {QString title,manufacturer,id;};
    QVector<SortKeys> m_sortKeys;
    mutable QMap<QString, QVariantList> m_choicesCache;
    mutable QVector<RowSearch> m_rowSearch;
    QString m_query, m_sort = "title";
    QVariantMap m_facets;
    bool m_scanComplete = false;
};
using GameFilterModel = FilterSortModel;
} // namespace ac
