// SPDX-License-Identifier: GPL-3.0-only
#include "OverlayCapture.h"
#include <cmath>
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
bool readNativeOverlayImage(NativeOverlayReadbackOps &ops,QImage *image,QJsonObject *metadata,QString *error){
    if(!image||!metadata)return fail(error,"Native image output is unavailable.");
    *image={};*metadata={{"evidence","native-overlay-image-readback"},{"texture_type","OpenGL"},
        {"texture_type_value",int(vr::TextureType_OpenGL)},{"color_space","Auto"},
        {"row_conversion","None: native API rows preserved"},{"row_origin","unspecified by SDK"},
        {"bounds_before_result",QJsonValue()},{"query_result",QJsonValue()},{"read_result",QJsonValue()},{"bounds_after_result",QJsonValue()},
        {"compositor_display_verified",false},{"exact_submission_frame_verified",false}};
    const auto asJson=[](const vr::VRTextureBounds_t &b){return QJsonObject{{"uMin",b.uMin},{"uMax",b.uMax},{"vMin",b.vMin},{"vMax",b.vMax}};};
    const auto valid=[](const vr::VRTextureBounds_t &b){
        for(float value:{b.uMin,b.uMax,b.vMin,b.vMax})if(!std::isfinite(value)||value<0||value>1)return false;
        return b.uMin!=b.uMax&&b.vMin!=b.vMax;
    };
    vr::VRTextureBounds_t before{};auto code=ops.bounds(&before);(*metadata)["bounds_before_result"]=int(code);
    if(code!=vr::VROverlayError_None)return fail(error,QString("GetOverlayTextureBounds failed (%1).").arg(int(code)));
    (*metadata)["texture_bounds"]=asJson(before);
    if(!valid(before))return fail(error,"Native overlay texture bounds are invalid.");
    uint32_t width=0,height=0;code=ops.image(nullptr,0,&width,&height);
    (*metadata)["query_result"]=int(code);(*metadata)["width"]=qint64(width);(*metadata)["height"]=qint64(height);
    if(code!=vr::VROverlayError_None&&code!=vr::VROverlayError_ArrayTooSmall)
        return fail(error,QString("GetOverlayImageData size query failed (%1).").arg(int(code)));
    if(!width||!height||width>4096||height>4096)
        return fail(error,"Native overlay dimensions exceed the bounded RGBA budget.");
    const quint64 bytes=quint64(width)*quint64(height)*4;
    if(bytes>16*1024*1024)return fail(error,"Native overlay dimensions exceed the bounded RGBA budget.");
    QImage pixels(int(width),int(height),QImage::Format_RGBA8888);
    if(pixels.isNull()||quint64(pixels.sizeInBytes())!=bytes)return fail(error,"Cannot allocate native RGBA image.");
    uint32_t returnedWidth=width,returnedHeight=height;
    code=ops.image(pixels.bits(),uint32_t(bytes),&returnedWidth,&returnedHeight);
    (*metadata)["read_result"]=int(code);(*metadata)["returned_width"]=qint64(returnedWidth);(*metadata)["returned_height"]=qint64(returnedHeight);
    (*metadata)["buffer_bytes"]=qint64(bytes);
    if(code!=vr::VROverlayError_None)return fail(error,QString("GetOverlayImageData failed (%1).").arg(int(code)));
    if(returnedWidth!=width||returnedHeight!=height)return fail(error,"Native image dimensions changed during readback.");
    vr::VRTextureBounds_t after{};code=ops.bounds(&after);(*metadata)["bounds_after_result"]=int(code);
    if(code!=vr::VROverlayError_None)return fail(error,QString("GetOverlayTextureBounds recheck failed (%1).").arg(int(code)));
    (*metadata)["texture_bounds_after"]=asJson(after);
    if(!valid(after)||before.uMin!=after.uMin||before.uMax!=after.uMax||before.vMin!=after.vMin||before.vMax!=after.vMax)
        return fail(error,"Native overlay bounds changed during readback.");
    *image=std::move(pixels);return true;
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
CaptureAttempt OverlayCapture::attempt(const Reader &reader,unsigned int texture,const OverlaySubmitStatus &submission,bool pointerFlipY,QString *error,const NativeReader &nativeReader){
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
        const auto imagePath=install::scopedPath("texture.png",target),receiptPath=install::scopedPath("receipt.json",target),nativePath=install::scopedPath("native.png",target);
        bool complete=false,imageCreated=false,receiptCreated=false,nativeCreated=false;
        const auto cleanup=qScopeGuard([&]{if(!complete){if(receiptCreated)QFile::remove(receiptPath);if(imageCreated)QFile::remove(imagePath);if(nativeCreated)QFile::remove(nativePath);}});
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
        QJsonObject receipt{{"schema",1},{"status","captured"},{"evidence","source-texture-readback"},
            {"image","texture.png"},{"width",image.width()},{"height",image.height()},{"texture_format","RGBA8"},
            {"row_conversion","GL bottom-first to PNG top-first: one row reversal"},
            {"render_target_mirror_vertically",overlayMirrorVertically},{"picture_flip_y",m_pictureFlipY},{"pointer_flip_y",pointerFlipY},
            {"uv_bounds",QJsonObject{{"uMin",bounds.uMin},{"vMin",bounds.vMin},{"uMax",bounds.uMax},{"vMax",bounds.vMax}}},
            {"source_texture_id",static_cast<qint64>(texture)},{"submission_api_called",submission.apiCalled},
            {"submission_api_result",submission.apiCalled?QJsonValue(submission.apiResult):QJsonValue()},
            {"submission_api_succeeded",submission.apiCalled&&submission.apiResult==int(vr::VROverlayError_None)},
            {"compositor_display_verified",false}};
        QJsonObject native;QString nativeError;QImage nativeImage;bool nativeOk=false;
        if(!nativeReader)nativeError="Native image reader is unavailable.";
        else if(!submission.apiCalled||submission.apiResult!=int(vr::VROverlayError_None))nativeError="No successful texture submission; native readback not attempted.";
        else{
            try{nativeOk=nativeReader(&nativeImage,&native,&nativeError);}catch(const std::exception &exception){nativeError=QString::fromUtf8(exception.what());}
            if(nativeOk&&(nativeImage.isNull()||nativeImage.format()!=QImage::Format_RGBA8888||nativeImage.width()>4096||nativeImage.height()>4096||nativeImage.sizeInBytes()>16*1024*1024)){
                nativeOk=false;nativeError="Native reader returned an invalid bounded RGBA image.";
            }
            if(nativeOk){
                QFile nativePixels(nativePath);
                if(!nativePixels.open(QIODevice::WriteOnly|QIODevice::NewOnly)){nativeOk=false;nativeError="Cannot create a fresh native overlay PNG.";}
                else{
                    nativeCreated=true;nativeOk=nativeImage.save(&nativePixels,"PNG")&&nativePixels.flush();nativePixels.close();
                    if(!nativeOk){nativeError="Cannot write native overlay PNG.";QFile::remove(nativePath);nativeCreated=false;}
                }
            }
            if(!nativeOk&&nativeError.isEmpty())nativeError="Native overlay image readback failed.";
        }
        const bool nativeAttempted=bool(nativeReader)&&submission.apiCalled&&submission.apiResult==int(vr::VROverlayError_None);
        native["status"]=nativeOk?"captured":nativeAttempted?"failed":"not-attempted";
        native["evidence"]="native-overlay-image-readback";native["compositor_display_verified"]=false;
        if(nativeOk){native["image"]="native.png";native["width"]=nativeImage.width();native["height"]=nativeImage.height();}
        else native["error"]=nativeError;
        receipt["native_overlay"]=native;
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
