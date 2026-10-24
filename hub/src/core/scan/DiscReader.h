// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QByteArray>
#include <QFile>
#include <atomic>
#include <functional>
namespace ac::scan::detail {
using DiscReader=std::function<QByteArray(qint64,qint64)>;
DiscReader compressedReader(QFile &file,const QString &format,std::atomic_bool &cancel,QString &error);
}
