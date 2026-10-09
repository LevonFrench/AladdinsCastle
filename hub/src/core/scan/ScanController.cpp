// SPDX-License-Identifier: GPL-3.0-only
#include "ScanController.h"
#include <QFile>
#include <QtConcurrent>
namespace ac::scan {
ScanController::ScanController(GameListModel *model, ScanOptions options,
                               QObject *parent)
    : QObject(parent), m_model(model), m_options(std::move(options)) {
  qRegisterMetaType<ScanResult>();
  QFile f(m_options.userRoot + "/scan-folders.json");
  if (f.open(QIODevice::ReadOnly))
    try {
      auto saved =
          Scanner::optionsFromJson(Json::parse(f.readAll().toStdString()));
      m_options.mediaRoots = saved.mediaRoots;
      m_options.toolRoots = saved.toolRoots;
      m_options.artRoots = saved.artRoots;
    } catch (const std::exception &) {
    }
  connect(&m_watcher, &QFutureWatcher<ScanResult>::finished, this, [this] {
    m_last = m_watcher.result();
    if (!m_last.cancelled) {
      // Merge only scanner-owned fields into current complete GUI snapshots.
      // Installation jobs / selected variants / timestamps may have changed
      // during scanning.
      for (auto &state : m_last.states)
        if (const auto *g = m_model->find(state.gameId)) {
          const auto found = state.mediaFound, tools = state.toolsOk,
                     older = state.toolsOlder;
          state = g->runtime;
          state.gameId = g->id;
          state.mediaFound = found;
          state.toolsOk = tools;
          state.toolsOlder = older;
        }
      m_model->applyRuntimeStates(m_last.states);
      emit resultsReady(m_last);
      ++m_artRevision;
      emit artRevisionChanged();
    }
    m_running = false;
    emit runningChanged();
    emit scanFinished(m_last.cancelled);
  });
}
ScanController::~ScanController() {
  cancel();
  m_watcher.waitForFinished();
}
void ScanController::scan(const QStringList &folders) {
  if (m_running)
    return;
  if (!folders.isEmpty()) {
    m_options.mediaRoots = folders;
    // The point/search flow may supply both emulator and media folders.
    m_options.toolRoots = folders;
    m_options.artRoots = folders;
  }
  auto catalog = m_model->catalog();
  const auto options = m_options;
  m_cancel = std::make_shared<std::atomic_bool>(false);
  const auto cancelled = m_cancel;
  m_running = true;
  emit runningChanged();
  emit scanStarted();
  m_watcher.setFuture(QtConcurrent::run([this, catalog, options, cancelled] {
    return Scanner::run(
        catalog, options, *cancelled, [this](const QVariantMap &v) {
          QMetaObject::invokeMethod(
              this, [this, v] { emit progress(v); }, Qt::QueuedConnection);
        });
  }));
}
void ScanController::cancel() {
  if (m_cancel)
    *m_cancel = true;
}
} // namespace ac::scan
