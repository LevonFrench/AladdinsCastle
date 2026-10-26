// SPDX-License-Identifier: GPL-3.0-only
#include "FilterSortModel.h"
#include <QDateTime>
#include <QRegularExpression>
#include <algorithm>
namespace ac {
namespace {
QStringList selected(const QVariant &v) {
    if (v.metaType().id() == QMetaType::QString)
        return v.toString().isEmpty() || v.toString() == "all" ? QStringList{}
                                                               : QStringList{v.toString()};
    return v.toStringList();
}
bool matches(const QStringList &values, const QString &value, bool known = true) {
    return values.isEmpty() || !known || value.isEmpty() || values.contains(value);
}
} // namespace
FilterSortModel::FilterSortModel(QObject *parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::modelReset, this, &FilterSortModel::visibleCountChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &FilterSortModel::visibleCountChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &FilterSortModel::visibleCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &FilterSortModel::visibleCountChanged);
}
void FilterSortModel::buildSortKeys(QAbstractItemModel *model){
    m_sortKeys.clear();forgetSearch();if(const auto *source=qobject_cast<GameListModel *>(model))for(const auto &game:source->records())m_sortKeys<<SortKeys{folded(game.roles.value("title").toString()),folded(game.roles.value("manufacturerLabel").toString()),folded(game.id)};
}
void FilterSortModel::forgetSearch() { m_rowSearch.clear(); }
void FilterSortModel::forgetRanks() {
    for (auto &s : m_rowSearch)
        s.rank = -2;
}
void FilterSortModel::setSourceModel(QAbstractItemModel *model) {
    buildSortKeys(model);
    m_choicesCache.clear();
    QSortFilterProxyModel::setSourceModel(model);
    // Search words are prepared per game on first use; any change of rows starts them again.
    connect(model, &QAbstractItemModel::modelAboutToBeReset, this, [this] { forgetSearch(); });
    connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, [this] { forgetSearch(); });
    connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, [this] { forgetSearch(); });
    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this] { forgetSearch(); });
    connect(model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &,const QModelIndex &,const QList<int> &roles){
        m_choicesCache.clear();const auto *source=qobject_cast<GameListModel *>(sourceModel());
        if(source&&(roles.isEmpty()||roles.contains(source->roleForName("title"))||roles.contains(source->roleForName("manufacturerLabel"))||roles.contains(source->roleForName("gameId")))){buildSortKeys(sourceModel());invalidate();sort(0);}
    });
    connect(model, &QAbstractItemModel::modelReset, this, [this]{ m_choicesCache.clear(); buildSortKeys(sourceModel()); });
    sort(0);
}
void FilterSortModel::setQuery(const QString &query) {
    if (m_query == query)
        return;
    m_query = query;
    m_choicesCache.clear();
    forgetRanks();
    // In A to Z order the ranks change with the words, so rows that stay visible are sorted again too.
    if (m_sort == "title")
        invalidate();
    else
        invalidateFilter();
    emit queryChanged();
    emit visibleCountChanged();
}
void FilterSortModel::setSortMode(const QString &mode) {
    if (m_sort == mode)
        return;
    m_sort = mode;
    if(const auto *source=qobject_cast<GameListModel *>(sourceModel()))setSortRole(source->roleForName(mode=="recent"?"lastPlayed":mode=="added"?"firstSeen":"title"));
    invalidate();
    sort(0);
    emit sortModeChanged();
}
void FilterSortModel::setScanComplete(bool value) {
    if (m_scanComplete == value)
        return;
    m_scanComplete = value;
    m_choicesCache.clear();
    invalidateFilter();
    emit facetsChanged();
    emit visibleCountChanged();
}
void FilterSortModel::setFacet(const QString &name, const QVariant &value) {
    QVariant normalized;
    if (name == "inLibraryOnly") {
        if (value.metaType().id() != QMetaType::Bool) {
            emit filterWarning("Ignored invalid boolean facet");
            return;
        }
        normalized = value;
    } else if (name == "yearMin" || name == "yearMax") {
        if (!value.isValid() || value.isNull())
            normalized = QVariant();
        else if ((value.metaType().id() == QMetaType::Int ||
                  value.metaType().id() == QMetaType::LongLong ||
                  value.metaType().id() == QMetaType::Double) &&
                 value.toDouble() == value.toInt() && value.toInt() >= 1970 &&
                 value.toInt() <= 2026)
            normalized = value.toInt();
        else {
            emit filterWarning("Ignored invalid year facet");
            return;
        }
    } else {
        QStringList allowed;
        for (const auto &choice : choices(name))
            allowed << choice.toMap().value("id").toString();
        if (name == "hardwareIds")
            for (const auto &key : {"hardwareKinds", "hardwareFamilies"})
                for (const auto &choice : choices(key))
                    allowed << choice.toMap().value("id").toString();
        if (name == "hardwareIds") {
            const auto *source = qobject_cast<GameListModel *>(sourceModel());
            if (source)
                for (const auto &g : source->records())
                    allowed << g.roles.value("hardwareKind").toString() + "/" +
                                   g.roles.value("hardwareFamily").toString();
        }
        if (name == "statePills")
            allowed = {"toInstall", "ready", "updates", "needsFiles"};
        if (name == "hiddenPublishers")
            for (const auto &choice : choices("manufacturerIds"))
                allowed << choice.toMap().value("id").toString();
        if (allowed.isEmpty() && name != "statePills") {
            emit filterWarning("Ignored unknown facet: " + name);
            return;
        }
        if (value.isValid() && value.metaType().id() != QMetaType::QString &&
            value.metaType().id() != QMetaType::QStringList &&
            value.metaType().id() != QMetaType::QVariantList) {
            emit filterWarning("Ignored invalid selection type: " + name);
            return;
        }
        QStringList valid;
        for (const auto &item : selected(value)) {
            if (allowed.contains(item))
                valid << item;
            else
                emit filterWarning("Ignored unknown facet value: " + name + "=" + item);
        }
        valid.removeDuplicates();
        normalized = valid;
    }
    if ((name == "yearMin" || name == "yearMax") && normalized.isValid()) {
        const auto other = m_facets.value(name == "yearMin" ? "yearMax" : "yearMin");
        if (other.isValid() && (name == "yearMin" ? normalized.toInt() > other.toInt()
                                                  : normalized.toInt() < other.toInt())) {
            emit filterWarning("Ignored reversed year range");
            return;
        }
    }
    if (m_facets.value(name) == normalized)
        return;
    m_facets[name] = normalized;
    m_choicesCache.clear();
    invalidateFilter();
    emit facetsChanged();
    emit visibleCountChanged();
}
void FilterSortModel::clearFacets() {
    m_facets.clear();
    m_query.clear();
    m_choicesCache.clear();
    forgetRanks();
    invalidateFilter();
    emit facetsChanged();
    emit queryChanged();
    emit visibleCountChanged();
}
QVariantList FilterSortModel::choices(const QString &name) const {
    if (m_choicesCache.contains(name)) return m_choicesCache.value(name);
    const auto *source = qobject_cast<GameListModel *>(sourceModel());
    QVariantList result;
    if (!source)
        return result;
    QMap<QString, QString> values;
    const QMap<QString, QString> fields{{"genre", "genreId"},
                                        {"manufacturerIds", "manufacturerId"},
                                        {"graphicsIds", "graphicsId"},
                                        {"controlsTypes", "controlsType"},
                                        {"playersBuckets", "playersBucket"},
                                        {"hardwareIds", "hardwareId"},
                                        {"hardwareKinds", "hardwareKind"},
                                        {"hardwareFamilies", "hardwareFamily"},
                                        {"vrKeys", "vrBest"},
                                        {"decades", "decade"}};
    const auto field = fields.value(name);
    if (field.isEmpty())
        return result;
    QMap<QString, int> counts;
    auto facets = m_facets;
    facets.remove(name);
    if (name.startsWith("hardware"))
        for (const auto &key : {"hardwareIds", "hardwareKinds", "hardwareFamilies"})
            facets.remove(key);
    int row = 0;
    for (const auto &g : source->records()) {
        const bool accepted = acceptsRow(row++, facets);
        const auto id = g.roles.value(field).toString();
        if (id.isEmpty())
            continue;
        const auto labelField = field == "hardwareId"       ? "hardwareLabel"
                                : field == "manufacturerId" ? "manufacturerLabel"
                                : field == "genreId"        ? "genreLabel"
                                : field == "graphicsId"     ? "graphicsLabel"
                                                            : "";
        const auto label = *labelField ? g.roles.value(labelField).toString() : id;
        if (label.startsWith("Unknown ("))
            continue;
        values[id] = label;
        if (accepted) {
            ++counts[id];
            if (name == "vrKeys" && g.roles.value("vrPlanned").toBool())
                ++counts["planned"];
        }
    }
    if (name == "vrKeys")
        values["planned"] = "True 3D planned";
    for (auto it = values.begin(); it != values.end(); ++it)
        result << QVariantMap{{"id", it.key()}, {"label", it.value()}, {"count", counts.value(it.key())}};
    m_choicesCache.insert(name, result);
    return result;
}
bool FilterSortModel::filterAcceptsRow(int row, const QModelIndex &) const {
    return acceptsRow(row, m_facets);
}
bool FilterSortModel::acceptsRow(int row, const QVariantMap &facets) const {
    const auto *source = qobject_cast<GameListModel *>(sourceModel());
    if (!source)
        return true;
    const auto &game = source->records().at(row);
    const auto &r = game.roles;
    if (!r.value("loadError").toString().isEmpty())
        return true;
    auto known = [&](const char *label) {
        return !r.value(label).toString().startsWith("Unknown (");
    };
    if (!matches(selected(facets.value("genre")), r.value("genreId").toString(),
                 known("genreLabel")))
        return false;
    if (!matches(selected(facets.value("manufacturerIds")), r.value("manufacturerId").toString(),
                 known("manufacturerLabel")))
        return false;
    if (!matches(selected(facets.value("graphicsIds")), r.value("graphicsId").toString(),
                 known("graphicsLabel")))
        return false;
    const auto year = r.value("year").toInt();
    if (year > 0) {
        if (facets.value("yearMin").isValid() && year < facets.value("yearMin").toInt())
            return false;
        if (facets.value("yearMax").isValid() && facets.value("yearMax").toInt() > 0 &&
            year > facets.value("yearMax").toInt())
            return false;
        if (!matches(selected(facets.value("decades")), r.value("decade").toString()))
            return false;
    }
    const auto hw = selected(facets.value("hardwareIds"));
    if (!hw.isEmpty() && known("hardwareLabel") && !hw.contains(r.value("hardwareId").toString()) &&
        !hw.contains(r.value("hardwareFamily").toString()) &&
        !hw.contains(r.value("hardwareKind").toString()) &&
        !hw.contains(r.value("hardwareKind").toString() + "/" +
                     r.value("hardwareFamily").toString()))
        return false;
    for (const auto *field : {"hardwareKinds", "hardwareFamilies"})
        if (!matches(selected(facets.value(field)),
                     r.value(QString(field) == "hardwareKinds" ? "hardwareKind" : "hardwareFamily")
                         .toString(),
                     known("hardwareLabel")))
            return false;
    if (!matches(selected(facets.value("playersBuckets")), r.value("playersBucket").toString(),
                 r.value("players").toInt() > 0))
        return false;
    if (!matches(
            selected(facets.value("controlsTypes")), r.value("controlsType").toString(),
            QStringList{"gun", "wheel", "handlebars", "bike", "ski", "joystick", "yoke", "boat"}
                .contains(r.value("controlsType").toString())))
        return false;
    const auto vr = selected(facets.value("vrKeys"));
    const auto best = r.value("vrBest").toString();
    if (!vr.isEmpty() && QStringList{"true3d", "theatre", "none"}.contains(best) &&
        !vr.contains(best) && !(vr.contains("planned") && r.value("vrPlanned").toBool()))
        return false;
    if (m_scanComplete) {
        if (facets.value("inLibraryOnly").toBool() && !r.value("inLibrary").toBool())
            return false;
        const auto state = selected(facets.value("statePills"));
        const auto pill = r.value("statePill").toString();
        if (!state.isEmpty() && !state.contains(pill) &&
            !(state.contains("ready") && pill == "updates"))
            return false;
    }
    if (m_query.isEmpty() && selected(facets.value("hiddenPublishers")).isEmpty())
        return true;
    const auto search = r.value("searchIndex").toMap();
    const auto tokens = folded(m_query).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    const auto hidden = selected(facets.value("hiddenPublishers"));
    bool isHidden = hidden.contains(r.value("manufacturerId").toString());
    for (const auto &token : tokens)
        if (token.startsWith('+') && token.size() > 1 &&
            search.value("manufacturer").toString().contains(token.mid(1)))
            isHidden = false;
    if (isHidden)
        return false;
    return queryClass(row, r) >= 0;
}
FilterSortModel::RowSearch &FilterSortModel::rowSearch(int row, const QVariantMap &r) const {
    if (m_rowSearch.size() <= row)
        m_rowSearch.resize(row + 1);
    auto &s = m_rowSearch[row];
    if (!s.ready) {
        s.names = match::searchKeys(QStringList{r.value("title").toString()} + r.value("altTitles").toStringList());
        s.series = match::searchKeys({r.value("series").toString()});
        s.ready = true;
    }
    return s;
}
int FilterSortModel::queryClass(int row, const QVariantMap &r) const {
    const auto search = r.value("searchIndex").toMap();
    const auto tokens = folded(m_query).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    const auto best = r.value("vrBest").toString();
    QStringList ordinary, missed;
    for (auto token : tokens) {
        if (token.startsWith('+') && token.size() > 1)
            continue;
        const bool exclude = token.startsWith('-') && token.size() > 1;
        if (exclude)
            token = token.mid(1);
        const bool special =
            QStringList{"gun", "racing", "true3d", "theatre", "planned", "new", "roomscale"}.contains(token);
        bool hit = false;
        if (QStringList{"gun", "racing"}.contains(token))
            hit = r.value("genreId").toString() == token;
        else if (QStringList{"true3d", "theatre", "planned"}.contains(token))
            hit = token == "planned" ? r.value("vrPlanned").toBool() : best == token;
        else if (token == "new") {
            const auto seen = r.value("firstSeen").toLongLong();
            hit = seen > 0 && seen >= QDateTime::currentSecsSinceEpoch() - 907200;
        } else if (token == "roomscale")
            hit = r.value("roomscale").toBool();
        else if (exclude) {
            for (const auto *key : {"manufacturer", "genre", "subgenre", "vr"})
                hit |= search.value(key).toString().contains(token);
        } else {
            for (const auto *key : {"title", "alt", "series", "developer", "manufacturer",
                                    "hardware", "subgenre", "blurb", "badges"})
                hit |= search.value(key).toString().contains(token);
        }
        if (exclude ? hit : !hit) {
            if (exclude || special)
                return -1;
            missed << token; // maybe a slip ("mraio kart") or a closed-up space ("kart7")
        }
        if (!exclude && !special)
            ordinary << token;
    }
    if (ordinary.isEmpty())
        return 2;
    auto &keys = rowSearch(row, r);
    if (!missed.isEmpty()) {
        const auto typed = missed.join(' ');
        return match::searchRank(keys.names, typed) != match::SearchRank::None ||
                       match::searchRank(keys.series, typed) != match::SearchRank::None
                   ? 3
                   : -1;
    }
    switch (match::searchRank(keys.names, ordinary.join(' '))) {
    case match::SearchRank::Exact:
        return 0;
    case match::SearchRank::Prefix:
        return 1;
    default:
        return 2;
    }
}
// Typed search lists what it was asked for first: a title that is the words, then titles that start
// with them, then the rest, then the near misses. Only in the A to Z order; another order is kept.
int FilterSortModel::relevance(int row) const {
    const auto *source = qobject_cast<GameListModel *>(sourceModel());
    if (!source || row < 0 || row >= source->records().size())
        return 0;
    // A game that shows although it does not match (one that failed to load) goes last.
    const auto place = [](int rank) { return rank < 0 ? 4 : rank; };
    if (m_rowSearch.size() > row && m_rowSearch[row].rank != -2)
        return place(m_rowSearch[row].rank);
    const auto rank = queryClass(row, source->records().at(row).roles);
    if (m_rowSearch.size() <= row)
        m_rowSearch.resize(row + 1);
    m_rowSearch[row].rank = qint8(rank);
    return place(rank);
}
bool FilterSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const auto *source = qobject_cast<GameListModel *>(sourceModel());
    if (!source)
        return left.row() < right.row();
    const auto &a = source->records()[left.row()].roles, &b = source->records()[right.row()].roles;
    auto text = [&](const char *key) {
        const auto &x=m_sortKeys[left.row()],&y=m_sortKeys[right.row()];
        return QString::compare(QString(key)=="title"?x.title:QString(key)=="manufacturerLabel"?x.manufacturer:x.id,QString(key)=="title"?y.title:QString(key)=="manufacturerLabel"?y.manufacturer:y.id);
    };
    auto number = [&](const char *key) {
        auto x = a.value(key).toLongLong(), y = b.value(key).toLongLong();
        return x < y ? -1 : x > y ? 1 : 0;
    };
    int cmp = m_sort == "title" && !m_query.isEmpty() ? relevance(left.row()) - relevance(right.row()) : 0;
    if (m_sort == "year")
        cmp = number("year");
    else if (m_sort == "manufacturer") {
        cmp = text("manufacturerLabel");
        if (!cmp)
            cmp = number("year");
    } else if (m_sort == "hardware") {
        const QStringList kinds{"arcade", "console", "pc"};
        cmp = kinds.indexOf(a.value("hardwareKind").toString()) -
              kinds.indexOf(b.value("hardwareKind").toString());
        if (!cmp)
            cmp = number("hardwareOrder");
        if (!cmp)
            cmp = number("year");
    } else if (m_sort == "recent")
        cmp = -number("lastPlayed");
    else if (m_sort == "added")
        cmp = -number("firstSeen");
    if (!cmp)
        cmp = text("title");
    if (!cmp)
        cmp = text("gameId");
    return cmp < 0;
}
} // namespace ac
