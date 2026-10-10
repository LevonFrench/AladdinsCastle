// SPDX-License-Identifier: GPL-3.0-only
#include "Steam.h"
#include "core/install/Support.h"
#include <QtEndian>
namespace ac::steam {
namespace {
using install::Error;
class Reader {
public:
  const QByteArray &bytes;
  qsizetype pos = 0;
  int nodes = 0;
  quint8 byte() {
    if (pos >= bytes.size())
      throw Error("E_VDF", "Truncated binary VDF");
    return static_cast<quint8>(bytes[pos++]);
  }
  QByteArray take(qsizetype n) {
    if (n < 0 || n > bytes.size() - pos)
      throw Error("E_VDF", "Truncated binary VDF payload");
    const auto b = bytes.mid(pos, n);
    pos += n;
    return b;
  }
  QByteArray string() {
    const auto end = bytes.indexOf('\0', pos);
    if (end < 0 || end - pos > 1024 * 1024)
      throw Error("E_VDF", "Unterminated or oversized VDF string");
    const auto b = bytes.mid(pos, end - pos);
    pos = end + 1;
    return b;
  }
  QVector<Node> list(quint8 &terminator, int depth) {
    if (depth > 32)
      throw Error("E_VDF", "Binary VDF nesting limit");
    QVector<Node> out;
    while (true) {
      const auto start = pos;
      Node n;
      n.type = byte();
      if (n.type == 8 || n.type == 11) {
        terminator = n.type;
        return out;
      }
      if (++nodes > 100000)
        throw Error("E_VDF", "Binary VDF entry limit");
      n.key = string();
      switch (n.type) {
      case 0:
        n.children = list(n.terminator, depth + 1);
        break;
      case 1:
        n.payload = string();
        break;
      case 2:
      case 3:
      case 4:
      case 6:
        n.payload = take(4);
        break;
      case 5: {
        // UTF-16 NUL terminator, aligned to code units; no length prefix.
        for (qsizetype units = 0;; ++units) {
          if (units >= 512 * 1024)
            throw Error("E_VDF", "Oversized VDF wide string");
          const auto unit = take(2);
          n.payload += unit;
          if (unit == QByteArray(2, '\0'))
            break;
        }
        break;
      }
      case 7:
      case 10:
        n.payload = take(8);
        break;
      default:
        throw Error("E_VDF", "Unsupported binary VDF type; no changes made");
      }
      n.raw = bytes.mid(start, pos - start);
      out << std::move(n);
    }
  }
};
QByteArray encode(const Node &n) {
  if (!n.raw.isEmpty())
    return n.raw;
  if (n.key.contains('\0') || (n.type == 1 && n.payload.contains('\0')))
    throw Error("E_VDF", "NUL in VDF text");
  QByteArray b(1, static_cast<char>(n.type));
  b += n.key;
  b += '\0';
  if (n.type == 0) {
    for (const auto &c : n.children)
      b += encode(c);
    b += static_cast<char>(n.terminator);
  } else {
    b += n.payload;
    if (n.type == 1)
      b += '\0';
  }
  return b;
}
} // namespace
Document parse(const QByteArray &bytes) {
  if (bytes.size() > 16 * 1024 * 1024)
    throw Error("E_VDF", "Binary VDF exceeds 16 MiB");
  if (bytes.isEmpty()) {
    Document d;
    d.originalEmpty = true;
    return d;
  }
  Reader r{bytes};
  Document d;
  d.roots = r.list(d.terminator, 0);
  if (r.pos != bytes.size())
    throw Error("E_VDF", "Trailing bytes in binary VDF");
  return d;
}
QByteArray serialize(const Document &d) {
  if (d.originalEmpty && d.roots.isEmpty())
    return {};
  QByteArray b;
  for (const auto &n : d.roots)
    b += encode(n);
  b += static_cast<char>(d.terminator);
  return b;
}
quint32 appId(const QString &exe, const QString &title) {
  const auto data = (exe + title).toUtf8();
  quint32 crc = 0xffffffffu;
  for (const auto c : data) {
    crc ^= static_cast<quint8>(c);
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
  }
  return (crc ^ 0xffffffffu) | 0x80000000u;
}
quint64 launchId(quint32 id) { return (quint64(id) << 32) | 0x02000000ull; }
QStringList artNames(quint32 id) {
  const auto prefix = QString::number(id);
  return {prefix + ".png", prefix + "p.png", prefix + "_hero.png",
          prefix + "_logo.png"};
}
} // namespace ac::steam
