// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QPointF>
#include <QMap>
#include <openvr.h>
namespace ac {
class SpikeState : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList cursors READ cursors NOTIFY observationsChanged)
    Q_PROPERTY(QPointF pointerPosition READ pointerPosition NOTIFY observationsChanged)
    Q_PROPERTY(quint32 pointerCursor READ pointerCursor NOTIFY observationsChanged)
    Q_PROPERTY(bool hasPointer READ hasPointer NOTIFY observationsChanged)
    Q_PROPERTY(int droppedPackets READ droppedPackets NOTIFY observationsChanged)
    Q_PROPERTY(QVariantList cornerHits READ cornerHits NOTIFY cornerHitsChanged)
    Q_PROPERTY(int clickCount READ clickCount NOTIFY clickCountChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool flipY READ flipY WRITE setFlipY NOTIFY flipYChanged)
    Q_PROPERTY(bool glow READ glow WRITE setGlow NOTIFY glowChanged)
    Q_PROPERTY(bool animate READ animate WRITE setAnimate NOTIFY animateChanged)
public:
    explicit SpikeState(QObject *parent = nullptr) : QObject(parent) {}
    QVariantList cursors() const { QVariantList rows; for(const auto &row:m_cursors)rows.append(row); return rows; }
    QPointF pointerPosition() const { return m_pointer; }
    quint32 pointerCursor() const { return m_pointerCursor; }
    bool hasPointer() const { return m_hasPointer; }
    int droppedPackets() const { return m_droppedPackets; }
    QVariantList cornerHits() const { return m_cornerHits; }
    // Numeric observations only; keyboard text never enters diagnostics.
    void recordInput(const vr::VREvent_t &event, QPointF position, QPoint angle, int qtType, Qt::MouseButtons buttons) {
        const bool wheel=event.eventType==vr::VREvent_ScrollSmooth||event.eventType==vr::VREvent_ScrollDiscrete;
        if(!wheel&&event.eventType!=vr::VREvent_MouseMove&&event.eventType!=vr::VREvent_MouseButtonDown&&event.eventType!=vr::VREvent_MouseButtonUp)return;
        const auto cursor=wheel?event.data.scroll.cursorIndex:event.data.mouse.cursorIndex;
        m_pointer=position;m_pointerCursor=cursor;m_hasPointer=true;
        if(!m_cursors.contains(cursor)&&m_cursors.size()>=16){++m_droppedPackets;emit observationsChanged();return;}
        auto &row=m_cursors[cursor];row["cursor"]=cursor;row["qtX"]=position.x();row["qtY"]=position.y();
        row["qtEvent"]=qtType;row["rawEvent"]=event.eventType;row["qtButtons"]=int(buttons);
        QString count;
        if(wheel){
            count=event.eventType==vr::VREvent_ScrollSmooth?"smooth":"discrete";
            row[count+"RawX"]=event.data.scroll.xdelta;row[count+"RawY"]=event.data.scroll.ydelta;
            row[count+"QtX"]=angle.x();row[count+"QtY"]=angle.y();row["viewportScale"]=event.data.scroll.viewportscale;
        }else{
            count=event.eventType==vr::VREvent_MouseMove?"moves":event.eventType==vr::VREvent_MouseButtonDown?"presses":"releases";
            row["rawX"]=event.data.mouse.x;row["rawY"]=event.data.mouse.y;row["rawButton"]=event.data.mouse.button;
        }
        row[count]=row.value(count).toLongLong()+1;
        emit observationsChanged();
    }
    Q_INVOKABLE void hitCorner(int index) { if(index<0||index>=m_cornerHits.size())return;m_cornerHits[index]=m_cornerHits[index].toInt()+1;emit cornerHitsChanged();clicked(); }
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
    void observationsChanged();
    void cornerHitsChanged();
    void clickCountChanged();
    void textChanged();
    void statusChanged();
    void flipYChanged();
    void glowChanged();
    void animateChanged();
private:
    QMap<quint32,QVariantMap> m_cursors;
    QPointF m_pointer;
    quint32 m_pointerCursor=0;
    bool m_hasPointer=false;
    int m_droppedPackets=0;
    QVariantList m_cornerHits{0,0,0,0};
    int m_clickCount = 0;
    QString m_text;
    QString m_status = "SteamVR keyboard: awaiting a headset check";
    bool m_flipY = true;
    bool m_glow = true;
    bool m_animate = false;
};
}
