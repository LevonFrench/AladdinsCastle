// SPDX-License-Identifier: GPL-3.0-only
#include "overlay/CompositorCapture.h"
#include "overlay/OverlayHost.h"
#include "overlay/SpikeState.h"
#include "core/app/LaunchOptions.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>
namespace {
class FakeMirrors final:public ac::CompositorReadbackOps {
public:
    int failAcquire=-1,failRead=-1,failRelease=-1,throwRead=-1,invalidAcquire=-1,invalidRead=-1;
    bool partialAcquire=false;
    QList<unsigned int> acquired,locked,unlocked,released;
    bool acquire(unsigned int eye,ac::CompositorMirror *mirror,QString *error)override{
        acquired<<eye;
        if(int(eye)!=failAcquire||partialAcquire)*mirror={41+eye,81+eye};
        if(int(eye)==invalidAcquire)mirror->sharedHandle=0;
        if(int(eye)==failAcquire){*error="synthetic acquisition failure";return false;}
        return true;
    }
    void lock(const ac::CompositorMirror &mirror)noexcept override{locked<<mirror.texture-41;}
    bool read(const ac::CompositorMirror &mirror,QImage *image,QString *error)override{
        const auto eye=mirror.texture-41;
        if(int(eye)==throwRead)throw std::runtime_error("synthetic read exception");
        if(int(eye)==failRead){*error="synthetic read failure";return false;}
        *image=QImage(3,2,int(eye)==invalidRead?QImage::Format_RGB888:QImage::Format_RGBA8888);image->fill(eye?Qt::magenta:Qt::green);return true;
    }
    void unlock(const ac::CompositorMirror &mirror)noexcept override{unlocked<<mirror.texture-41;}
    bool release(const ac::CompositorMirror &mirror)noexcept override{const auto eye=mirror.texture-41;released<<eye;return int(eye)!=failRelease;}
};
}
class CompositorCaptureTest:public QObject {
    Q_OBJECT
private slots:
    void pairPreservesEyeLabelsAndReleases(){
        FakeMirrors ops;std::array<QImage,2> eyes;QString error;
        QVERIFY2(ac::readCompositorEyes(ops,&eyes,&error),qPrintable(error));
        QCOMPARE(ops.acquired,QList<unsigned int>({0,1}));
        QCOMPARE(ops.locked,QList<unsigned int>({0,1}));QCOMPARE(ops.unlocked,ops.locked);
        QCOMPARE(ops.released,QList<unsigned int>({1,0}));
        QCOMPARE(eyes[0].pixelColor(0,0),QColor(Qt::green));QCOMPARE(eyes[1].pixelColor(0,0),QColor(Qt::magenta));
    }
    void failedSecondAcquireReleasesFirstAndPartial(){
        for(bool partial:{false,true}){
            FakeMirrors ops;ops.failAcquire=1;ops.partialAcquire=partial;std::array<QImage,2> eyes;QString error;
            QVERIFY(!ac::readCompositorEyes(ops,&eyes,&error));QVERIFY(eyes[0].isNull()&&eyes[1].isNull());
            QCOMPARE(ops.released,partial?QList<unsigned int>({1,0}):QList<unsigned int>({0}));QVERIFY(ops.locked.isEmpty());
        }
    }
    void readFailuresAndExceptionsUnlockAndRelease(){
        for(int eye:{0,1})for(bool throwing:{false,true}){
            FakeMirrors ops;if(throwing)ops.throwRead=eye;else ops.failRead=eye;
            std::array<QImage,2> eyes;QString error;
            QVERIFY(!ac::readCompositorEyes(ops,&eyes,&error));QVERIFY(eyes[0].isNull()&&eyes[1].isNull());
            QCOMPARE(ops.unlocked,ops.locked);QCOMPARE(ops.released,QList<unsigned int>({1,0}));
        }
    }
    void invalidSuccessfulApiResultsFailWithErrorAndCleanup(){
        for(int eye:{0,1}){
            FakeMirrors ops;ops.invalidAcquire=eye;std::array<QImage,2> eyes;QString error;
            QVERIFY(!ac::readCompositorEyes(ops,&eyes,&error));
            QCOMPARE(error,QString("Compositor acquisition returned an invalid mirror."));
            QVERIFY(eyes[0].isNull()&&eyes[1].isNull());QVERIFY(ops.locked.isEmpty());
            QCOMPARE(ops.released,eye?QList<unsigned int>({1,0}):QList<unsigned int>({0}));
        }
        for(int eye:{0,1}){
            FakeMirrors ops;ops.invalidRead=eye;std::array<QImage,2> eyes;QString error;
            QVERIFY(!ac::readCompositorEyes(ops,&eyes,&error));
            QCOMPARE(error,QString("Compositor mirror returned invalid pixels."));
            QVERIFY(eyes[0].isNull()&&eyes[1].isNull());QCOMPARE(ops.unlocked,ops.locked);
            QCOMPARE(ops.released,QList<unsigned int>({1,0}));
        }
    }
    void releaseErrorNeverPublishesPair(){
        FakeMirrors ops;ops.failRelease=1;std::array<QImage,2> eyes;QString error;
        eyes[0]=QImage(1,1,QImage::Format_RGBA8888);eyes[1]=eyes[0];
        QVERIFY(!ac::readCompositorEyes(ops,&eyes,&error));QVERIFY(eyes[0].isNull()&&eyes[1].isNull());
        QCOMPARE(ops.released,QList<unsigned int>({1,0}));QCOMPARE(ops.unlocked,ops.locked);
    }
    void freshOutputIsCompositorEvidenceWithoutPrivateMetadata(){
        QTemporaryDir root;QVERIFY(root.isValid());const auto directory=root.path()+"/.local/pair";
        FakeMirrors ops;std::array<QImage,2> eyes;QString error;QVERIFY(ac::readCompositorEyes(ops,&eyes,&error));
        QVERIFY2(ac::writeCompositorEyes(root.path(),directory,eyes,false,&error),qPrintable(error));
        QCOMPARE(QImage(directory+"/left.png").pixelColor(0,0),QColor(Qt::green));
        QCOMPARE(QImage(directory+"/right.png").pixelColor(0,0),QColor(Qt::magenta));
        QFile receipt(directory+"/receipt.json");QVERIFY(receipt.open(QIODevice::ReadOnly));const auto bytes=receipt.readAll();
        const auto object=QJsonDocument::fromJson(bytes).object();
        QCOMPARE(object["evidence"].toString(),QString("OpenVR-compositor-mirror"));
        QVERIFY(!object["exact_frame_sync"].toBool());QVERIFY(object["content_visual_review_required"].toBool());
        QVERIFY(!object["physical_headset_verified"].toBool());QVERIFY(!object["dashboard_visible"].toBool());
        QVERIFY(!bytes.contains(root.path().toUtf8()));
        QVERIFY(!ac::writeCompositorEyes(root.path(),directory,eyes,true,&error));
        QFile after(directory+"/receipt.json");QVERIFY(after.open(QIODevice::ReadOnly));QCOMPARE(after.readAll(),bytes);
    }
    void invalidPairAndOutsideDirectoryRefusedBeforeWriting(){
        QTemporaryDir root,outside;QVERIFY(root.isValid()&&outside.isValid());QString error;
        std::array<QImage,2> invalid;
        QVERIFY(!ac::writeCompositorEyes(root.path(),root.path()+"/.local/pair",invalid,false,&error));
        QVERIFY(!QFileInfo::exists(root.path()+"/.local/pair"));
        QVERIFY(!ac::validateCompositorDirectory(root.path(),outside.path()+"/pair",&error));
        QVERIFY(!ac::validateCompositorDirectory(root.path(),"relative/pair",&error));
        QVERIFY(!ac::validateCompositorDirectory(root.path(),root.path()+"/.local",&error));
        QVERIFY(!ac::validateCompositorDirectory(root.path(),root.path()+"/.local/../escaped",&error));
    }
    void dashboardGuardDoesNotAdmitContaminatedScene(){
        QVERIFY(ac::compositorDashboardMatches("hidden",false));QVERIFY(!ac::compositorDashboardMatches("hidden",true));
        QVERIFY(ac::compositorDashboardMatches("visible",true));QVERIFY(!ac::compositorDashboardMatches("visible",false));
        QVERIFY(ac::compositorDashboardMatches("any",false)&&ac::compositorDashboardMatches("any",true));
        QVERIFY(!ac::compositorDashboardMatches("invalid",false));
    }
    void isolatedModeAndExplicitDashboardParser(){
        QTemporaryDir root;const auto path=root.path()+"/.local/pair";
        const auto capture=ac::parseLaunchOptions({"--capture-compositor-dir",path,"--compositor-dashboard","hidden"});
        QVERIFY(capture.error.isEmpty());QCOMPARE(capture.mode,ac::Mode::CompositorCapture);
        QCOMPARE(capture.compositorDashboard,QString("hidden"));QVERIFY(ac::wantsParentConsole(capture));
        QVERIFY(!ac::shouldOpenDesktop(capture,false));QVERIFY(!capture.showDashboard);
        const auto show=ac::parseLaunchOptions({"--overlay","--show-dashboard"});QVERIFY(show.error.isEmpty()&&show.showDashboard);
        for(const auto &args:QList<QStringList>{{"--show-dashboard"},{"--capture-compositor-dir",path,"--show-dashboard"},
                {"--overlay","--capture-compositor-dir",path},{"--capture-compositor-dir",path,"--quit-after-ms","100"},
                {"--compositor-dashboard","hidden"},{"--capture-compositor-dir",path,"--compositor-dashboard","invalid"},
                {"--capture-compositor-dir","relative"}})QVERIFY(!ac::parseLaunchOptions(args).error.isEmpty());
    }
    void uninitializedHostDoesNotShowDashboard(){
        ac::SpikeState state;ac::OverlayHost host(state);QString error;QVERIFY(!host.showDashboard(&error));
    }
};
QTEST_GUILESS_MAIN(CompositorCaptureTest)
#include "CompositorCaptureTest.moc"
