// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "models/FilterSortModel.h"
#include "UiSettings.h"
#include <QObject>
#include <QUrl>
namespace ac {
class UiController : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailChanged)
 Q_PROPERTY(QVariantList consoleEvents READ consoleEvents NOTIFY installChanged)
 Q_PROPERTY(QVariantMap recovery READ recovery NOTIFY installChanged)
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
 public:
 UiController(GameListModel *games,FilterSortModel *filter,UiSettings *settings,QObject *parent=nullptr);
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
 Q_INVOKABLE QString localPath(const QUrl &url)const{return url.isLocalFile()?url.toLocalFile():QString();}
 Q_INVOKABLE QString vrLabel(int badge) const;
 Q_INVOKABLE QString primaryLabel(const QString &gameId) const;
 Q_INVOKABLE QVariantMap game(const QString &id) const;
 Q_INVOKABLE QVariantMap filteredGame(int row) const;
 Q_INVOKABLE void openDetail(const QString &id);
 Q_INVOKABLE void selectVariant(const QString &id);
 Q_INVOKABLE void primary(const QString &gameId);
 Q_INVOKABLE void scan(const QStringList &roots);
 Q_INVOKABLE void startInstall(const QString &gameId,const QString &variantId);
 Q_INVOKABLE void play(const QString &gameId,const QString &variantId);
 Q_INVOKABLE void cancelInstall();
 Q_INVOKABLE void retryInstall(bool fromStart,const QString &handover);
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
 void scanStarted(); void scanProgress(const QVariantMap &progress); void scanFinished(bool success);
 void installStarted(); void installEvent(const QVariantMap &event); void installFinished(bool success,const QString &message);
 void launchStarted(const QString &gameId); void launchFinished(const QString &gameId,const QString &error);
 void applyRuntimeStates(const QVector<RuntimeState> &states);
 signals:
 void artRevisionChanged(); void artProviderChanged(); void detailChanged(); void installChanged(); void scanChanged(); void statusChanged(); void facetsChanged();
 void scanRequested(const QStringList &roots); void cancelScanRequested();
 void installRequested(const QString &gameId,const QString &variantId); void playRequested(const QString &gameId,const QString &variantId);
 void cancelInstallRequested(); void retryInstallRequested(bool fromStart,const QString &handover);
 void promptAnswered(bool proceed); void skipStepRequested(); void uninstallPreviewRequested(const QString &gameId,const QString &variantId);
 void locationRequested(const QString &kind); void writeConfigRequested(const QString &gameId,const QVariantMap &settings);
 private:
 const Variant *variant(const GameRecord &record) const;
 QString readme(const GameRecord &record) const;
 void message(const QString &text);
 GameListModel *m_games; FilterSortModel *m_filter; UiSettings *m_settings;
 QString m_detailId,m_variantId,m_status; QVariantList m_events; QVariantMap m_recovery;
 int m_artRevision=0;
 bool m_artProviderReady=false;
 bool m_installing=false,m_scanning=false;
};
}
