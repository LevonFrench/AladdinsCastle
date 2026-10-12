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
#include <limits>
class MockNativeReadback final:public ac::NativeOverlayReadbackOps {
public:
    vr::VRTextureBounds_t before{0,1,1,0},after=before;
    vr::EVROverlayError beforeCode=vr::VROverlayError_None,afterCode=vr::VROverlayError_None;
    vr::EVROverlayError queryCode=vr::VROverlayError_ArrayTooSmall,readCode=vr::VROverlayError_None;
    uint32_t width=3,height=2,returnedWidth=3,returnedHeight=2,bufferBytes=0;
    int boundCalls=0,queries=0,copies=0;
    vr::EVROverlayError bounds(vr::VRTextureBounds_t *value)override{
        *value=boundCalls?after:before;return boundCalls++?afterCode:beforeCode;
    }
    vr::EVROverlayError image(void *buffer,uint32_t bytes,uint32_t *w,uint32_t *h)override{
        if(!buffer){++queries;*w=width;*h=height;return queryCode;}
        ++copies;bufferBytes=bytes;*w=returnedWidth;*h=returnedHeight;
        if(bytes==24){QImage pixels(3,2,QImage::Format_RGBA8888);pixels.fill(Qt::green);pixels.setPixelColor(0,1,Qt::blue);std::memcpy(buffer,pixels.constBits(),24);}
        return readCode;
    }
};
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
    void nativeApiPreservesRowsAndReversedBounds(){
        MockNativeReadback ops;QImage image;QJsonObject metadata;QString error;
        QVERIFY2(ac::readNativeOverlayImage(ops,&image,&metadata,&error),qPrintable(error));
        QCOMPARE(image.pixelColor(0,0),QColor(Qt::green));QCOMPARE(image.pixelColor(0,1),QColor(Qt::blue));
        QCOMPARE(ops.bufferBytes,24u);QCOMPARE(ops.queries,1);QCOMPARE(ops.copies,1);QCOMPARE(ops.boundCalls,2);
        QCOMPARE(metadata.value("texture_bounds").toObject().value("vMin").toDouble(),1.0);
        QCOMPARE(metadata.value("texture_bounds").toObject().value("vMax").toDouble(),0.0);
        QCOMPARE(metadata.value("query_result").toInt(),22);QCOMPARE(metadata.value("read_result").toInt(),0);
        QVERIFY(!metadata.value("compositor_display_verified").toBool());QVERIFY(!metadata.value("exact_submission_frame_verified").toBool());
    }
    void nativeApiErrorsReturnNoImage_data(){
        QTest::addColumn<QString>("kind");
        for(const auto *name:{"bounds","query","read","after","zero","oversized","overflow","budget","changed-size","changed-bounds","nan"})QTest::newRow(name)<<QString(name);
    }
    void nativeApiErrorsReturnNoImage(){
        QFETCH(QString,kind);MockNativeReadback ops;QImage image(1,1,QImage::Format_RGBA8888);QJsonObject metadata;QString error;
        if(kind=="bounds")ops.beforeCode=vr::VROverlayError_InvalidHandle;
        if(kind=="query")ops.queryCode=vr::VROverlayError_RequestFailed;
        if(kind=="read")ops.readCode=vr::VROverlayError_ArrayTooSmall;
        if(kind=="after")ops.afterCode=vr::VROverlayError_RequestFailed;
        if(kind=="zero")ops.width=0;
        if(kind=="oversized")ops.width=4097;
        if(kind=="overflow")ops.width=ops.height=std::numeric_limits<uint32_t>::max();
        if(kind=="budget")ops.width=ops.height=4096;
        if(kind=="changed-size")ops.returnedHeight=3;
        if(kind=="changed-bounds")ops.after.vMin=0.5F;
        if(kind=="nan")ops.before.uMin=std::numeric_limits<float>::quiet_NaN();
        QVERIFY(!ac::readNativeOverlayImage(ops,&image,&metadata,&error));QVERIFY(image.isNull());QVERIFY(!error.isEmpty());
        const bool copied=kind=="read"||kind=="after"||kind=="changed-size"||kind=="changed-bounds";
        QCOMPARE(ops.copies,copied?1:0);
        if(!copied)QVERIFY(metadata.value("read_result").isNull());
    }
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
    void nativeDiagnosticKeepsEvidenceSeparate_data(){
        QTest::addColumn<QString>("kind");
        for(const auto *name:{"success","native-error","not-submitted","native-invalid-image"})QTest::newRow(name)<<QString(name);
    }
    void nativeDiagnosticKeepsEvidenceSeparate(){
        QFETCH(QString,kind);QTemporaryDir t;ac::OverlayCapture capture;QString error;
        const auto path=t.filePath(".local/native");QVERIFY(capture.configure(t.path(),path,false,&error));
        MockReadback gl;int nativeReads=0;
        const auto qtReader=[&](QImage *image,QString *message){return ac::readSourceTexture(gl,41,{3,2},image,message);};
        const auto nativeReader=[&](QImage *image,QJsonObject *metadata,QString *message){
            ++nativeReads;*metadata={{"texture_type","OpenGL"},{"query_result",22},{"read_result",kind=="native-error"?23:0}};
            if(kind=="native-error"){*message="Synthetic unsupported native image";return false;}
            *image=QImage(kind=="native-invalid-image"?0:3,2,QImage::Format_RGBA8888);
            if(!image->isNull()){image->fill(Qt::green);image->setPixelColor(0,1,Qt::blue);}return true;
        };
        const bool submitted=kind!="not-submitted";
        QCOMPARE(capture.attempt(qtReader,41,{submitted,0},true,&error,nativeReader),ac::CaptureAttempt::Captured);
        QCOMPARE(nativeReads,submitted?1:0);
        const QImage source(path+"/texture.png");QCOMPARE(source.pixelColor(0,0),QColor(Qt::magenta));
        QFile file(path+"/receipt.json");QVERIFY(file.open(QIODevice::ReadOnly));
        const auto receipt=QJsonDocument::fromJson(file.readAll()).object();
        const auto native=receipt.value("native_overlay").toObject();
        QCOMPARE(native.value("status").toString(),kind=="success"?QString("captured"):submitted?QString("failed"):QString("not-attempted"));
        QVERIFY(!receipt.value("compositor_display_verified").toBool());
        const QImage nativePng(path+"/native.png");
        if(kind=="success"){
            QCOMPARE(nativePng.pixelColor(0,0),QColor(Qt::green));QCOMPARE(nativePng.pixelColor(0,1),QColor(Qt::blue));
            QCOMPARE(native.value("read_result").toInt(),0);QVERIFY(!native.value("compositor_display_verified").toBool());
        }else{QVERIFY(nativePng.isNull());QVERIFY(!native.value("error").toString().isEmpty());}
        QCOMPARE(capture.attempt(qtReader,41,{submitted,0},true,&error,nativeReader),ac::CaptureAttempt::Disabled);
        QCOMPARE(nativeReads,submitted?1:0);
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
