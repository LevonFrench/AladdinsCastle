// SPDX-License-Identifier: GPL-3.0-only
#include "ArtifactStore.h"
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QStorageInfo>
#include <QTimer>
#include <QUrl>
namespace ac::install {
namespace {
class NoCookies : public QNetworkCookieJar {
  QList<QNetworkCookie> cookiesForUrl(const QUrl &) const override {
    return {};
  }
  bool setCookiesFromUrl(const QList<QNetworkCookie> &, const QUrl &) override {
    return false;
  }
};
bool digest(const QString &s) {
  return QRegularExpression("^[a-fA-F0-9]{64}$").match(s).hasMatch();
}
} // namespace
QSet<QString> ArtifactStore::globalHosts() {
  return {"github.com",
          "api.github.com",
          "release-assets.githubusercontent.com",
          "objects.githubusercontent.com",
          "downloads.sourceforge.net",
          "supermodel3.com",
          "www.supermodel3.com",
          "pcsx2.net",
          "www.pcsx2.net"};
}
void ArtifactStore::validateUrl(const QString &input,
                                const QStringList &hosts) {
  const QUrl url(input);
  if (!url.isValid() || url.scheme() != "https")
    throw Error("E_HTTP_NOT_ALLOWED", "HTTPS required");
  if (!url.userInfo().isEmpty() || url.hasFragment())
    throw Error("E_PLAN_INVALID", "Credentials and fragments forbidden in URL");
  if (!globalHosts().contains(url.host().toLower()) ||
      (!hosts.isEmpty() && !hosts.contains(url.host().toLower())))
    throw Error("E_HOST_NOT_ALLOWED", "Unlisted artifact host: " + url.host());
}
ArtifactStore::ArtifactStore(QString root, Options options)
    : root_(std::move(root)), options_(std::move(options)) {
  scopedPath("user/cache", root_);
  scopedPath("user/cache/artifacts", root_);
}
ArtifactStore::ArtifactStore(QString root, Options options,
                             const TestTrust &trust)
    : root_(std::move(root)), options_(std::move(options)), trust_(trust) {
  scopedPath("user/cache", root_);
  scopedPath("user/cache/artifacts", root_);
}
Json ArtifactStore::request(const QString &input, const QString &method,
                            const QString &destination,
                            const QByteArray &validator, qint64 offset,
                            qint64 maxBytes) {
  if (!destination.isEmpty())
    scopedPath(destination, root_);
  QUrl url(input);
  if (!trust_.baseUrl.isEmpty())
    url = QUrl(trust_.baseUrl + url.path());
  QNetworkAccessManager manager;
  manager.setCookieJar(new NoCookies);
  for (int hop = 0; hop <= 5; ++hop) {
    if (trust_.hosts.isEmpty())
      validateUrl(url.toString(), perHosts_);
    else if (url.scheme() != "https" || !url.userInfo().isEmpty() ||
             !trust_.hosts.contains(url.host()))
      throw Error("E_HOST_NOT_ALLOWED", "Test transport host refused");
    const auto backoffPath = root_ + "/user/cache/github.json";
    Json backoff = Json::object();
    if (QFileInfo::exists(backoffPath))
      try {
        backoff = Json::parse(readBytes(backoffPath).toStdString());
      } catch (const std::exception &) {
      }
    const auto hostState = backoff.value("_hosts", Json::object())
                               .value(url.host().toStdString(), Json::object());
    if (hostState.value("rate_limited_until", int64_t(0)) >
        QDateTime::currentSecsSinceEpoch())
      throw Error("E_RATE_LIMITED", "Artifact host backoff active");
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("User-Agent", "AladdinsCastle-Hub/0.1");
    req.setTransferTimeout(120000);
    auto ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    if (!trust_.authorities.isEmpty()) {
      auto ca = ssl.caCertificates();
      ca.append(trust_.authorities);
      ssl.setCaCertificates(ca);
    }
    req.setSslConfiguration(ssl);
    if (offset > 0) {
      req.setRawHeader("Range", "bytes=" + QByteArray::number(offset) + "-");
      if (!validator.isEmpty())
        req.setRawHeader("If-Range", validator);
    } else if (destination.isEmpty() && !validator.isEmpty())
      req.setRawHeader("If-None-Match", validator);
    QNetworkReply *reply =
        method == "HEAD" ? manager.head(req) : manager.get(req);
    QEventLoop loop;
    QTimer stall, cancelTimer;
    stall.setSingleShot(true);
    stall.start(30000);
    cancelTimer.start(50);
    QObject::connect(&stall, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(&cancelTimer, &QTimer::timeout, reply, [&] {
      if (options_.cancel && options_.cancel->load())
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QByteArray body;
    QFile file(destination);
    bool opened = false, tooLarge = false, writeFailed = false;
    qint64 received = 0;
    auto drain = [&] {
      const auto status =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (status != 200 && status != 206)
        return;
      if (!destination.isEmpty() && !opened) {
        const bool resume =
            status == 206 && offset > 0 &&
            reply->rawHeader("Content-Range")
                .startsWith("bytes " + QByteArray::number(offset) + "-") &&
            (!validator.isEmpty() &&
             (reply->rawHeader("ETag") == validator ||
              reply->rawHeader("Last-Modified") == validator));
        if (status == 206 && !resume) {
          writeFailed = true;
          reply->abort();
          return;
        }
        QDir().mkpath(QFileInfo(destination).absolutePath());
        opened = file.open(QIODevice::WriteOnly |
                           (resume ? QIODevice::Append : QIODevice::Truncate));
        if (!opened) {
          writeFailed = true;
          reply->abort();
          return;
        }
        if (resume)
          received = offset;
        const auto etag = reply->rawHeader("ETag"),
                   modified = reply->rawHeader("Last-Modified");
        if (!etag.isEmpty() || !modified.isEmpty())
          atomicWrite(destination + ".validator",
                      etag.isEmpty() ? modified : etag);
      }
      while (reply->bytesAvailable()) {
        const auto chunk = reply->read(128 * 1024);
        received += chunk.size();
        if (received > maxBytes) {
          tooLarge = true;
          reply->abort();
          return;
        }
        if (destination.isEmpty())
          body += chunk;
        else if (file.write(chunk) != chunk.size()) {
          writeFailed = true;
          reply->abort();
          return;
        }
      }
      stall.start(120000);
    };
    QObject::connect(reply, &QNetworkReply::readyRead, reply, drain);
    loop.exec();
    drain();
    if (opened) {
      file.flush();
      file.close();
    }
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto target =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    const auto error = reply->error();
    const auto etag = reply->rawHeader("ETag"),
               modified = reply->rawHeader("Last-Modified"),
               ctype = reply->rawHeader("Content-Type");
    Json result{{"status", status},
                {"body", body.toStdString()},
                {"etag", etag.toStdString()},
                {"modified", modified.toStdString()},
                {"type", ctype.toStdString()},
                {"retry_after", reply->rawHeader("Retry-After").toStdString()},
                {"reset", reply->rawHeader("x-ratelimit-reset").toStdString()}};
    delete reply;
    if (tooLarge)
      throw Error("E_ARTIFACT_TOO_LARGE", "Download exceeds size cap");
    if (writeFailed)
      throw Error("E_WRITE_DENIED",
                  "Cannot write download or invalid range response");
    if (options_.cancel && options_.cancel->load())
      throw Error("E_CANCELLED", "Download cancelled");
    if (status >= 300 && status < 400 && status != 304 && !target.isEmpty()) {
      url = url.resolved(target);
      continue;
    }
    if (status == 403 || status == 429) {
      const auto now = QDateTime::currentSecsSinceEpoch();
      bool ok = false;
      const auto delay = string(result, "retry_after").toLongLong(&ok);
      const auto reset = string(result, "reset").toLongLong();
      backoff["_hosts"][url.host().toStdString()] = Json{
          {"rate_limited_until", ok ? now + delay : std::max(now + 60, reset)}};
      atomicWrite(backoffPath, QByteArray::fromStdString(backoff.dump()));
      return result;
    }
    if (status == 404)
      throw Error("E_RELEASE_NOT_FOUND", "Pinned release not found");
    if (error != QNetworkReply::NoError)
      throw Error(
          "E_NETWORK",
          "HTTPS request failed (TLS, connection, timeout or truncated body)");
    if (status == 304 && !destination.isEmpty())
      throw Error("E_NETWORK", "304 is invalid for an artifact download");
    if (status != 200 && status != 206 && status != 304)
      throw Error("E_NETWORK", "Unexpected HTTP response");
    if (!destination.isEmpty() && (!etag.isEmpty() || !modified.isEmpty()))
      atomicWrite(destination + ".validator", etag.isEmpty() ? modified : etag);
    return result;
  }
  throw Error("E_NETWORK", "Too many redirects");
}
Json ArtifactStore::githubRelease(const QString &repo, const QString &tag) {
  const auto path = root_ + "/user/cache/github.json";
  Json cache = Json::object();
  if (QFileInfo::exists(path))
    try {
      cache = Json::parse(readBytes(path).toStdString());
    } catch (const std::exception &) {
    }
  const auto key = (repo + "/" + tag).toStdString();
  auto saved = cache.value(key, Json::object());
  const auto now = QDateTime::currentSecsSinceEpoch();
  if (saved.value("checked", int64_t(0)) + 21600 > now ||
      saved.value("rate_limited_until", int64_t(0)) > now) {
    if (saved.contains("release"))
      return saved["release"];
    throw Error("E_RATE_LIMITED", "GitHub rate limit active");
  }
  try {
    const auto response = request(
        "https://api.github.com/repos/" + repo + "/releases/tags/" + tag, "GET",
        {}, string(saved, "etag").toUtf8(), 0, 4 * 1024 * 1024);
    const auto status = response.value("status", 0);
    if (status == 403 || status == 429) {
      bool ok = false;
      const auto delay = string(response, "retry_after").toLongLong(&ok);
      const auto reset = string(response, "reset").toLongLong();
      saved["rate_limited_until"] =
          ok ? now + delay : std::max(now + 60, reset);
      cache[key] = saved;
      atomicWrite(path, QByteArray::fromStdString(cache.dump()));
      if (saved.contains("release"))
        return saved["release"];
      throw Error("E_RATE_LIMITED", "GitHub rate limited");
    }
    if (status != 304) {
      saved["release"] = Json::parse(response["body"].get<std::string>());
      saved["etag"] = response["etag"];
    }
    saved["checked"] = now;
    cache[key] = saved;
    atomicWrite(path, QByteArray::fromStdString(cache.dump()));
    return saved["release"];
  } catch (const Error &) {
    if (saved.contains("release"))
      return saved["release"];
    throw;
  }
}
QString ArtifactStore::acquire(const Json &step, const Json &guard) {
  perHosts_.clear();
  for (const auto &h : step.value("hosts", Json::array()))
    perHosts_ << QString::fromStdString(h.get<std::string>());
  auto pin = string(step, "sha256").toLower();
  const bool record = step.value("record_sha256", false);
  auto url = string(step, "url");
  if (string(step, "do") == "github-release") {
    const auto repo = string(step, "repo"), tag = string(step, "tag"),
               asset = string(step, "asset");
    if (tag.isEmpty() || tag == "latest" || asset.contains('*') ||
        repo.isEmpty())
      throw Error("E_PLAN_INVALID", "Exact GitHub pin required");
    url = "https://github.com/" + repo + "/releases/download/" + tag + "/" +
          asset;
    contentGuard(asset, guard);
  } else
    contentGuard(string(step, "name", QFileInfo(QUrl(url).path()).fileName()),
                 guard);
  if (trust_.hosts.isEmpty())
    validateUrl(url);
  const auto pinsPath = root_ + "/user/cache/artifact-pins.toml";
  auto pins = readEnvelope(pinsPath);
  const auto key = sha256(url.toUtf8()).toStdString();
  if (pin.isEmpty() && record && pins.contains(key))
    pin = string(pins[key], "sha256");
  if (!digest(pin) && !record)
    throw Error("E_PLAN_INVALID", "SHA-256 pin required");
  const auto dir = root_ + "/user/cache/artifacts/";
  QDir().mkpath(dir);
  if (digest(pin))
    scopedPath(dir + pin, root_);
  if (digest(pin) && QFileInfo::exists(dir + pin)) {
    if (hashFile(dir + pin) == pin) {
      for (const auto &blocked : guard.value("sha256", Json::array()))
        if (blocked.is_string() &&
            QString::fromStdString(blocked.get<std::string>())
                    .compare(pin, Qt::CaseInsensitive) == 0)
          throw Error("E_CONTENT_GUARD", "Known content hash refused in cache");
      if (QSet<QString>{"zip", "7z", "tar", "tar.gz"}.contains(
              string(step, "archive")))
        Archive::inspect(dir + pin, {}, guard);
      return dir + pin;
    }
    if (!QFile::remove(dir + pin))
      throw Error("E_WRITE_DENIED",
                  "Cannot evict corrupt artifact cache entry");
  }
  const auto part = dir + (digest(pin) ? pin : sha256(url.toUtf8())) + ".part";
  scopedPath(part, root_);
  if (string(step, "do") == "github-release") {
    Json release;
    const auto unavailable = [&] {
      release = Json();
      if (options_.event) options_.event(QVariantMap{{"kind", "warn"}, {"text", "GitHub API cross-check unavailable; verifying the pinned artifact"}});
    };
    try {
      release = githubRelease(string(step, "repo"), string(step, "tag"));
      if (!release.is_object() || !release.contains("assets") || !release["assets"].is_array())
        throw Error("E_FORMAT", "Malformed GitHub cross-check response");
    } catch (const Error &e) {
      if (!QSet<QString>{"E_NETWORK", "E_RATE_LIMITED", "E_RELEASE_NOT_FOUND", "E_FORMAT", "E_HOST_NOT_ALLOWED"}.contains(e.code)) throw;
      unavailable();
    } catch (const Json::exception &) {
      unavailable();
    }
    Json matched = Json::array();
    for (const auto &asset : release.is_object() ? release.value("assets", Json::array()) : Json::array())
      if (string(asset, "name") == string(step, "asset"))
        matched.push_back(asset);
    if (!release.is_null() && matched.size() != 1)
      throw Error("E_ASSET_AMBIGUOUS",
                  "Pinned GitHub asset must match exactly once");
    const auto upstreamDigest = matched.empty() ? QString() : string(matched[0], "digest");
    if (!upstreamDigest.isEmpty() && !pin.isEmpty() &&
        upstreamDigest != "sha256:" + pin)
      throw Error("E_HASH_MISMATCH",
                  "GitHub asset digest differs from recipe pin");
  }
  const auto cap = std::min<int64_t>(
      step.value("max_bytes", int64_t(8LL * 1024 * 1024 * 1024)),
      8LL * 1024 * 1024 * 1024);
  const auto expected = step.value("bytes", int64_t(0));
  QStorageInfo disk(dir);
  if (expected && disk.bytesAvailable() < expected + expected / 2)
    throw Error("E_DISK_SPACE", "Insufficient download space");
  const auto offset = QFileInfo::exists(part) ? QFileInfo(part).size() : 0;
  if (offset == 0) QFile::remove(part + ".validator");
  const auto validator = QFileInfo::exists(part + ".validator")
                             ? readBytes(part + ".validator")
                             : QByteArray();
  const auto response = request(url, "GET", part, validator,
                                validator.isEmpty() ? 0 : offset, cap);
  if (response.value("status", 0) == 403 || response.value("status", 0) == 429)
    throw Error("E_RATE_LIMITED", "Artifact host rate limited");
  if (string(response, "type").contains("text/html"))
    throw Error("E_FORMAT", "HTML instead of artifact");
  if (expected && QFileInfo(part).size() != expected)
    throw Error("E_FORMAT", "Artifact size mismatch");
  const auto actual = hashFile(part);
  for (const auto &blocked : guard.value("sha256", Json::array()))
    if (blocked.is_string() &&
        QString::fromStdString(blocked.get<std::string>())
                .compare(actual, Qt::CaseInsensitive) == 0)
      throw Error("E_CONTENT_GUARD", "Known game content hash refused");
  if (!pin.isEmpty() && pin != actual) {
    QFile::remove(part);
    QFile::remove(part + ".validator");
    throw Error("E_HASH_MISMATCH", "Artifact SHA-256 differs from pin");
  }
  QFile f(part);
  f.open(QIODevice::ReadOnly);
  const auto magic = f.read(512);
  f.close();
  const auto kind = string(step, "archive", string(step, "kind"));
  if (magic.trimmed().toLower().startsWith("<!doctype html") ||
      magic.trimmed().toLower().startsWith("<html"))
    throw Error("E_FORMAT", "HTML artifact refused");
  if ((kind == "zip" && !magic.startsWith("PK\003\004")) ||
      (kind == "7z" &&
       !magic.startsWith(QByteArray::fromHex("377abcaf271c"))) ||
      (kind == "tar.gz" && !magic.startsWith(QByteArray::fromHex("1f8b"))) ||
      (kind == "exe" && !magic.startsWith("MZ")))
    throw Error("E_FORMAT", "Artifact magic differs from declared type");
  if (QSet<QString>{"zip", "7z", "tar", "tar.gz"}.contains(kind))
    Archive::inspect(part, {}, guard);
  if (record && pin.isEmpty()) {
    pins[key] =
        Json{{"url", url.toStdString()}, {"sha256", actual.toStdString()}};
    writeEnvelope(pinsPath, pins);
  }
  if (!QFileInfo::exists(dir + actual) && !QFile::rename(part, dir + actual))
    throw Error("E_WRITE_DENIED", "Cannot promote artifact cache");
  QFile::remove(part + ".validator");
  if (QFileInfo::exists(part)) QFile::remove(part);
  return dir + actual;
}
} // namespace ac::install
