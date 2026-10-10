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
    explicit DiagnosticScene(bool controlled=false,quint64 handle=42,ac::OverlayInput::Clock clock={})
        :input({1280,800},std::move(clock)),renderControl(controlled?std::make_unique<QQuickRenderControl>():nullptr),window(renderControl.get()) {
        runtime.overlay=handle;
        input.setDiagnosticObserver([this](const auto &event,const auto &value){state.recordInput(event,value);});
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
    void contractScene(){
        QQmlComponent component(&engine);component.setData(R"(
import QtQuick
import QtQuick.Controls
Item {
 property int activations: 0
 Button {objectName:"contractButton";x:20;y:20;width:200;height:100;text:"Button";onClicked:parent.activations++}
 Rectangle {x:300;y:20;width:200;height:100;MouseArea{objectName:"contractMouseArea";anchors.fill:parent;onClicked:parent.parent.activations++}}
 Rectangle {objectName:"contractTap";x:600;y:20;width:200;height:100;TapHandler{onTapped:parent.parent.activations++}}
 Flickable {id:left;objectName:"contractLeftList";x:20;y:180;width:350;height:500;contentWidth:350;contentHeight:1600;boundsBehavior:Flickable.StopAtBounds
  Button{objectName:"contractFlickButton";width:300;height:100;text:"List button";onClicked:left.parent.activations++}
 }
 Flickable {objectName:"contractRightList";x:500;y:180;width:350;height:500;contentWidth:350;contentHeight:1600;boundsBehavior:Flickable.StopAtBounds}
}
)",QUrl());auto replacement=qobject_cast<QQuickItem *>(component.create());if(!replacement)qFatal("Contract scene did not load");
        delete root;root=replacement;root->setParent(window.contentItem());root->setParentItem(window.contentItem());root->setSize({1280,800});QCoreApplication::processEvents();activate();
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
    void focusHideCancellationCannotActivateAndFreshClickRecovers_data(){QTest::addColumn<int>("kind");QTest::newRow("focus leave")<<int(vr::VREvent_FocusLeave);QTest::newRow("hidden")<<int(vr::VREvent_OverlayHidden);}
    void focusHideCancellationCannotActivateAndFreshClickRecovers(){
        QFETCH(int,kind);DiagnosticScene scene;auto target=item(scene.root,"spikeCorner0");QVERIFY(target);
        const auto center=target->mapToScene({target->width()/2,target->height()/2});
        QVERIFY(scene.point(center));QVERIFY(scene.point(center,0,vr::VREvent_MouseButtonDown));
        vr::VREvent_t event{};event.eventType=quint32(kind);QVERIFY(scene.deliver(event));
        QCOMPARE(scene.state.clickCount(),0);QCOMPARE(scene.input.buttons(),Qt::NoButton);
        scene.point(center,0,vr::VREvent_MouseButtonUp);QCOMPARE(scene.state.clickCount(),0);
        event.eventType=kind==int(vr::VREvent_OverlayHidden)?vr::VREvent_OverlayShown:vr::VREvent_FocusEnter;QVERIFY(scene.deliver(event));
        QVERIFY(scene.click(target));QCOMPARE(scene.state.clickCount(),1);
    }
    void allQtControlKindsCancelAndRecover_data(){
        QTest::addColumn<QString>("target");QTest::addColumn<int>("cancelKind");
        for(const auto &target:{"contractButton","contractMouseArea","contractTap","contractFlickButton"})
          for(const auto kind:{0,1,2,3,4})QTest::newRow(qPrintable(QString(target)+QString::number(kind)))<<QString(target)<<kind;
    }
    void allQtControlKindsCancelAndRecover(){
        QFETCH(QString,target);QFETCH(int,cancelKind);DiagnosticScene scene;scene.contractScene();auto control=item(scene.root,target);QVERIFY(control);
        const auto center=control->mapToScene({control->width()/2,control->height()/2});QVERIFY(scene.point(center));QVERIFY(scene.point(center,0,vr::VREvent_MouseButtonDown));
        if(cancelKind==0||cancelKind==1){vr::VREvent_t event{};event.eventType=cancelKind?vr::VREvent_OverlayHidden:vr::VREvent_FocusLeave;QVERIFY(scene.deliver(event));}
        else if(cancelKind==2){QFocusEvent event(QEvent::FocusOut,Qt::OtherFocusReason);QCoreApplication::sendEvent(&scene.window,&event);}
        else if(cancelKind==3){delete control;control=nullptr;}
        else QVERIFY(!scene.point({-1,100}));
        QCOMPARE(scene.root->property("activations").toInt(),0);QCOMPARE(scene.input.buttons(),Qt::NoButton);
        QVERIFY(!scene.point(center,0,vr::VREvent_MouseButtonUp));QCOMPARE(scene.root->property("activations").toInt(),0);
        vr::VREvent_t resumed{};resumed.eventType=vr::VREvent_OverlayShown;QVERIFY(scene.deliver(resumed));scene.activate();if(!control)scene.contractScene();control=item(scene.root,target);QVERIFY(control);QVERIFY(scene.click(control));QCOMPARE(scene.root->property("activations").toInt(),1);
    }
    void wheelUsesItsCursorListAndNativeDesktopStillWorks(){
        DiagnosticScene scene;scene.contractScene();auto left=item(scene.root,"contractLeftList"),right=item(scene.root,"contractRightList");QVERIFY(left);QVERIFY(right);
        QVERIFY(scene.point(left->mapToScene({200,250}),0));QVERIFY(scene.point(right->mapToScene({200,250}),1));
        vr::VREvent_t event{};event.eventType=vr::VREvent_ScrollDiscrete;event.data.scroll.ydelta=-1;event.data.scroll.cursorIndex=0;QVERIFY(scene.deliver(event));
        QTRY_VERIFY(left->property("contentY").toDouble()>0);QCOMPARE(right->property("contentY").toDouble(),0.0);
        event.data.scroll.cursorIndex=1;QVERIFY(scene.deliver(event));QTRY_VERIFY(right->property("contentY").toDouble()>0);
        auto button=item(scene.root,"contractButton");QVERIFY(button);QTest::mouseClick(&scene.window,Qt::LeftButton,Qt::NoModifier,button->mapToScene({50,50}).toPoint());QCOMPARE(scene.root->property("activations").toInt(),1);
        const auto old=right->property("contentY").toDouble();const auto where=right->mapToScene({200,250});QWheelEvent native(where,where,{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(&scene.window,&native);QTRY_VERIFY(right->property("contentY").toDouble()>old);
    }
    void cancelSyntheticDeviceDoesNotReleaseNativeDesktopPress(){
        DiagnosticScene scene;scene.contractScene();auto button=item(scene.root,"contractButton"),area=item(scene.root,"contractMouseArea");QVERIFY(button);QVERIFY(area);
        const auto native=button->mapToScene({50,50}).toPoint();QTest::mousePress(&scene.window,Qt::LeftButton,Qt::NoModifier,native);QVERIFY(button->property("down").toBool());
        const auto virtualPoint=area->mapToScene({50,50});QVERIFY(scene.point(virtualPoint));QVERIFY(scene.point(virtualPoint,0,vr::VREvent_MouseButtonDown));scene.input.releaseButtons(&scene.window);
        QVERIFY(button->property("down").toBool());QCOMPARE(scene.root->property("activations").toInt(),0);
        QTest::mouseRelease(&scene.window,Qt::LeftButton,Qt::NoModifier,native);QCOMPARE(scene.root->property("activations").toInt(),1);
        QVERIFY(!scene.point(virtualPoint,0,vr::VREvent_MouseButtonUp));
    }
    void receiverChangeAndSuppressedOwnerRecover(){
        DiagnosticScene first,second;auto control=item(first.root,"spikeCorner0");QVERIFY(control);const auto where=control->mapToScene({50,30});
        QVERIFY(first.point(where));QVERIFY(first.point(where,0,vr::VREvent_MouseButtonDown));
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={100,700,0,0};QVERIFY(first.input.dispatch(event,&second.window));
        QCOMPARE(first.state.clickCount(),0);QCOMPARE(first.input.buttons(),Qt::NoButton);
        event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.button=vr::VRMouseButton_Left;QVERIFY(!first.input.dispatch(event,&second.window));event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!first.input.dispatch(event,&second.window));
        event.eventType=vr::VREvent_MouseMove;QVERIFY(first.input.dispatch(event,&second.window));event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(first.input.dispatch(event,&second.window));
        first.input.releaseButtons(&second.window);QCOMPARE(first.state.clickCount(),0);QCOMPARE(second.state.clickCount(),0);
    }
    void receiverDestructionAndShutdownCancelActualQtGrabs(){
        DiagnosticScene survivor;survivor.contractScene();auto receiver=std::make_unique<QQuickWindow>();receiver->setGeometry(0,0,1280,800);
        QQmlComponent component(&survivor.engine);component.setData("import QtQuick; import QtQuick.Controls; Button {width:200;height:100;text: 'temporary'}",QUrl());
        auto target=qobject_cast<QQuickItem *>(component.create());QVERIFY(target);target->setParent(receiver->contentItem());target->setParentItem(receiver->contentItem());receiver->show();QTest::qWait(10);QSignalSpy clicks(target,SIGNAL(clicked()));QVERIFY(clicks.isValid());
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseMove;event.data.mouse={50,750,0,0};QVERIFY(survivor.input.dispatch(event,receiver.get()));event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.button=vr::VRMouseButton_Left;QVERIFY(survivor.input.dispatch(event,receiver.get()));
        receiver.reset();QCOMPARE(survivor.input.buttons(),Qt::NoButton);QCOMPARE(clicks.count(),0);
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(!survivor.input.dispatch(event,&survivor.window));survivor.activate();QVERIFY(survivor.click(item(survivor.root,"contractButton")));QCOMPARE(survivor.root->property("activations").toInt(),1);
        auto separate=std::make_unique<ac::OverlayInput>();QVERIFY(separate->dispatch(event,&survivor.window)==false);event.eventType=vr::VREvent_MouseMove;QVERIFY(separate->dispatch(event,&survivor.window));event.eventType=vr::VREvent_MouseButtonDown;QVERIFY(separate->dispatch(event,&survivor.window));separate.reset();QCOMPARE(survivor.root->property("activations").toInt(),1);
        QTest::mouseClick(&survivor.window,Qt::LeftButton,Qt::NoModifier,QPoint(70,70));QCOMPARE(survivor.root->property("activations").toInt(),2);
    }
    void externalTargetDeathAdvancesReceiptAndCannotRearmEarly(){
        qint64 now=100;DiagnosticScene scene(false,42,[&]{return now;});scene.contractScene();auto target=item(scene.root,"contractButton");QVERIFY(target);const auto point=target->mapToScene({50,50});QVERIFY(scene.point(point));now=101;QVERIFY(scene.point(point,0,vr::VREvent_MouseButtonDown));
        now=200;delete target;QCOMPARE(scene.input.buttons(),Qt::NoButton);now=150;QVERIFY(!scene.point(point));QCOMPARE(scene.root->property("activations").toInt(),0);
        now=201;QVERIFY(!scene.point(point,0,vr::VREvent_MouseButtonUp));scene.contractScene();QVERIFY(scene.click(item(scene.root,"contractButton")));QCOMPARE(scene.root->property("activations").toInt(),1);
    }
    void inactivePacketsDoNotEstablishAFreshEpoch(){
        DiagnosticScene scene;auto target=item(scene.root,"spikeCorner0");QVERIFY(target);const auto where=target->mapToScene({50,30});QVERIFY(scene.point(where));
        vr::VREvent_t event{};event.eventType=vr::VREvent_OverlayHidden;QVERIFY(scene.deliver(event));QVERIFY(!scene.point(where));QVERIFY(!scene.point(where,0,vr::VREvent_MouseButtonDown));
        event.eventType=vr::VREvent_OverlayShown;QVERIFY(scene.deliver(event));QVERIFY(scene.point(where));QVERIFY(!scene.point(where,0,vr::VREvent_MouseButtonDown));QVERIFY(!scene.point(where,0,vr::VREvent_MouseButtonUp));QVERIFY(scene.click(target));QCOMPARE(scene.state.clickCount(),1);
    }
    void nativeFlickableDragCancelsWithoutClickAndRecovers(){
        DiagnosticScene scene;scene.contractScene();auto list=item(scene.root,"contractLeftList"),button=item(scene.root,"contractFlickButton");QVERIFY(list);QVERIFY(button);const auto where=button->mapToScene({150,50});QVERIFY(scene.point(where));QVERIFY(scene.point(where,0,vr::VREvent_MouseButtonDown));
        for(const auto delta:{20,60,90}){QTest::qWait(2);QVERIFY(scene.point(where-QPointF(0,delta)));}
        QTRY_VERIFY(list->property("contentY").toDouble()>0);scene.input.releaseButtons(&scene.window);QVERIFY(!list->property("dragging").toBool());QCOMPARE(scene.root->property("activations").toInt(),0);
        QVERIFY(!scene.point(where,0,vr::VREvent_MouseButtonUp));list->setProperty("contentY",0.0);QVERIFY(scene.click(button));QCOMPARE(scene.root->property("activations").toInt(),1);
    }
    void oldQueuedCleanupCannotReleaseANewGesture_data(){QTest::addColumn<bool>("rebind");QTest::newRow("surviving receiver")<<false;QTest::newRow("new receiver")<<true;}
    void oldQueuedCleanupCannotReleaseANewGesture(){
        QFETCH(bool,rebind);DiagnosticScene scene;scene.contractScene();std::unique_ptr<DiagnosticScene> other;
        if(rebind){other=std::make_unique<DiagnosticScene>(true,84);other->contractScene();}
        scene.activate();auto old=item(scene.root,"contractButton");QVERIFY(old);const auto oldPoint=old->mapToScene({50,50});QVERIFY(scene.point(oldPoint));QVERIFY(scene.point(oldPoint,0,vr::VREvent_MouseButtonDown));delete old;QCOMPARE(scene.input.buttons(),Qt::NoButton);
        auto receiver=rebind?&other->window:&scene.window;auto root=rebind?other->root:scene.root;auto next=item(root,rebind?"contractButton":"contractFlickButton");QVERIFY(next);
        vr::VREvent_t event{};event.eventType=vr::VREvent_MouseButtonUp;event.data.mouse={0,0,vr::VRMouseButton_Left,0};QVERIFY(!scene.input.dispatch(event,receiver));
        const auto point=next->mapToScene({50,50});event.eventType=vr::VREvent_MouseMove;event.data.mouse={float(point.x()),float(800-point.y()),0,0};QVERIFY(scene.input.dispatch(event,receiver));event.eventType=vr::VREvent_MouseButtonDown;event.data.mouse.button=vr::VRMouseButton_Left;QVERIFY(scene.input.dispatch(event,receiver));QVERIFY(next->property("down").toBool());
        QCoreApplication::processEvents();QCOMPARE(scene.input.buttons(),Qt::LeftButton);QVERIFY(next->property("down").toBool());QCOMPARE(scene.root->property("activations").toInt(),0);QCOMPARE(root->property("activations").toInt(),0);
        event.eventType=vr::VREvent_MouseButtonUp;QVERIFY(scene.input.dispatch(event,receiver));QCOMPARE(root->property("activations").toInt(),1);QCOMPARE(scene.input.buttons(),Qt::NoButton);
    }
    void hostVisibilityCannotBeOverriddenByQueuedEvents_data(){QTest::addColumn<bool>("falseHost");QTest::newRow("stale shown under false host")<<true;QTest::newRow("queued hidden under earlier true host")<<false;}
    void hostVisibilityCannotBeOverriddenByQueuedEvents(){
        QFETCH(bool,falseHost);DiagnosticScene scene;auto target=item(scene.root,"spikeCorner0");QVERIFY(target);const auto point=target->mapToScene({50,30});
        vr::VREvent_t event{};if(falseHost){scene.input.setVisible(false);event.eventType=vr::VREvent_OverlayShown;QVERIFY(scene.deliver(event));}
        else{scene.input.setVisible(true);event.eventType=vr::VREvent_OverlayHidden;QVERIFY(scene.deliver(event));scene.input.setVisible(true);}
        scene.point(point);scene.point(point,0,vr::VREvent_MouseButtonDown);scene.point(point,0,vr::VREvent_MouseButtonUp);QCOMPARE(scene.state.clickCount(),0);
        if(falseHost)scene.input.setVisible(true);else{event.eventType=vr::VREvent_OverlayShown;QVERIFY(scene.deliver(event));}
        QVERIFY(!scene.point(point,0,vr::VREvent_MouseButtonDown));QVERIFY(!scene.point(point,0,vr::VREvent_MouseButtonUp));QVERIFY(scene.click(target));QCOMPARE(scene.state.clickCount(),1);
    }
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
        QVERIFY(scene.point(grid->mapToScene({grid->width()/2+20,grid->height()/2}),1));
        const auto before=grid->property("contentY").toDouble();vr::VREvent_t event{};event.eventType=vr::VREvent_ScrollSmooth;
        event.data.scroll.ydelta=-0.25F;event.data.scroll.viewportscale=0.75F;event.data.scroll.cursorIndex=1;QVERIFY(scene.deliver(event));
        event.eventType=vr::VREvent_ScrollDiscrete;event.data.scroll.ydelta=-1;event.data.scroll.cursorIndex=0;QVERIFY(scene.deliver(event));
        QTRY_VERIFY(grid->property("contentY").toDouble()>before);
        const auto rows=scene.state.cursors();QCOMPARE(rows.size(),2);const auto zero=rows[0].toMap(),one=rows[1].toMap();
        QCOMPARE(zero.value("discrete").toInt(),1);QCOMPARE(zero.value("discreteRawY").toDouble(),-1.0);QCOMPARE(zero.value("discreteQtY").toInt(),-120);
        QCOMPARE(one.value("smooth").toInt(),1);QCOMPARE(one.value("smoothRawY").toDouble(),-0.25);QCOMPARE(one.value("smoothQtY").toInt(),-30);QCOMPARE(one.value("viewportScale").toDouble(),0.75);
        // Each scroll uses its cursor position; the x120 factors are unchanged.
        QVERIFY(one.value("qtX").toDouble()!=zero.value("qtX").toDouble());QCOMPARE(one.value("qtY").toDouble(),zero.value("qtY").toDouble());
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
        for(const auto &row:scene.state.cursors()){const auto map=row.toMap();for(auto it=map.cbegin();it!=map.cend();++it)if(it.key()!="reason")QVERIFY(it.value().metaType()!=QMetaType::fromType<QString>());}
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
