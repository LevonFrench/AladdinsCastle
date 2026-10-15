// SPDX-License-Identifier: GPL-3.0-only
#include "PanelAttacher.h"
namespace ac {
PanelAttacher::PanelAttacher(std::function<bool(QString *)> attach, std::function<void()> detach,
                             QObject *parent)
    : QObject(parent), attach_(std::move(attach)), detach_(std::move(detach)) {
  retry_.setSingleShot(true);
  retry_.setInterval(5000);
  connect(&retry_, &QTimer::timeout, this, [this] {
    if (running_ && !attached_ && !waitForStop_)
      tryAttach();
  });
}
void PanelAttacher::setRetry(int intervalMs, int attempts) {
  retry_.setInterval(intervalMs);
  attempts_ = attempts;
}
void PanelAttacher::setSteamVrRunning(bool running) {
  running_ = running;
  if (!running) {
    // Gone without a goodbye (closed or crashed): let the panel go, and be ready for the next start.
    waitForStop_ = false;
    tries_ = 0;
    retry_.stop();
    detach();
    return;
  }
  if (!attached_ && !waitForStop_ && !retry_.isActive())
    tryAttach();
}
void PanelAttacher::steamVrQuit() {
  // SteamVR's server lingers for a moment after asking apps to quit; wait for it to stop first.
  waitForStop_ = true;
  retry_.stop();
  detach();
}
void PanelAttacher::tryAttach() {
  ++tries_;
  QString error;
  if (attach_ && attach_(&error)) {
    attached_ = true;
    tries_ = 0;
    emit attachedChanged(true);
    return;
  }
  // SteamVR's server runs a little before its dashboard accepts panels: try again for a while.
  if (tries_ < attempts_)
    retry_.start();
  else
    emit gaveUp(error.isEmpty() ? QString("SteamVR did not accept the panel.") : error);
}
void PanelAttacher::detach() {
  if (!attached_)
    return;
  attached_ = false;
  if (detach_)
    detach_();
  emit attachedChanged(false);
}
} // namespace ac
