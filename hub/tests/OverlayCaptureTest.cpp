// SPDX-License-Identifier: GPL-3.0-only
#include "overlay/OverlayCapture.h"
#include "overlay/QuickTextureRenderer.h"
#include "core/app/LaunchOptions.h"
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <array>
#include <cstring>
#include <utility>
#include <filesystem>
class MockReadback final:public ac::TextureReadbackOps {
public:
    std::array<int,7> state{17,29,31,8,23,3,7};
    QMap<unsigned int,int> selectors{{17,29}};
    bool ready=true,complete=true,rgba8=true,readError=false,restoreError=false;
    unsigned int pendingError=0,attached=0,deleted=0;
    int reads=0,creates=0,errorChecks=0,drawFramebuffer=77,textureBinding=83;
    QImage bottomFirst{3,2,QImage::Format_RGBA8888};
    MockReadback(){
        bottomFirst.setPixelColor(0,0,Qt::blue);bottomFirst.setPixelColor(1,0,Qt::white);bottomFirst.setPixelColor(2,0,QColor("#ff8800"));
        bottomFirst.setPixelColor(0,1,Qt::magenta);bottomFirst.setPixelColor(1,1,Qt::cyan);bottomFirst.setPixelColor(2,1,Qt::green);
    }
    bool supported()const override{return ready;}
    int integer(ac::ReadbackState key)override{return state.at(static_cast<size_t>(key));}
    unsigned int error()override{++errorChecks;return std::exchange(pendingError,0u);}
    unsigned int createFramebuffer()override{++creates;return 99;}
    void bindReadFramebuffer(unsigned int value)override{state[0]=int(value);state[1]=selectors.value(value);}
    void attachTexture(unsigned int texture)override{attached=texture;}
    bool framebufferComplete()override{return complete;}
    bool rgba8Attachment()override{return rgba8;}
    void selectColorAttachment()override{restoreReadBuffer(123);}
    void restoreReadBuffer(int value)override{state[1]=value;selectors[static_cast<unsigned int>(state[0])]=value;}
    void bindPackBuffer(unsigned int value)override{state[2]=int(value);}
    void pack(ac::ReadbackState key,int value)override{state.at(static_cast<size_t>(key))=value;}
    void readRgba(QSize size,unsigned char *pixels)override{
        ++reads;
        if(size!=bottomFirst.size()){pendingError=0x0502;return;}
        std::memcpy(pixels,bottomFirst.constBits(),static_cast<size_t>(bottomFirst.sizeInBytes()));
        if(readError)pendingError=0x0502;
    }
    void deleteFramebuffer(unsigned int value)override{deleted=value;if(restoreError)pendingError=0x0502;}
};
class OverlayCaptureTest:public QObject {
    Q_OBJECT
private slots:
    void oneRowConversionUsesExactTextureAndRestoresState(){
        MockReadback gl;const auto before=gl.state;QImage image;QString error;
        QVERIFY2(ac::readSourceTexture(gl,41,{3,2},&image,&error),qPrintable(error));
        QCOMPARE(image.pixelColor(0,0),QColor(Qt::magenta));QCOMPARE(image.pixelColor(2,0),QColor(Qt::green));
        QCOMPARE(image.pixelColor(0,1),QColor(Qt::blue));QCOMPARE(image.pixelColor(2,1),QColor("#ff8800"));
        QCOMPARE(image.pixelColor(1,0),QColor(Qt::cyan));QCOMPARE(image.pixelColor(1,1),QColor(Qt::white));
        QVERIFY(gl.state==before);QCOMPARE(gl.attached,41u);QCOMPARE(gl.deleted,99u);QCOMPARE(gl.reads,1);
        QCOMPARE(gl.drawFramebuffer,77);QCOMPARE(gl.textureBinding,83);
    }
    void failuresRestoreStateAndReturnNoImage_data(){
        QTest::addColumn<QString>("kind");for(const auto *name:{"context","incomplete","format","read","restore"})QTest::newRow(name)<<QString(name);
    }
    void failuresRestoreStateAndReturnNoImage(){
        QFETCH(QString,kind);MockReadback gl;const auto before=gl.state;QImage image(1,1,QImage::Format_RGBA8888);QString error;
        gl.ready=kind!="context";gl.complete=kind!="incomplete";gl.rgba8=kind!="format";gl.readError=kind=="read";gl.restoreError=kind=="restore";
        QVERIFY(!ac::readSourceTexture(gl,41,{3,2},&image,&error));QVERIFY(image.isNull());QVERIFY(!error.isEmpty());
        QVERIFY(gl.state==before);QCOMPARE(gl.drawFramebuffer,77);QCOMPARE(gl.textureBinding,83);
        QCOMPARE(gl.creates,kind=="context"?0:1);if(gl.creates)QCOMPARE(gl.deleted,99u);
    }
    void preexistingErrorIsNotDrainedOrRendered(){
        MockReadback gl;gl.pendingError=0x0500;QImage image;QString error;
        QVERIFY(!ac::readSourceTexture(gl,41,{3,2},&image,&error));QCOMPARE(gl.errorChecks,1);QCOMPARE(gl.creates,0);QCOMPARE(gl.reads,0);
        QVERIFY(!ac::readSourceTexture(gl,0,{3,2},&image,&error));QCOMPARE(gl.creates,0);
        QVERIFY(!ac::readSourceTexture(gl,41,{4096,4096},&image,&error));QCOMPARE(gl.creates,0);
    }
    void rendererDoesNotCreateAnImplicitReadbackContext(){
        ac::QuickTextureRenderer renderer;QImage image(1,1,QImage::Format_RGBA8888);QString error;
        QVERIFY(!renderer.window());QVERIFY(!renderer.readbackCurrentTexture(&image,&error));QVERIFY(image.isNull());QVERIFY(!renderer.window());
    }
    void optInCliIsOverlayOnly(){
        QTemporaryDir t;const auto path=t.filePath(".local/fresh");
        const auto options=ac::parseLaunchOptions({"--overlay","--overlay-capture-dir",path});
        QVERIFY2(options.error.isEmpty(),qPrintable(options.error));QCOMPARE(options.overlayCaptureDir,path);QVERIFY(ac::wantsParentConsole(options));
        for(const auto &args:QList<QStringList>{{"--overlay-capture-dir",path},{"--overlay","--overlay-capture-dir"},{"--overlay","--overlay-capture-dir","relative"},{"--overlay","--overlay-capture-dir",path,"--overlay-capture-dir",path}})
            QVERIFY(!ac::parseLaunchOptions(args).error.isEmpty());
    }
    void defaultAndFailureAttemptNeverWriteOrRetry(){
        QTemporaryDir t;ac::OverlayCapture disabled;int reads=0;QString error;
        const auto reader=[&](QImage *,QString *message){++reads;*message="Synthetic read failure";return false;};
        QCOMPARE(disabled.attempt(reader,41,{},true,&error),ac::CaptureAttempt::Disabled);QCOMPARE(reads,0);
        ac::OverlayCapture enabled;const auto path=t.filePath(".local/failed");QVERIFY(enabled.configure(t.path(),path,false,&error));
        QCOMPARE(enabled.attempt(reader,41,{},true,&error),ac::CaptureAttempt::Failed);QCOMPARE(reads,1);QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(enabled.attempt(reader,41,{},true,&error),ac::CaptureAttempt::Disabled);QCOMPARE(reads,1);
    }
    void receiptKeepsBoundsSeparateAndHiddenNoopCannotClaimSubmission_data(){
        QTest::addColumn<bool>("called");QTest::newRow("hidden noop")<<false;QTest::newRow("actual API success")<<true;
    }
    void receiptKeepsBoundsSeparateAndHiddenNoopCannotClaimSubmission(){
        QFETCH(bool,called);QTemporaryDir t;ac::OverlayCapture capture;QString error;const auto path=t.filePath(".local/captured");
        QVERIFY(capture.configure(t.path(),path,true,&error));MockReadback gl;
        const auto reader=[&](QImage *image,QString *message){return ac::readSourceTexture(gl,41,{3,2},image,message);};
        QCOMPARE(capture.attempt(reader,41,{called,0},false,&error),ac::CaptureAttempt::Captured);
        QFile file(path+"/receipt.json");QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();const auto receipt=QJsonDocument::fromJson(bytes).object();
        QCOMPARE(receipt.value("submission_api_called").toBool(),called);QCOMPARE(receipt.value("submission_api_succeeded").toBool(),called);
        QCOMPARE(receipt.value("submission_api_result").isNull(),!called);QVERIFY(!receipt.value("compositor_display_verified").toBool());
        QCOMPARE(receipt.value("uv_bounds").toObject().value("vMin").toDouble(),1.0);QCOMPARE(receipt.value("uv_bounds").toObject().value("vMax").toDouble(),0.0);
        QVERIFY(receipt.value("picture_flip_y").toBool());QVERIFY(!receipt.value("pointer_flip_y").toBool());
        QVERIFY(!bytes.contains(t.path().toUtf8()));QVERIFY(!receipt.contains("games"));
        const QImage png(path+"/texture.png");QCOMPARE(png.pixelColor(0,0),QColor(Qt::magenta));QCOMPARE(png.pixelColor(0,1),QColor(Qt::blue));
        QCOMPARE(capture.attempt(reader,41,{called,0},false,&error),ac::CaptureAttempt::Disabled);QCOMPARE(gl.reads,1);
    }
    void reparseOutputParentsAreRefused(){
        QTemporaryDir t;const auto outside=t.filePath("outside"),link=t.filePath(".local/link");
        QDir().mkpath(outside);QDir().mkpath(t.filePath(".local"));std::error_code code;
#ifdef Q_OS_WIN
        std::filesystem::create_directory_symlink(std::filesystem::path(outside.toStdWString()),std::filesystem::path(link.toStdWString()),code);
#else
        std::filesystem::create_directory_symlink(std::filesystem::path(outside.toStdString()),std::filesystem::path(link.toStdString()),code);
#endif
        if(code){QSKIP("Directory symlink creation is unavailable; no privilege change attempted.");}
        ac::OverlayCapture capture;QString error;QVERIFY(!capture.configure(t.path(),link+"/fresh",false,&error));
        QVERIFY(!QFileInfo::exists(outside+"/fresh"));
    }
    void confinedFreshPathsAreCheckedAgainBeforeWriting(){
        QTemporaryDir t;QString error;ac::OverlayCapture outside,existing,raced;
        QVERIFY(!outside.configure(t.path(),t.filePath("outside"),false,&error));
        QDir().mkpath(t.filePath(".local/existing"));QVERIFY(!existing.configure(t.path(),t.filePath(".local/existing"),false,&error));
        const auto target=t.filePath(".local/raced");QVERIFY(raced.configure(t.path(),target,false,&error));
        const auto reader=[&](QImage *image,QString *){*image=QImage(3,2,QImage::Format_RGBA8888);QDir().mkpath(target);return true;};
        QCOMPARE(raced.attempt(reader,41,{},true,&error),ac::CaptureAttempt::Failed);QVERIFY(!QFileInfo::exists(target+"/receipt.json"));QVERIFY(!QFileInfo::exists(target+"/texture.png"));
    }
};
QTEST_GUILESS_MAIN(OverlayCaptureTest)
#include "OverlayCaptureTest.moc"
