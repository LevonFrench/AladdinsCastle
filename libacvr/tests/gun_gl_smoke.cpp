// SPDX-License-Identifier: MIT
// Manual, owner-approved Windows GPU check. Never registered with CTest.
#include "gun_gl.hpp"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstring>
#include <iostream>
#include <stdexcept>

template<class T> T init() {T v{};ACVR_INIT(&v);return v;}
void require(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
void *ACVR_CALL resolve(void *user,const char *name) {
    const auto fn=static_cast<QOpenGLContext *>(user)->getProcAddress(name);void *p=nullptr;
    static_assert(sizeof(fn)==sizeof(p));std::memcpy(&p,&fn,sizeof(p));return p;
}
int main(int argc,char **argv) {
    // Check permission gate before QGuiApplication/platform/driver initialization.
    if(argc!=3||std::strcmp(argv[1],"--owner-approved-gpu-smoke")!=0) {
        std::cerr<<"Requires explicit owner approval, then --owner-approved-gpu-smoke <private-output-directory>\n";return 2;
    }
    try {
        QGuiApplication app(argc,argv);QOpenGLContext context;QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);format.setVersion(3,3);format.setProfile(QSurfaceFormat::CompatibilityProfile);
        context.setFormat(format);require(context.create(),"GL context creation failed");
        require(context.format().profile()==QSurfaceFormat::CompatibilityProfile,"driver did not supply a compatibility context");
        QOffscreenSurface surface;surface.setFormat(context.format());surface.create();
        require(surface.isValid()&&context.makeCurrent(&surface),"offscreen context could not become current");
        QOpenGLFunctions_3_3_Core gl;require(gl.initializeOpenGLFunctions(),"GL3.3 functions unavailable");
        auto device=init<acvr_graphics_device>();device.api=ACVR_GRAPHICS_GL;device.flags=ACVR_DEVICE_GL_COMPATIBILITY;
        device.context=reinterpret_cast<uint64_t>(wglGetCurrentContext());device.get_proc=resolve;device.proc_user=&context;
        acvr::GunGlRenderer renderer;require(renderer.initialize(device)==ACVR_OK,renderer.diagnostic().c_str());
        GLuint colour=0,depth=0,fbo=0;gl.glGenTextures(1,&colour);gl.glBindTexture(GL_TEXTURE_2D,colour);
        gl.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        gl.glGenTextures(1,&depth);gl.glBindTexture(GL_TEXTURE_2D,depth);
        gl.glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,64,64,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
        gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);gl.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        gl.glGenFramebuffers(1,&fbo);gl.glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
        require(gl.glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"synthetic framebuffer incomplete");
        acvr::GunAsset asset;asset.lod_count=1;asset.nodes.resize(1);asset.materials.push_back({"body",{1,0,0,1}});
        acvr::GunPrimitive primitive;primitive.vertices.resize(3);primitive.indices={0,1,2};
        primitive.vertices[0].position={-.8f,-.7f,-2};primitive.vertices[1].position={.8f,-.7f,-2};primitive.vertices[2].position={0,.8f,-2};
        asset.primitives.push_back(primitive);acvr::GunDraw gun;gun.asset=&asset;gun.body=gun.accent={1,1,1,1};gun.scene_from_node.resize(1);
        for(unsigned i=0;i<4;++i) gun.scene_from_node[0][5*i]=1;
        auto eye=init<acvr_eye>();eye.rect_width=eye.rect_height=64;
        for(unsigned i=0;i<4;++i) eye.view_from_scene[5*i]=1;
        eye.projection_from_view[0]=eye.projection_from_view[5]=1;eye.projection_from_view[10]=-1.020202f;
        eye.projection_from_view[11]=-1;eye.projection_from_view[14]=-.2020202f;
        auto draw=init<acvr_draw_info>();draw.target=init<acvr_render_target>();auto &t=draw.target;
        t.api=ACVR_GRAPHICS_GL;t.width=t.height=64;t.array_layers=t.sample_count=1;t.colour_format=GL_RGBA8;t.depth_format=GL_DEPTH_COMPONENT24;
        t.colour_image=colour;t.depth_image=depth;t.framebuffer=fbo;draw.views=&eye;draw.view_count=1;draw.view_stride=sizeof(eye);
        QImage combined(128,64,QImage::Format_RGBA8888);combined.fill(Qt::black);
        for(unsigned pass=0;pass<2;++pass) {
            gl.glDisable(GL_SCISSOR_TEST);gl.glDepthMask(GL_TRUE);gl.glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            gl.glClearColor(0,0,1,1);gl.glClearDepth(1);gl.glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            gl.glEnable(GL_SCISSOR_TEST);gl.glScissor(0,0,32,64);gl.glClearDepth(.2);gl.glClear(GL_DEPTH_BUFFER_BIT);
            gl.glDisable(GL_SCISSOR_TEST);gl.glViewport(1,2,20,21);gl.glDepthMask(GL_FALSE);gl.glEnable(GL_BLEND);
            eye.eye_index=pass;eye.view_from_scene[12]=pass?.1f:-.1f;
            require(renderer.draw(draw,gun)==ACVR_OK,renderer.diagnostic().empty()?"gun GL draw failed":renderer.diagnostic().c_str());
            GLint viewport[4]{};GLboolean write=GL_TRUE;gl.glGetIntegerv(GL_VIEWPORT,viewport);gl.glGetBooleanv(GL_DEPTH_WRITEMASK,&write);
            require(viewport[0]==1&&viewport[1]==2&&viewport[2]==20&&viewport[3]==21&&!write&&gl.glIsEnabled(GL_BLEND),"caller GL state changed");
            QImage image(64,64,QImage::Format_RGBA8888);gl.glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,image.bits());
            const auto blocked=image.pixelColor(28,30),visible=image.pixelColor(36,30);
            require(blocked.blue()>240&&blocked.red()<10,"world depth did not occlude gun");
            require(visible.red()>240&&visible.blue()<10,"unoccluded gun pixel missing");
            float z=0;gl.glReadPixels(28,30,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z);require(z>.19f&&z<.21f,"occluded world depth overwritten");
            const auto flipped=image.mirrored();for(int y=0;y<64;++y) std::memcpy(combined.scanLine(y)+pass*64*4,flipped.constScanLine(y),64*4);
        }
        require(gl.glGetError()==GL_NO_ERROR,"unexpected GL error after synthetic checks");
        const auto output=QString::fromLocal8Bit(argv[2]);require(QDir().mkpath(output),"cannot create private receipt directory");
        require(combined.save(QDir(output).filePath("synthetic-gun-depth.png")),"cannot save synthetic capture");
        QJsonObject receipt{{"synthetic_only",true},{"world_depth_occlusion",true},{"state_restore",true},
            {"gl_version",QString::fromLatin1(reinterpret_cast<const char *>(gl.glGetString(GL_VERSION)))},
            {"renderer",QString::fromLatin1(reinterpret_cast<const char *>(gl.glGetString(GL_RENDERER)))}};
        QFile file(QDir(output).filePath("receipt.json"));require(file.open(QIODevice::WriteOnly),"cannot save private receipt");file.write(QJsonDocument(receipt).toJson());file.close();
        require(renderer.shutdown()==ACVR_OK,"renderer cleanup failed");gl.glDeleteFramebuffers(1,&fbo);gl.glDeleteTextures(1,&depth);gl.glDeleteTextures(1,&colour);
        context.doneCurrent();std::cout<<"Synthetic GPU depth/state checks passed; no headset or game acceptance\n";return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
