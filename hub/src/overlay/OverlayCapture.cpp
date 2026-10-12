// SPDX-License-Identifier: GPL-3.0-only
#include "OverlayCapture.h"
#include "core/install/Support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
namespace ac {
namespace {
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
}
bool readSourceTexture(TextureReadbackOps &ops,unsigned int texture,QSize size,QImage *image,QString *error){
    if(image)*image=QImage();
    if(!image||!texture||size.width()<=0||size.height()<=0||size.width()>4096||size.height()>4096||qint64(size.width())*size.height()*4>16*1024*1024)
        return fail(error,"Source texture readback has an invalid or oversized target.");
    if(!ops.supported())return fail(error,"Source texture readback requires its current desktop GL3 context.");
    // One check, never a loop that drains preexisting errors into a clean result.
    if(ops.error())return fail(error,"Source texture readback found a preexisting GL error.");
    const auto framebuffer=ops.integer(ReadbackState::ReadFramebuffer),selector=ops.integer(ReadbackState::ReadBuffer),packBuffer=ops.integer(ReadbackState::PackBuffer);
    const auto alignment=ops.integer(ReadbackState::Alignment),rowLength=ops.integer(ReadbackState::RowLength),skipRows=ops.integer(ReadbackState::SkipRows),skipPixels=ops.integer(ReadbackState::SkipPixels);
    if(ops.error())return fail(error,"Cannot inspect source texture readback state.");
    const auto temporary=ops.createFramebuffer();
    if(!temporary)return fail(error,"Cannot allocate the source texture read framebuffer.");
    const auto restore=[&]{
        ops.bindReadFramebuffer(static_cast<unsigned int>(framebuffer));ops.restoreReadBuffer(selector);
        ops.bindPackBuffer(static_cast<unsigned int>(packBuffer));ops.pack(ReadbackState::Alignment,alignment);
        ops.pack(ReadbackState::RowLength,rowLength);ops.pack(ReadbackState::SkipRows,skipRows);ops.pack(ReadbackState::SkipPixels,skipPixels);
        ops.deleteFramebuffer(temporary);
    };
    auto cleanup=qScopeGuard(restore);
    ops.bindReadFramebuffer(temporary);ops.attachTexture(texture);
    if(!ops.framebufferComplete())return fail(error,"Source texture read framebuffer is incomplete.");
    if(!ops.rgba8Attachment())return fail(error,"Source texture attachment is not unsigned normalized RGBA8.");
    ops.selectColorAttachment();ops.bindPackBuffer(0);ops.pack(ReadbackState::Alignment,1);
    ops.pack(ReadbackState::RowLength,0);ops.pack(ReadbackState::SkipRows,0);ops.pack(ReadbackState::SkipPixels,0);
    QImage bottomFirst(size,QImage::Format_RGBA8888);
    if(bottomFirst.isNull())return fail(error,"Cannot allocate source texture readback pixels.");
    ops.readRgba(size,bottomFirst.bits());
    if(ops.error())return fail(error,"Source texture pixel readback failed.");
    restore();cleanup.dismiss();
    if(ops.error())return fail(error,"Source texture readback state restoration failed.");
    // PNG rows are top-first. This is the only conversion: no picture/pointer flip.
    auto topFirst=bottomFirst.mirrored(false,true);
    if(topFirst.isNull())return fail(error,"Cannot convert source texture row order.");
    *image=std::move(topFirst);return true;
}
bool OverlayCapture::configure(const QString &catalogRoot,const QString &directory,bool pictureFlipY,QString *error){
    if(!m_directory.isEmpty()||m_attempted)return fail(error,"Source texture capture is already configured.");
    if(!QDir::isAbsolutePath(directory))return fail(error,"Source texture capture requires an absolute fresh directory.");
    try{
        const auto root=install::scopedPath(".local",catalogRoot),target=install::scopedPath(directory,root);
        if(target==root||QFileInfo::exists(target))return fail(error,"Source texture capture directory must be fresh and private.");
        m_root=root;m_directory=target;m_pictureFlipY=pictureFlipY;return true;
    }catch(const std::exception &exception){return fail(error,QString::fromUtf8(exception.what()));}
}
CaptureAttempt OverlayCapture::attempt(const Reader &reader,unsigned int texture,const OverlaySubmitStatus &submission,bool pointerFlipY,QString *error){
    if(!requested())return CaptureAttempt::Disabled;
    m_attempted=true;
    QImage image;
    if(!reader||!reader(&image,error)||image.isNull()||image.format()!=QImage::Format_RGBA8888||!texture){
        if(error&&error->isEmpty())*error="Source texture capture returned no valid image.";
        return CaptureAttempt::Failed;
    }
    try{
        const auto target=install::scopedPath(m_directory,m_root);
        if(QFileInfo::exists(target)||!QDir().mkpath(QFileInfo(target).absolutePath())||!QDir().mkdir(target)){
            fail(error,"Source texture capture directory is no longer fresh or cannot be created.");return CaptureAttempt::Failed;
        }
        const auto imagePath=install::scopedPath("texture.png",target),receiptPath=install::scopedPath("receipt.json",target);
        bool complete=false,imageCreated=false,receiptCreated=false;
        const auto cleanup=qScopeGuard([&]{if(!complete){if(receiptCreated)QFile::remove(receiptPath);if(imageCreated)QFile::remove(imagePath);}});
        QFile pixels(imagePath);
        if(!pixels.open(QIODevice::WriteOnly|QIODevice::NewOnly)){
            fail(error,"Cannot create a fresh source texture PNG.");return CaptureAttempt::Failed;
        }
        imageCreated=true;
        if(!image.save(&pixels,"PNG")||!pixels.flush()){
            fail(error,"Cannot write the source texture PNG.");return CaptureAttempt::Failed;
        }
        pixels.close();
        const auto bounds=overlayPictureBounds(m_pictureFlipY);
        const QJsonObject receipt{{"schema",1},{"status","captured"},{"evidence","source-texture-readback"},
            {"image","texture.png"},{"width",image.width()},{"height",image.height()},{"texture_format","RGBA8"},
            {"row_conversion","GL bottom-first to PNG top-first: one row reversal"},
            {"render_target_mirror_vertically",overlayMirrorVertically},{"picture_flip_y",m_pictureFlipY},{"pointer_flip_y",pointerFlipY},
            {"uv_bounds",QJsonObject{{"uMin",bounds.uMin},{"vMin",bounds.vMin},{"uMax",bounds.uMax},{"vMax",bounds.vMax}}},
            {"source_texture_id",static_cast<qint64>(texture)},{"submission_api_called",submission.apiCalled},
            {"submission_api_result",submission.apiCalled?QJsonValue(submission.apiResult):QJsonValue()},
            {"submission_api_succeeded",submission.apiCalled&&submission.apiResult==int(vr::VROverlayError_None)},
            {"compositor_display_verified",false}};
        const auto bytes=QJsonDocument(receipt).toJson();QFile metadata(receiptPath);
        if(!metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly)){
            fail(error,"Cannot create a fresh source texture receipt.");return CaptureAttempt::Failed;
        }
        receiptCreated=true;
        if(metadata.write(bytes)!=bytes.size()||!metadata.flush()){
            fail(error,"Cannot write the source texture receipt.");return CaptureAttempt::Failed;
        }
        metadata.close();complete=true;return CaptureAttempt::Captured;
    }catch(const std::exception &exception){fail(error,QString::fromUtf8(exception.what()));return CaptureAttempt::Failed;}
}
}
