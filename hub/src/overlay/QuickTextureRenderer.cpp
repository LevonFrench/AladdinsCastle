// SPDX-License-Identifier: GPL-3.0-only
#include "QuickTextureRenderer.h"
#include "OverlayRuntime.h"
#include "OverlayCapture.h"
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QCoreApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
namespace ac {
namespace {
class CurrentTextureReadback final:public TextureReadbackOps {
public:
    explicit CurrentTextureReadback(QOpenGLContext *context):m_context(context),f(context->extraFunctions()){f->initializeOpenGLFunctions();}
    bool supported()const override{return QOpenGLContext::currentContext()==m_context&&!m_context->isOpenGLES()&&m_context->format().majorVersion()>=3;}
    int integer(ReadbackState state)override{
        GLenum name=0;
        switch(state){
        case ReadbackState::ReadFramebuffer:name=GL_READ_FRAMEBUFFER_BINDING;break;
        case ReadbackState::ReadBuffer:name=GL_READ_BUFFER;break;
        case ReadbackState::PackBuffer:name=GL_PIXEL_PACK_BUFFER_BINDING;break;
        case ReadbackState::Alignment:name=GL_PACK_ALIGNMENT;break;
        case ReadbackState::RowLength:name=GL_PACK_ROW_LENGTH;break;
        case ReadbackState::SkipRows:name=GL_PACK_SKIP_ROWS;break;
        case ReadbackState::SkipPixels:name=GL_PACK_SKIP_PIXELS;break;
        }
        GLint value=0;f->glGetIntegerv(name,&value);return value;
    }
    unsigned int error()override{return f->glGetError();}
    unsigned int createFramebuffer()override{GLuint result=0;f->glGenFramebuffers(1,&result);return result;}
    void bindReadFramebuffer(unsigned int framebuffer)override{f->glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer);}
    void attachTexture(unsigned int texture)override{f->glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);}
    bool framebufferComplete()override{return f->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;}
    bool rgba8Attachment()override{
        for(const auto name:{GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE}){
            GLint size=0;f->glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,static_cast<GLenum>(name),&size);if(size!=8)return false;
        }
        GLint type=0;f->glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,&type);
        GLint encoding=0;f->glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);
        return type==GL_UNSIGNED_NORMALIZED&&encoding==GL_LINEAR;
    }
    void selectColorAttachment()override{f->glReadBuffer(GL_COLOR_ATTACHMENT0);}
    void restoreReadBuffer(int selector)override{f->glReadBuffer(static_cast<GLenum>(selector));}
    void bindPackBuffer(unsigned int buffer)override{f->glBindBuffer(GL_PIXEL_PACK_BUFFER,buffer);}
    void pack(ReadbackState state,int value)override{
        GLenum name=0;
        switch(state){
        case ReadbackState::Alignment:name=GL_PACK_ALIGNMENT;break;
        case ReadbackState::RowLength:name=GL_PACK_ROW_LENGTH;break;
        case ReadbackState::SkipRows:name=GL_PACK_SKIP_ROWS;break;
        case ReadbackState::SkipPixels:name=GL_PACK_SKIP_PIXELS;break;
        default:return;
        }
        f->glPixelStorei(name,value);
    }
    void readRgba(QSize size,unsigned char *pixels)override{f->glReadPixels(0,0,size.width(),size.height(),GL_RGBA,GL_UNSIGNED_BYTE,pixels);}
    void deleteFramebuffer(unsigned int framebuffer)override{f->glDeleteFramebuffers(1,&framebuffer);}
