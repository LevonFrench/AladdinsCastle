// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
namespace ac::install {
// What an install-engine E_* code means to a person: what happened and what to do next.
// This is the one place for that wording; the technical message stays in the log.
struct PlainError {
  QString what, todo;
  QString text() const { return what + " " + todo; }
};
// Every code the install engine can raise has an entry. An unknown code gets a generic
// answer (hasPlainError is false) rather than the raw code.
PlainError plainError(const QString &code);
bool hasPlainError(const QString &code);
QStringList plainErrorCodes();
// One line for the install console: "<what happened> <what to do>".
inline QString plainErrorText(const QString &code) { return plainError(code).text(); }
} // namespace ac::install
