// SPDX-License-Identifier: MIT
#include "gun_gl.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace acvr {
namespace {
// Registry enums used with installed platform gl.h (Windows exposes 1.1 there).
constexpr GLenum DrawFramebuffer=0x8ca9,DrawFramebufferBinding=0x8ca6,Complete=0x8cd5;
constexpr GLenum ColourAttachment=0x8ce0,DepthAttachment=0x8d00,ObjectType=0x8cd0,ObjectName=0x8cd1;
constexpr GLenum VertexShader=0x8b31,FragmentShader=0x8b30,CompileStatus=0x8b81,LinkStatus=0x8b82;
constexpr GLenum CurrentProgram=0x8b8d,VertexArrayBinding=0x85b5,ArrayBuffer=0x8892,ArrayBufferBinding=0x8894,StreamDraw=0x88e0;
constexpr GLenum MajorVersion=0x821b,MinorVersion=0x821c,FramebufferSrgb=0x8db9,Srgb8Alpha8=0x8c43;
constexpr GLenum RasterizerDiscard=0x8c89,DepthClamp=0x864f,SampleMask=0x8e51,SampleAlphaToCoverage=0x809e;
constexpr GLenum ClipDistance0=0x3000,MaxClipDistances=0x0d32,FuncAdd=0x8006;
constexpr GLenum modern_caps[]{FramebufferSrgb,RasterizerDiscard,DepthClamp,SampleMask,SampleAlphaToCoverage};
bool finite(const float *v,size_t n) {for(size_t i=0;i<n;++i) if(!std::isfinite(v[i])) return false;return true;}
bool rgba(const float *v) {if(!finite(v,4)) return false;for(unsigned i=0;i<4;++i) if(v[i]<0||v[i]>1) return false;return true;}
std::array<float,3> point(const Matrix &m,const std::array<float,3> &v) {
    return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12],m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13],m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]};
}
Matrix product(const float *a,const float *b) {
    Matrix m{};for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) for(unsigned k=0;k<4;++k) m[4*c+r]+=a[4*k+r]*b[4*c+k];return m;
}
struct Triangle {GunGlVertex vertices[3]{};float depth=0;};
template<class Fn> bool load(Fn &fn,const acvr_graphics_device &d,const char *name) {
    void *p=d.get_proc(d.proc_user,name);const auto n=reinterpret_cast<uintptr_t>(p);
    if(!p||n<=3||n==std::numeric_limits<uintptr_t>::max()) return false;
    static_assert(sizeof(fn)==sizeof(p));std::memcpy(&fn,&p,sizeof(fn));return true;
}
struct State {
    GunGlDispatch &g;GLint program=0,vao=0,buffer=0,fbo=0;
    explicit State(GunGlDispatch &dispatch):g(dispatch) {}
    bool pushed=false,touched=false;
    GLboolean modern[5]{};std::vector<GLboolean> clips;
    void restore() {
        if(!touched) return;
        if(pushed) g.PopAttrib();
        for(unsigned i=0;i<5;++i) (modern[i]?g.Enable:g.Disable)(modern_caps[i]);
        for(size_t i=0;i<clips.size();++i) (clips[i]?g.Enable:g.Disable)(ClipDistance0+GLenum(i));
        g.BindVertexArray(GLuint(vao));g.BindBuffer(ArrayBuffer,GLuint(buffer));
        g.UseProgram(GLuint(program));g.BindFramebuffer(DrawFramebuffer,GLuint(fbo));touched=false;
    }
    ~State() {restore();}
};
}
acvr_result pack_gun_vertices(const GunDraw &gun,const acvr_eye &eye,std::vector<GunGlVertex> &out,uint32_t &opaque_count) {
    if(!gun.asset||gun.scene_from_node.size()!=gun.asset->nodes.size()||gun.lod>=gun.asset->lod_count||
       !rgba(gun.body.data())||!rgba(gun.accent.data())||!finite(eye.view_from_scene,16)) return ACVR_BAD_ARGUMENT;
    try {
        std::vector<GunGlVertex> packed;std::vector<Triangle> transparent;
        if(gun.visible) for(const auto &p:gun.asset->primitives) if(p.lod==gun.lod) {
            if(p.node>=gun.scene_from_node.size()||p.material>=gun.asset->materials.size()||p.indices.size()%3) return ACVR_BAD_ARGUMENT;
            const auto &m=gun.scene_from_node[p.node];const auto &material=gun.asset->materials[p.material];
            if(!finite(m.data(),16)||m[3]!=0||m[7]!=0||m[11]!=0||m[15]!=1||!rgba(material.base_colour.data())) return ACVR_BAD_ARGUMENT;
            const std::array<float,4> fixed{1,1,1,1};
            const auto &tint=material.name=="body"?gun.body:material.name=="accent"?gun.accent:fixed;
            for(size_t at=0;at<p.indices.size();at+=3) {
                Triangle t;bool opaque=true;
                for(unsigned j=0;j<3;++j) {
                    if(p.indices[at+j]>=p.vertices.size()) return ACVR_BAD_ARGUMENT;
                    const auto &v=p.vertices[p.indices[at+j]];if(!finite(v.position.data(),3)||!rgba(v.colour.data())) return ACVR_BAD_ARGUMENT;
                    const auto position=point(m,v.position);if(!finite(position.data(),3)) return ACVR_BAD_ARGUMENT;
                    std::copy(position.begin(),position.end(),t.vertices[j].position);
                    for(unsigned k=0;k<4;++k) t.vertices[j].colour[k]=v.colour[k]*material.base_colour[k]*tint[k];
                    opaque=opaque && t.vertices[j].colour[3]>=1.f;
                    t.depth+=eye.view_from_scene[2]*position[0]+eye.view_from_scene[6]*position[1]+eye.view_from_scene[10]*position[2]+eye.view_from_scene[14];
                }
                if(!std::isfinite(t.depth)) return ACVR_BAD_ARGUMENT;
                if(opaque) packed.insert(packed.end(),std::begin(t.vertices),std::end(t.vertices));else transparent.push_back(t);
                if(packed.size()+transparent.size()*3>30000) return ACVR_BAD_ARGUMENT;
            }
        }
        const auto count=uint32_t(packed.size());
        std::stable_sort(transparent.begin(),transparent.end(),[](const Triangle &a,const Triangle &b){return a.depth<b.depth;});
        for(const auto &t:transparent) packed.insert(packed.end(),std::begin(t.vertices),std::end(t.vertices));
        out=std::move(packed);opaque_count=count;return ACVR_OK;
    } catch(...) {return ACVR_ERROR;}
}
acvr_result GunGlRenderer::initialize(const acvr_graphics_device &d) {
    if(ready_||program_||vao_||buffer_) return ACVR_BAD_STATE;
    if(d.size<sizeof(d)||d.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(d.api!=ACVR_GRAPHICS_GL||(d.flags&ACVR_DEVICE_GLES)||!(d.flags&ACVR_DEVICE_GL_COMPATIBILITY)) return ACVR_UNSUPPORTED;
    if(!d.context||!d.get_proc) return ACVR_BAD_ARGUMENT;
    GunGlDispatch candidate{};
#define ACVR_LOAD(name,ret,args) if(!load(candidate.name,d,"gl" #name)) {diagnostic_="missing gl" #name;return ACVR_UNSUPPORTED;}
    ACVR_GUN_GL_FUNCTIONS(ACVR_LOAD)
#undef ACVR_LOAD
    gl_=candidate;owner_=std::this_thread::get_id();diagnostic_.clear();
    if(gl_.GetError()!=GL_NO_ERROR) {diagnostic_="pre-existing GL error before initialization";return ACVR_BAD_STATE;}
    GLint major=0,minor=0;gl_.GetIntegerv(MajorVersion,&major);gl_.GetIntegerv(MinorVersion,&minor);
    if(gl_.GetError()!=GL_NO_ERROR||major<3||(major==3&&minor<3)) {diagnostic_="requires desktop GL 3.3";return ACVR_UNSUPPORTED;}
    const char *vertex="#version 330\nlayout(location=0) in vec3 position;layout(location=1) in vec4 colour;uniform mat4 mvp;out vec4 tint;void main(){gl_Position=mvp*vec4(position,1);tint=colour;}";
    const char *fragment="#version 330\nin vec4 tint;out vec4 pixel;void main(){pixel=vec4(tint.rgb*tint.a,tint.a);}";
    GLuint shaders[2]{};bool good=true;
    for(unsigned i=0;i<2;++i) {
        shaders[i]=gl_.CreateShader(i?FragmentShader:VertexShader);if(!shaders[i]) {good=false;break;}
        const char *source=i?fragment:vertex;gl_.ShaderSource(shaders[i],1,&source,nullptr);gl_.CompileShader(shaders[i]);
        GLint compiled=0;gl_.GetShaderiv(shaders[i],CompileStatus,&compiled);if(!compiled) {good=false;break;}
    }
    if(good) {
        program_=gl_.CreateProgram();good=program_!=0;
        if(good) {for(auto shader:shaders) gl_.AttachShader(program_,shader);gl_.LinkProgram(program_);GLint linked=0;gl_.GetProgramiv(program_,LinkStatus,&linked);good=linked!=0;}
    }
    for(auto shader:shaders) if(shader) gl_.DeleteShader(shader);
    if(good) {matrix_=gl_.GetUniformLocation(program_,"mvp");gl_.GenVertexArrays(1,&vao_);gl_.GenBuffers(1,&buffer_);good=matrix_>=0&&vao_&&buffer_;}
    if(gl_.GetError()!=GL_NO_ERROR||!good) {diagnostic_="gun shader/program/resource creation failed";shutdown();return ACVR_ERROR;}
    ready_=true;return ACVR_OK;
}
acvr_result GunGlRenderer::shutdown() {
    if(!program_&&!vao_&&!buffer_) {ready_=false;return ACVR_OK;}
    if(owner_!=std::this_thread::get_id()) return ACVR_BAD_STATE;
    if(buffer_) gl_.DeleteBuffers(1,&buffer_);
    if(vao_) gl_.DeleteVertexArrays(1,&vao_);
    if(program_) gl_.DeleteProgram(program_);
    buffer_=vao_=program_=0;ready_=false;return gl_.GetError()==GL_NO_ERROR?ACVR_OK:ACVR_ERROR;
}
acvr_result GunGlRenderer::draw(const acvr_draw_info &in,const GunDraw &gun) {
    if(!ready_||owner_!=std::this_thread::get_id()) return ACVR_BAD_STATE;
    diagnostic_.clear();
    if(in.size<sizeof(in)||in.version!=ACVR_STRUCT_VERSION||in.target.size<sizeof(in.target)||in.target.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    const auto &target=in.target;
    if(target.api!=ACVR_GRAPHICS_GL||target.array_layers!=1||target.sample_count!=1||!target.framebuffer||
       !target.depth_image||!target.colour_image||!target.depth_format||gun.laser_mode!=ACVR_LASER_OFF) return ACVR_UNSUPPORTED;
    if(target.colour_format!=GL_RGBA8&&target.colour_format!=Srgb8Alpha8) return ACVR_UNSUPPORTED;
    if(target.framebuffer>UINT32_MAX||target.depth_image>UINT32_MAX||target.colour_image>UINT32_MAX||!in.views||in.view_count!=1||
       in.view_stride<sizeof(acvr_eye)||in.view_stride%alignof(acvr_eye)||reinterpret_cast<uintptr_t>(in.views)%alignof(acvr_eye)) return ACVR_BAD_ARGUMENT;
    const auto &eye=*in.views;
    if(eye.size<sizeof(eye)||eye.size>in.view_stride||eye.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(eye.eye_index>1||eye.array_layer||eye.rect_x<0||eye.rect_y<0||!eye.rect_width||!eye.rect_height||
       uint64_t(eye.rect_x)+eye.rect_width>target.width||uint64_t(eye.rect_y)+eye.rect_height>target.height||
       target.width>INT32_MAX||target.height>INT32_MAX||!finite(eye.view_from_scene,16)||!finite(eye.projection_from_view,16)) return ACVR_BAD_ARGUMENT;
    if(eye.projection_from_view[11]!=-1||eye.projection_from_view[15]!=0||eye.projection_from_view[10]>-1||eye.projection_from_view[14]>=0) return ACVR_UNSUPPORTED;
    std::vector<GunGlVertex> vertices;uint32_t opaque=0;const auto packed=pack_gun_vertices(gun,eye,vertices,opaque);if(packed!=ACVR_OK) return packed;
    const auto mvp=product(eye.projection_from_view,eye.view_from_scene);if(!finite(mvp.data(),16)) return ACVR_BAD_ARGUMENT;
    if(gl_.GetError()!=GL_NO_ERROR) {diagnostic_="pre-existing GL error before gun draw";return ACVR_BAD_STATE;}
    State state{gl_};gl_.GetIntegerv(CurrentProgram,&state.program);gl_.GetIntegerv(VertexArrayBinding,&state.vao);
    gl_.GetIntegerv(ArrayBufferBinding,&state.buffer);gl_.GetIntegerv(DrawFramebufferBinding,&state.fbo);
    GLint stack=0,limit=0,clips=0;gl_.GetIntegerv(GL_ATTRIB_STACK_DEPTH,&stack);gl_.GetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH,&limit);gl_.GetIntegerv(MaxClipDistances,&clips);
    if(gl_.GetError()!=GL_NO_ERROR||stack<0||stack>=limit||clips<0||clips>32) return ACVR_UNSUPPORTED;
    for(unsigned i=0;i<5;++i) state.modern[i]=gl_.IsEnabled(modern_caps[i]);
    state.clips.resize(size_t(clips));for(GLint i=0;i<clips;++i) state.clips[size_t(i)]=gl_.IsEnabled(ClipDistance0+GLenum(i));
    if(gl_.GetError()!=GL_NO_ERROR) return ACVR_ERROR;
    const auto finish=[&](acvr_result result) {state.restore();if(gl_.GetError()!=GL_NO_ERROR&&result==ACVR_OK) result=ACVR_ERROR;return result;};
    state.touched=true;gl_.BindFramebuffer(DrawFramebuffer,GLuint(target.framebuffer));
    if(gl_.CheckFramebufferStatus(DrawFramebuffer)!=Complete) return finish(ACVR_BAD_ARGUMENT);
    for(const auto attachment:{ColourAttachment,DepthAttachment}) {
        GLint type=0,name=0;gl_.GetFramebufferAttachmentParameteriv(DrawFramebuffer,attachment,ObjectType,&type);gl_.GetFramebufferAttachmentParameteriv(DrawFramebuffer,attachment,ObjectName,&name);
        if(type!=GL_TEXTURE||GLuint(name)!=(attachment==DepthAttachment?target.depth_image:target.colour_image)) return finish(ACVR_BAD_ARGUMENT);
    }
    if(gl_.GetError()!=GL_NO_ERROR) return finish(ACVR_ERROR);
    gl_.PushAttrib(GL_ALL_ATTRIB_BITS);if(gl_.GetError()!=GL_NO_ERROR) return finish(ACVR_ERROR);state.pushed=true;
    gl_.UseProgram(program_);gl_.BindVertexArray(vao_);gl_.BindBuffer(ArrayBuffer,buffer_);
    gl_.BufferData(ArrayBuffer,std::ptrdiff_t(vertices.size()*sizeof(GunGlVertex)),vertices.data(),StreamDraw);
    gl_.EnableVertexAttribArray(0);gl_.EnableVertexAttribArray(1);
    gl_.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GunGlVertex),nullptr);
    gl_.VertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(GunGlVertex),reinterpret_cast<void *>(offsetof(GunGlVertex,colour)));
    gl_.UniformMatrix4fv(matrix_,1,GL_FALSE,mvp.data());
    gl_.Viewport(eye.rect_x,eye.rect_y,GLsizei(eye.rect_width),GLsizei(eye.rect_height));gl_.Scissor(eye.rect_x,eye.rect_y,GLsizei(eye.rect_width),GLsizei(eye.rect_height));
    gl_.Enable(GL_SCISSOR_TEST);gl_.Enable(GL_DEPTH_TEST);gl_.DepthFunc(GL_LEQUAL);gl_.DepthRange(0,1);gl_.DepthMask(GL_TRUE);
    gl_.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);gl_.PolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    for(GLenum cap:{GL_BLEND,GL_ALPHA_TEST,GL_CULL_FACE,GL_STENCIL_TEST,GL_DITHER,GL_COLOR_LOGIC_OP,GL_POLYGON_OFFSET_FILL,GL_POLYGON_STIPPLE}) gl_.Disable(cap);
    for(auto cap:modern_caps) gl_.Disable(cap);
    if(target.colour_format==Srgb8Alpha8) gl_.Enable(FramebufferSrgb);
    for(GLint i=0;i<clips;++i) gl_.Disable(ClipDistance0+GLenum(i));
    if(gl_.GetError()!=GL_NO_ERROR) return finish(ACVR_ERROR);
    // Preserve borrowed scene colour/depth. This module has no clear, detach,
    // swap, flush, readback, texture delete or simulation function in its table.
    if(opaque) gl_.DrawArrays(GL_TRIANGLES,0,GLsizei(opaque));
    if(vertices.size()>opaque) {
        gl_.Enable(GL_BLEND);gl_.BlendEquation(FuncAdd);gl_.BlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);gl_.DepthMask(GL_FALSE);
        gl_.DrawArrays(GL_TRIANGLES,GLint(opaque),GLsizei(vertices.size()-opaque));
    }
    return finish(gl_.GetError()==GL_NO_ERROR?ACVR_OK:ACVR_ERROR);
}
}