private:
    QOpenGLContext *m_context;
    QOpenGLExtraFunctions *f;
};
}
bool readCurrentGlTexture(unsigned int texture,QSize size,QImage *image,QString *error){
    auto *context=QOpenGLContext::currentContext();
    if(!context){if(error)*error="Texture readback requires a current GL context.";return false;}
    CurrentTextureReadback ops(context);return readSourceTexture(ops,texture,size,image,error);
}
QuickTextureRenderer::QuickTextureRenderer(QObject *parent) : QObject(parent) {}
QuickTextureRenderer::~QuickTextureRenderer() {
    // Scenegraph and texture must die while their external context is current.
    if (m_context && m_surface) m_context->makeCurrent(m_surface.get());
    m_window.reset();
    m_control.reset();
    destroyTexture();
    if (m_context) m_context->doneCurrent();
}
bool QuickTextureRenderer::initialize(QQmlEngine &engine, QSize size, QString *error,bool captureMarkers) {
    m_size = size;
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setDepthBufferSize(24); format.setStencilBufferSize(8);
    m_context = std::make_unique<QOpenGLContext>();
    m_context->setFormat(format);
    if (auto *shared = QOpenGLContext::globalShareContext()) m_context->setShareContext(shared);
    if (!m_context->create()) { *error = "Cannot create the overlay OpenGL context."; return false; }
    m_surface = std::make_unique<QOffscreenSurface>();
    m_surface->setFormat(m_context->format()); m_surface->create();
    if (!m_surface->isValid() || !m_context->makeCurrent(m_surface.get())) {
        *error = "Cannot make the offscreen OpenGL surface current."; return false;
    }
    m_context->functions()->initializeOpenGLFunctions();
    auto *f = m_context->functions();
    m_graphics = QString::fromLatin1(reinterpret_cast<const char *>(f->glGetString(GL_RENDERER)))
               + " / " + QString::fromLatin1(reinterpret_cast<const char *>(f->glGetString(GL_VERSION)));
    m_control = std::make_unique<QQuickRenderControl>();
    m_window = std::make_unique<QQuickWindow>(m_control.get());
    m_window->setColor(Qt::transparent);
    m_window->setGeometry(0, 0, size.width(), size.height());
    connect(m_control.get(), &QQuickRenderControl::renderRequested, this, &QuickTextureRenderer::dirty);
    connect(m_control.get(), &QQuickRenderControl::sceneChanged, this, &QuickTextureRenderer::dirty);
    connect(m_window.get(), &QQuickWindow::sceneGraphInitialized, this, &QuickTextureRenderer::createTexture);
    connect(m_window.get(), &QQuickWindow::sceneGraphInvalidated, this, &QuickTextureRenderer::destroyTexture);
    const bool spike = engine.rootContext()->contextProperty("overlaySpikeEnabled").toBool();
    QQmlComponent component(&engine, QUrl(spike ? "qrc:/qt/qml/AladdinsCastle/Hub/SpikeRoot.qml" : "qrc:/qt/qml/AladdinsCastle/Hub/HubRoot.qml"));
    QObject *object = component.createWithInitialProperties({{"overlayPresentation", true},{"captureMarkers",captureMarkers}});
    m_root = qobject_cast<QQuickItem *>(object);
    if (!m_root) {
        *error = "Overlay HubRoot must be a QQuickItem: " + component.errorString();
        delete object; return false;
    }
    m_root->setParent(m_window->contentItem());
    m_root->setParentItem(m_window->contentItem());
    m_root->setSize(size);
    m_root->setVisible(false);
    m_root->forceActiveFocus();
    QFocusEvent focused(QEvent::FocusIn, Qt::OtherFocusReason);
    QCoreApplication::sendEvent(m_window.get(), &focused);
    m_window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(m_context.get()));
    if (!m_control->initialize()) { *error = "QQuickRenderControl initialization failed."; return false; }
    m_initialized = true;
    // initialize() emits sceneGraphInitialized; keep a guard for lazy drivers.
    if (m_texture == 0) createTexture();
    if (m_texture == 0) { *error = "Cannot allocate the overlay RGBA texture."; return false; }
    doneCurrent();
    return true;
}
void QuickTextureRenderer::createTexture() {
    if (m_texture != 0) return;
    auto *f = m_context->functions();
    f->glGenTextures(1, &m_texture);
    f->glBindTexture(GL_TEXTURE_2D, m_texture);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_size.width(), m_size.height(), 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    if (f->glGetError() != GL_NO_ERROR) { destroyTexture(); return; }
    auto target = QQuickRenderTarget::fromOpenGLTexture(m_texture, m_size);
    target.setDevicePixelRatio(1);
    target.setMirrorVertically(overlayMirrorVertically);
    // Keep target ordering unchanged; compositor bounds must not invert it.
    m_window->setRenderTarget(target);
}
void QuickTextureRenderer::destroyTexture() {
    if (m_texture && m_context && QOpenGLContext::currentContext() == m_context.get())
        m_context->functions()->glDeleteTextures(1, &m_texture);
    m_texture = 0;
}
void QuickTextureRenderer::setVisible(bool visible) { if (m_root) m_root->setVisible(visible); }
bool QuickTextureRenderer::render(QString *error) {
    if (!m_initialized || !m_context->makeCurrent(m_surface.get())) {
        *error = "Offscreen OpenGL context is unavailable."; return false;
    }
    QElapsedTimer elapsed; elapsed.start();
    m_control->polishItems(); m_control->beginFrame(); m_control->sync();
    m_control->render(); m_control->endFrame();
    m_context->functions()->glFlush();
    m_lastCpuMs = static_cast<double>(elapsed.nsecsElapsed()) / 1000000.0;
    // Keep this context current for IVROverlay::SetOverlayTexture.
    return true;
}
bool QuickTextureRenderer::readbackCurrentTexture(QImage *image,QString *error){
    if(image)*image=QImage();
    if(!m_initialized||!m_context||!m_texture||QOpenGLContext::currentContext()!=m_context.get()){
        if(error){*error="Source texture readback requires the initialized renderer's current context.";}
        return false;
    }
    return readCurrentGlTexture(m_texture,m_size,image,error);
}
void QuickTextureRenderer::doneCurrent() { if (m_context) m_context->doneCurrent(); }
QString QuickTextureRenderer::graphicsDescription() const { return m_graphics; }
}
