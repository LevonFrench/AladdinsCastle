// SPDX-License-Identifier: GPL-3.0-only
// Update detection (spec section 10) with an in-memory stand-in for GitHub: no test here reaches the
// internet, and every request the module makes is counted and checked.
#include "core/update/Update.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <stdexcept>
namespace {
using ac::update::Report;
QByteArray release(const QString &tag, const QString &published) {
  return QJsonDocument(QJsonObject{{"tag_name", tag}, {"published_at", published}}).toJson(QJsonDocument::Compact);
}
const QString api = "https://api.github.com/repos/";
// GitHub as the module sees it: pages by URL, and a log of every request that was made.
struct Github {
  QMap<QString, QByteArray> pages;
  QStringList asked;
  bool failEverything = false;
  ac::update::Fetcher fetcher() {
    return [this](const QString &url) {
      asked << url;
      if (failEverything || !pages.contains(url))
        throw std::runtime_error("HTTP error");
      return pages.value(url);
    };
  }
};
Json manifest(const QString &id, const QString &name, const QString &block) {
  return Json::parse(QString("{\"id\":\"%1\",\"name\":\"%2\",\"install\":{%3}}").arg(id, name, block).toStdString());
}
Json flycast(const QString &tag = "v2.7") {
  return manifest("flycast", "Flycast",
                  QString("\"windows-x64\":{\"do\":\"github-release\",\"repo\":\"flyinghead/flycast\",\"tag\":\"%1\","
                          "\"asset\":\"flycast-win64.zip\",\"sha256\":\"%2\",\"bytes\":9885102}")
                      .arg(tag, QString(64, 'a')));
}
Json supermodel() {
  return manifest("supermodel", "Supermodel",
                  "\"windows-x64\":{\"do\":\"github-release\",\"repo\":\"trzy/Supermodel\","
                  "\"tag\":\"v0.3a-20260928-git-8b4de23\",\"asset\":\"supermodel.zip\"}");
}
} // namespace

class UpdateTest : public QObject {
  Q_OBJECT
private slots:
  void tagsCompareAsNumbers() {
    using ac::update::compareTags;
    QCOMPARE(compareTags("v2.8.2", "v2.7"), std::optional<int>(1));
    QCOMPARE(compareTags("v2.7", "v2.7"), std::optional<int>(0));
    QCOMPARE(compareTags("v2.7", "v2.8"), std::optional<int>(-1));
    // Numbers, not text: 10 is more than 9.
    QCOMPARE(compareTags("v1.10", "v1.9"), std::optional<int>(1));
    QCOMPARE(compareTags("v2.7.0", "v2.7"), std::optional<int>(0));
    QCOMPARE(compareTags("2.8", "v2.7"), std::optional<int>(1));
    QCOMPARE(compareTags("mame0290", "mame0289"), std::optional<int>(1));
    QCOMPARE(compareTags("v1.5.230.0", "v1.5.229.9"), std::optional<int>(1));
    // Not comparable: different letters, hashes, dates, empty or huge numbers.
    QVERIFY(!compareTags("mame0289", "v2.7"));
    QVERIFY(!compareTags("build-304d544bc1b3af4f15400a47b34132ff49b1bc69", "build-1"));
    QVERIFY(!compareTags("v0.3a-20260928-git-8b4de23", "v0.3a-20260101-git-1234567"));
    QVERIFY(!compareTags("", "v1"));
    QVERIFY(!compareTags("v1", ""));
    QVERIFY(!compareTags("v99999999999999999999", "v1"));
  }

