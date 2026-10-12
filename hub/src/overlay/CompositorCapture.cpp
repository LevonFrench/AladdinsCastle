// SPDX-License-Identifier: GPL-3.0-only
#include "CompositorCapture.h"
#include "OverlayRuntime.h"
#include "QuickTextureRenderer.h"
#include "core/install/Support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QScopeGuard>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif
namespace ac {
namespace {
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
bool imageValid(const QImage &image){return !image.isNull()&&image.format()==QImage::Format_RGBA8888&&image.width()<=4096&&image.height()<=4096&&qint64(image.width())*image.height()*4<=16*1024*1024;}
QString capturePath(const QString &root,const QString &directory){return install::scopedPath(directory,install::scopedPath(".local",root));}
bool existingServer(){
#ifdef Q_OS_WIN
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return false;
    const auto close=qScopeGuard([&]{CloseHandle(snapshot);});
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"vrserver.exe")==0)return true;}while(Process32NextW(snapshot,&entry));
#endif
    return false;
}
class NativeMirrors final:public CompositorReadbackOps {
public:
    NativeMirrors(vr::IVRCompositor *compositor,QOpenGLContext &context):m_compositor(compositor),m_context(context){}
    bool acquire(unsigned int eye,CompositorMirror *mirror,QString *error)override{
        vr::glUInt_t texture=0;vr::glSharedTextureHandle_t handle=nullptr;
        const auto result=m_compositor->GetMirrorTextureGL(eye==0?vr::Eye_Left:vr::Eye_Right,&texture,&handle);
        *mirror={texture,reinterpret_cast<std::uintptr_t>(handle)};
        return result==vr::VRCompositorError_None?true:fail(error,QString("Compositor mirror acquisition failed (%1).").arg(int(result)));
    }
    void lock(const CompositorMirror &mirror)noexcept override{m_compositor->LockGLSharedTextureForAccess(shared(mirror));}
    void unlock(const CompositorMirror &mirror)noexcept override{m_compositor->UnlockGLSharedTextureForAccess(shared(mirror));}
    bool release(const CompositorMirror &mirror)noexcept override{
        return mirror.texture&&mirror.sharedHandle&&m_compositor->ReleaseSharedGLTexture(mirror.texture,shared(mirror));
    }
    bool read(const CompositorMirror &mirror,QImage *image,QString *error)override{
        if(QOpenGLContext::currentContext()!=&m_context||m_context.isOpenGLES()||m_context.format().majorVersion()<3)return fail(error,"Compositor readback requires its current desktop GL3 context.");
        auto *f=m_context.extraFunctions();f->initializeOpenGLFunctions();
        if(f->glGetError()!=GL_NO_ERROR)return fail(error,"Compositor readback found a preexisting GL error.");
        GLint previous=0;f->glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);
        if(f->glGetError()!=GL_NO_ERROR)return fail(error,"Cannot query compositor texture binding.");
        f->glBindTexture(GL_TEXTURE_2D,mirror.texture);
        auto restore=qScopeGuard([&]{f->glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previous));});
        GLint width=0,height=0,format=0;
        f->glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
        f->glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
        f->glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
        const auto queryError=f->glGetError();
        f->glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previous));restore.dismiss();
        if(f->glGetError()!=GL_NO_ERROR||queryError!=GL_NO_ERROR)return fail(error,"Compositor texture query or binding restoration failed.");
        if(format!=GL_RGBA8)return fail(error,"Compositor mirror format is not supported linear RGBA8.");
        return readCurrentGlTexture(mirror.texture,QSize(width,height),image,error);
    }
