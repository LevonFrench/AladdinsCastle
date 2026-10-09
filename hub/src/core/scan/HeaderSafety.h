// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QtGlobal>
#include <atomic>

namespace ac::scan::detail {
// A bounded view of libchdr's normalized v5 map. Reference resolution completes
// before calling the decoder, so its recursive self-reference branch is unused.
bool resolveChdHunk(const unsigned char *map, quint64 mapBytes,
                    quint32 hunkCount, quint32 entryBytes, bool compressed,
                    quint32 requested, quint32 &terminal,
                    std::atomic_bool &cancel, QString &error);
} // namespace ac::scan::detail
