// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
namespace ac::secret {
// A secret the user gave, such as an API key, kept so only this Windows account on this PC can read it
// back (Windows data protection). The text returned is safe to save in the settings file; it is empty
// when protection is not available, so a secret is never saved in the clear.
QString protect(const QString &secret);
// The secret again, or empty when the text is not one this account protected.
QString reveal(const QString &protectedText);
} // namespace ac::secret
