// SPDX-License-Identifier: GPL-3.0-only
#include "DetailControlsController.h"
#include <QDir>
#include <QFutureWatcher>
#include <QtConcurrent>

namespace ac {
namespace {
struct PreparedControls {
  std::shared_ptr<const ControlsResolver> library;
  QVariantMap result;
  QString error;
};
}
DetailControlsController::DetailControlsController(QObject *parent)
    :QObject(parent),model_(this),generation_(std::make_shared<std::atomic<quint64>>(0)){}
DetailControlsController::~DetailControlsController(){++*generation_;}
void DetailControlsController::resetView(){
  model_.clear();resolved_.clear();loading_=false;status_.clear();
}
void DetailControlsController::configure(const QString &root,const QStringList &packs){
  root_=root;packs_=packs;++libraryEpoch_;library_.reset();resolve();
}
void DetailControlsController::selectGame(const QString &game,const QString &variant){
  game_=game;variant_=variant;resolve();
}
void DetailControlsController::refresh(){++libraryEpoch_;library_.reset();resolve();}
void DetailControlsController::setOptionsSource(OptionsSource source){options_=std::move(source);resolve();}
void DetailControlsController::clear(){game_.clear();variant_.clear();++*generation_;resetView();emit changed();}
void DetailControlsController::resolve(){
  const auto generation=++*generation_;
  resetView();
  if(game_.isEmpty()){emit changed();return;}
  if(root_.isEmpty()){status_="Controls configuration is unavailable.";emit changed();return;}
  ControlsResolveOptions options;
  try {if(options_)options=options_(game_,variant_);}
  catch(...){status_="Controls context could not be read.";emit changed();return;}
  options.packIds=packs_; // The catalog's actual registry/priority order wins.
  loading_=true;status_="Loading controls…";emit changed();
  const auto epoch=libraryEpoch_;
  auto *watcher=new QFutureWatcher<PreparedControls>(this);
  connect(watcher,&QFutureWatcher<PreparedControls>::finished,this,[this,watcher,generation,epoch]{
    PreparedControls prepared;
    try {prepared=watcher->result();}
    catch(...){prepared.error="Controls configuration could not be resolved.";}
    watcher->deleteLater();
    // A stale request may populate only the matching immutable library cache.
    if(epoch==libraryEpoch_ && prepared.library)library_=prepared.library;
    if(generation!=generation_->load())return;
    loading_=false;
    if(!prepared.error.isEmpty())status_=prepared.error;
    else if(!model_.loadResolved(prepared.result,QDir(root_).filePath("assets/guns/preview")))status_=model_.lastError();
    else {resolved_=prepared.result;status_.clear();}
    emit changed();
  });
  watcher->setFuture(QtConcurrent::run([root=root_,game=game_,library=library_,options,generation,token=generation_]()mutable{
    PreparedControls prepared;
    if(generation!=token->load())return prepared;
    try {
      if(!library){
        auto loaded=std::make_shared<ControlsResolver>();
        if(!loaded->loadLibrary(root,prepared.error))return prepared;
        library=loaded;
      }
      prepared.library=library;
      if(generation!=token->load())return prepared;
      library->resolveGame(game,options,prepared.result,prepared.error);
    }catch(...){prepared.error="Controls configuration could not be resolved.";}
    return prepared;
  }));
}
}
