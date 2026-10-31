// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <functional>
namespace ac::scan {
// What each disc said when it was last read (its serial or product number, never the game that matched), kept with
// the file's size and time. While both are unchanged the disc is not opened again, so reading a large folder a second
// time opens only new or changed discs; a change to the lists of games still applies, as matching is done afresh.
// Kept per system, so one file read as two systems never answers for the other. Used from one thread at a time.
class DiscMemo {
public:
    DiscMemo()=default;
    // A memo saved earlier ({version, systems: {system: {path: {size, modified, identity}}}}); one written by another
    // version of the readers is ignored, so every disc is read again.
    explicit DiscMemo(const QVariantMap &saved);
    // The disc's identity: the remembered one while the file is unchanged, otherwise read's answer, kept only when
    // read says the reading finished (one stopped by a time limit or by closing is tried again next time). A cue, gdi
    // or m3u sheet is always read, as the files it names can change on their own.
    QString identity(const QString &system,const QFileInfo &file,const std::function<QString(bool &finished)> &read);
    // A system read now: what is saved for it is replaced by the discs seen now, even when there are none.
    void reading(const QString &system){if(!after_.contains(system))after_.insert(system,QVariantMap{});}
    // A system whose folder was not read to the end keeps the discs it had as well as those read now.
    void cutShort(const QString &system){cutShort_<<system;}
    // What to save: for each system read now, only the discs seen now, so removed discs are forgotten.
    QVariantMap saved() const;
    int reused() const{return reused_;}
private:
    QVariantMap before_,after_;
    QSet<QString> cutShort_;
    int reused_=0;
};
}
