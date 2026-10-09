// SPDX-License-Identifier: GPL-3.0-only
#include "QuickTextureRenderer.h"
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QCoreApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
namespace ac {
QuickTextureRenderer::QuickTextureRenderer(QObject *parent) : QObject(parent) {}
QuickTextureRenderer::~QuickTextureRenderer() {
    // Scenegraph and texture must die while their external context is current.
    if (m_context && m_surface) m_context->makeCurrent(m_surface.get());
    m_window.reset();
    m_control.reset();
    destroyTexture();
    if (m_context) m_context->doneCurrent();
}
bool QuickTextureRenderer::initialize(QQmlEngine &engine, QSize size, QString *error) {
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
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/AladdinsCastle/Hub/HubRoot.qml"));
    QObject *object = component.createWithInitialProperties({{"overlayPresentation", true}});
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
    // OpenGL's texture origin is bottom-left; leave target mirroring disabled.
    // SteamVR bounds are flipped at submission for the Qt top-left image.
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
void QuickTextureRenderer::doneCurrent() { if (m_context) m_context->doneCurrent(); }
QString QuickTextureRenderer::graphicsDescription() const { return m_graphics; }
}
