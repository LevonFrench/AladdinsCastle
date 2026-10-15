// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/art/Download.h"
#include "models/FilterSortModel.h"
#include "UiSettings.h"
#include "DetailControlsController.h"
#include "ProductIdentity.h"
#include <QObject>
#include <QUrl>
namespace ac {
// One model per grid presentation: desktop and overlay can use different columns.
class SectionedGridModel : public QAbstractListModel {
 Q_OBJECT
 Q_PROPERTY(int count READ count NOTIFY countChanged)
 Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY columnsChanged)
 public:
 SectionedGridModel(GameListModel *games,FilterSortModel *filter,int columns,QObject *parent=nullptr);
 int count()const{return static_cast<int>(m_rows.size());}
 int columns()const{return m_columns;}
 void setColumns(int columns);
 int rowCount(const QModelIndex &parent={})const override{return parent.isValid()?0:count();}
 QVariant data(const QModelIndex &index,int role)const override;
 QHash<int,QByteArray> roleNames()const override{return {{Qt::UserRole+1,"rowData"}};}
 Q_INVOKABLE QVariantMap row(int index)const{return index>=0&&index<count()?m_rows[index].data:QVariantMap{};}
 signals: void countChanged();void columnsChanged();
 private:
 struct Row {QString key;QVariantMap data;};
 void scheduleRefresh();void refresh();
 GameListModel *m_games;FilterSortModel *m_filter;int m_columns;bool m_queued=false;
 QVector<Row> m_rows;
};
class UiController : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailChanged)
 Q_PROPERTY(QObject *detailControls READ detailControls CONSTANT)
 Q_PROPERTY(QString detailControlsStatus READ detailControlsStatus NOTIFY detailControlsChanged)
 Q_PROPERTY(bool detailControlsLoading READ detailControlsLoading NOTIFY detailControlsChanged)
 Q_PROPERTY(QVariantList consoleEvents READ consoleEvents NOTIFY installChanged)
 Q_PROPERTY(QVariantMap recovery READ recovery NOTIFY installChanged)
 Q_PROPERTY(bool retryGameAvailable READ retryGameAvailable NOTIFY installChanged)
 Q_PROPERTY(bool retryToolAvailable READ retryToolAvailable NOTIFY installChanged)
 Q_PROPERTY(bool installing READ installing NOTIFY installChanged)
 Q_PROPERTY(bool scanning READ scanning NOTIFY scanChanged)
 Q_PROPERTY(QString status READ status NOTIFY statusChanged)
 Q_PROPERTY(QVariantList activeFacets READ activeFacets NOTIFY facetsChanged)
 Q_PROPERTY(QVariantList hardwareTree READ hardwareTree NOTIFY facetsChanged)
 Q_PROPERTY(QVariantList sections READ sections NOTIFY facetsChanged)
 Q_PROPERTY(QVariantList recent READ recent NOTIFY detailChanged)
 Q_PROPERTY(QVariantList featured READ featured CONSTANT)
 Q_PROPERTY(QVariantList exploreRows READ exploreRows NOTIFY facetsChanged)
 Q_PROPERTY(int artRevision READ artRevision WRITE setArtRevision NOTIFY artRevisionChanged)
 Q_PROPERTY(bool artProviderReady READ artProviderReady WRITE setArtProviderReady NOTIFY artProviderChanged)
 Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
 Q_PROPERTY(QString appName READ appName CONSTANT)
 Q_PROPERTY(QVariantMap nowPlaying READ nowPlaying NOTIFY nowPlayingChanged)
 // How the last game ended: {title, outcome (closed|stopped|crashed|failed), text, details}. Empty once dismissed.
 Q_PROPERTY(QVariantMap lastSession READ lastSession NOTIFY nowPlayingChanged)
 Q_PROPERTY(QVariantMap pinballData READ pinballData NOTIFY pinballChanged)
 Q_PROPERTY(QVariantMap pinballSelection READ pinballSelection NOTIFY pinballChanged)
 Q_PROPERTY(QString pinballPlan READ pinballPlan NOTIFY pinballChanged)
 Q_PROPERTY(QString pinballLaunchHelp READ pinballLaunchHelp CONSTANT)
 Q_PROPERTY(bool pinballScanning READ pinballScanning NOTIFY pinballChanged)
 Q_PROPERTY(QObject *pinballControls READ pinballControls CONSTANT)
 Q_PROPERTY(QString pinballControlsStatus READ pinballControlsStatus CONSTANT)
 Q_PROPERTY(QVariantList model3Games READ model3Games NOTIFY model3Changed)
 Q_PROPERTY(QVariantList model3Views READ model3Views CONSTANT)
 Q_PROPERTY(QVariantList otherGames READ otherGames NOTIFY model3Changed)
 // Catalog games set up and ready on this PC: shown on the VR games page even with the catalog switched off.
 Q_PROPERTY(QVariantList readyCatalogGames READ readyCatalogGames NOTIFY catalogReadyChanged)
 Q_PROPERTY(QVariantList setupSystems READ setupSystems NOTIFY setupChanged)
 Q_PROPERTY(QVariantMap setupKinds READ setupKinds NOTIFY setupChanged)
 Q_PROPERTY(QVariantMap steamVr READ steamVr NOTIFY setupChanged)
 Q_PROPERTY(QVariantMap setupReport READ setupReport NOTIFY setupChanged)
 Q_PROPERTY(bool setupScanning READ setupScanning NOTIFY setupChanged)
 Q_PROPERTY(QString setupProgress READ setupProgress NOTIFY setupChanged)
 public:
 UiController(GameListModel *games,FilterSortModel *filter,UiSettings *settings,QObject *parent=nullptr);
 QObject *detailControls(){return m_detailControls.model();}
 QString detailControlsStatus()const{return m_detailControls.status();}
 bool detailControlsLoading()const{return m_detailControls.loading();}
 DetailControlsController *controlsController(){return &m_detailControls;}
 Q_INVOKABLE void refreshDetailControls();
 Q_INVOKABLE void reloadDetailControls(){m_detailControls.refresh();}
 Q_INVOKABLE void leaveDetail(){m_detailControlsVisible=false;m_detailControls.clear();}
 Q_INVOKABLE void clearDetailControlStates(){m_detailControls.model()->clearBindingStates();}
 bool retryGameAvailable()const;
 bool retryToolAvailable()const;
 int artRevision()const{return m_artRevision;}
 void setArtRevision(int revision){if(m_artRevision!=revision){m_artRevision=revision;emit artRevisionChanged();}}
 bool artProviderReady()const{return m_artProviderReady;}
 void setArtProviderReady(bool ready){if(m_artProviderReady!=ready){m_artProviderReady=ready;emit artProviderChanged();}}
 static QString safeMarkdown(QString text);
 QVariantMap detail() const; QVariantList activeFacets() const; QVariantList hardwareTree() const;
 QVariantList sections() const; QVariantList recent() const; QVariantList featured() const; QVariantList exploreRows() const;
 QVariantList consoleEvents() const{return m_events;} QVariantMap recovery() const{return m_recovery;}
 bool installing() const{return m_installing;} bool scanning() const{return m_scanning;}
 QString status() const{return m_status;} QString appVersion() const;
 QString appName()const{return QString::fromUtf8(product::display_name);}
 QVariantMap nowPlaying()const{return m_nowPlaying;}
 QVariantMap lastSession()const{return m_lastSession;}
 Q_INVOKABLE void dismissLastSession(){m_lastSession.clear();emit nowPlayingChanged();}
 Q_INVOKABLE void launchQueued(const QString &id,const QString &variant,const QString &closeUnavailable);
 void queueSession(const QString &id,const QString &variant,const QString &closeUnavailable);
 Q_INVOKABLE void launchRoomQueued(const QString &model,const QString &hand,bool calibrateGrip=false);
 void launchClosing();
 void cancelPendingRestart(){m_restartAfterClose=false;}
 Q_INVOKABLE void closeLaunch();
 Q_INVOKABLE void restartLaunch();
 QVariantMap pinballData()const{return m_pinballData;}
 QVariantMap pinballSelection()const{return m_pinballSelection;}
 QString pinballPlan()const{return m_pinballPlan;}
 QString pinballLaunchHelp()const;
 bool pinballScanning()const{return m_pinballScanning;}
 QObject *pinballControls(){return &m_pinballControls;}
 QString pinballControlsStatus()const{return m_pinballControlsStatus;}
 Q_INVOKABLE void pinballLoaded(const QVariantMap &data);
 Q_INVOKABLE void selectPinball(const QString &id);
 Q_INVOKABLE void scanPinball();
 Q_INVOKABLE void openPinball(); // Shows the last scan of the chosen folder, or scans it.
 Q_INVOKABLE void markPinball(bool ready);
 Q_INVOKABLE void favouritePinball();
 Q_INVOKABLE void planPinball();
 // view: "desktop" or "headset"; empty is the headset, as Play always was.
 Q_INVOKABLE void playPinball(const QString &view={});
 // A table's own launch button, from its tile or its page: exactly that view, every time.
 Q_INVOKABLE void launchPinball(const QString &id,const QString &view);
 Q_PROPERTY(bool steamVrRunning READ steamVrRunning NOTIFY steamVrRunningChanged)
 // Play in headset while SteamVR is off: the game waiting for SteamVR to be ready, empty otherwise.
 Q_PROPERTY(QString steamVrStarting READ steamVrStarting NOTIFY steamVrStartingChanged)
 QString steamVrStarting()const{return m_steamVrStarting;}
 void setSteamVrStarting(const QString &title){if(title!=m_steamVrStarting){m_steamVrStarting=title;emit steamVrStartingChanged();}}
 Q_INVOKABLE void cancelSteamVrStart(){if(!m_steamVrStarting.isEmpty())emit cancelSteamVrStartRequested();}
 // A game's own Play button: exactly this view ("monitor", or one of its headset views), not remembered.
 Q_INVOKABLE void playGame(const QString &id,const QString &view);
 // The watchdog: programs of the set-up systems running outside any game (End, End all), the running
 // game's trouble ("hung", "hidden", "minimized"), and the crash records kept in the Hub's folder.
 Q_PROPERTY(QVariantList backgroundPrograms READ backgroundPrograms NOTIFY watchdogChanged)
 Q_PROPERTY(QString gameTrouble READ gameTrouble NOTIFY watchdogChanged)
 Q_PROPERTY(QVariantList crashRecords READ crashRecords NOTIFY crashRecordsChanged)
 QVariantList backgroundPrograms()const{return m_background;}
 QString gameTrouble()const{return m_gameTrouble;}
 QVariantList crashRecords()const{return m_crashRecords;}
 void setWatchdog(const QVariantList &background,const QString &trouble){if(background==m_background&&trouble==m_gameTrouble)return;m_background=background;m_gameTrouble=trouble;emit watchdogChanged();}
 void setCrashRecords(const QVariantList &records){m_crashRecords=records;emit crashRecordsChanged();}
 // A crash said in words on the last-session banner.
 void markCrash(const QString &id,const QString &text);
 Q_INVOKABLE void endBackgroundProgram(qint64 pid){emit endBackgroundRequested(pid);}
 Q_INVOKABLE void endAllBackgroundPrograms(){emit endAllBackgroundRequested();}
 Q_INVOKABLE void bringGameToFront(){emit bringGameToFrontRequested();}
 bool steamVrRunning()const{return m_steamVrRunning;}
 void setSteamVrRunning(bool running){if(running!=m_steamVrRunning){m_steamVrRunning=running;emit steamVrRunningChanged();}}
 void launchPinballQueued(const QVariantMap &row,const QString &closeUnavailable,const QString &view={},const QString &gamepad={});
 Q_INVOKABLE QVariantList pinballGamepadLayout()const;
 // Pictures made from the tables' own backglass files: id -> {"thumb","backglass"} files in the Hub's cache.
 void setPinballArt(const QVariantMap &art);
 void pinballFailed(const QString &error);
 // Model 3: one row per game with its remembered view and any set-up problem.
 QVariantList model3Games()const;
 QVariantList model3Views()const;
 Q_INVOKABLE void chooseModel3View(const QString &id,const QString &view);
 Q_INVOKABLE void playModel3(const QString &id);
 Q_INVOKABLE void refreshModel3(){emit model3Changed();}
 // Optional artwork from the internet (D66): only when the setting is on, and only when asked.
 // scope is a game's id ("model2-vcop2"; Model 3 games as "model3-<id>") or "system:<system>".
 Q_INVOKABLE void getArtwork(const QString &scope);
 Q_PROPERTY(bool artworkBusy READ artworkBusy NOTIFY artworkChanged)
 Q_PROPERTY(QString artworkStatus READ artworkStatus NOTIFY artworkChanged)
 bool artworkBusy()const{return m_artworkBusy;}
 QString artworkStatus()const{return m_artworkStatus;}
 void setArtwork(bool busy,const QString &status);
 // The folders of artwork on this PC, indexed in the background; rows show them at once.
 Q_PROPERTY(int localArtFiles READ localArtFiles NOTIFY artworkChanged)
 int localArtFiles()const{return m_localArt?m_localArt->files():0;}
 void setLocalArt(std::shared_ptr<const art::LocalIndex> index){m_localArt=std::move(index);emit artworkChanged();emit model3Changed();}
 void launchModel3Queued(const QString &id,const QString &view,const QString &closeUnavailable);
 // Games of the other systems, in the same row shape as model3Games plus system,
 // pill, views and warning. Ids carry the system: "dreamcast-hotd2", "system22-timecris".
 QVariantList otherGames()const;
 QVariantList readyCatalogGames()const;
 Q_INVOKABLE void chooseGameView(const QString &id,const QString &view);
 Q_INVOKABLE void playOtherGame(const QString &id);
 void launchOtherQueued(const QString &id,const QString &view,const QString &title,const QString &hint,const QString &closeUnavailable);
 // First-run set-up: what the user has, and where each system's files and program are.
 QVariantList setupSystems()const;
 QVariantMap setupKinds()const;
 QVariantMap steamVr()const{return m_steamVr;}
 void setSteamVr(const QVariantMap &state){m_steamVr=state;emit setupChanged();}
 Q_INVOKABLE void setSetupKind(const QString &kind,bool on);
 Q_INVOKABLE void setSetupSystem(const QString &id,bool on);
 Q_INVOKABLE void setSetupFolder(const QString &id,const QString &folder);
 Q_INVOKABLE void setSetupProgram(const QString &id,const QString &program);
 Q_INVOKABLE void acceptSetupGuesses();
 Q_INVOKABLE void finishSetup();
 Q_INVOKABLE void addToSteamVr(){emit steamVrRequested();}
 // The report of what was found: {scanned, systems:[{id,title,note,ready,notRight,missing}]}.
 QVariantMap setupReport()const{return m_setupReport;}
 bool setupScanning()const{return m_setupScanning;}
 QString setupProgress()const{return m_setupProgress;}
 Q_INVOKABLE void scanSetup();
 Q_INVOKABLE void setupScanProgress(const QString &text){m_setupProgress=text;emit setupChanged();}
 Q_INVOKABLE void setSetupReport(const QVariantMap &report){m_setupReport=report;m_setupScanning=false;m_setupProgress.clear();emit setupChanged();emit model3Changed();}
 void setPinballPlan(const QString &plan){m_pinballPlan=plan;emit pinballChanged();}
 Q_INVOKABLE QString localPath(const QUrl &url)const{return url.isLocalFile()?url.toLocalFile():QString();}
 Q_INVOKABLE QString vrLabel(int badge) const;
 Q_INVOKABLE QString primaryLabel(const QString &gameId) const;
 Q_INVOKABLE QVariantMap game(const QString &id) const;
 Q_INVOKABLE QVariantMap filteredGame(int row) const;
 Q_INVOKABLE void openDetail(const QString &id);
 Q_INVOKABLE void selectVariant(const QString &id);
 Q_INVOKABLE void primary(const QString &gameId);
 Q_INVOKABLE void scan(const QStringList &roots);
 Q_INVOKABLE void startScan(const QStringList &roots);
 Q_INVOKABLE void cancelScan();
 Q_INVOKABLE QObject *createGridModel(int columns);
 Q_INVOKABLE void releaseGridModel(QObject *model);
 Q_INVOKABLE void startInstall(const QString &gameId,const QString &variantId);
 Q_INVOKABLE void play(const QString &gameId,const QString &variantId);
 Q_INVOKABLE void cancelInstall();
 Q_INVOKABLE void stopLaunch();
 // Ends the running game's whole process tree and anything left from games the Hub started.
 Q_INVOKABLE void stopEverything(){emit stopEverythingRequested();message("Stop everything: every program the Hub started is being ended.");}
 Q_INVOKABLE void retryInstall(bool fromStart,const QString &handover);
 Q_INVOKABLE void retryFailedTool(bool fromStart,const QString &handover);
 Q_INVOKABLE void answerPrompt(bool proceed);
 Q_INVOKABLE void skipStep();
 Q_INVOKABLE void uninstall(const QString &gameId,const QString &variantId);
 Q_INVOKABLE void toggleFacet(const QString &key,const QString &value);
 Q_INVOKABLE bool selected(const QString &key,const QString &value) const;
 Q_INVOKABLE int hardwareSelection(const QString &node) const;
 Q_INVOKABLE void copyLog() const;
 Q_INVOKABLE bool openLink(const QString &url) const;
 Q_INVOKABLE void openLocation(const QString &kind);
 Q_INVOKABLE void removeRecent(const QString &id);
 public slots:
 void showError(const QString &operation,const QString &message);
 void overlayFallback(const QString &reason);
 void settingsSaved(const QString &gameId);
 void scanStarted(); void scanProgress(const QVariantMap &progress); void scanFinished(bool success);
 void installStarted(); void installEvent(const QVariantMap &event); void installFinished(bool success,const QString &message);
 void launchPreparing(const QString &gameId,const QString &message);
 void launchStarted(const QString &gameId); void launchFinished(const QString &gameId,const QString &error,int exitCode=0);
 void applyRuntimeStates(const QVector<RuntimeState> &states);
 signals:
 void detailControlsChanged();
 void artRevisionChanged(); void artProviderChanged(); void detailChanged(); void installChanged(); void scanChanged(); void statusChanged(); void facetsChanged();
 void scanRequested(const QStringList &roots); void cancelScanRequested();
 void installRequested(const QString &gameId,const QString &variantId); void playRequested(const QString &gameId,const QString &variantId);
 void cancelInstallRequested(); void retryInstallRequested(const QString &gameId,const QString &variantId,bool fromStart,const QString &handover);
 void stopLaunchRequested();
 void closeLaunchRequested();
 void nowPlayingChanged();
 void pinballChanged();
 void pinballPlayRequested(const QString &id,const QString &view);
 void steamVrRunningChanged();
 void steamVrStartingChanged();
 void watchdogChanged();
 void crashRecordsChanged();
 void endBackgroundRequested(qint64 pid);
 void endAllBackgroundRequested();
 void bringGameToFrontRequested();
 void cancelSteamVrStartRequested();
 void stopEverythingRequested();
 void model3Changed();
 void artworkChanged();
 // Each target: id, system, title and names (the game's own file and folder names on this PC).
 void artworkRequested(const QVariantList &targets);
 void catalogReadyChanged();
 void setupChanged();
 void steamVrRequested();
 void setupScanRequested(const QStringList &systems);
 void model3PlayRequested(const QString &id,const QString &view);
 void otherPlayRequested(const QString &id,const QString &view,const QString &file);
 void pinballScanRequested(const QString &folder,const QString &database,const QString &roms,const QString &media);
 void pinballPlanRequested(const QString &table,const QString &executable,const QString &id);
 void pinballRestoreRequested(const QString &folder,const QString &database,const QString &roms,const QString &media);
 void retryToolInstallRequested(const QString &gameId,const QString &variantId,bool fromStart,const QString &handover);
 void promptAnswered(bool proceed); void skipStepRequested(); void uninstallPreviewRequested(const QString &gameId,const QString &variantId);
 void locationRequested(const QString &kind); void writeConfigRequested(const QString &gameId,const QVariantMap &settings);
 private:
 DetailControlsController m_detailControls;
 bool m_detailControlsVisible=false;
 void syncDetailControls(bool force);
 const Variant *variant(const GameRecord &record) const;
 QString readme(const GameRecord &record) const;
 void message(const QString &text);
 void scheduleFacetsChanged();
 void retryGameInstall(const QString &gameId,const QString &variantId,bool fromStart,const QString &handover);
 bool m_facetsQueued=false;
 GameListModel *m_games; FilterSortModel *m_filter; UiSettings *m_settings;bool m_steamVrRunning=false;QVariantMap m_pinballArt;
 QString m_detailId,m_variantId,m_status; QVariantList m_events; QVariantMap m_recovery;
 int m_artRevision=0;
 bool m_artworkBusy=false;QString m_artworkStatus,m_steamVrStarting,m_gameTrouble;QVariantList m_background,m_crashRecords;
 // A row's pictures: the user's own art for it first, then a downloaded one.
 void addArt(QVariantMap &row,const QString &artId,const QString &system)const;
 static QStringList artNames(const QVariantMap &row,const QString &artId);
 std::shared_ptr<const art::LocalIndex> m_localArt;
 bool m_artProviderReady=false;
 bool m_installing=false,m_scanning=false;
 QVariantMap m_nowPlaying,m_lastSession;
 QVariantMap m_steamVr,m_setupReport;
 QString m_setupProgress;
 bool m_setupScanning=false;
 bool m_restartAfterClose=false;
 QVariantMap m_pinballData,m_pinballRaw,m_pinballSelection;
 QString m_pinballPlan;
 bool m_pinballScanning=false;
 ControlsViewModel m_pinballControls;
 QString m_pinballControlsStatus;
};
}
