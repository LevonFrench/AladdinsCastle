// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QTimer>
#include <functional>
namespace ac {
// The desktop Hub is also SteamVR's dashboard panel whenever SteamVR runs, whichever started first.
// It attaches when SteamVR is running (trying again while SteamVR is still coming up), and detaches
// when SteamVR quits or stops; the window carries on either way. It never opens the dashboard.
class PanelAttacher : public QObject {
  Q_OBJECT
public:
  // attach makes the Hub the panel and says why not when it cannot; detach stops being the panel.
  PanelAttacher(std::function<bool(QString *)> attach, std::function<void()> detach,
                QObject *parent = nullptr);
  void setRetry(int intervalMs, int attempts);
  // How long to wait for a quitting SteamVR to stop, and to rest after giving up, before trying again.
  void setRejoin(int intervalMs);
  bool attached() const { return attached_; }
public slots:
  void setSteamVrRunning(bool running);
  // SteamVR asked its apps to quit: the panel goes now and comes back the next time SteamVR runs.
  void steamVrQuit();
signals:
  void attachedChanged(bool attached);
  void gaveUp(const QString &reason);

private:
  void tryAttach();
  void detach();
  std::function<bool(QString *)> attach_;
  std::function<void()> detach_;
  QTimer retry_, rejoin_;
  int tries_ = 0, attempts_ = 12;
  bool running_ = false, attached_ = false, waitForStop_ = false;
};
} // namespace ac
