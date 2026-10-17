// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/launch/Watchdog.h"
#include "core/art/Art.h"
#include "core/art/Download.h"
#include "core/scan/ScanController.h"
#include "core/install/Install.h"
#include "core/launch/Launch.h"
#include "core/gunroom/GunRoom.h"
#include "core/steam/Steam.h"
#include "ui/UiController.h"
#include <QElapsedTimer>
#include <QThreadPool>
#include <QTimer>
#include <functional>
#include <atomic>
#include <memory>
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
    // The optional artwork download; the tests give it a stand-in network.
    art::Downloader *artDownloader() { return artDownloader_.get(); }
    // The watchdog over the set-up systems' programs; the tests shorten its timings.
    launch::Watchdog *watchdog() { return &watchdog_; }
    // Play in headset while SteamVR is off starts it, waits until it has run for settleMs, then
    // launches; nothing is launched if it fails, is cancelled or takes longer than timeoutMs.
    // Only main.cpp gives the real starter; without one nothing is started, so no test can start SteamVR.
    void setSteamVrStarter(std::function<bool(QString *)> starter) { steamVrStarter_ = std::move(starter); }
    void setSteamVrWait(int timeoutMs, int settleMs) { steamVrTimeoutMs_ = timeoutMs; steamVrSettleMs_ = settleMs; }
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
    // Launch proof only: variables ("NAME=value;NAME=" removes) and extra arguments for the next game's program,
    // so a check can keep the game hidden, silent and off the headset. Never used by a player's launch.
    void setDiagnosticChild(const QString &environment,const QString &arguments){diagnosticEnv_=environment;diagnosticArgs_=arguments;}
    // Starts a VR games tab entry with a given view, as its Start button does, without changing the remembered view.
    void startForDiagnostic(const QString &id,const QString &view);
    // SteamVR dashboard: the manifest this Hub would add, and the result of adding it.
    QString steamVrManifest() const;
    void steamVrAnswered(const QString &error);
    // Whether SteamVR's server is running; tests replace it.
    void setVrServerProbe(std::function<bool()> probe){vrServerProbe_=probe;launcher_.setVrServerProbe(std::move(probe));}
    // Gamepads as Visual Pinball will name them; tests replace the probe of VPX's own SDL library.
    void setGamepadProbe(std::function<QVariantList(const QString &)> probe){gamepadProbe_=std::move(probe);}
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
    void startPinball(const QVariantMap &row,const QString &executable,const QString &view);
    QString pinballVrRoot()const; // Empty when the user left the headset choice to VPX.
    void startModel3(const QString &id,const QString &view);
    void refreshSteamVr(const QString &error={});
    void finishPendingExit();
    void scanSetup(const QStringList &systems);
    void startOtherGame(const QString &id,const QString &view,const QString &file);
    gunroom::Inputs gunRoomInputs(const QString &model,const QString &hand,const QString &executable) const;
    void applyDiagnosticChild(launch::Request &request) const;
    QString diagnosticEnv_,diagnosticArgs_;
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
    QVariantMap retainedPinball_;QString retainedPinballExecutable_,retainedPinballView_;
    QTimer steamVrWatch_;bool vrServerRunning()const;
    QString pinballDiagnosticIni_;
    gunroom::Inputs retainedGunRoom_;
    gunroom::Service gunRoom_;
    std::function<bool()> vrServerProbe_;
    std::function<QVariantList(const QString &)> gamepadProbe_;
    // Pinball pictures from the tables' backglass files, made in the background into the Hub's cache.
    void startPinballArt(const QVariantMap &scan);
    QThreadPool artPool_;std::shared_ptr<std::atomic_bool> artCancel_;
    bool askPs2ToQuit();
    // Model 2 quits cleanly through a Hub-owned stop file: the viewer asks MAME to exit, which saves its
    // NVRAM and settings. Stop forces both programs only if nothing has ended well after that.
    bool askModel2ToQuit(bool forceLater);
    // How long Stop waits for a graceful close before ending the whole tree; tests shorten it.
public:
    void setStopGrace(int ms){stopGraceMs_=ms;}
private:
    int stopGraceMs_=15000;quint64 stopSession_=0;
    void stopRunning();
    std::unique_ptr<art::Downloader> artDownloader_;
    launch::Watchdog watchdog_;
    void applyWatchdogRules();
    void recordCrash(const QString &id,int code,const QString &how,const QString &logPath);
    QString sessionSystem_,sessionTitle_;qint64 sessionStartMs_=0;bool crashReported_=false;
    void launchInHeadset(const QString &title, std::function<void()> launch);
    // The kill switch: the setting "startSteamVr" off, or AC_NO_STEAMVR_START set, and the Hub never starts SteamVR.
    bool mayStartSteamVr() const;
    void endSteamVrWait(const QString &message, bool error);
    std::function<bool(QString *)> steamVrStarter_;
    std::function<void()> pendingHeadset_;
    QString pendingHeadsetTitle_;
    QTimer steamVrStartWatch_;
    QElapsedTimer steamVrStartClock_;
    qint64 steamVrSeenAt_ = -1;
    int steamVrTimeoutMs_ = 120000, steamVrSettleMs_ = 5000;
    // Folders of artwork already on this PC: indexed off the main thread whenever the list changes.
    void scanLocalArt();
    QStringList localArtFolders_;bool localArtScanned_=false;std::shared_ptr<std::atomic_bool> localArtCancel_;
    QString model2StopFile_;quint64 model2Session_=0;
};
}
