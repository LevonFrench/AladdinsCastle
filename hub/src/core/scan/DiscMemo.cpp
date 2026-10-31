// SPDX-License-Identifier: GPL-3.0-only
#include "DiscMemo.h"
#include <QDateTime>
#include <QDir>
#include <QStringList>
namespace ac::scan {
namespace {
// Raised whenever a reader finds a disc's identity differently, so every remembered identity is read again.
constexpr int kVersion=1;
}
DiscMemo::DiscMemo(const QVariantMap &saved){
    if(saved.value("version").toInt()==kVersion)before_=saved.value("systems").toMap();
}
QString DiscMemo::identity(const QString &system,const QFileInfo &file,const std::function<QString(bool &)> &read){
    const auto path=QDir::fromNativeSeparators(file.absoluteFilePath());
    const auto size=file.size(),modified=file.lastModified().toMSecsSinceEpoch();
    const bool sheet=QStringList{"cue","gdi","m3u"}.contains(file.suffix().toLower());
    // Sizes and times as text: a saved number comes back as a double.
    const auto last=before_.value(system).toMap().value(path).toMap();
    if(!sheet&&!last.isEmpty()&&last.value("size").toString()==QString::number(size)&&last.value("modified").toString()==QString::number(modified)){
        auto discs=after_.value(system).toMap();discs.insert(path,last);after_.insert(system,discs);
        ++reused_;return last.value("identity").toString();
    }
    bool finished=false;const auto identity=read(finished);
    if(finished&&!sheet){
        auto discs=after_.value(system).toMap();
        discs.insert(path,QVariantMap{{"size",QString::number(size)},{"modified",QString::number(modified)},{"identity",identity}});
        after_.insert(system,discs);
    }
    return identity;
}
QVariantMap DiscMemo::saved() const{
    auto systems=before_;
    for(auto it=after_.cbegin();it!=after_.cend();++it){
        if(!cutShort_.contains(it.key())){systems.insert(it.key(),it.value());continue;}
        auto discs=before_.value(it.key()).toMap();
        const auto now=it.value().toMap();for(auto disc=now.cbegin();disc!=now.cend();++disc)discs.insert(disc.key(),disc.value());
        systems.insert(it.key(),discs);
    }
    return {{"version",kVersion},{"systems",systems}};
}
}
