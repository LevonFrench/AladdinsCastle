// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QString>
#include <QVariantMap>
#include <atomic>
namespace ac::pinball {
// The backglass file beside a table: <table name>.directb2s, any letter case. Empty when there is none.
QString backglassFile(const QString &table);
// The backglass picture inside a .directb2s file (its BackglassImage, else its ThumbnailImage),
// decoded from the file's base64 text. Null when the file holds no picture Qt can read.
QImage backglassImage(const QString &directb2s,std::atomic_bool &cancel);
// Pictures for one table, made once from its backglass file and kept in cacheFolder (the
// Hub's own user folder; the table's folder is only read): {"thumb": file, "backglass": file}.
// Made again only when the backglass file changes. Empty when the table has no backglass.
QVariantMap tableArt(const QString &table,const QString &tableId,const QString &cacheFolder,std::atomic_bool &cancel);
}
