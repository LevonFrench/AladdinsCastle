// SPDX-License-Identifier: GPL-3.0-only
#include "Art.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QSaveFile>
#include <QXmlStreamReader>
namespace ac::pinball {
namespace {
constexpr qint64 kMaxFile=96LL*1024*1024;
bool link(const QFileInfo &f){return f.isSymLink()||f.isJunction();}
bool save(const QImage &image,const QString &path,int width){
    const auto scaled=image.width()>width?image.scaledToWidth(width,Qt::SmoothTransformation):image;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))return false;
    if(!scaled.convertToFormat(QImage::Format_RGB32).save(&file,"JPG",85)){file.cancelWriting();return false;}
    return file.commit();
}
}
QString backglassFile(const QString &table){
    const QFileInfo info(table);
    const auto wanted=info.completeBaseName()+".directb2s";
    for(const auto &candidate:info.absoluteDir().entryInfoList({info.completeBaseName()+".*"},QDir::Files))
        if(candidate.fileName().compare(wanted,Qt::CaseInsensitive)==0&&!link(candidate))return candidate.absoluteFilePath();
    return {};
}
QImage backglassImage(const QString &directb2s,std::atomic_bool &cancel){
    QFile file(directb2s);
    if(file.size()>kMaxFile||!file.open(QIODevice::ReadOnly))return {};
    QXmlStreamReader xml(&file);QByteArray thumbnail;
    while(!xml.atEnd()&&!cancel){
        if(xml.readNext()!=QXmlStreamReader::StartElement)continue;
        const auto name=xml.name();
        if(name==QLatin1String("BackglassImage")||name==QLatin1String("ThumbnailImage")){
            const auto value=xml.attributes().value(QLatin1String("Value")).toLatin1();
            if(value.isEmpty())continue;
            if(name==QLatin1String("ThumbnailImage")){thumbnail=value;continue;}
            QImage image;if(image.loadFromData(QByteArray::fromBase64(value)))return image;
        }
    }
    QImage image;if(!thumbnail.isEmpty())image.loadFromData(QByteArray::fromBase64(thumbnail));
    return image;
}
QVariantMap tableArt(const QString &table,const QString &tableId,const QString &cacheFolder,std::atomic_bool &cancel){
    if(!QRegularExpression("^[a-f0-9]{64}$").match(tableId).hasMatch())return {};
    const auto source=backglassFile(table);
    if(source.isEmpty())return {};
    // Named after the table's id and the backglass file's size and time, so a changed file is read again.
    const QFileInfo info(source);
    const auto stamp=QString::fromLatin1(QCryptographicHash::hash(QByteArray::number(info.size())+":"+QByteArray::number(info.lastModified().toMSecsSinceEpoch()),QCryptographicHash::Sha256).toHex().left(12));
    const QDir folder(cacheFolder);
    const auto thumb=folder.filePath(tableId+"-"+stamp+"-thumb.jpg"),backglass=folder.filePath(tableId+"-"+stamp+"-backglass.jpg");
    if(QFileInfo(thumb).isFile()&&QFileInfo(backglass).isFile())return {{"thumb",thumb},{"backglass",backglass}};
    const auto image=backglassImage(source,cancel);
    if(image.isNull()||cancel||!QDir().mkpath(cacheFolder))return {};
    // Earlier pictures of this table, from an older backglass file, go.
    for(const auto &old:folder.entryList({tableId+"-*.jpg"},QDir::Files))QFile::remove(folder.filePath(old));
    if(!save(image,backglass,1280)||!save(image,thumb,480))return {};
    return {{"thumb",thumb},{"backglass",backglass}};
}
}
