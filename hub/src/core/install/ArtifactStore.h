// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Support.h"
#include <QSslCertificate>
namespace ac::install {
// Test CA trust is explicit dependency injection, never a CLI/config TLS
// bypass.
struct TestTrust {
  QList<QSslCertificate> authorities;
  QSet<QString> hosts;
  QString baseUrl;
};
class ArtifactStore {
public:
  explicit ArtifactStore(QString root, Options options = {});
  ArtifactStore(QString root, Options options, const TestTrust &trust);
  QString acquire(const Json &step, const Json &guard = Json::object());
  Json githubRelease(const QString &repo, const QString &tag);
  static void validateUrl(const QString &url, const QStringList &hosts = {});
  static QSet<QString> globalHosts();

private:
  QString root_;
  Options options_;
  TestTrust trust_;
  QStringList perHosts_;
  Json request(const QString &url, const QString &method,
               const QString &destination = {},
               const QByteArray &validator = {}, qint64 offset = 0,
               qint64 maxBytes = 8LL * 1024 * 1024 * 1024);
};
} // namespace ac::install
