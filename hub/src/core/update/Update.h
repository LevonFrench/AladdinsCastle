// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/catalog/GameRecord.h"
#include <QByteArray>
#include <QString>
#include <QVariantMap>
#include <functional>
#include <optional>
namespace ac::update {
// Update detection for the tools the Hub installs from a manifest in data/emulators (spec section
// 10). It answers one question: is a newer release of the pinned tool published? It reports
// "update available" and nothing else. It never downloads, installs, swaps or edits anything;
// installing a newer version is a separate step the user starts, with a new pin.
//
// The module has no network code of its own. The caller passes a Fetcher, which returns the
// body of one HTTPS GET or throws. A tool with no GitHub release to ask about makes no request.

// Fetches one URL (always https://api.github.com/repos/...) and returns the response body.
// Throws on a network or HTTP error.
using Fetcher = std::function<QByteArray(const QString &url)>;

struct Report {
  enum class State { Current, Available, Unknown };
  State state = State::Unknown;
  QString id, name, platform;
  QString pinnedTag;       // the tag the manifest pins for this platform
  QString latestTag;       // newest published tag, empty when not known
  QString latestPublished; // its publish time (ISO 8601), empty when not known
  QString url;             // the release page, for a "see what is new" link
  QString text;            // one plain sentence for the user
};
// "current", "available" or "unknown".
QString stateName(Report::State state);
// The report as a map for the UI: state, id, name, platform, pinnedTag, latestTag,
// latestPublished, url, text.
QVariantMap toMap(const Report &report);

// Compares two release tags that are version numbers with the same letters in front ("v2.7" and
// "v2.8.2", "mame0289" and "mame0290"). Returns -1, 0 or 1, or nothing when either tag is not
// plain numbers (a commit hash, a date) or the letters differ.
std::optional<int> compareTags(const QString &a, const QString &b);

// Checks one manifest (as the install engine reads it) for one platform key such as
// "windows-x64". Never throws: any failure gives an Unknown report with a plain sentence.
Report check(const Json &manifest, const QString &platform, const Fetcher &fetch);
// check() for each manifest, in order.
QList<Report> checkAll(const QList<Json> &manifests, const QString &platform, const Fetcher &fetch);
} // namespace ac::update
