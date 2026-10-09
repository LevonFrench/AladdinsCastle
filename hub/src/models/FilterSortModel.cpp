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
void FilterSortModel::setSourceModel(QAbstractItemModel *model) {
    m_choicesCache.clear();
    QSortFilterProxyModel::setSourceModel(model);
    connect(model, &QAbstractItemModel::dataChanged, this, [this]{ m_choicesCache.clear(); });
    connect(model, &QAbstractItemModel::modelReset, this, [this]{ m_choicesCache.clear(); });
    sort(0);
}
void FilterSortModel::setQuery(const QString &query) {
    if (m_query == query)
        return;
    m_query = query;
    m_choicesCache.clear();
    invalidateFilter();
    emit queryChanged();
    emit visibleCountChanged();
}
void FilterSortModel::setSortMode(const QString &mode) {
    if (m_sort == mode)
        return;
    m_sort = mode;
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
    for (auto token : tokens) {
        if (token.startsWith('+') && token.size() > 1)
            continue;
        const bool exclude = token.startsWith('-') && token.size() > 1;
        if (exclude)
            token = token.mid(1);
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
        if (exclude ? hit : !hit)
            return false;
    }
    return true;
}
bool FilterSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const auto *source = qobject_cast<GameListModel *>(sourceModel());
    if (!source)
        return left.row() < right.row();
    const auto &a = source->records()[left.row()].roles, &b = source->records()[right.row()].roles;
    auto text = [&](const char *key) {
        return QString::compare(folded(a.value(key).toString()), folded(b.value(key).toString()));
    };
    auto number = [&](const char *key) {
        auto x = a.value(key).toLongLong(), y = b.value(key).toLongLong();
        return x < y ? -1 : x > y ? 1 : 0;
    };
    int cmp = 0;
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
