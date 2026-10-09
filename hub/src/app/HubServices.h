// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/art/Art.h"
#include "core/scan/ScanController.h"
#include "core/install/Install.h"
#include "core/launch/Launch.h"
#include "core/steam/Steam.h"
#include "ui/UiController.h"
namespace ac {
class HubServices : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString toolPlan READ toolPlan CONSTANT)
    Q_PROPERTY(QString removalPlan READ removalPlan NOTIFY removalPlanChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool launchBusy READ launchBusy NOTIFY playingChanged)
    Q_PROPERTY(bool preparing READ preparing NOTIFY playingChanged)
    Q_PROPERTY(bool canRemoveSteam READ canRemoveSteam NOTIFY removalPlanChanged)
    Q_PROPERTY(bool steamRemoving READ steamRemoving NOTIFY steamPreviewChanged)
    Q_PROPERTY(QStringList steamAccounts READ steamAccounts NOTIFY steamPreviewChanged)
    Q_PROPERTY(QString steamPreview READ steamPreview NOTIFY steamPreviewChanged)
    Q_PROPERTY(bool steamWriteReady READ steamWriteReady NOTIFY steamPreviewChanged)
public:
    HubServices(GameListModel *games, FilterSortModel *filter, UiController *ui,
                UiSettings *settings, QString root, QObject *parent=nullptr,
                std::function<QString()> steamRootSource = {},
                std::function<bool()> steamRunningProbe = {});
    QString toolPlan() const;
    QString removalPlan() const { return removalText_; }
    bool playing() const { return childPlaying_; }
    bool launchBusy() const { return launcher_.playing(); }
    bool preparing() const { return launchBusy() && !playing(); }
    bool canRemoveSteam() const { return games_->find(pendingRemoval_.gameId) != nullptr; }
    bool steamRemoving() const { return steamRequest_.remove; }
    std::shared_ptr<art::Resolver> artResolver() const { return art_; }
    QStringList steamAccounts()const{return steamAccounts_;}
    QString steamPreview()const{return steamText_;}
    bool steamWriteReady()const{return steamPreviewReady_;}
    Q_INVOKABLE bool beginSteam(const QString &gameId,const QString &variantId,bool remove=false);
    Q_INVOKABLE void previewSteam(const QString &accountId);
    Q_INVOKABLE void approveSteamWrite();
    Q_INVOKABLE void installSupermodel();
    Q_INVOKABLE void previewSupermodelRemoval();
    Q_INVOKABLE bool confirmRemoval(bool removeSteam=false);
    const Json &bindings() const { return bindings_; }
    launch::LaunchService *launcher() { return &launcher_; }
signals:
    void removalPlanChanged();
    void steamPreviewChanged();
    void playingChanged();
    void raiseHubRequested();
private:
    RuntimeState current(const QString &id) const;
    install::Request gameRequest(const QString &id, const QString &variant);
    void restore();
    void scan(const QStringList &roots);
    void previewRemoval(const install::Request &request);
    void report(const std::exception &error);
    QVariantMap retryContext() const;
    void startInstallRequest(const install::Request &request,const QString &kind);
    void installTool(const QString &gameId,const QString &variantId,const QString &handover={});
    GameListModel *games_; FilterSortModel *filter_; UiController *ui_;
    UiSettings *settings_; QString root_, removalText_;
    scan::ScanController scanner_;
    install::InstallService installer_;
    launch::LaunchService launcher_;
    std::shared_ptr<art::Resolver> art_;
    Json bindings_=Json::object();
    install::Request pendingRemoval_, activeInstall_;
    QString activeInstallKind_;
    std::function<QString()> steamRootSource_;
    std::function<bool()> steamRunningProbe_;
    bool pendingSteamRemoval_=false;
    bool childPlaying_=false;
    bool rescanPending_=false, steamPreviewReady_=false;
    QStringList steamAccounts_;QString steamText_;
    steam::WriteRequest steamRequest_;steam::Preview steamPreview_;
};
}