private:
    static vr::glSharedTextureHandle_t shared(const CompositorMirror &mirror){return reinterpret_cast<vr::glSharedTextureHandle_t>(mirror.sharedHandle);}
    vr::IVRCompositor *m_compositor;
    QOpenGLContext &m_context;
};
}
bool readCompositorEyes(CompositorReadbackOps &ops,std::array<QImage,2> *eyes,QString *error){
    if(!eyes)return fail(error,"Compositor readback requires output storage.");
    *eyes={};std::array<CompositorMirror,2> mirrors{};std::array<bool,2> locked{};
    const auto release=[&]{
        bool good=true;
        for(int i=1;i>=0;--i){
            if(locked[size_t(i)]){ops.unlock(mirrors[size_t(i)]);locked[size_t(i)]=false;}
            if(mirrors[size_t(i)].texture||mirrors[size_t(i)].sharedHandle){if(!ops.release(mirrors[size_t(i)]))good=false;mirrors[size_t(i)]={};}
        }
        return good;
    };
    auto cleanup=qScopeGuard([&]{release();});std::array<QImage,2> candidate;
    try{
        for(unsigned int i=0;i<2;++i){
            if(!ops.acquire(i,&mirrors[i],error)||!mirrors[i].texture||!mirrors[i].sharedHandle){
                if(error&&error->isEmpty()) {
                    *error="Compositor acquisition returned an invalid mirror.";
                }
                return false;
            }
        }
        // Lock/read each separately. The API does not promise an atomic stereo pair.
        for(unsigned int i=0;i<2;++i){
            ops.lock(mirrors[i]);locked[i]=true;
            if(!ops.read(mirrors[i],&candidate[i],error)||!imageValid(candidate[i])){
                if(error&&error->isEmpty()) {
                    *error="Compositor mirror returned invalid pixels.";
                }
                return false;
            }
            ops.unlock(mirrors[i]);locked[i]=false;
        }
        const bool released=release();cleanup.dismiss();
        if(!released)return fail(error,"Compositor mirror release failed; snapshot refused.");
        *eyes=std::move(candidate);return true;
    }catch(const std::exception &exception){return fail(error,QString::fromUtf8(exception.what()));}
}
bool validateCompositorDirectory(const QString &catalogRoot,const QString &directory,QString *error){
    if(!QDir::isAbsolutePath(directory))return fail(error,"Compositor capture requires an absolute fresh catalog .local directory.");
    try{
        const auto root=install::scopedPath(".local",catalogRoot),target=capturePath(catalogRoot,directory);
        if(target==root||QFileInfo::exists(target))return fail(error,"Compositor capture directory must be fresh and private.");
        return true;
    }catch(const std::exception &exception){return fail(error,QString::fromUtf8(exception.what()));}
}
bool compositorDashboardMatches(const QString &requirement,bool visible){
    return requirement=="any"||(requirement=="visible"&&visible)||(requirement=="hidden"&&!visible);
}
bool writeCompositorEyes(const QString &catalogRoot,const QString &directory,const std::array<QImage,2> &eyes,bool dashboardVisible,QString *error){
    if(!imageValid(eyes[0])||!imageValid(eyes[1]))return fail(error,"Compositor export needs both valid eye images.");
    if(!validateCompositorDirectory(catalogRoot,directory,error))return false;
    try{
        const auto target=capturePath(catalogRoot,directory);
        if(!QDir().mkpath(QFileInfo(target).absolutePath())||!QDir().mkdir(target))return fail(error,"Cannot reserve fresh compositor output.");
        QStringList created;bool complete=false;
        auto cleanup=qScopeGuard([&]{if(!complete)for(const auto &path:created)QFile::remove(path);});
        QJsonArray records;const std::array<const char *,2> names{"left","right"};
        for(size_t i=0;i<2;++i){
            const auto file=QString::fromLatin1(names[i])+".png",path=install::scopedPath(file,target);QFile output(path);
            if(!output.open(QIODevice::WriteOnly|QIODevice::NewOnly))return fail(error,"Cannot create fresh compositor eye image.");
            created<<path;
            if(!eyes[i].save(&output,"PNG")||!output.flush())return fail(error,"Cannot write compositor eye image.");
            records.append(QJsonObject{{"eye",QString::fromLatin1(names[i])},{"image",file},{"width",eyes[i].width()},{"height",eyes[i].height()}});
        }
        const QJsonObject receipt{{"schema",1},{"status","captured"},{"evidence","OpenVR-compositor-mirror"},{"eyes",records},
            {"texture_format","linear RGBA8"},{"row_conversion","GL bottom-first to PNG top-first: one row reversal"},
            {"dashboard_visible",dashboardVisible},{"exact_frame_sync",false},{"content_visual_review_required",true},{"physical_headset_verified",false}};
        const auto path=install::scopedPath("receipt.json",target);QFile output(path);
        if(!output.open(QIODevice::WriteOnly|QIODevice::NewOnly))return fail(error,"Cannot create compositor receipt.");
        created<<path;const auto bytes=QJsonDocument(receipt).toJson();
        if(output.write(bytes)!=bytes.size()||!output.flush())return fail(error,"Cannot write compositor receipt.");
        complete=true;return true;
    }catch(const std::exception &exception){return fail(error,QString::fromUtf8(exception.what()));}
}
bool captureCompositorEyes(const QString &catalogRoot,const QString &directory,const QString &dashboardRequirement,QString *error){
    if(!QStringList{"any","visible","hidden"}.contains(dashboardRequirement))return fail(error,"Invalid compositor dashboard requirement.");
    if(!validateCompositorDirectory(catalogRoot,directory,error))return false;
    if(!existingServer())return fail(error,"Compositor capture requires an already-running SteamVR service; it does not start one.");
    if(!probeOpenVrLibrary(error))return false;
    vr::EVRInitError init=vr::VRInitError_None;vr::VR_Init(&init,vr::VRApplication_Background);
    if(init!=vr::VRInitError_None)return fail(error,QString("Compositor client initialization failed (%1).").arg(int(init)));
    auto shutdown=qScopeGuard([]{vr::VR_Shutdown();});
    if(!vr::VRCompositor()||!vr::VROverlay())return fail(error,"Compositor capture interfaces are unavailable.");
    const bool dashboard=vr::VROverlay()->IsDashboardVisible();
    if(!compositorDashboardMatches(dashboardRequirement,dashboard))return fail(error,QString("Dashboard requirement %1 refused: observed visible=%2.").arg(dashboardRequirement).arg(dashboard?"true":"false"));
    QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QOpenGLContext context;context.setFormat(format);
    if(!context.create())return fail(error,"Cannot create compositor readback GL context.");
    QOffscreenSurface surface;surface.setFormat(context.format());surface.create();
    if(!surface.isValid()||!context.makeCurrent(&surface))return fail(error,"Cannot make compositor readback context current.");
    auto done=qScopeGuard([&]{context.doneCurrent();});
    NativeMirrors ops(vr::VRCompositor(),context);std::array<QImage,2> eyes;
    if(!readCompositorEyes(ops,&eyes,error))return false;
    if(vr::VROverlay()->IsDashboardVisible()!=dashboard)return fail(error,"Dashboard state changed during snapshot; pair refused.");
    return writeCompositorEyes(catalogRoot,directory,eyes,dashboard,error);
}
}
