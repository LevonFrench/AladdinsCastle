// SPDX-License-Identifier: GPL-3.0-only
#include "NetworkPolicy.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
namespace ac {
namespace {
class DeniedReply final : public QNetworkReply {
public:
    DeniedReply(const QNetworkRequest &request, QObject *parent) : QNetworkReply(parent) {
        setRequest(request); setUrl(request.url()); open(QIODevice::ReadOnly);
        setError(QNetworkReply::ContentAccessDenied, "Remote QML resources are disabled");
        QTimer::singleShot(0, this, [this] {
            setFinished(true); emit errorOccurred(error()); emit finished();
        });
    }
    void abort() override {}
protected:
    qint64 readData(char *, qint64) override { return -1; }
};
class LocalManager final : public QNetworkAccessManager {
public:
    using QNetworkAccessManager::QNetworkAccessManager;
protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *data) override {
        const auto scheme = request.url().scheme().toLower();
        if (scheme == "http" || scheme == "https") return new DeniedReply(request, this);
        return QNetworkAccessManager::createRequest(op, request, data);
    }
};
}
QNetworkAccessManager *LocalQmlNetworkFactory::create(QObject *parent) { return new LocalManager(parent); }
}
