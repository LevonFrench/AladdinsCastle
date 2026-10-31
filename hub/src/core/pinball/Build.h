// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
namespace ac::pinball {
// The product version a VPX program reports, from its version information only: the program is never run.
// Empty when the file has none or on a platform without version resources.
QString productVersion(const QString &executable);
// AladdinsCastle's patched VPX (setups/vpx-vr) ends its product version with "-acvr", for example
// 10.8.1.5436.af26b2d93-acvr. That build quits a table after both stick clicks are held 3 s and moves the table
// only while both clicks are held; the standard build does neither.
bool isPatchedVersion(const QString &productVersion);
bool isPatchedBuild(const QString &executable);
}
