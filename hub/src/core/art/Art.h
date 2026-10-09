// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/scan/Scan.h"
#include <QImage>
#include <QMutex>
#include <QQuickAsyncImageProvider>
#include <memory>
namespace ac::art {
struct ArtOptions {
  QString userRoot;
  QStringList roots;
};
struct ArtResult {
  QImage image;
  QString path, source;
};
class Resolver {
public:
  Resolver(CatalogData catalog, ArtOptions options);
  void setBindings(const QVector<scan::Binding> &bindings);
  void setRoots(const QStringList &roots);
  ArtResult resolve(const QString &id, const QString &kind,
                    const QSize &size) const;
  static QString normalizedTitle(QString title);

private:
  CatalogData m_catalog;
  QMap<QString, QStringList> m_serials;
  ArtOptions m_options;
  QVector<scan::Binding> m_bindings;
  mutable QMutex m_mutex;
};
class Provider : public QQuickAsyncImageProvider {
public:
  explicit Provider(std::shared_ptr<Resolver> resolver)
      : m_resolver(std::move(resolver)) {}
  QQuickImageResponse *requestImageResponse(const QString &id,
                                            const QSize &size) override;

private:
  std::shared_ptr<Resolver> m_resolver;
};
} // namespace ac::art
