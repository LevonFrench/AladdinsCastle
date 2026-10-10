// SPDX-License-Identifier: GPL-3.0-only
#include "n22_gl_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <charconv>
#include <limits>

namespace n22 {
namespace {
template<class Fn> bool resolve(Fn &fn,const acvr_graphics_device &d,const char *name,const char *fallback=nullptr) {
    void *address=d.get_proc(d.proc_user,name);
    auto good=[](void *p) {const auto bits=reinterpret_cast<uintptr_t>(p);
        return p && bits>3 && bits!=std::numeric_limits<uintptr_t>::max();};
    if(!good(address) && fallback) address=d.get_proc(d.proc_user,fallback);
    if(!good(address)) return false;
    static_assert(sizeof(fn)==sizeof(address),"GL function/data pointers must share width on this target");
    std::memcpy(&fn,&address,sizeof(fn));return true;
}
bool extension(const char *list,const char *name) {
    if(!list) return false;
    const size_t size=std::strlen(name);
    for(const char *p=list;(p=std::strstr(p,name));p+=size)
        if((p==list || p[-1]==' ') && (p[size]==0 || p[size]==' ')) return true;
    return false;
}
struct Saved {
    GlDispatch &g;
    explicit Saved(GlDispatch &dispatch):g(dispatch) {}
    GLint framebuffer=0,read_framebuffer=0,program=0,texture=0,mode=GL_MODELVIEW;
    bool split_framebuffer=false;
    bool attrib=false,projection=false,model=false,changed=false;
    std::array<GLboolean,glc::ModernCaps.size()> modern{};
    std::vector<GLboolean> clips,rectangles;
    bool texture_state=false;
    GLint texture_binding=0,sampler=0,unpack_buffer=0;
    std::array<GLint,glc::UnpackParams.size()> unpack{};
    std::array<GLfloat,16> texture_matrix{};
    void restore() {
        if(!changed) return;
        if(model) {g.MatrixMode(GL_MODELVIEW);g.PopMatrix();}
        if(projection) {g.MatrixMode(GL_PROJECTION);g.PopMatrix();}
        if(attrib) g.PopAttrib();
        if(texture_state) {
            g.ActiveTexture(glc::Texture0);g.BindTexture(GL_TEXTURE_2D,static_cast<GLuint>(texture_binding));
            g.BindSampler(0,static_cast<GLuint>(sampler));
            g.MatrixMode(GL_TEXTURE);g.LoadMatrixf(texture_matrix.data());
            g.BindBuffer(glc::PixelUnpackBuffer,static_cast<GLuint>(unpack_buffer));
            for(size_t i=0;i<unpack.size();++i) g.PixelStorei(glc::UnpackParams[i],unpack[i]);
        }
        // Modern enables are not guaranteed to be covered by legacy attribs.
        for(size_t i=0;i<modern.size();++i) (modern[i]?g.Enable:g.Disable)(glc::ModernCaps[i]);
        for(size_t i=0;i<clips.size();++i) (clips[i]?g.Enable:g.Disable)(glc::ClipDistance0+static_cast<GLenum>(i));
        for(size_t i=0;i<rectangles.size();++i) {
            g.ActiveTexture(glc::Texture0+static_cast<GLenum>(i));
            (rectangles[i]?g.Enable:g.Disable)(glc::TextureRectangle);
        }
        g.MatrixMode(static_cast<GLenum>(mode));
        g.ActiveTexture(static_cast<GLenum>(texture));
        g.UseProgram(static_cast<GLuint>(program));
        if(split_framebuffer) {
            g.BindFramebuffer(glc::DrawFramebuffer,static_cast<GLuint>(framebuffer));
            g.BindFramebuffer(glc::ReadFramebuffer,static_cast<GLuint>(read_framebuffer));
        } else g.BindFramebuffer(glc::Framebuffer,static_cast<GLuint>(framebuffer));
        changed=false;
    }
    ~Saved() {restore();}
};
bool finite(const float *v,size_t n) {for(size_t i=0;i<n;++i) if(!std::isfinite(v[i])) return false;return true;}
}
acvr_result GlRenderer::initialize(const acvr_graphics_device &device) {
    if(ready_) return ACVR_BAD_STATE;
    if(device.size<sizeof(device) || device.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(device.api!=ACVR_GRAPHICS_GL || device.flags&ACVR_DEVICE_GLES ||
       !(device.flags&ACVR_DEVICE_GL_COMPATIBILITY)) return ACVR_UNSUPPORTED;
    if(!device.context || !device.get_proc) return ACVR_BAD_ARGUMENT;
    GlDispatch candidate{};
#define N22_LOAD(name,ret,args) if(!resolve(candidate.name,device,"gl" #name, \
        std::strcmp(#name,"BindFramebuffer")==0?"glBindFramebufferEXT": \
        std::strcmp(#name,"ActiveTexture")==0?"glActiveTextureARB": \
        std::strcmp(#name,"GetFramebufferAttachmentParameteriv")==0?"glGetFramebufferAttachmentParameterivEXT":nullptr)) return ACVR_UNSUPPORTED;
    N22_GL_FUNCTIONS(N22_LOAD)
    N22_GL_TEXTURE_FUNCTIONS(N22_LOAD)
#undef N22_LOAD
    if(!resolve(candidate.CheckFramebufferStatus,device,"glCheckFramebufferStatus","glCheckFramebufferStatusEXT") ||
       !resolve(candidate.DrawBuffer,device,"glDrawBuffer")) return ACVR_UNSUPPORTED;
    gl_=candidate;owner_=std::this_thread::get_id();ready_=true;return ACVR_OK;
}
acvr_result GlRenderer::draw(const Frame &frame,const acvr_draw_info &in) {
    if(!ready_ || owner_!=std::this_thread::get_id()) return ACVR_BAD_STATE;
    diagnostic_={};
    if(in.size<sizeof(in) || in.version!=ACVR_STRUCT_VERSION || in.target.size<sizeof(in.target) ||
       in.target.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(in.frame_id!=frame.id) return ACVR_BAD_STATE;
    if(resource_frame_ && (resource_frame_!=&frame || resource_frame_id_!=frame.id ||
       texture_plan_.triangle_textures.size()!=frame.triangles.size())) return ACVR_BAD_STATE;
    if(in.target.api!=ACVR_GRAPHICS_GL || in.target.array_layers!=1 || in.target.sample_count!=1 ||
       !in.target.depth_format || !in.target.depth_image || !in.target.colour_image || !in.target.framebuffer) return ACVR_UNSUPPORTED;
    if(in.target.colour_format!=0 && in.target.colour_format!=GL_RGB8 && in.target.colour_format!=GL_RGBA8 &&
       in.target.colour_format!=glc::Srgb8Alpha8) return ACVR_UNSUPPORTED;
    if(in.view_count!=1 || !in.views || in.view_stride<sizeof(acvr_eye) ||
       in.view_stride%alignof(acvr_eye) || reinterpret_cast<uintptr_t>(in.views)%alignof(acvr_eye)) return ACVR_BAD_ARGUMENT;
    const auto &eye=*in.views;
    if(eye.size<sizeof(eye) || eye.size>in.view_stride || eye.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(eye.eye_index>1 || eye.array_layer!=0 || eye.rect_x<0 || eye.rect_y<0 || !eye.rect_width || !eye.rect_height ||
       static_cast<uint64_t>(eye.rect_x)+eye.rect_width>in.target.width ||
       static_cast<uint64_t>(eye.rect_y)+eye.rect_height>in.target.height ||
       in.target.width>static_cast<uint32_t>(INT32_MAX) || in.target.height>static_cast<uint32_t>(INT32_MAX) ||
       in.target.framebuffer>UINT32_MAX || in.target.depth_image>UINT32_MAX || in.target.colour_image>UINT32_MAX ||
       !finite(eye.view_from_scene,16) || !finite(eye.projection_from_view,16)) return ACVR_BAD_ARGUMENT;
    // Initial path uses canonical RH OpenGL forward-depth perspective only.
    if(eye.projection_from_view[11]!=-1 || eye.projection_from_view[15]!=0 ||
       eye.projection_from_view[10]>-1 || eye.projection_from_view[14]>=0) return ACVR_UNSUPPORTED;
    for(const auto &t:frame.triangles) {
        if(t.layer!=Layer::World && t.layer!=Layer::Hud && t.layer!=Layer::Backdrop && t.layer!=Layer::GunFlash) return ACVR_BAD_ARGUMENT;
        for(auto v:t.vertices) {const float p[]={v.x,v.y,v.z};if(!finite(p,3)) return ACVR_BAD_ARGUMENT;}
    }
    TexturePlan prepared;
    if(!resource_frame_) {
        if(auto r=prepare_texture_plan(frame,prepared);r!=ACVR_OK) return r;
    }
    const auto &plan=resource_frame_?texture_plan_:prepared;
    const bool has_textures=!plan.rectangles.empty();
    auto check_error=[&](GlPhase phase) {
        const GLenum error=gl_.GetError();
        if(error!=GL_NO_ERROR && diagnostic_.error==GL_NO_ERROR) {diagnostic_.phase=phase;diagnostic_.error=error;}
        return error!=GL_NO_ERROR;
    };
    // Capture, do not silently drain, a pre-existing caller error. No target is
    // bound or cleared; diagnostic preserves its code for the runtime owner.
    if(check_error(GlPhase::Entry)) return ACVR_BAD_STATE;
    Saved saved{gl_};
    gl_.GetIntegerv(glc::FramebufferBinding,&saved.framebuffer);
    gl_.GetIntegerv(glc::CurrentProgram,&saved.program);
    gl_.GetIntegerv(glc::ActiveTexture,&saved.texture);
    gl_.GetIntegerv(GL_MATRIX_MODE,&saved.mode);
    GLint ad=0,am=0,pd=0,pm=0,md=0,mm=0,units=0,clips=0;
    gl_.GetIntegerv(GL_ATTRIB_STACK_DEPTH,&ad);gl_.GetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH,&am);
    gl_.GetIntegerv(GL_PROJECTION_STACK_DEPTH,&pd);gl_.GetIntegerv(GL_MAX_PROJECTION_STACK_DEPTH,&pm);
    gl_.GetIntegerv(GL_MODELVIEW_STACK_DEPTH,&md);gl_.GetIntegerv(GL_MAX_MODELVIEW_STACK_DEPTH,&mm);
    gl_.GetIntegerv(glc::MaxTextureUnits,&units);gl_.GetIntegerv(glc::MaxClipDistances,&clips);
    const auto *raw_version=gl_.GetString(GL_VERSION);
    const auto *raw_ext=gl_.GetString(GL_EXTENSIONS);
    if(check_error(GlPhase::Preflight)) return ACVR_UNSUPPORTED;
    const char *version=reinterpret_cast<const char *>(raw_version);
    const char *extensions=reinterpret_cast<const char *>(raw_ext);
    int major=0,minor=0;
    if(!version) return ACVR_UNSUPPORTED;
    const auto *end=version+std::strlen(version);
    const auto first=std::from_chars(version,end,major);
    if(first.ec!=std::errc{} || first.ptr==end || *first.ptr!='.') return ACVR_UNSUPPORTED;
    const auto second=std::from_chars(first.ptr+1,end,minor);
    if(second.ec!=std::errc{} || major<3 || (major==3 && minor<3)) return ACVR_UNSUPPORTED;
    GLint profile=0;gl_.GetIntegerv(glc::ContextProfileMask,&profile);
    if(check_error(GlPhase::Preflight) || !(profile&glc::CompatibilityProfileBit)) return ACVR_UNSUPPORTED;
    if(!version || std::strstr(version,"OpenGL ES") || ad<0 || pd<1 || md<1 || ad>=am || pd>=pm || md>=mm ||
       units<1 || units>64 || clips<0 || clips>64) return ACVR_UNSUPPORTED;
    const bool srgb=(version[0]>='3' && version[0]<='9') || extension(extensions,"GL_EXT_framebuffer_sRGB") || extension(extensions,"GL_ARB_framebuffer_sRGB");
    saved.split_framebuffer=(version[0]>='3' && version[0]<='9') || extension(extensions,"GL_ARB_framebuffer_object") || extension(extensions,"GL_EXT_framebuffer_blit");
    if(saved.split_framebuffer) gl_.GetIntegerv(glc::ReadFramebufferBinding,&saved.read_framebuffer);
    if(check_error(GlPhase::Preflight)) return ACVR_UNSUPPORTED;
    if(in.target.colour_format==glc::Srgb8Alpha8 && !srgb) return ACVR_UNSUPPORTED;
    for(size_t i=0;i<saved.modern.size();++i) saved.modern[i]=gl_.IsEnabled(glc::ModernCaps[i]);
    saved.clips.resize(static_cast<size_t>(clips));
    for(GLint i=0;i<clips;++i) saved.clips[static_cast<size_t>(i)]=gl_.IsEnabled(glc::ClipDistance0+static_cast<GLenum>(i));
    // Query per-unit rectangle state while restoring the selector immediately.
    saved.rectangles.resize(static_cast<size_t>(units));
    for(GLint i=0;i<units;++i) {
        gl_.ActiveTexture(glc::Texture0+static_cast<GLenum>(i));
        saved.rectangles[static_cast<size_t>(i)]=gl_.IsEnabled(glc::TextureRectangle);
    }
    gl_.ActiveTexture(static_cast<GLenum>(saved.texture));
    if(check_error(GlPhase::Preflight)) return ACVR_ERROR;
    if(has_textures) {
        // Optional imaging subset has additional transfer stages. Admit only
        // inactive/identity state rather than corrupting a texture silently.
        if(extension(extensions,"GL_ARB_imaging")) {
            bool altered=false;
            for(auto cap:glc::ImagingCaps) altered=gl_.IsEnabled(cap)!=GL_FALSE || altered;
            std::array<GLfloat,16> matrix{};gl_.GetFloatv(glc::ColourMatrix,matrix.data());
            for(size_t i=0;i<16;++i) altered=matrix[i]!=(i%5==0?1.f:0.f) || altered;
            for(GLenum base:std::array<GLenum,2>{0x801c,0x80b4}) for(GLenum i=0;i<8;++i) {
                GLfloat value=0;gl_.GetFloatv(base+i,&value);altered=value!=(i<4?1.f:0.f) || altered;
            }
            if(check_error(GlPhase::Preflight) || altered) return ACVR_UNSUPPORTED;
        }
        GLint maximum=0;gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if(check_error(GlPhase::Preflight) || maximum<1) return ACVR_UNSUPPORTED;
        for(const auto &rect:plan.rectangles)
            if(rect.width>static_cast<uint32_t>(maximum) || rect.height>static_cast<uint32_t>(maximum)) return ACVR_UNSUPPORTED;
        gl_.GetIntegerv(glc::PixelUnpackBinding,&saved.unpack_buffer);
        for(size_t i=0;i<saved.unpack.size();++i) gl_.GetIntegerv(glc::UnpackParams[i],&saved.unpack[i]);
        gl_.ActiveTexture(glc::Texture0);
        gl_.GetIntegerv(GL_TEXTURE_BINDING_2D,&saved.texture_binding);gl_.GetIntegerv(glc::SamplerBinding,&saved.sampler);
        gl_.GetFloatv(GL_TEXTURE_MATRIX,saved.texture_matrix.data());
        gl_.ActiveTexture(static_cast<GLenum>(saved.texture));
        if(check_error(GlPhase::Preflight)) return ACVR_ERROR;
        saved.texture_state=true;
    }
    auto finish=[&](acvr_result value) {
        saved.restore();
        const GLenum error=gl_.GetError();
        if(error!=GL_NO_ERROR) {
            diagnostic_.restore_error=error;
            if(diagnostic_.error==GL_NO_ERROR) {diagnostic_.phase=GlPhase::Restore;diagnostic_.error=error;}
            if(value==ACVR_OK) value=ACVR_ERROR;
        }
        return value;
    };
    saved.changed=true;
    const GLenum draw_target=saved.split_framebuffer?glc::DrawFramebuffer:glc::Framebuffer;
    gl_.BindFramebuffer(draw_target,static_cast<GLuint>(in.target.framebuffer));gl_.UseProgram(0);
    if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
    const GLenum status=gl_.CheckFramebufferStatus(draw_target);
    if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
    if(status!=glc::FramebufferComplete) {diagnostic_={GlPhase::State,GL_NO_ERROR,status};return finish(ACVR_BAD_STATE);}
    for(GLenum attachment:std::array<GLenum,2>{glc::ColourAttachment0,glc::DepthAttachment}) {
        GLint type=0,name=0;
        gl_.GetFramebufferAttachmentParameteriv(draw_target,attachment,glc::AttachmentObjectType,&type);
        gl_.GetFramebufferAttachmentParameteriv(draw_target,attachment,glc::AttachmentObjectName,&name);
        if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
        const auto expected=attachment==glc::DepthAttachment?in.target.depth_image:in.target.colour_image;
        if(type!=GL_TEXTURE || static_cast<GLuint>(name)!=expected) return finish(ACVR_BAD_ARGUMENT);
    }
    GLint depth=0;gl_.GetIntegerv(GL_DEPTH_BITS,&depth);
    if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
    if(depth<16) return finish(ACVR_UNSUPPORTED);
    gl_.PushAttrib(GL_ALL_ATTRIB_BITS|glc::MultisampleBit);
    if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
    saved.attrib=true;
    gl_.MatrixMode(GL_PROJECTION);gl_.PushMatrix();
    if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
    saved.projection=true;
    gl_.MatrixMode(GL_MODELVIEW);gl_.PushMatrix();
    if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
    saved.model=true;gl_.LoadMatrixf(eye.view_from_scene);
    if(has_textures) {
        gl_.ActiveTexture(glc::Texture0);gl_.BindSampler(0,0);gl_.BindBuffer(glc::PixelUnpackBuffer,0);
        for(auto parameter:glc::UnpackParams) gl_.PixelStorei(parameter,parameter==GL_UNPACK_ALIGNMENT?1:0);
        for(size_t i=0;i<glc::TransferParams.size();++i) gl_.PixelTransferf(glc::TransferParams[i],i<4?1.f:0.f);
        std::array<GLfloat,16> identity{};for(size_t i=0;i<16;i+=5) identity[i]=1;
        gl_.MatrixMode(GL_TEXTURE);gl_.LoadMatrixf(identity.data());gl_.MatrixMode(GL_MODELVIEW);
        if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
        if(!resource_frame_) {
            std::vector<GLuint> created(plan.rectangles.size(),0);
            auto rollback=[&]() {
                for(auto name:created) if(name) {
                    gl_.DeleteTextures(1,&name);const auto error=gl_.GetError();
                    if(error!=GL_NO_ERROR && diagnostic_.cleanup_error==GL_NO_ERROR) diagnostic_.cleanup_error=error;
                }
            };
            gl_.GenTextures(static_cast<GLsizei>(created.size()),created.data());
            bool invalid=false;
            for(size_t i=0;i<created.size();++i) {
                const auto name=created[i];
                if(name==in.target.colour_image || name==in.target.depth_image || name==static_cast<GLuint>(saved.texture_binding)) {
                    created[i]=0;invalid=true; // never delete an alleged borrowed name
                } else if(!name || std::find(created.begin(),created.begin()+static_cast<ptrdiff_t>(i),name)!=created.begin()+static_cast<ptrdiff_t>(i)) {
                    created[i]=0;invalid=true;
                }
            }
            if(check_error(GlPhase::Upload) || invalid) {
                if(diagnostic_.phase==GlPhase::None) diagnostic_.phase=GlPhase::Upload;
                rollback();return finish(ACVR_ERROR);
            }
            for(size_t i=0;i<created.size();++i) {
                const auto &rect=plan.rectangles[i];gl_.BindTexture(GL_TEXTURE_2D,created[i]);
                gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glc::ClampToEdge);gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glc::ClampToEdge);
                gl_.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,static_cast<GLsizei>(rect.width),static_cast<GLsizei>(rect.height),0,GL_RGBA,GL_UNSIGNED_BYTE,rect.rgba.data());
                if(check_error(GlPhase::Upload)) {rollback();return finish(ACVR_ERROR);}
            }
            textures_=std::move(created);texture_plan_=std::move(prepared);
            resource_frame_=&frame;resource_frame_id_=frame.id;
        }
    }
    if(in.target.framebuffer) gl_.DrawBuffer(glc::ColourAttachment0);
    else {GLint doubled=0;gl_.GetIntegerv(GL_DOUBLEBUFFER,&doubled);gl_.DrawBuffer(doubled?GL_BACK:GL_FRONT);}
    gl_.Viewport(eye.rect_x,eye.rect_y,static_cast<GLsizei>(eye.rect_width),static_cast<GLsizei>(eye.rect_height));
    gl_.Scissor(eye.rect_x,eye.rect_y,static_cast<GLsizei>(eye.rect_width),static_cast<GLsizei>(eye.rect_height));
    gl_.Enable(GL_SCISSOR_TEST);gl_.Enable(GL_DEPTH_TEST);
    gl_.DepthFunc(GL_LEQUAL);gl_.DepthMask(GL_TRUE);gl_.DepthRange(0,1);gl_.ClearDepth(1);
    gl_.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);gl_.ClearColor(0,0,0,1);gl_.PolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    for(GLenum cap:std::array<GLenum,14>{GL_BLEND,GL_ALPHA_TEST,GL_CULL_FACE,GL_LIGHTING,GL_FOG,GL_STENCIL_TEST,GL_DITHER,GL_COLOR_LOGIC_OP,GL_POLYGON_OFFSET_FILL,GL_POLYGON_STIPPLE,glc::Multisample,glc::SampleAlphaCoverage,glc::SampleAlphaOne,glc::SampleCoverage}) gl_.Disable(cap);
    for(auto cap:glc::ModernCaps) gl_.Disable(cap);
    // packed RGB is display-referred; framebuffer sRGB stays disabled.
    for(GLint i=0;i<clips;++i) gl_.Disable(glc::ClipDistance0+static_cast<GLenum>(i));
    for(GLint i=0;i<units;++i) {
        gl_.ActiveTexture(glc::Texture0+static_cast<GLenum>(i));
        for(GLenum cap:std::array<GLenum,9>{GL_TEXTURE_1D,GL_TEXTURE_2D,glc::Texture3D,glc::TextureCube,glc::TextureRectangle,GL_TEXTURE_GEN_S,GL_TEXTURE_GEN_T,GL_TEXTURE_GEN_R,GL_TEXTURE_GEN_Q}) gl_.Disable(cap);
    }
    gl_.ActiveTexture(glc::Texture0);
    if(has_textures) {
        gl_.ShadeModel(GL_SMOOTH);
        gl_.TexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,glc::Combine);
        gl_.TexEnvi(GL_TEXTURE_ENV,glc::CombineRgb,GL_MODULATE);gl_.TexEnvi(GL_TEXTURE_ENV,glc::CombineAlpha,GL_MODULATE);
        gl_.TexEnvi(GL_TEXTURE_ENV,glc::Source0Rgb,GL_TEXTURE);gl_.TexEnvi(GL_TEXTURE_ENV,glc::Source1Rgb,glc::PrimaryColour);
        gl_.TexEnvi(GL_TEXTURE_ENV,glc::Source0Alpha,GL_TEXTURE);gl_.TexEnvi(GL_TEXTURE_ENV,glc::Source1Alpha,glc::PrimaryColour);
        gl_.TexEnvi(GL_TEXTURE_ENV,glc::Operand0Rgb,GL_SRC_COLOR);gl_.TexEnvi(GL_TEXTURE_ENV,glc::Operand1Rgb,GL_SRC_COLOR);
        gl_.TexEnvi(GL_TEXTURE_ENV,glc::Operand0Alpha,GL_SRC_ALPHA);gl_.TexEnvi(GL_TEXTURE_ENV,glc::Operand1Alpha,GL_SRC_ALPHA);
        gl_.TexEnvf(GL_TEXTURE_ENV,glc::RgbScale,4);gl_.TexEnvf(GL_TEXTURE_ENV,GL_ALPHA_SCALE,1);
    }
    if(check_error(GlPhase::State)) return finish(ACVR_ERROR);
    gl_.Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    // Infinity first, then world + explicitly flattened HUD. Same anchor/matrices,
    // no native projection multiplied a second time, no emulation or output work.
    for(int pass=0;pass<2;++pass) {
        float projection[16];std::copy(std::begin(eye.projection_from_view),std::end(eye.projection_from_view),projection);
        if(pass==0) {projection[10]=-1;projection[14]=0;}
        gl_.MatrixMode(GL_PROJECTION);gl_.LoadMatrixf(projection);
        gl_.MatrixMode(GL_MODELVIEW);
        for(size_t i=0;i<frame.triangles.size();++i) {
            const auto &t=frame.triangles[i];
            if(t.layer==Layer::GunFlash || (t.layer==Layer::Backdrop)!=(pass==0)) continue;
            const TextureRectangle *rect=nullptr;
            if(t.material!=NoMaterial) {
                const auto id=texture_plan_.triangle_textures[i];rect=&texture_plan_.rectangles[id];
                gl_.Enable(GL_TEXTURE_2D);gl_.BindTexture(GL_TEXTURE_2D,textures_[id]);
            } else {
                gl_.Disable(GL_TEXTURE_2D);
                gl_.Color4ub(static_cast<GLubyte>((t.rgb>>16)&255),static_cast<GLubyte>((t.rgb>>8)&255),static_cast<GLubyte>(t.rgb&255),255);
            }
            gl_.Begin(GL_TRIANGLES);
            for(size_t k=0;k<t.vertices.size();++k) {
                const auto v=t.vertices[k];
                if(rect) {
                    const auto &attribute=t.attributes[k];const auto &material=frame.materials.materials[t.material];
                    const float shade=(material.objectflags&6)?64.f:attribute.brightness;
                    gl_.Color4f(shade/256.f,shade/256.f,shade/256.f,1);
                    const float u=material.objectflags?.5f:(attribute.u-static_cast<float>(rect->min_u))/static_cast<float>(rect->width);
                    const float tv=material.objectflags?.5f:(attribute.v-static_cast<float>(rect->min_v))/static_cast<float>(rect->height);
                    gl_.TexCoord2f(u,tv);
                }
                if(pass==0) gl_.Vertex4f(v.x,v.y,v.z,0);else gl_.Vertex3f(v.x,v.y,v.z);
            }
            gl_.End();
        }
    }
    const auto result=check_error(GlPhase::Draw)?ACVR_ERROR:ACVR_OK;
    // Restore context state, preserving scene colour AND the shared target depth
    // contents for the runtime's subsequent gun pass. No swap, flush or delete.
    return finish(result);
}
acvr_result GlRenderer::release_frame(uint64_t id) {
    if(!owner_thread()) return ACVR_BAD_STATE;
    if(!resource_frame_) return ACVR_OK;
    if(id!=resource_frame_id_) return ACVR_BAD_STATE;
    acvr_result result=ACVR_OK;
    const auto prior=gl_.GetError();
    if(prior!=GL_NO_ERROR) {
        if(diagnostic_.error==GL_NO_ERROR) {diagnostic_.phase=GlPhase::Cleanup;diagnostic_.error=prior;}
        result=ACVR_ERROR;
    }
    for(auto name:textures_) {
        gl_.DeleteTextures(1,&name);const auto error=gl_.GetError();
        if(error!=GL_NO_ERROR) {if(diagnostic_.cleanup_error==GL_NO_ERROR) diagnostic_.cleanup_error=error;result=ACVR_ERROR;}
    }
    textures_.clear();texture_plan_={};resource_frame_=nullptr;resource_frame_id_=0;
    return result;
}
acvr_result GlRenderer::shutdown() {
    if(!owner_thread()) return ACVR_BAD_STATE;
    const auto result=release_frame(resource_frame_id_);ready_=false;return result;
}
}
