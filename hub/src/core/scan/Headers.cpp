// SPDX-License-Identifier: GPL-3.0-only
#include "HeaderSafety.h"
#include "Scan.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QtEndian>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>
extern "C" {
#include <7z.h>
#include <7zCrc.h>
#include <libchdr/chd.h>
}
namespace ac::scan {
namespace {
quint16 u16(const QByteArray &b, qsizetype p) {
  return qFromLittleEndian<quint16>(b.constData() + p);
}
quint32 u32(const QByteArray &b, qsizetype p) {
  return qFromLittleEndian<quint32>(b.constData() + p);
}
quint64 u64(const QByteArray &b, qsizetype p) {
  return qFromLittleEndian<quint64>(b.constData() + p);
}
QByteArray readAt(QFile &f, qint64 pos, qint64 size) {
  if (pos < 0 || size < 0 || pos > f.size() || size > f.size() - pos ||
      !f.seek(pos))
    return {};
  return f.read(size);
}
void zip(QFile &f, FileIdentity &r, std::atomic_bool &cancel) {
  const auto tail = readAt(f, qMax(qint64(0), f.size() - 65557),
                           qMin(f.size(), qint64(65557)));
  qsizetype end = tail.lastIndexOf(QByteArray("PK\5\6", 4));
  if (end < 0 || end + 22 > tail.size() ||
      end + 22 + u16(tail, end + 20) != tail.size()) {
    r.error = "Invalid ZIP end directory";
    return;
  }
  if (u16(tail, end + 4) || u16(tail, end + 6)) {
    r.error = "Multi-volume ZIP unsupported";
    return;
  }
  quint64 count = u16(tail, end + 10), bytes = u32(tail, end + 12),
          offset = u32(tail, end + 16);
  if (count == 65535 || bytes == 0xffffffffU || offset == 0xffffffffU) {
    const qint64 absolute = qMax(qint64(0), f.size() - 65557) + end;
    auto locator = readAt(f, absolute - 20, 20);
    if (locator.size() != 20 || u32(locator, 0) != 0x07064b50 ||
        u32(locator, 4) != 0 || u32(locator, 16) != 1) {
      r.error = "Invalid ZIP64 locator";
      return;
    }
    auto z = readAt(f, static_cast<qint64>(u64(locator, 8)), 56);
    if (z.size() != 56 || u32(z, 0) != 0x06064b50 || u32(z, 16) || u32(z, 20)) {
      r.error = "Invalid ZIP64 directory";
      return;
    }
    count = u64(z, 32);
    bytes = u64(z, 40);
    offset = u64(z, 48);
  }
  if (bytes > 64 * 1024 * 1024 || count > 1000000 ||
      offset > static_cast<quint64>(f.size()) ||
      bytes > static_cast<quint64>(f.size()) - offset) {
    r.error = "ZIP directory exceeds bounded metadata limits";
    return;
  }
  auto cd = readAt(f, static_cast<qint64>(offset), static_cast<qint64>(bytes));
  qsizetype p = 0;
  for (quint64 i = 0; i < count && !cancel; ++i) {
    if (p + 46 > cd.size() || u32(cd, p) != 0x02014b50) {
      r.error = "Invalid ZIP central entry";
      break;
    }
    auto nameLen = u16(cd, p + 28), extraLen = u16(cd, p + 30),
         commentLen = u16(cd, p + 32);
    if (p + 46 + nameLen + extraLen + commentLen > cd.size()) {
      r.error = "Truncated ZIP central entry";
      break;
    }
    QString name = QString::fromUtf8(cd.mid(p + 46, nameLen));
    quint64 size = u32(cd, p + 24);
    if (size == 0xffffffffU) {
      qsizetype e = p + 46 + nameLen, limit = e + extraLen;
      bool found = false;
      while (e + 4 <= limit) {
        auto type = u16(cd, e), len = u16(cd, e + 2);
        e += 4;
        if (e + len > limit)
          break;
        if (type == 1 && len >= 8) {
          size = u64(cd, e);
          found = true;
          break;
        }
        e += len;
      }
      if (!found) {
        r.error = "Missing ZIP64 size";
        break;
      }
    }
    if (!name.endsWith('/'))
      r.entries.push_back({name, u32(cd, p + 16), size, true});
    p += 46 + nameLen + extraLen + commentLen;
  }
  if (!r.error.isEmpty())
    r.entries.clear();
}
struct Input {
  ISeekInStream vt;
  QFile *file;
  std::atomic_bool *cancel;
  quint64 read = 0;
  static SRes Read(ISeekInStreamPtr v, void *buf, size_t *size) {
    auto *s = const_cast<Input *>(reinterpret_cast<const Input *>(v));
    if (*s->cancel || *size > 64 * 1024 * 1024 ||
        s->read + *size > 64 * 1024 * 1024)
      return SZ_ERROR_FAIL;
    auto n =
        s->file->read(static_cast<char *>(buf), static_cast<qint64>(*size));
    if (n < 0)
      return SZ_ERROR_READ;
    *size = static_cast<size_t>(n);
    s->read += *size;
    return SZ_OK;
  }
  static SRes Seek(ISeekInStreamPtr v, Int64 *pos, ESzSeek origin) {
    auto *s = const_cast<Input *>(reinterpret_cast<const Input *>(v));
    const qint64 base = origin == SZ_SEEK_CUR   ? s->file->pos()
                        : origin == SZ_SEEK_END ? s->file->size()
                                                : 0;
    if (*s->cancel || *pos < -base || *pos > s->file->size() - base ||
        !s->file->seek(base + *pos))
      return SZ_ERROR_FAIL;
    *pos = s->file->pos();
    return SZ_OK;
  }
};
struct Budget {
  ISzAlloc vt;
  size_t used = 0;
  std::atomic_bool *cancel = nullptr;
  QHash<const void *, size_t> *allocations = nullptr;
};
void *allocate(ISzAllocPtr v, size_t bytes) {
  auto *budget = const_cast<Budget *>(reinterpret_cast<const Budget *>(v));
  if (*budget->cancel || !bytes || bytes > 64 * 1024 * 1024 ||
      bytes > 128 * 1024 * 1024 - budget->used ||
      budget->allocations->size() >= 4096)
    return nullptr;
  auto *p = static_cast<char *>(std::malloc(bytes + sizeof(std::max_align_t)));
  if (!p)
    return nullptr;
  std::memcpy(p, &bytes, sizeof bytes);
  budget->used += bytes;
  budget->allocations->insert(p + sizeof(std::max_align_t), bytes);
  return p + sizeof(std::max_align_t);
}
void release(ISzAllocPtr v, void *p) {
  if (!p)
    return;
  auto *budget = const_cast<Budget *>(reinterpret_cast<const Budget *>(v));
  auto *base = static_cast<char *>(p) - sizeof(std::max_align_t);
  size_t bytes = 0;
  std::memcpy(&bytes, base, sizeof bytes);
  budget->used -= bytes;
  budget->allocations->remove(p);
  std::free(base);
}
void seven(QFile &f, FileIdentity &r, std::atomic_bool &cancel) {
  f.seek(0);
  static std::once_flag once;
  std::call_once(once, [] { CrcGenerateTable(); });
  Input in{{Input::Read, Input::Seek}, &f, &cancel};
  CLookToRead2 look{};
  LookToRead2_CreateVTable(&look, False);
  Byte buffer[32768];
  look.buf = buffer;
  look.bufSize = sizeof buffer;
  look.realStream = &in.vt;
  LookToRead2_INIT(&look) QHash<const void *, size_t> allocations;
  Budget budget{{allocate, release}, 0, &cancel, &allocations};
  const auto *alloc = &budget.vt;
  CSzArEx db;
  SzArEx_Init(&db);
  const auto code = SzArEx_Open(&db, &look.vt, alloc, alloc);
  if (code == SZ_OK) {
    const auto allocated = [&](const void *p) {
      return allocations.value(p, 0);
    };
    const quint64 count = db.NumFiles;
    if (count > 1000000 ||
        (count &&
         (allocated(db.FileNameOffsets) < (count + 1) * sizeof(size_t) ||
          !allocated(db.FileNames) ||
          allocated(db.UnpackPositions) < (count + 1) * sizeof(UInt64) ||
          allocated(db.IsDirs) < (count + 7) / 8 ||
          (db.CRCs.Defs &&
           (allocated(db.CRCs.Defs) < (count + 7) / 8 ||
            allocated(db.CRCs.Vals) < count * sizeof(UInt32))))))
      r.error = "7z member metadata or names missing; archive unverified";
    for (UInt32 i = 0; i < db.NumFiles && !cancel; ++i) {
      if (!r.error.isEmpty())
        break;
      if (SzArEx_IsDir(&db, i))
        continue;
      const auto first = db.FileNameOffsets[i], end = db.FileNameOffsets[i + 1];
      if (end <= first || end > allocated(db.FileNames) / 2 ||
          end - first > 32768 ||
          db.UnpackPositions[i + 1] < db.UnpackPositions[i]) {
        r.error = "7z member bounds invalid; archive unverified";
        break;
      }
      const auto n = SzArEx_GetFileNameUtf16(&db, i, nullptr);
      if (n != end - first || n <= 1 || n > 32768) {
        r.error = "7z name exceeds limit";
        break;
      }
      QVector<UInt16> name(static_cast<qsizetype>(n));
      SzArEx_GetFileNameUtf16(&db, i, name.data());
      if (name.back() != 0) {
        r.error = "7z member name not terminated";
        break;
      }
      const bool crc = SzBitWithVals_Check(&db.CRCs, i);
      r.entries.push_back(
          {QString::fromUtf16(
               reinterpret_cast<const char16_t *>(name.constData())),
           crc ? db.CRCs.Vals[i] : 0, SzArEx_GetFileSize(&db, i), crc});
    }
  } else
    r.error = QString("7z metadata open failed (%1); encrypted or unsupported "
                      "headers remain unverified")
                  .arg(code);
  SzArEx_Free(&db, alloc);
  if (!r.error.isEmpty())
    r.entries.clear();
}
QString serial(const QByteArray &b) {
  const QRegularExpression re("(?:BOOT2?|BOOT)\\s*=\\s*[^\\r\\n]*?([A-Z]{4})[_-"
                              "]([0-9]{3})\\.([0-9]{2})",
                              QRegularExpression::CaseInsensitiveOption);
  auto m = re.match(QString::fromLatin1(b));
  return m.hasMatch()
             ? m.captured(1).toUpper() + "-" + m.captured(2) + m.captured(3)
             : QString();
}
using Reader = std::function<QByteArray(qint64, qint64)>;
QString isoReader(const Reader &read, std::atomic_bool &cancel) {
  // ISO9660 root descriptor, followed only by the bounded root directory and
  // SYSTEM.CNF.
  int stride = 0, payload = 0;
  for (const auto &layout : QVector<QPair<int, int>>{
           {2048, 0}, {2352, 16}, {2352, 24}, {2448, 16}, {2448, 24}}) {
    auto pvd = read(qint64(16) * layout.first + layout.second, 2048);
    if (pvd.size() == 2048 && pvd.mid(1, 5) == "CD001") {
      stride = layout.first;
      payload = layout.second;
      break;
    }
  }
  if (!stride)
    return {};
  const auto logical = [&](quint32 sector, quint32 bytes) {
    QByteArray out;
    for (quint32 done = 0; done < bytes && !cancel; done += 2048) {
      const auto n = qMin(quint32(2048), bytes - done);
      const auto block =
          read(static_cast<qint64>((quint64(sector) + done / 2048) *
                                       quint64(stride) +
                                   quint64(payload)),
               n);
      if (block.size() != static_cast<qsizetype>(n))
        return QByteArray();
      out += block;
    }
    return out;
  };
  for (int sector = 16; sector < 48 && !cancel; ++sector) {
    auto pvd = logical(quint32(sector), 2048);
    if (pvd.size() != 2048 || pvd.mid(1, 5) != "CD001")
      return {};
    if (static_cast<unsigned char>(pvd[0]) == 255)
      return {};
    if (pvd[0] != char(1))
      continue;
    const quint32 extent = u32(pvd, 158), size = u32(pvd, 166);
    if (size > 2 * 1024 * 1024)
      return {};
    auto dir = logical(extent, size);
    for (qsizetype p = 0; p < dir.size() && !cancel;) {
      const auto len = static_cast<unsigned char>(dir[p]);
      if (!len) {
        p = ((p / 2048) + 1) * 2048;
        continue;
      }
      if (len < 34 || p + len > dir.size())
        return {};
      const auto nameLen = static_cast<unsigned char>(dir[p + 32]);
      if (33 + nameLen > len)
        return {};
      if (dir.mid(p + 33, nameLen).toUpper().startsWith("SYSTEM.CNF")) {
        auto n = u32(dir, p + 10);
        if (n > 65536)
          return {};
        return serial(logical(u32(dir, p + 2), n));
      }
      p += len;
    }
    return {};
  }
  return {};
}
QString iso(QFile &f, std::atomic_bool &cancel) {
  return isoReader(
      [&](qint64 offset, qint64 bytes) { return readAt(f, offset, bytes); },
      cancel);
}
struct ChdInput {
  QFile *file;
  std::atomic_bool *cancel;
  quint64 bytes = 0;
  static uint64_t Size(void *value) {
    return static_cast<uint64_t>(static_cast<ChdInput *>(value)->file->size());
  }
  static size_t Read(void *output, size_t unit, size_t count, void *value) {
    auto *in = static_cast<ChdInput *>(value);
    if (*in->cancel || unit == 0 || count > 64 * 1024 * 1024 / unit ||
        in->bytes + unit * count > 64 * 1024 * 1024)
      return 0;
    auto n = in->file->read(static_cast<char *>(output),
                            static_cast<qint64>(unit * count));
    if (n <= 0)
      return 0;
    in->bytes += static_cast<quint64>(n);
    return static_cast<size_t>(n) / unit;
  }
  static int Close(void *) { return 0; }
  static int Seek(void *value, int64_t offset, int origin) {
    auto *in = static_cast<ChdInput *>(value);
    const qint64 base = origin == SEEK_CUR   ? in->file->pos()
                        : origin == SEEK_END ? in->file->size()
                                             : 0;
    if (*in->cancel || offset < -base || offset > in->file->size() - base)
      return -1;
    return in->file->seek(base + offset) ? 0 : -1;
  }
};
QString chdSerial(QFile &f, const QByteArray &header, std::atomic_bool &cancel,
                  QString &error) {
  const auto logical = qFromBigEndian<quint64>(header.constData() + 32),
             mapOffset = qFromBigEndian<quint64>(header.constData() + 40);
  const auto hunk = qFromBigEndian<quint32>(header.constData() + 56),
             unit = qFromBigEndian<quint32>(header.constData() + 60);
  // Validate every allocation-driving dimension before libchdr materializes the
  // map.
  if (hunk < 2048 || hunk > 1024 * 1024 || unit == 0 || hunk % unit ||
      logical == 0 || logical > 100ULL * 1024 * 1024 * 1024 ||
      (logical + hunk - 1) / hunk >
          (32ULL * 1024 * 1024) / (qFromBigEndian<quint32>(header.constData() + 16) ? 12 : 4) ||
      mapOffset > static_cast<quint64>(f.size())) {
    error = "CHD dimensions exceed bounded metadata limits";
    return {};
  }
  if (header.mid(104, 20) != QByteArray(20, 0)) {
    error = "CHD parent required; sparse scan remains unverified";
    return {};
  }
  if (mapOffset) {
    auto map = readAt(f, static_cast<qint64>(mapOffset), 16);
    if (map.size() == 16 &&
        qFromBigEndian<quint32>(header.constData() + 16) != 0 &&
        qFromBigEndian<quint32>(map.constData()) > 32 * 1024 * 1024) {
      error = "CHD map exceeds metadata budget";
      return {};
    }
  }
  f.seek(0);
  ChdInput input{&f, &cancel};
  const core_file_callbacks callbacks{ChdInput::Size, ChdInput::Read,
                                      ChdInput::Close, ChdInput::Seek};
  chd_file *chd = nullptr;
  const auto code = chd_open_core_file_callbacks(&callbacks, &input,
                                                 CHD_OPEN_READ, nullptr, &chd);
  if (code != CHDERR_NONE) {
    error = QString("CHD sparse open failed (%1)").arg(static_cast<int>(code));
    return {};
  }
  QByteArray cache(static_cast<qsizetype>(hunk), 0);
  quint32 last = 0xffffffffU;
  quint64 decoded = 0;
  int decodedHunks = 0;
  const auto read = [&](qint64 offset, qint64 bytes) {
    QByteArray out;
    if (offset < 0 || bytes < 0 || bytes > 2 * 1024 * 1024 ||
        static_cast<quint64>(offset) > logical ||
        static_cast<quint64>(bytes) > logical - static_cast<quint64>(offset))
      return out;
    while (bytes > 0 && !cancel) {
      const auto number =
                     static_cast<quint32>(static_cast<quint64>(offset) / hunk),
                 position =
                     static_cast<quint32>(static_cast<quint64>(offset) % hunk);
      if (number != last) {
        if (decoded + hunk > 8 * 1024 * 1024 || ++decodedHunks > 128) {
          error = "CHD decoded-header budget exceeded";
          return QByteArray();
        }
        decoded += hunk;
        const auto *map = chd_get_header(chd);
        quint32 terminal = number;
        if (!map || map->version != 5 ||
            !detail::resolveChdHunk(
                map->rawmap, quint64(map->hunkcount) * map->mapentrybytes,
                map->hunkcount, map->mapentrybytes, map->compression[0] != 0,
                number, terminal, cancel, error))
          return QByteArray();
        if (chd_read(chd, terminal, cache.data()) != CHDERR_NONE) {
          error = "CHD header-sector decode failed";
          return QByteArray();
        }
        last = number;
      }
      const auto n = qMin(bytes, qint64(hunk - position));
      out += cache.mid(position, n);
      offset += n;
      bytes -= n;
    }
    return out;
  };
  const auto identity = isoReader(read, cancel);
  chd_close(chd);
  return identity;
}
} // namespace
FileIdentity Scanner::inspect(const QString &path, std::atomic_bool &cancel) {
  QFileInfo info(path);
  FileIdentity r;
  r.path = info.absoluteFilePath();
  r.size = info.size();
  r.mtime = info.lastModified().toMSecsSinceEpoch();
  if (info.suffix().compare("rom1", Qt::CaseInsensitive) == 0) {
    r.kind = "ps2-bios-component";
    return r;
  }
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    r.error = "Unreadable media";
    return r;
  }
  auto header = readAt(f, 0, qMin(f.size(), qint64(4096)));
  for (const auto offset : {16, 24})
    if (header.mid(offset).startsWith("SEGA SEGAKATANA") ||
        header.mid(offset).startsWith("SEGA SEGASATURN ")) {
      header = header.mid(offset);
      break;
    }
  const auto ext = info.suffix().toLower();
  if (ext == "zip") {
    r.kind = "archive-crc";
    zip(f, r, cancel);
  } else if (ext == "7z") {
    r.kind = "archive-crc";
    seven(f, r, cancel);
  } else if (header.startsWith("MComprHD") && header.size() >= 124 &&
             qFromBigEndian<quint32>(header.constData() + 12) == 5 &&
             qFromBigEndian<quint32>(header.constData() + 8) == 124) {
    r.kind = "chd-sha1";
    r.identity = QString::fromLatin1(header.mid(84, 20).toHex());
    r.chdHeaderSha1 = r.identity;
    if (header.mid(84, 20) == QByteArray(20, '\0'))
      r.identity.clear();
    QString sparseError;
    const auto discSerial = chdSerial(f, header, cancel, sparseError);
    if (!discSerial.isEmpty()) {
      r.kind = "chd-disc-serial";
      r.identity = discSerial;
    } else if (!sparseError.isEmpty())
      r.error = sparseError;
  } else if (header.startsWith("SEGA SEGAKATANA") && header.size() >= 256) {
    r.kind = "disc-id";
    r.identity = QString::fromLatin1(header.mid(64, 10)).trimmed();
  } else if (header.startsWith("SEGA SEGASATURN ") && header.size() >= 256) {
    r.kind = "disc-id";
    r.identity = QString::fromLatin1(header.mid(32, 10)).trimmed();
  } else if (header.size() >= 32 &&
             (qFromBigEndian<quint32>(header.constData() + 28) == 0xc2339f3d ||
              qFromBigEndian<quint32>(header.constData() + 24) == 0x5d1c9ea3)) {
    r.kind = "disc-id";
    r.identity = QString::fromLatin1(header.left(6));
  } else if (ext == "iso" || ext == "bin" || ext == "img") {
    r.kind = "disc-serial";
    r.identity = iso(f, cancel);
  }
  if (r.identity.isEmpty() && (ext == "bin" || ext == "rom" || ext == "rom0") &&
      r.size >= 512 * 1024 && r.size <= 8 * 1024 * 1024) {
    // PS2 ROMDIR structures identify a BIOS, never a basename alone.
    auto b = readAt(f, 0, qMin(f.size(), qint64(512 * 1024)));
    qsizetype start = b.indexOf(QByteArray("RESET\0\0\0\0\0", 10));
    if (start >= 0) {
      bool romdir = false, extinfo = false;
      quint64 offset = 0;
      for (qsizetype p = start; p + 16 <= b.size() && p < start + 4096;
           p += 16) {
        auto name = b.mid(p, 10);
        if (name[0] == '\0')
          break;
        if (name.startsWith("ROMDIR"))
          romdir = true;
        if (name.startsWith("EXTINFO"))
          extinfo = true;
        auto size = u32(b, p + 12);
        if (name.startsWith("ROMVER") && romdir && extinfo && size >= 14) {
          auto version = readAt(f, static_cast<qint64>(offset), 14);
          if (QRegularExpression("^[0-9]{4}[A-Z]{2}[0-9]{8}$")
                  .match(QString::fromLatin1(version))
                  .hasMatch()) {
            r.kind = "ps2-bios-romdir";
            r.identity = QString::fromLatin1(version);
          }
          break;
        }
        offset += (quint64(size) + 15) & ~quint64(15);
      }
    }
  }
  if (r.kind.isEmpty())
    r.error = "Unsupported header";
  return r;
}
} // namespace ac::scan
