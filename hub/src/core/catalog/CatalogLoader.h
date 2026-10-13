// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "GameRecord.h"
#include <QMap>
namespace ac {
struct CatalogReport {
    int records = 0, errors = 0, warnings = 0;
    QMap<QString, int> counts;
    QStringList messages;
};
struct CatalogData {
    QString root;
    QStringList packIds;
    QVector<GameRecord> games;
    Json vocab = Json::object(), emulators = Json::object(), sharedSetups = Json::object(),
         theme = Json::object();
    CatalogReport report;
    const GameRecord *find(const QString &id) const;
};
class CatalogLoader {
  public:
    CatalogData load(const QString &root) const;
    static Json parseToml(const QString &file);
    static void merge(Json &base, const Json &layer);
};
} // namespace ac
