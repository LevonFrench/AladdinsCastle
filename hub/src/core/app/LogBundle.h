// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
// The problem report: the Hub's own logs and the last launch's logs in one ZIP a tester can send.
// Everything in it is text with the person's details taken out, and it is never larger than the limits.
namespace ac::report {
// What to take out of a log. Home and root are folders; names are account or computer names.
struct Redaction {
  QString home;
  QString root;
  QStringList names;
  // This PC's home folder and account names, with the Hub's folder.
  static Redaction thisPc(const QString &root);
};
// Folders and account names become <home>, <hub> and <name>; other absolute paths keep only their last part;
// emails, web logins, keys and tokens, and Steam account numbers are replaced. Running it twice changes nothing more.
QString redact(const QString &text, const Redaction &who);
struct BundleLimits {
  qint64 perFile = 256 * 1024; // Only the end of a longer log is kept.
  qint64 total = 2 * 1024 * 1024;
  int files = 40;
};
struct Bundle {
  bool ok = false;
  QString path;
  QString error;
  QStringList included; // The names inside the ZIP, in order.
  QStringList leftOut;  // Files not added, each with the reason.
  qint64 bytes = 0;     // The text inside, before the ZIP's own headers.
};
// Writes <folder>/<name>-report-<time>.zip from the logs under <root>/user/logs: the install logs, crash records,
// launch failure logs, the overlay's input log and the newest launch's log. Only .log, .txt and .json files are read, and
// never through a link. Game files, BIOS, saves and the emulators' own folders are never looked at.
Bundle writeBundle(const QString &root, const QString &folder, const Redaction &who, const QString &appName,
                   const QString &appVersion, const BundleLimits &limits = {},
                   const QDateTime &when = QDateTime::currentDateTimeUtc());
// The ZIP's bytes for named texts (stored, no compression): for the bundle and for its test.
QByteArray zipOf(const QList<QPair<QString, QByteArray>> &entries, const QDateTime &when);
} // namespace ac::report
