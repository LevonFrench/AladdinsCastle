// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Scan.h"
#include "models/GameListModel.h"
#include <QFutureWatcher>
namespace ac::scan {
class ScanController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool running READ running NOTIFY runningChanged)
  Q_PROPERTY(quint64 artRevision READ artRevision NOTIFY artRevisionChanged)
public:
  // Explicit worker injection for synthetic completion-failure regressions.
  using Runner = std::function<ScanResult(const CatalogData &, const ScanOptions &,
                                         std::atomic_bool &, const Progress &)>;
  ScanController(GameListModel *model, ScanOptions options,
                 QObject *parent = nullptr, Runner runner = {});
  ~ScanController() override;
  bool running() const { return m_running; }
  quint64 artRevision() const { return m_artRevision; }
  const ScanOptions &options() const { return m_options; }
  const ScanResult &lastResult() const { return m_last; }
public slots:
  void scan(const QStringList &folders = {});
  void cancel();
signals:
  void runningChanged();
  void artRevisionChanged();
  void scanStarted();
  void progress(const QVariantMap &value);
  void scanFinished(bool cancelled);
  void scanFailed(const QString &message);
  void resultsReady(const ac::scan::ScanResult &result);

private:
  GameListModel *m_model;
  ScanOptions m_options;
  Runner m_runner;
  QFutureWatcher<ScanResult> m_watcher;
  std::shared_ptr<std::atomic_bool> m_cancel;
  ScanResult m_last;
  bool m_running = false;
  quint64 m_artRevision = 0;
};
} // namespace ac::scan
