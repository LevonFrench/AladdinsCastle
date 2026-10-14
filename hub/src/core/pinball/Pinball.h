// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QVariantMap>
#include <QVariantList>
#include <QString>
namespace ac::pinball {
QVariantMap inspect(const QString &path);
QVariantMap scan(const QString &folder,const QString &database={},const QString &romFolder={},const QString &mediaFolder={});
QVariantMap launchGuidance();
// With vr and an owned root the plan asks VPX for the headset explicitly: it adds
// -TableIni <ownedRoot>/user/pinball/<table id>.ini, a Hub-owned per-table settings
// file that VPX layers over the user's own settings. Planning creates nothing.
QVariantMap launchPlan(const QString &table,const QString &executable,const QString &ownedRoot,bool vr);
// INI text with [PlayerVR] AskToTurnOn = 0 (VR on); everything else is kept.
QByteArray vrOnSettings(const QByteArray &settings);
// Creates or updates the plan's Hub-owned table settings file. The first time it
// starts from the table's own settings beside the table, which is only read.
void writeVrSettings(const QVariantMap &plan);
}
