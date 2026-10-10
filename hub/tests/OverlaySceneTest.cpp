// SPDX-License-Identifier: GPL-3.0-only
#include "overlay/OverlayInput.h"
#include "overlay/OverlayKeyboard.h"
#include "overlay/OverlayRuntime.h"
#include "overlay/SpikeState.h"
#include <QGuiApplication>
#include <QFocusEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickRenderControl>
#include <QQuickStyle>
#include <QtTest>
#include <cstring>
namespace {
QList<QQuickItem *> visualItems(QQuickItem *root,const QString &name) {
    QList<QQuickItem *> result;
    for(auto *child:root->childItems()){
        if(child->objectName()==name)result.append(child);
        result.append(visualItems(child,name));
    }
    return result;
}
QQuickItem *item(QQuickItem *root,const QString &name) {return visualItems(root,name).value(0);}
class KeyboardAdapter final:public ac::OverlayRuntime {
public:
    int initializations=0,requests=0,hides=0;
    quint64 token=0,overlay=42;
    bool initialize(QSize,const QString &,QString *)override{++initializations;return false;}
    bool isVisible()const override{return false;}
    vr::VROverlayHandle_t handle()const override{return overlay;}
    bool pollEvent(vr::VREvent_t *)override{return false;}
    bool submit(unsigned int,QString *)override{return false;}
    bool showKeyboard(const QString &,quint64 value,QString *)override{++requests;token=value;return true;}
    void hideKeyboard()override{++hides;}
    void acknowledgeQuit()override{}
    void shutdown()override{}
};
}
class DiagnosticHostProbe:public QObject {
    Q_OBJECT
public:
    explicit DiagnosticHostProbe(ac::OverlayKeyboard &keyboard):session(keyboard){}
    Q_INVOKABLE void requestKeyboard(QQuickItem *target){session.request(target);}
    Q_INVOKABLE void dismissKeyboard(){session.close(true,true);}
private:
    ac::OverlayKeyboard &session;
};
struct DiagnosticScene {
    ac::SpikeState state;
    KeyboardAdapter runtime;
    ac::OverlayInput input;
    ac::OverlayKeyboard keyboard{state,input,runtime};
    DiagnosticHostProbe host{keyboard};
    QQmlEngine engine;
    std::unique_ptr<QQuickRenderControl> renderControl;
    QQuickWindow window;
    QQuickItem *root=nullptr;
    explicit DiagnosticScene(bool controlled=false,quint64 handle=42)
        :renderControl(controlled?std::make_unique<QQuickRenderControl>():nullptr),window(renderControl.get()) {
        runtime.overlay=handle;
        input.setObserver([this](const auto &event,auto position,auto angle,int type,auto buttons){state.recordInput(event,position,angle,type,buttons);});
        QObject::connect(&state,&ac::SpikeState::flipYChanged,&host,[this]{input.setFlipY(state.flipY());});
        engine.rootContext()->setContextProperty("spikeState",&state);
        engine.rootContext()->setContextProperty("overlayHost",&host);
        engine.rootContext()->setContextProperty("surfaceColor","#10131c");
        engine.rootContext()->setContextProperty("primaryTextColor","#ffffff");
        engine.rootContext()->setContextProperty("brandColor","#ee892a");
        QQmlComponent component(&engine,QUrl::fromLocalFile(QString::fromUtf8(AC_SPIKE_SCENE)));
        root=qobject_cast<QQuickItem *>(component.create());
        if(!root)qFatal("Diagnostic scene did not load");
        window.setGeometry(0,0,1280,800);root->setParent(window.contentItem());root->setParentItem(window.contentItem());
        root->setSize({1280,800});root->setProperty("overlayPresentation",true);
        if(!controlled)window.show(); // software/offscreen only; render-control window stays hidden
        keyboard.setWindow(&window);QCoreApplication::processEvents();QTest::qWait(20);activate();
    }
    ~DiagnosticScene(){keyboard.setWindow(nullptr);}
    void activate(){root->forceActiveFocus();QFocusEvent focus(QEvent::FocusIn,Qt::OtherFocusReason);QCoreApplication::sendEvent(&window,&focus);}
    bool deliver(const vr::VREvent_t &event){return keyboard.handle(event)||input.dispatch(event,&window);}
    bool point(QPointF position,quint32 cursor=0,quint32 type=vr::VREvent_MouseMove){
        vr::VREvent_t event{};event.eventType=type;
        event.data.mouse={float(position.x()),float(state.flipY()?800-position.y():position.y()),vr::VRMouseButton_Left,cursor};
        return deliver(event);
    }
    bool click(QQuickItem *target,quint32 cursor=0){
        if(!target)return false;
        const auto center=target->mapToScene({target->width()/2,target->height()/2});
        const auto ok=point(center,cursor)&&point(center,cursor,vr::VREvent_MouseButtonDown)&&point(center,cursor,vr::VREvent_MouseButtonUp);
        QCoreApplication::processEvents();return ok;
    }
    void packet(quint32 type,quint64 token,quint64 handle,const char *text=""){
        vr::VREvent_t event{};event.eventType=type;event.data.keyboard.uUserValue=token;event.data.keyboard.overlayHandle=handle;
        const auto length=std::min(std::strlen(text),sizeof(event.data.keyboard.cNewInput));std::memcpy(event.data.keyboard.cNewInput,text,length);
        deliver(event);QCoreApplication::processEvents();
    }
};
class OverlaySceneTest:public QObject {
    Q_OBJECT
private slots:
    void diagnosticSceneHasFourOffCentreTargetsAndObservations_data(){QTest::addColumn<bool>("flip");QTest::newRow("bottom-left packets")<<true;QTest::newRow("top-left packets")<<false;}
    void diagnosticSceneHasFourOffCentreTargetsAndObservations(){
        QFETCH(bool,flip);DiagnosticScene scene;scene.state.setFlipY(flip);
        QCOMPARE(visualItems(scene.root,"syntheticCard").size(),qsizetype(50));
        for(int i=0;i<4;++i){auto target=item(scene.root,"spikeCorner"+QString::number(i));QVERIFY(target);
            const auto center=target->mapToScene({target->width()/2,target->height()/2});
            QVERIFY(center.x()<220||center.x()>1060);QVERIFY(center.y()<120||center.y()>680);QVERIFY(target->height()>=48);
            QVERIFY(scene.click(target,quint32(i%2)));QCOMPARE(scene.state.cornerHits().at(i).toInt(),1);
        }
        QCOMPARE(scene.state.clickCount(),4);QCOMPARE(scene.state.cursors().size(),2);
        for(const auto &value:scene.state.cursors()){const auto row=value.toMap();QCOMPARE(row.value("moves").toInt(),2);QCOMPARE(row.value("presses").toInt(),2);QCOMPARE(row.value("releases").toInt(),2);QCOMPARE(row.value("qtEvent").toInt(),int(QEvent::MouseButtonRelease));}
        auto marker=item(scene.root,"spikePointer");QVERIFY(marker);QVERIFY(marker->isVisible());
        QCOMPARE(marker->x()+marker->width()/2,scene.input.position().x());QCOMPARE(marker->y()+marker->height()/2,scene.input.position().y());
        QVERIFY(item(scene.root,"spikeInputObservations"));QVERIFY(item(scene.root,"spikeScrollPosition"));QCOMPARE(scene.runtime.initializations,0);
    }
    void focusAloneDoesNotRequestSteamVrKeyboard(){
        DiagnosticScene scene;auto editor=item(scene.root,"spikeTextInput");QVERIFY(editor);
        editor->forceActiveFocus();QCoreApplication::processEvents();QCOMPARE(scene.runtime.requests,0);
        QVERIFY(scene.click(item(scene.root,"spikeKeyboardRequest")));QCOMPARE(scene.runtime.requests,1);QCOMPARE(scene.runtime.initializations,0);
    }
    void everyPanelKeyAndDoneByGeometry_data(){QTest::addColumn<bool>("flip");QTest::newRow("bottom-left packets")<<true;QTest::newRow("top-left packets")<<false;}
    void everyPanelKeyAndDoneByGeometry(){
        QFETCH(bool,flip);DiagnosticScene scene;scene.state.setFlipY(flip);
        const auto keys=visualItems(scene.root,"spikePanelKey");QCOMPARE(keys.size(),qsizetype(39));
        for(auto key:keys){const auto label=key->property("modelData").toString();
            const auto center=key->mapToScene({key->width()/2,key->height()/2});QVERIFY(center.x()>0&&center.x()<1280&&center.y()>0&&center.y()<800);QVERIFY(key->height()>=48);
            scene.state.setText(label==QString::fromUtf8("⌫")?"xy":"");QVERIFY(scene.click(key));
            const auto expected=label=="Space"?QString(" "):label==QString::fromUtf8("⌫")?QString("x"):label;
            QVERIFY2(scene.state.text()==expected,qPrintable("Hit geometry failed for panel key "+label));
        }
        auto editor=item(scene.root,"spikeTextInput");QVERIFY(editor);
        QVERIFY(scene.click(item(scene.root,"spikeKeyboardRequest")));QCOMPARE(scene.runtime.requests,1);QVERIFY(editor->hasActiveFocus());
        QVERIFY(scene.click(item(scene.root,"spikeKeyboardDone")));QVERIFY(!editor->hasActiveFocus());QCOMPARE(scene.runtime.hides,1);QCOMPARE(scene.runtime.initializations,0);
    }
    void rawAndQtScrollObservationsMoveTheRealList(){
        DiagnosticScene scene;auto grid=item(scene.root,"spikeGrid");QVERIFY(grid);QVERIFY(grid->height()>40);
        QVERIFY(scene.point(grid->mapToScene({grid->width()/2,grid->height()/2}),0));
        const auto before=grid->property("contentY").toDouble();vr::VREvent_t event{};event.eventType=vr::VREvent_ScrollSmooth;
        event.data.scroll.ydelta=-0.25F;event.data.scroll.viewportscale=0.75F;event.data.scroll.cursorIndex=1;QVERIFY(scene.deliver(event));
        event.eventType=vr::VREvent_ScrollDiscrete;event.data.scroll.ydelta=-1;event.data.scroll.cursorIndex=0;QVERIFY(scene.deliver(event));
        QTRY_VERIFY(grid->property("contentY").toDouble()>before);
        const auto rows=scene.state.cursors();QCOMPARE(rows.size(),2);const auto zero=rows[0].toMap(),one=rows[1].toMap();
        QCOMPARE(zero.value("discrete").toInt(),1);QCOMPARE(zero.value("discreteRawY").toDouble(),-1.0);QCOMPARE(zero.value("discreteQtY").toInt(),-120);
        QCOMPARE(one.value("smooth").toInt(),1);QCOMPARE(one.value("smoothRawY").toDouble(),-0.25);QCOMPARE(one.value("smoothQtY").toInt(),-30);QCOMPARE(one.value("viewportScale").toDouble(),0.75);
        // Current global position/x120 behavior is observed, not silently repaired.
        QCOMPARE(one.value("qtX").toDouble(),zero.value("qtX").toDouble());QCOMPARE(one.value("qtY").toDouble(),zero.value("qtY").toDouble());
        QVERIFY(item(scene.root,"spikeScrollPosition")->property("text").toString().contains(QString::number(grid->property("contentY").toDouble(),'f',1)));
    }
    void matchingStaleWrongAndClosedKeyboardSessionsThroughScene(){
        DiagnosticScene scene;auto request=item(scene.root,"spikeKeyboardRequest");QVERIFY(request);QVERIFY(scene.click(request));
        const auto old=scene.runtime.token;scene.packet(vr::VREvent_KeyboardCharInput,old,42,"a");QCOMPARE(scene.state.text(),QString("a"));
        for(const auto type:{vr::VREvent_KeyboardCharInput,vr::VREvent_KeyboardDone,vr::VREvent_KeyboardClosed,vr::VREvent_KeyboardClosed_Global}){
            scene.packet(type,old-1,42,"b");scene.packet(type,old,43,"b");QCOMPARE(scene.state.text(),QString("a"));QCOMPARE(scene.runtime.hides,0);
        }
        scene.packet(vr::VREvent_KeyboardDone,old,42);QCOMPARE(scene.runtime.hides,1);
        QVERIFY(scene.click(request));const auto current=scene.runtime.token;QVERIFY(current>old);
        for(const auto type:{vr::VREvent_KeyboardCharInput,vr::VREvent_KeyboardDone,vr::VREvent_KeyboardClosed,vr::VREvent_KeyboardClosed_Global})scene.packet(type,old,42,"b");
        QCOMPARE(scene.state.text(),QString("a"));QCOMPARE(scene.runtime.hides,1);
        scene.packet(vr::VREvent_KeyboardCharInput,current,42,"é");QCOMPARE(scene.state.text(),QString::fromUtf8("aé"));
        scene.packet(vr::VREvent_KeyboardCharInput,current,42,"漢😀");QCOMPARE(scene.state.text(),QString::fromUtf8("aé漢😀"));
        scene.packet(vr::VREvent_KeyboardCharInput,current,42,"\b");QCOMPARE(scene.state.text(),QString::fromUtf8("aé漢"));
        scene.packet(vr::VREvent_KeyboardClosed,current,42);QVERIFY(!item(scene.root,"spikeTextInput")->hasActiveFocus());QCOMPARE(scene.runtime.hides,1);
        scene.packet(vr::VREvent_KeyboardCharInput,current,42,"b");QCOMPARE(scene.state.text(),QString::fromUtf8("aé漢"));QCOMPARE(scene.runtime.initializations,0);
        // Numeric diagnostic rows never capture payload strings.
        for(const auto &row:scene.state.cursors())for(const auto &value:row.toMap())QVERIFY(value.metaType()!=QMetaType::fromType<QString>());
    }
    void twoIndependentScenesHaveIndependentTextTargetsAndObservations(){
        DiagnosticScene first(true,42),second(false,84);first.activate();
        QVERIFY(first.click(item(first.root,"spikeCorner0")));QCOMPARE(first.state.clickCount(),1);QCOMPARE(second.state.clickCount(),0);QVERIFY(second.state.cursors().isEmpty());
        QVERIFY(first.click(item(first.root,"spikeKeyboardRequest")));first.packet(vr::VREvent_KeyboardCharInput,first.runtime.token,42,"one");
        QCOMPARE(first.state.text(),QString("one"));QVERIFY(second.state.text().isEmpty());
        second.activate();QVERIFY(second.click(item(second.root,"spikeCorner3"),1));QCOMPARE(first.state.cornerHits()[3].toInt(),0);QCOMPARE(second.state.cornerHits()[3].toInt(),1);
        QVERIFY(second.click(item(second.root,"spikeKeyboardRequest")));second.packet(vr::VREvent_KeyboardCharInput,second.runtime.token,42,"x");QVERIFY(second.state.text().isEmpty());
        second.packet(vr::VREvent_KeyboardCharInput,second.runtime.token,84,"two");QCOMPARE(second.state.text(),QString("two"));QCOMPARE(first.state.text(),QString("one"));
        QCOMPARE(first.runtime.initializations,0);QCOMPARE(second.runtime.initializations,0);
    }
};
int main(int argc,char **argv){QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);QGuiApplication app(argc,argv);QQuickStyle::setStyle("Basic");OverlaySceneTest test;return QTest::qExec(&test,argc,argv);}
#include "OverlaySceneTest.moc"
