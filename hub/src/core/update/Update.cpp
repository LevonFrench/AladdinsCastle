// SPDX-License-Identifier: GPL-3.0-only
#include "Update.h"
#include "core/install/ArtifactStore.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>
namespace ac::update {
namespace {
QString field(const Json &j, const char *key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_string() ? QString::fromStdString(it->get<std::string>()) : QString();
}
struct Version {
  QString letters;
  QList<qint64> parts;
};
std::optional<Version> parseTag(const QString &tag) {
  static const QRegularExpression pattern("^([A-Za-z]*)[-_]?[vV]?(\\d+(?:\\.\\d+)*)$");
  const auto match = pattern.match(tag);
  if (!match.hasMatch())
    return std::nullopt;
  Version v;
  v.letters = match.captured(1).toLower();
  if (v.letters == "v")
    v.letters.clear();
  for (const auto &piece : match.captured(2).split('.')) {
    bool ok = false;
    const auto n = piece.size() > 12 ? -1 : piece.toLongLong(&ok);
    if (!ok)
      return std::nullopt;
    v.parts << n;
  }
  return v;
}
// The body of one GitHub release query, or nothing when it cannot be had or read.
std::optional<Json> fetchRelease(const Fetcher &fetch, const QString &repo, const QString &what) {
  try {
    const auto url = "https://api.github.com/repos/" + repo + "/releases/" + what;
    install::ArtifactStore::validateUrl(url);
    const auto body = fetch(url);
    auto json = Json::parse(body.toStdString(), nullptr, false);
    if (json.is_discarded() || !json.is_object())
      return std::nullopt;
    return json;
  } catch (...) {
    return std::nullopt;
  }
}
QDateTime published(const Json &rel) { return QDateTime::fromString(field(rel, "published_at"), Qt::ISODate); }
} // namespace

QString stateName(Report::State state) {
  switch (state) {
  case Report::State::Current:
    return "current";
  case Report::State::Available:
    return "available";
  case Report::State::Unknown:
    break;
  }
  return "unknown";
}
QVariantMap toMap(const Report &r) {
  return {{"state", stateName(r.state)}, {"id", r.id}, {"name", r.name}, {"platform", r.platform},
          {"pinnedTag", r.pinnedTag}, {"latestTag", r.latestTag}, {"latestPublished", r.latestPublished},
          {"url", r.url}, {"text", r.text}};
}
std::optional<int> compareTags(const QString &a, const QString &b) {
  const auto x = parseTag(a), y = parseTag(b);
  if (!x || !y || x->letters != y->letters)
    return std::nullopt;
  const auto size = std::max(x->parts.size(), y->parts.size());
  for (qsizetype i = 0; i < size; ++i) {
    const auto p = i < x->parts.size() ? x->parts[i] : 0, q = i < y->parts.size() ? y->parts[i] : 0;
    if (p != q)
      return p < q ? -1 : 1;
  }
  return 0;
}

Report check(const Json &manifest, const QString &platform, const Fetcher &fetch) {
  Report r;
  r.id = field(manifest, "id");
  r.name = field(manifest, "name");
  if (r.name.isEmpty())
    r.name = r.id;
  r.platform = platform;
  const Json *block = nullptr;
  if (const auto installs = manifest.find("install"); installs != manifest.end() && installs->is_object())
    if (const auto it = installs->find(platform.toStdString()); it != installs->end() && it->is_object())
      block = &*it;
  if (!block) {
    r.text = r.name + " has no download for this PC, so there is nothing to check.";
    return r;
  }
  r.pinnedTag = field(*block, "tag");
  const auto repo = field(*block, "repo");
  if (field(*block, "do") != "github-release" || repo.isEmpty()) {
    r.text = "The Hub does not check " + r.name + " for updates. Look on its official page.";
    return r;
  }
  if (r.pinnedTag.isEmpty() || r.pinnedTag == "latest") {
    r.text = r.name + " follows a rolling build, so there is no version to compare.";
    return r;
  }
  const auto latest = fetchRelease(fetch, repo, "latest");
  if (!latest || field(*latest, "tag_name").isEmpty()) {
    r.text = "Could not check for a newer " + r.name + " right now.";
    return r;
  }
  r.latestTag = field(*latest, "tag_name");
  r.latestPublished = field(*latest, "published_at");
  r.url = "https://github.com/" + repo + "/releases/tag/" + QString::fromLatin1(QUrl::toPercentEncoding(r.latestTag));
  bool newer = false, known = true;
  if (r.latestTag == r.pinnedTag) {
    // Same release: current.
  } else if (const auto order = compareTags(r.latestTag, r.pinnedTag)) {
    newer = *order > 0;
  } else {
    // Not comparable as numbers (a commit tag, a date): the pinned release's own publish time decides.
    const auto pinned = fetchRelease(fetch, repo, "tags/" + QString::fromLatin1(QUrl::toPercentEncoding(r.pinnedTag)));
    const auto then = pinned ? published(*pinned) : QDateTime(), now = published(*latest);
    known = then.isValid() && now.isValid();
    newer = known && now > then;
  }
  if (!known) {
    r.text = "A different " + r.name + " is out (" + r.latestTag + "), but the Hub cannot tell if it is newer than " + r.pinnedTag + ".";
    return r;
  }
  r.state = newer ? Report::State::Available : Report::State::Current;
  r.text = newer ? "A newer " + r.name + " is out: " + r.latestTag + " (you have " + r.pinnedTag + ")."
                 : r.name + " is up to date (" + r.pinnedTag + ").";
  return r;
}
QList<Report> checkAll(const QList<Json> &manifests, const QString &platform, const Fetcher &fetch) {
  QList<Report> reports;
  for (const auto &manifest : manifests)
    reports << check(manifest, platform, fetch);
  return reports;
}
} // namespace ac::update
