// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/art/Art.h"
#include "core/scan/ScanController.h"
#include "core/install/Install.h"
#include "core/launch/Launch.h"
#include "core/gunroom/GunRoom.h"
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
    Q_PROPERTY(QVariantList gunRoomModels READ gunRoomModels CONSTANT)
    Q_PROPERTY(bool gunRoomAvailable READ gunRoomAvailable NOTIFY gunRoomChanged)
    Q_PROPERTY(QString gunRoomReason READ gunRoomReason NOTIFY gunRoomChanged)
    Q_PROPERTY(bool gunRoomCalibrationAvailable READ gunRoomCalibrationAvailable NOTIFY gunRoomChanged)
    Q_PROPERTY(QString gunRoomCalibrationReason READ gunRoomCalibrationReason NOTIFY gunRoomChanged)
    Q_PROPERTY(bool gunRoomBusy READ gunRoomBusy NOTIFY playingChanged)
public:
    HubServices(GameListModel *games, FilterSortModel *filter, UiController *ui,
                UiSettings *settings, QString root, QObject *parent=nullptr,
                std::function<QString()> steamRootSource = {},
                std::function<bool()> steamRunningProbe = {});
    ~HubServices() override;
    QString toolPlan() const;
    QString removalPlan() const { return removalText_; }
    bool playing() const { return childPlaying_; }
    bool launchBusy() const { return launcher_.busy() || gunRoom_.busy(); }
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
    QVariantList gunRoomModels()const{return gunRoomModels_;}
    bool gunRoomAvailable()const{return gunRoomAvailable_;}
    QString gunRoomReason()const{return gunRoomReason_;}
    bool gunRoomCalibrationAvailable()const{return gunRoomCalibrationAvailable_&&!launchBusy()&&!hubExitPending_;}
    QString gunRoomCalibrationReason()const{return gunRoomCalibrationReason_;}
    bool gunRoomBusy()const{return gunRoom_.busy();}
    Q_INVOKABLE void previewGunRoom(const QString &model,const QString &hand,const QString &executable);
    Q_INVOKABLE void launchGunRoom(const QString &model,const QString &hand,const QString &executable);
    Q_INVOKABLE void calibrateGunRoom(const QString &model,const QString &hand,const QString &executable);
    Q_INVOKABLE void requestHubExit(int code=0);
    void setPinballDiagnosticIni(const QString &path);
    // SteamVR dashboard: the manifest this Hub would add, and the result of adding it.
    QString steamVrManifest() const;
    void steamVrAnswered(const QString &error);
    // Whether SteamVR's server is running; tests replace it.
    void setVrServerProbe(std::function<bool()> probe){vrServerProbe_=std::move(probe);}
signals:
    void gunRoomChanged();
    void hubExitReady(int code);
    void removalPlanChanged();
    void steamPreviewChanged();
    void playingChanged();
    void raiseHubRequested();
    void steamVrRegistrationRequested(const QString &manifest);
private:
    QString catalogRoot() const;
    RuntimeState current(const QString &id) const;
    install::Request gameRequest(const QString &id, const QString &variant);
    void restore();
    void scan(const QStringList &roots);
    void previewRemoval(const install::Request &request);
    void report(const std::exception &error);
    QVariantMap retryContext() const;
    void startInstallRequest(const install::Request &request,const QString &kind);
    void installTool(const QString &gameId,const QString &variantId,const QString &handover={});
    void startGunRoom(const gunroom::Inputs &inputs);
    void startPinball(const QVariantMap &row,const QString &executable);
    QString pinballVrRoot()const; // Empty when the user left the headset choice to VPX.
    void startModel3(const QString &id,const QString &view);
    void refreshSteamVr(const QString &error={});
    void finishPendingExit();
    void scanSetup(const QStringList &systems);
    void startOtherGame(const QString &id,const QString &view,const QString &file);
    gunroom::Inputs gunRoomInputs(const QString &model,const QString &hand,const QString &executable) const;
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
    QVariantList gunRoomModels_;
    QString gunRoomReason_;
    QString gunRoomCalibrationReason_;
    bool gunRoomCalibrationAvailable_=false;
    bool gunRoomAvailable_=false, hubExitPending_=false;
    int hubExitCode_=0;
    QVariantMap retainedPinball_;QString retainedPinballExecutable_;
    QString pinballDiagnosticIni_;
    gunroom::Inputs retainedGunRoom_;
    gunroom::Service gunRoom_;
    std::function<bool()> vrServerProbe_;
    bool askPs2ToQuit();
};
}
