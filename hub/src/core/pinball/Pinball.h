// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QVariantMap>
#include <QVariantList>
#include <QString>
namespace ac::pinball {
QVariantMap inspect(const QString &path);
QVariantMap scan(const QString &folder,const QString &database={},const QString &romFolder={},const QString &mediaFolder={});
// What a saved scan was made from: the scanner's version, how many tables there are and when the newest table,
// ROM or database changed. A saved scan whose fingerprint differs from the folders now is out of date.
// Reads names and dates only (no table is opened); bounded like the scan.
QVariantMap fingerprint(const QString &folder,const QString &database={},const QString &romFolder={});
QVariantMap launchGuidance();
// With vr and an owned root the plan asks VPX for the headset explicitly: it adds
// -TableIni <ownedRoot>/user/pinball/<table id>.ini, a Hub-owned per-table settings
// file that VPX layers over the user's own settings. Planning creates nothing.
QVariantMap launchPlan(const QString &table,const QString &executable,const QString &ownedRoot,bool vr);
// The same with a view: "headset" asks VPX for VR (AskToTurnOn = 0), "desktop" turns VR off
// (AskToTurnOn = 2, so VPX never starts OpenXR), "" leaves it to VPX's own setting.
// A non-empty appSettings adds -Ini <appSettings>: a Hub-owned copy of the user's VPX settings.
QVariantMap launchPlan(const QString &table,const QString &executable,const QString &ownedRoot,const QString &view,const QString &appSettings={});
// INI text with [PlayerVR] AskToTurnOn = 0 (VR on); everything else is kept.
QByteArray vrOnSettings(const QByteArray &settings);
// The same with any AskToTurnOn value: 0 VR on, 1 autodetect, 2 VR off.
QByteArray vrModeSettings(const QByteArray &settings,int mode);
// Creates or updates the plan's Hub-owned table settings file. The first time it
// starts from the table's own settings beside the table, which is only read.
void writeVrSettings(const QVariantMap &plan);
}
