// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QVariantMap>
namespace ac::pinball {
// Metadata only: never evaluates VBScript or opens its includes.
QVariantMap scriptMetadata(const QString &script,bool locked=false);
bool canStart(const QVariantMap &row);
}
