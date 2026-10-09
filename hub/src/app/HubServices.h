// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/art/Art.h"
#include "core/scan/ScanController.h"
#include "core/install/Install.h"
#include "core/launch/Launch.h"
#include "ui/UiController.h"
namespace ac {
class HubServices : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString toolPlan READ toolPlan CONSTANT)
    Q_PROPERTY(QString removalPlan READ removalPlan NOTIFY removalPlanChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
public:
    HubServices(GameListModel *games, FilterSortModel *filter, UiController *ui,
                UiSettings *settings, QString root, QObject *parent=nullptr);
    QString toolPlan() const;
    QString removalPlan() const { return removalText_; }
    bool playing() const { return launcher_.playing(); }
    std::shared_ptr<art::Resolver> artResolver() const { return art_; }
    Q_INVOKABLE void installSupermodel();
    Q_INVOKABLE void previewSupermodelRemoval();
    Q_INVOKABLE void confirmRemoval();
    const Json &bindings() const { return bindings_; }
    launch::LaunchService *launcher() { return &launcher_; }
signals:
    void removalPlanChanged();
    void playingChanged();
    void raiseHubRequested();
private:
    RuntimeState current(const QString &id) const;
    install::Request gameRequest(const QString &id, const QString &variant);
    void restore();
    void scan(const QStringList &roots);
    void previewRemoval(const install::Request &request);
    void report(const std::exception &error);
    GameListModel *games_; FilterSortModel *filter_; UiController *ui_;
    UiSettings *settings_; QString root_, removalText_;
    scan::ScanController scanner_;
    install::InstallService installer_;
    launch::LaunchService launcher_;
    std::shared_ptr<art::Resolver> art_;
    Json bindings_=Json::object();
    install::Request lastInstall_, pendingRemoval_;
    bool rescanPending_=false;
};
}