  void aNewerVersionIsReported() {
    Github net;
    net.pages[api + "flyinghead/flycast/releases/latest"] = release("v2.8", "2026-10-01T00:00:00Z");
    const auto r = ac::update::check(flycast(), "windows-x64", net.fetcher());
    QCOMPARE(r.state, Report::State::Available);
    QCOMPARE(r.pinnedTag, QString("v2.7"));
    QCOMPARE(r.latestTag, QString("v2.8"));
    QCOMPARE(r.latestPublished, QString("2026-10-01T00:00:00Z"));
    QCOMPARE(r.url, QString("https://github.com/flyinghead/flycast/releases/tag/v2.8"));
    QCOMPARE(r.text, QString("A newer Flycast is out: v2.8 (you have v2.7)."));
    QCOMPARE(net.asked, QStringList({api + "flyinghead/flycast/releases/latest"}));
    // The UI map carries the same facts.
    const auto map = ac::update::toMap(r);
    QCOMPARE(map.value("state").toString(), QString("available"));
    QCOMPARE(map.value("id").toString(), QString("flycast"));
    QCOMPARE(map.value("latestTag").toString(), QString("v2.8"));
    QCOMPARE(map.value("text").toString(), r.text);
  }

  void theSameOrAnOlderReleaseIsCurrent() {
    Github net;
    net.pages[api + "flyinghead/flycast/releases/latest"] = release("v2.7", "2026-08-19T11:47:09Z");
    auto r = ac::update::check(flycast("v2.7"), "windows-x64", net.fetcher());
    QCOMPARE(r.state, Report::State::Current);
    QCOMPARE(r.text, QString("Flycast is up to date (v2.7)."));
    // Pinned past the newest published release (a pre-release, say): nothing to offer.
    r = ac::update::check(flycast("v2.9"), "windows-x64", net.fetcher());
    QCOMPARE(r.state, Report::State::Current);
    QCOMPARE(net.asked.size(), 2);
  }

  void tagsThatAreNotNumbersUseThePublishTime() {
    const auto latest = api + "trzy/Supermodel/releases/latest";
    const auto pinned = api + "trzy/Supermodel/releases/tags/v0.3a-20260928-git-8b4de23";
    {
      Github net;
      net.pages[latest] = release("v0.3a-20261101-git-1234567", "2026-11-01T00:00:00Z");
      net.pages[pinned] = release("v0.3a-20260928-git-8b4de23", "2026-09-28T00:00:00Z");
      const auto r = ac::update::check(supermodel(), "windows-x64", net.fetcher());
      QCOMPARE(r.state, Report::State::Available);
      QCOMPARE(net.asked, QStringList({latest, pinned}));
    }
    {
      Github net; // a "latest" that was published before the pin is not an update
      net.pages[latest] = release("v0.3a-20260101-git-0000000", "2026-01-01T00:00:00Z");
      net.pages[pinned] = release("v0.3a-20260928-git-8b4de23", "2026-09-28T00:00:00Z");
      QCOMPARE(ac::update::check(supermodel(), "windows-x64", net.fetcher()).state, Report::State::Current);
    }
    {
      Github net; // the pinned release cannot be read: say so, never guess
      net.pages[latest] = release("v0.3a-20261101-git-1234567", "2026-11-01T00:00:00Z");
      const auto r = ac::update::check(supermodel(), "windows-x64", net.fetcher());
      QCOMPARE(r.state, Report::State::Unknown);
      QVERIFY(r.text.contains("cannot tell"));
      QCOMPARE(r.latestTag, QString("v0.3a-20261101-git-1234567"));
    }
    {
      Github net; // the tag gets one whole URL component
      net.pages[latest] = release("v0.3a-20261101-git-1234567", "2026-11-01T00:00:00Z");
      auto odd = supermodel();
      odd["install"]["windows-x64"]["tag"] = "build/1 2";
      ac::update::check(odd, "windows-x64", net.fetcher());
      QCOMPARE(net.asked.last(), api + "trzy/Supermodel/releases/tags/build%2F1%202");
    }
  }

  void failuresNeverThrowAndNeverClaimAnUpdate() {
    for (const auto &body : {QByteArray("not json"), QByteArray("[]"), QByteArray("{}"), QByteArray("{\"tag_name\":7}"), QByteArray()}) {
      Github net;
      net.pages[api + "flyinghead/flycast/releases/latest"] = body;
      const auto r = ac::update::check(flycast(), "windows-x64", net.fetcher());
      QCOMPARE(r.state, Report::State::Unknown);
      QCOMPARE(r.text, QString("Could not check for a newer Flycast right now."));
      QVERIFY(r.latestTag.isEmpty());
    }
    Github down;
    down.failEverything = true;
    QCOMPARE(ac::update::check(flycast(), "windows-x64", down.fetcher()).state, Report::State::Unknown);
    QCOMPARE(down.asked.size(), 1);
  }

