// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QVariantList>
#include <QVariantMap>
namespace ac::scan {
class DiscIndex {
public:
 static QVariantList games(const QString &system);
 static QVariantMap metadata(const QString &system);
 static QVariantMap match(const QString &system,const QString &identity);
 static QString fingerprint();
};
}
