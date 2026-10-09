// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QString>
namespace ac {
class SpikeState : public QObject {
    Q_OBJECT
    Q_PROPERTY(int clickCount READ clickCount NOTIFY clickCountChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool flipY READ flipY WRITE setFlipY NOTIFY flipYChanged)
    Q_PROPERTY(bool glow READ glow WRITE setGlow NOTIFY glowChanged)
    Q_PROPERTY(bool animate READ animate WRITE setAnimate NOTIFY animateChanged)
public:
    explicit SpikeState(QObject *parent = nullptr) : QObject(parent) {}
    int clickCount() const { return m_clickCount; }
    QString text() const { return m_text; }
    QString status() const { return m_status; }
    bool flipY() const { return m_flipY; }
    bool glow() const { return m_glow; }
    bool animate() const { return m_animate; }
    void setText(const QString &value) { if (m_text == value) return; m_text = value; emit textChanged(); }
    void setStatus(const QString &value) { if (m_status == value) return; m_status = value; emit statusChanged(); }
    void setFlipY(bool value) { if (m_flipY == value) return; m_flipY = value; emit flipYChanged(); }
    void setGlow(bool value) { if (m_glow == value) return; m_glow = value; emit glowChanged(); }
    void setAnimate(bool value) { if (m_animate == value) return; m_animate = value; emit animateChanged(); }
    Q_INVOKABLE void clicked() { ++m_clickCount; emit clickCountChanged(); }
signals:
    void clickCountChanged();
    void textChanged();
    void statusChanged();
    void flipYChanged();
    void glowChanged();
    void animateChanged();
private:
    int m_clickCount = 0;
    QString m_text;
    QString m_status = "SteamVR keyboard: awaiting a headset check";
    bool m_flipY = true;
    bool m_glow = true;
    bool m_animate = false;
};
}
