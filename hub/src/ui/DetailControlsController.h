// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <ControlsResolver.h>
#include <ControlsViewModel.h>
#include <QObject>
#include <atomic>
#include <memory>

namespace ac {
// Configuration review only. Production has no live runtime/node evidence yet.
class DetailControlsController final : public QObject {
  Q_OBJECT
public:
  using OptionsSource=std::function<ControlsResolveOptions(const QString &,const QString &)>;
  explicit DetailControlsController(QObject *parent=nullptr);
  ~DetailControlsController() override;
  void configure(const QString &root,const QStringList &packIds);
  void selectGame(const QString &gameId,const QString &variantId);
  void refresh();
  void clear();
  void setOptionsSource(OptionsSource source);
  ControlsViewModel *model(){return &model_;}
  QVariantMap resolved()const{return resolved_;}
  QString status()const{return status_;}
  bool loading()const{return loading_;}
signals:
  void changed();
private:
  void resolve();
  void resetView();
  ControlsViewModel model_;
  QString root_,game_,variant_,status_;
  QStringList packs_;
  QVariantMap resolved_;
  OptionsSource options_;
  std::shared_ptr<const ControlsResolver> library_;
  std::shared_ptr<std::atomic<quint64>> generation_;
  quint64 libraryEpoch_=0;
  bool loading_=false;
};
}
