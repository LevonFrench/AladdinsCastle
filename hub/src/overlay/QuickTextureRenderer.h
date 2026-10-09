// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QSize>
#include <memory>
class QOpenGLContext;
class QOffscreenSurface;
class QQuickRenderControl;
class QQuickWindow;
class QQuickItem;
class QQmlEngine;
namespace ac {
class QuickTextureRenderer : public QObject {
    Q_OBJECT
public:
    explicit QuickTextureRenderer(QObject *parent = nullptr);
    ~QuickTextureRenderer() override;
    bool initialize(QQmlEngine &engine, QSize size, QString *error);
    bool render(QString *error);
    void doneCurrent();
    void setVisible(bool visible);
    QQuickWindow *window() const { return m_window.get(); }
    unsigned int texture() const { return m_texture; }
    QSize size() const { return m_size; }
    double lastCpuMs() const { return m_lastCpuMs; }
    QString graphicsDescription() const;
signals:
    void dirty();
private:
    void createTexture();
    void destroyTexture();
    std::unique_ptr<QOpenGLContext> m_context;
    std::unique_ptr<QOffscreenSurface> m_surface;
    std::unique_ptr<QQuickRenderControl> m_control;
    std::unique_ptr<QQuickWindow> m_window;
    QQuickItem *m_root = nullptr;
    QSize m_size;
    unsigned int m_texture = 0;
    double m_lastCpuMs = 0;
    bool m_initialized = false;
    QString m_graphics;
};
}
