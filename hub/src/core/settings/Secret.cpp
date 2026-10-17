// SPDX-License-Identifier: GPL-3.0-only
#include "Secret.h"
#include <QByteArray>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif
namespace ac::secret {
namespace {
constexpr char prefix[] = "dpapi:";
#ifdef Q_OS_WIN
// Ties the protected text to the Hub: another program's protected text is not read back as a key.
DATA_BLOB entropy() {
  static QByteArray value("AladdinsCastle Hub secret v1");
  return {DWORD(value.size()), reinterpret_cast<BYTE *>(value.data())};
}
#endif
} // namespace
QString protect(const QString &secret) {
  if (secret.isEmpty())
    return {};
#ifdef Q_OS_WIN
  auto plain = secret.toUtf8();
  DATA_BLOB in{DWORD(plain.size()), reinterpret_cast<BYTE *>(plain.data())}, salt = entropy(), out{};
  const bool ok = CryptProtectData(&in, L"AladdinsCastle Hub", &salt, nullptr, nullptr,
                                   CRYPTPROTECT_UI_FORBIDDEN, &out);
  plain.fill('\0');
  if (!ok)
    return {};
  const auto text = QString::fromLatin1(prefix) +
                    QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(out.pbData), int(out.cbData)).toBase64());
  LocalFree(out.pbData);
  return text;
#else
  return {};
#endif
}
QString reveal(const QString &protectedText) {
  if (!protectedText.startsWith(QLatin1String(prefix)))
    return {};
#ifdef Q_OS_WIN
  auto data = QByteArray::fromBase64(protectedText.mid(int(sizeof prefix) - 1).toLatin1());
  if (data.isEmpty())
    return {};
  DATA_BLOB in{DWORD(data.size()), reinterpret_cast<BYTE *>(data.data())}, salt = entropy(), out{};
  if (!CryptUnprotectData(&in, nullptr, &salt, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
    return {};
  const auto text = QString::fromUtf8(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
  SecureZeroMemory(out.pbData, out.cbData);
  LocalFree(out.pbData);
  return text;
#else
  return {};
#endif
}
} // namespace ac::secret
