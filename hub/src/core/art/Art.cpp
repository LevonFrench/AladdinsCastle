// SPDX-License-Identifier: GPL-3.0-only
#include "Art.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QLinearGradient>
#include <QPainter>
#include <QQuickTextureFactory>
#include <QRegularExpression>
#include <QtConcurrent>
namespace ac::art {
namespace {
QString text(const Json &j, const char *k, const QString &fallback = {}) {
  return j.contains(k) && j[k].is_string()
             ? QString::fromStdString(j[k].get<std::string>())
             : fallback;
}
QImage decode(const QString &path, QSize size, bool contain) {
  QImageReader r(path);
  r.setAutoTransform(true);
  auto original = r.size();
  if (!original.isValid() || original.width() > 32768 ||
      original.height() > 32768 ||
      qint64(original.width()) * original.height() > 100000000)
    return {};
  const auto scaled = original.scaled(
      size, contain ? Qt::KeepAspectRatio : Qt::KeepAspectRatioByExpanding);
  if (scaled.width() > 8192 || scaled.height() > 8192 ||
      qint64(scaled.width()) * scaled.height() > 16 * 1024 * 1024)
    return {};
  r.setScaledSize(scaled);
  const auto image = r.read();
  if (image.isNull())
    return {};
  if (!contain)
    return image.copy(qMax(0, (image.width() - size.width()) / 2),
                      qMax(0, (image.height() - size.height()) / 2),
                      size.width(), size.height());
  QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
  canvas.fill(Qt::transparent);
  QPainter painter(&canvas);
  painter.drawImage((size.width() - image.width()) / 2,
                    (size.height() - image.height()) / 2, image);
  return canvas;
}
QStringList extensions(const QString &base) {
  QStringList r;
  for (const auto &e : QStringList{"png", "jpg", "jpeg", "webp", "bmp"})
    r << base + "." + e;
  return r;
}
QStringList roles(const QString &kind) {
  if (kind == "portrait" || kind == "capsule")
    return {"portrait", "boxart", "flyer"};
  if (kind == "logo" || kind == "wheel")
    return {"logo", "wheel"};
  if (kind == "snap" || kind == "preview")
    return {"snap", "images"};
  if (kind == "hero" || kind == "fanart")
    return {"hero", "fanart", "banner", "marquee", "snap"};
  return {"banner", "marquee", "fanart", "snap", "images"};
}
QImage fallback(const GameRecord *g, const QString &id, const QSize &size,
                const QString &kind) {
  auto image = QImage(size, QImage::Format_ARGB32_Premultiplied);
  QColor base("#101418"), accent("#dd6600");
  QString title = id, manufacturer = "ALADDINSCASTLE";
  if (g) {
    title = text(g->raw, "title", id);
    manufacturer = text(g->raw, "manufacturer", manufacturer).toUpper();
    if (g->raw.contains("hub")) {
      base = QColor(text(g->raw["hub"], "colour", "#101418"));
      accent = QColor(text(g->raw["hub"], "accent", "#dd6600"));
    }
  }
  if (!base.isValid())
    base = QColor("#101418");
  if (!accent.isValid())
    accent = QColor("#dd6600");
  const bool logo = kind == "logo" || kind == "wheel";
  image.fill(logo ? QColor(Qt::transparent) : base);
  QPainter p(&image);
  p.setRenderHint(QPainter::Antialiasing);
  QLinearGradient gradient(0, 0, size.width(), size.height());
  gradient.setColorAt(0, base);
  gradient.setColorAt(1, accent.darker(350));
  if (!logo) {
    p.fillRect(image.rect(), gradient);
    p.fillRect(0, 0, qMax(4, size.width() / 70), size.height(), accent);
  }
  p.setPen(Qt::white);
  QFont font("Sans Serif");
  font.setBold(true);
  font.setPixelSize(qMax(14, qMin(size.width() / 13, size.height() / 5)));
  p.setFont(font);
  p.drawText(image.rect().adjusted(size.width() / 15, size.height() / 7,
                                   -size.width() / 15, -size.height() / 4),
             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, title);
  if (logo)
    return image;
  font.setPixelSize(qMax(9, size.height() / 19));
  p.setFont(font);
  p.setPen(accent.lighter(160));
  p.drawText(image.rect().adjusted(size.width() / 15, 0, -size.width() / 15,
                                   -size.height() / 10),
             Qt::AlignLeft | Qt::AlignBottom, manufacturer);
  return image;
}
class Response : public QQuickImageResponse {
public:
  Response(std::shared_ptr<Resolver> resolver, QString id, QString kind,
           QSize size) {
    QObject::connect(&watcher, &QFutureWatcher<QImage>::finished, this, [this] {
      if (!cancelled)
        image = watcher.result();
      emit finished();
    });
    watcher.setFuture(QtConcurrent::run([resolver, id, kind, size] {
      return resolver->resolve(id, kind, size).image;
    }));
  }
  ~Response() override { watcher.waitForFinished(); }
  QQuickTextureFactory *textureFactory() const override {
    return QQuickTextureFactory::textureFactoryForImage(image);
  }
  void cancel() override { cancelled = true; }

private:
  QFutureWatcher<QImage> watcher;
  QImage image;
  std::atomic_bool cancelled = false;
};
} // namespace
Resolver::Resolver(CatalogData catalog, ArtOptions options)
    : m_catalog(std::move(catalog)), m_options(std::move(options)) {
  QFile index(":/scan/serial-index.json");
  if (index.open(QIODevice::ReadOnly))
    try {
      const auto records = Json::parse(index.readAll().toStdString());
      for (auto it = records.begin(); it != records.end(); ++it)
        m_serials[text(it.value(), "gameId")]
            << QString::fromStdString(it.key());
    } catch (const std::exception &) {
    }
}
void Resolver::setBindings(const QVector<scan::Binding> &bindings) {
  QMutexLocker lock(&m_mutex);
  m_bindings = bindings;
}
void Resolver::setRoots(const QStringList &roots) {
  QMutexLocker lock(&m_mutex);
  m_options.roots = roots;
}
QString Resolver::normalizedTitle(QString title) {
  title.replace(QRegularExpression("[&*/:`<>?\\\\|\"]"), "_");
  return title.trimmed();
}
ArtResult Resolver::resolve(const QString &id, const QString &kind,
                            const QSize &requested) const {
  const QSize size = requested.isValid()
                         ? QSize(qBound(16, requested.width(), 4096),
                                 qBound(16, requested.height(), 4096))
                         : QSize(920, 430);
  const auto *game = m_catalog.find(id);
  QStringList candidates, sources;
  const auto add = [&](const QString &base, const QString &source) {
    for (const auto &p : extensions(base)) {
      candidates << p;
      sources << source;
    }
  };
  const auto roleNames = roles(kind);
  if (game) {
    if (!m_options.userRoot.isEmpty())
      for (const auto &role : roleNames)
        add(m_options.userRoot + "/art/" + id + "/" + role, "user");
    QVector<scan::Binding> bindings;
    QStringList roots;
    {
      QMutexLocker lock(&m_mutex);
      bindings = m_bindings;
      roots = m_options.roots;
    }
    QStringList keys, serials;
    for (const auto &b : bindings)
      if (b.gameId == id && b.verified) {
        roots << QFileInfo(b.path).absolutePath();
        roots << QFileInfo(b.path).absoluteDir().absolutePath();
        keys << QFileInfo(b.path).completeBaseName();
        if (b.proof.contains("crc"))
          keys << b.identity;
        else if (b.proof == "disc-serial" || b.proof == "chd-disc-serial")
          serials << b.identity;
      }
    if (game->raw.contains("media"))
      for (const auto &m : game->raw["media"])
        if (!text(m, "set").isEmpty())
          keys << text(m, "set");
    roots.removeDuplicates();
    keys.removeDuplicates();
    serials << m_serials.value(id);
    serials.removeDuplicates();
    for (const auto &root : roots) {
      for (const auto &role : roleNames)
        for (const auto &k : keys)
          add(root + "/" + role + "/" + k, "local");
      for (const auto &folder : QStringList{"images", "media"})
        for (const auto &k : keys) {
          for (const auto &role : roleNames)
            add(root + "/" + folder + "/" + k + "-" + role, "local");
          if (kind != "logo" && kind != "wheel")
            add(root + "/" + folder + "/" + k, "local");
        }
    }
    if (kind != "logo" && kind != "wheel") {
      const auto title = normalizedTitle(text(game->raw, "title", id));
      for (const auto &root : roots) {
        QDir thumb(root + "/thumbnails");
        if (QFileInfo(root).fileName().compare("thumbnails",
                                               Qt::CaseInsensitive) == 0)
          thumb = QDir(root);
        for (const auto &playlist :
             thumb.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
          for (const auto &folder :
               QStringList{"Named_Boxarts", "Named_Snaps", "Named_Titles"})
            add(thumb.filePath(playlist + "/" + folder + "/" + title),
                "retroarch");
      }
      for (const auto &root : roots) {
        const auto covers = QFileInfo(root).fileName().compare(
                                "covers", Qt::CaseInsensitive) == 0
                                ? root
                                : root + "/covers";
        for (const auto &serial : serials) {
          add(covers + "/" + serial, "pcsx2");
          auto compact = serial;
          compact.remove('-');
          add(covers + "/" + compact, "pcsx2");
          const auto parts =
              QRegularExpression("^([A-Z]{4})-([0-9]{3})([0-9]{2})$")
                  .match(serial);
          if (parts.hasMatch())
            add(covers + "/" + parts.captured(1) + "_" + parts.captured(2) +
                    "." + parts.captured(3),
                "pcsx2");
        }
      }
    }
  }
  for (qsizetype i = 0; i < candidates.size(); ++i) {
    QFileInfo info(candidates[i]);
    if (!info.isFile() || info.isSymLink())
      continue;
    const auto stamp =
        info.absoluteFilePath() + ":" + QString::number(info.size()) + ":" +
        QString::number(info.lastModified().toMSecsSinceEpoch()) + ":" +
        QString::number(size.width()) + "x" + QString::number(size.height()) +
        (kind == "logo" || kind == "wheel" ? ":v2-fit" : ":v2-crop");
    const auto cache =
        m_options.userRoot + "/cache/art/" +
        QString::fromLatin1(
            QCryptographicHash::hash(stamp.toUtf8(), QCryptographicHash::Sha256)
                .toHex()) +
        ".png";
    QImage image;
    if (!m_options.userRoot.isEmpty() && QFileInfo::exists(cache))
      image.load(cache);
    if (image.isNull())
      image = decode(info.absoluteFilePath(), size,
                     kind == "logo" || kind == "wheel");
    if (!image.isNull()) {
      if (!m_options.userRoot.isEmpty() && !QFileInfo::exists(cache)) {
        QDir().mkpath(m_options.userRoot + "/cache/art");
        image.save(cache, "PNG");
      }
      return {image, info.absoluteFilePath(), sources[i]};
    }
  }
  return {fallback(game, id, size, kind), {}, "generated"};
}
QQuickImageResponse *Provider::requestImageResponse(const QString &id,
                                                    const QSize &size) {
  auto parts = id.section('?', 0, 0).split('/');
  return new Response(m_resolver, parts.value(0), parts.value(1, "banner"),
                      size);
}
} // namespace ac::art
