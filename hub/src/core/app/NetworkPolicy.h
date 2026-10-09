// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQmlNetworkAccessManagerFactory>
namespace ac {
// README/QML media stays local even when untrusted Markdown contains URLs.
class LocalQmlNetworkFactory final : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager *create(QObject *parent) override;
};
}