  // Nothing to ask means no request at all: the rolling build, a manual or plain download, a missing
  // platform, a manifest with no install table, and an empty repo.
  void zeroRequestsWhenThereIsNothingToAsk() {
    Github net;
    const auto fetch = net.fetcher();
    const QList<Json> manifests{
        manifest("duckstation", "DuckStation",
                 "\"windows-x64\":{\"do\":\"github-release\",\"repo\":\"stenzek/duckstation\",\"tag\":\"latest\",\"asset\":\"d.zip\"}"),
        manifest("adb", "adb", "\"windows-x64\":{\"do\":\"download\",\"url\":\"https://dl.google.com/x.zip\",\"sha256\":\"?\"}"),
        manifest("dolphin", "Dolphin", "\"windows-x64\":{\"do\":\"manual\",\"url\":\"https://dolphin-emu.org/download/\"}"),
        manifest("noplat", "No Platform", "\"linux-x64\":{\"do\":\"github-release\",\"repo\":\"a/b\",\"tag\":\"v1\",\"asset\":\"x\"}"),
        manifest("emptyrepo", "Empty Repo", "\"windows-x64\":{\"do\":\"github-release\",\"repo\":\"\",\"tag\":\"v1\",\"asset\":\"x\"}"),
        manifest("notag", "No Tag", "\"windows-x64\":{\"do\":\"github-release\",\"repo\":\"a/b\",\"asset\":\"x\"}"),
        Json::parse("{\"id\":\"bare\"}"),
        Json::object()};
    const auto reports = ac::update::checkAll(manifests, "windows-x64", fetch);
    QCOMPARE(reports.size(), manifests.size());
    for (const auto &r : reports) {
      QCOMPARE(r.state, Report::State::Unknown);
      QVERIFY(!r.text.isEmpty());
    }
    QVERIFY2(net.asked.isEmpty(), qPrintable(net.asked.join(", ")));
    QVERIFY(reports[0].text.contains("rolling"));
    QVERIFY(reports[1].text.contains("official page"));
    QVERIFY(reports[2].text.contains("official page"));
    QVERIFY(reports[3].text.contains("no download for this PC"));
  }

  // Every request is a GET of one GitHub API URL over https, nothing is written back, and the manifest
  // is left exactly as it was: the module reports and does nothing else.
  void onlyGithubApiRequestsAndNothingIsChanged() {
    Github net;
    net.pages[api + "flyinghead/flycast/releases/latest"] = release("v2.8", "2026-10-01T00:00:00Z");
    net.pages[api + "trzy/Supermodel/releases/latest"] = release("v0.3a-20261101-git-1234567", "2026-11-01T00:00:00Z");
    net.pages[api + "trzy/Supermodel/releases/tags/v0.3a-20260928-git-8b4de23"] = release("v0.3a-20260928-git-8b4de23", "2026-09-28T00:00:00Z");
    const QList<Json> manifests{flycast(), supermodel()};
    const auto before = manifests[0].dump() + manifests[1].dump();
    const auto reports = ac::update::checkAll(manifests, "windows-x64", net.fetcher());
    QCOMPARE(reports.size(), 2);
    QCOMPARE(reports[0].state, Report::State::Available);
    QCOMPARE(reports[1].state, Report::State::Available);
    QCOMPARE(net.asked.size(), 3);
    for (const auto &url : net.asked) {
      QVERIFY2(url.startsWith("https://api.github.com/repos/"), qPrintable(url));
      QVERIFY2(!url.contains('?') && !url.contains('#') && !url.contains('@'), qPrintable(url));
    }
    QCOMPARE(manifests[0].dump() + manifests[1].dump(), before);
  }
};
QTEST_GUILESS_MAIN(UpdateTest)
#include "UpdateTest.moc"
