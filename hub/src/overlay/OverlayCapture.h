// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "OverlayRuntime.h"
#include <QImage>
#include <QJsonObject>
#include <functional>
namespace ac {
enum class ReadbackState { ReadFramebuffer, ReadBuffer, PackBuffer, Alignment, RowLength, SkipRows, SkipPixels };
// The production adapter operates only on the current context's read target.
// Tests exercise the same state/row logic without a GL context or VR service.
class TextureReadbackOps {
public:
    virtual ~TextureReadbackOps()=default;
    virtual bool supported()const=0;
    virtual int integer(ReadbackState state)=0;
    virtual unsigned int error()=0;
    virtual unsigned int createFramebuffer()=0;
    virtual void bindReadFramebuffer(unsigned int framebuffer)=0;
    virtual void attachTexture(unsigned int texture)=0;
    virtual bool framebufferComplete()=0;
    virtual bool rgba8Attachment()=0;
    virtual void selectColorAttachment()=0;
    virtual void restoreReadBuffer(int selector)=0;
    virtual void bindPackBuffer(unsigned int buffer)=0;
    virtual void pack(ReadbackState state,int value)=0;
    virtual void readRgba(QSize size,unsigned char *pixels)=0;
    virtual void deleteFramebuffer(unsigned int framebuffer)=0;
};
bool readSourceTexture(TextureReadbackOps &ops,unsigned int texture,QSize size,QImage *image,QString *error);

class NativeOverlayReadbackOps {
public:
    virtual ~NativeOverlayReadbackOps()=default;
    virtual vr::EVROverlayError bounds(vr::VRTextureBounds_t *value)=0;
    virtual vr::EVROverlayError image(void *buffer,uint32_t bytes,uint32_t *width,uint32_t *height)=0;
};
bool readNativeOverlayImage(NativeOverlayReadbackOps &ops,QImage *image,QJsonObject *metadata,QString *error);
enum class CaptureAttempt { Disabled, Captured, Failed };
class OverlayCapture {
public:
    using Reader=std::function<bool(QImage *,QString *)>;
    using NativeReader=std::function<bool(QImage *,QJsonObject *,QString *)>;
    bool configure(const QString &catalogRoot,const QString &directory,bool pictureFlipY,QString *error);
    bool requested()const{return !m_directory.isEmpty()&&!m_attempted;}
    void cancel(){m_attempted=true;}
    CaptureAttempt attempt(const Reader &reader,unsigned int texture,const OverlaySubmitStatus &submission,bool pointerFlipY,QString *error,const NativeReader &nativeReader={});
private:
    QString m_root,m_directory;
    bool m_attempted=false,m_pictureFlipY=false;
};
}
