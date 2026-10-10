// SPDX-License-Identifier: MIT
// Explicit CPU mock dispatch. No GL library/context, GPU or rendered-pixel claim.
#include "gun_gl.hpp"
#include "gun_fixture.hpp"
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <tuple>
#include <type_traits>

unsigned checks=0;
void check(bool ok) {++checks;if(!ok) throw std::runtime_error("GL mock check "+std::to_string(checks));}
template<class T> T init() {T v{};ACVR_INIT(&v);return v;}
struct Mock {
    GLint program=17,vao=18,buffer=19,fbo=20;
    std::map<GLenum,GLboolean> caps{{0x8db9,GLboolean(GL_FALSE)},{0x8c89,GLboolean(GL_TRUE)},{0x864f,GLboolean(GL_TRUE)},{0x8e51,GLboolean(GL_TRUE)},{0x809e,GLboolean(GL_TRUE)}};
    std::map<GLenum,GLboolean> saved_caps;
    GLboolean depth_write=GL_FALSE,saved_depth=GL_FALSE;
    GLenum error=GL_NO_ERROR;
    bool compile=true,complete=true,fail_draw=false,fail_upload=false;
    int major=3,minor=3;
    GLuint next=100;
    unsigned draws=0,pushes=0,pops=0,deletes=0;
    std::vector<acvr::GunGlVertex> uploaded;
    std::vector<GLboolean> draw_depth;
    std::vector<GLsizei> draw_counts;
} mock;
template<class Fn> struct Noop;
template<class R,class... Args> struct Noop<R(ACVR_GL_CALL *)(Args...)> {
    static R ACVR_GL_CALL call(Args...) {if constexpr(!std::is_void_v<R>) return R{};}
};
template<class Fn> void *address(Fn fn) {void *p=nullptr;static_assert(sizeof(p)==sizeof(fn));std::memcpy(&p,&fn,sizeof(p));return p;}
GLenum ACVR_GL_CALL error() {const auto e=mock.error;mock.error=GL_NO_ERROR;return e;}
void ACVR_GL_CALL integer(GLenum name,GLint *out) {
    switch(name) {
    case 0x821b:*out=mock.major;break;case 0x821c:*out=mock.minor;break;
    case 0x8b8d:*out=mock.program;break;case 0x85b5:*out=mock.vao;break;case 0x8894:*out=mock.buffer;break;case 0x8ca6:*out=mock.fbo;break;
    case GL_ATTRIB_STACK_DEPTH:*out=0;break;case GL_MAX_ATTRIB_STACK_DEPTH:*out=16;break;case 0x0d32:*out=4;break;
    default:throw std::runtime_error("unexpected query");
    }
}
GLboolean ACVR_GL_CALL enabled(GLenum cap) {return mock.caps[cap];}
void ACVR_GL_CALL enable(GLenum cap) {mock.caps[cap]=GL_TRUE;}
void ACVR_GL_CALL disable(GLenum cap) {mock.caps[cap]=GL_FALSE;}
GLuint ACVR_GL_CALL shader(GLenum) {return ++mock.next;}
GLuint ACVR_GL_CALL program() {return ++mock.next;}
void ACVR_GL_CALL shader_status(GLuint,GLenum,GLint *status) {*status=mock.compile?1:0;}
void ACVR_GL_CALL program_status(GLuint,GLenum,GLint *status) {*status=1;}
GLint ACVR_GL_CALL uniform(GLuint,const char *) {return 3;}
void ACVR_GL_CALL generate(GLsizei count,GLuint *out) {for(GLsizei i=0;i<count;++i) out[i]=++mock.next;}
void ACVR_GL_CALL use(GLuint id) {mock.program=GLint(id);}
void ACVR_GL_CALL vao(GLuint id) {mock.vao=GLint(id);}
void ACVR_GL_CALL buffer(GLenum,GLuint id) {mock.buffer=GLint(id);}
void ACVR_GL_CALL framebuffer(GLenum target,GLuint id) {check(target==0x8ca9);mock.fbo=GLint(id);}
GLenum ACVR_GL_CALL complete(GLenum) {return mock.complete?0x8cd5:0;}
void ACVR_GL_CALL attachment(GLenum,GLenum slot,GLenum query,GLint *out) {*out=query==0x8cd0?GL_TEXTURE:slot==0x8d00?44:33;}
void ACVR_GL_CALL push(GLbitfield) {++mock.pushes;mock.saved_caps=mock.caps;mock.saved_depth=mock.depth_write;}
void ACVR_GL_CALL pop() {++mock.pops;mock.caps=mock.saved_caps;mock.depth_write=mock.saved_depth;}
void ACVR_GL_CALL depth(GLboolean value) {mock.depth_write=value;}
void ACVR_GL_CALL upload(GLenum,std::ptrdiff_t bytes,const void *data,GLenum) {
    if(mock.fail_upload) {mock.error=GL_OUT_OF_MEMORY;return;}
    const auto *v=static_cast<const acvr::GunGlVertex *>(data);mock.uploaded.assign(v,v+bytes/std::ptrdiff_t(sizeof(*v)));
}
void ACVR_GL_CALL draw(GLenum mode,GLint first,GLsizei count) {
    check(mode==GL_TRIANGLES && first>=0 && count>0);check(mock.fbo==55 && mock.caps[GL_DEPTH_TEST] && !mock.caps[0x8c89]);
    ++mock.draws;mock.draw_depth.push_back(mock.depth_write);mock.draw_counts.push_back(count);
    if(mock.fail_draw) mock.error=GL_INVALID_OPERATION;
}
void ACVR_GL_CALL remove_one(GLuint) {++mock.deletes;}
void ACVR_GL_CALL remove_many(GLsizei n,const GLuint *) {mock.deletes+=unsigned(n);}
void *ACVR_CALL proc(void *,const char *name) {
#define MAP(n,fn) if(std::strcmp(name,"gl" #n)==0) return address(&fn)
    MAP(GetError,error);MAP(GetIntegerv,integer);MAP(IsEnabled,enabled);MAP(Enable,enable);MAP(Disable,disable);
    MAP(CreateShader,shader);MAP(CreateProgram,program);MAP(GetShaderiv,shader_status);MAP(GetProgramiv,program_status);MAP(GetUniformLocation,uniform);
    MAP(GenVertexArrays,generate);MAP(GenBuffers,generate);MAP(UseProgram,use);MAP(BindVertexArray,vao);MAP(BindBuffer,buffer);
    MAP(BindFramebuffer,framebuffer);MAP(CheckFramebufferStatus,complete);MAP(GetFramebufferAttachmentParameteriv,attachment);
    MAP(PushAttrib,push);MAP(PopAttrib,pop);MAP(DepthMask,depth);MAP(BufferData,upload);MAP(DrawArrays,draw);
    MAP(DeleteProgram,remove_one);MAP(DeleteShader,remove_one);MAP(DeleteBuffers,remove_many);MAP(DeleteVertexArrays,remove_many);
#undef MAP
#define DEFAULT(name_,ret,args) if(std::strcmp(name,"gl" #name_)==0) return address(&Noop<acvr::GunGlDispatch::name_##Fn>::call);
    ACVR_GUN_GL_FUNCTIONS(DEFAULT)
#undef DEFAULT
    return nullptr;
}
acvr_graphics_device device() {auto d=init<acvr_graphics_device>();d.api=ACVR_GRAPHICS_GL;d.flags=ACVR_DEVICE_GL_COMPATIBILITY;d.context=1;d.get_proc=proc;return d;}
acvr_eye eye() {auto e=init<acvr_eye>();for(unsigned i=0;i<4;++i) e.view_from_scene[5*i]=1;e.projection_from_view[0]=e.projection_from_view[5]=1;e.projection_from_view[10]=-1.1f;e.projection_from_view[11]=-1;e.projection_from_view[14]=-.2f;e.rect_width=e.rect_height=512;return e;}
acvr_draw_info info(acvr_eye &e) {
    auto d=init<acvr_draw_info>();d.target=init<acvr_render_target>();auto &t=d.target;t.api=ACVR_GRAPHICS_GL;t.width=t.height=512;t.array_layers=t.sample_count=1;
    t.colour_format=0x8c43;t.depth_format=0x81a6;t.framebuffer=55;t.colour_image=33;t.depth_image=44;d.views=&e;d.view_count=1;d.view_stride=sizeof(e);return d;
}
acvr::GunDraw packet(acvr::GunInstance &gun) {
    std::string error;check(acvr::decode_gun_model(Fixture{}.bytes(),"id='synthetic'","synthetic",gun.model,error));gun.reset();
    auto p=init<acvr_pose>();p.orientation_xyzw[3]=1;auto c=init<acvr_gun_slot_config>();c.angle_xyzw[3]=1;c.show_gun=1;
    for(unsigned i=0;i<4;++i) {c.body_rgba[i]=1;c.accent_rgba[i]=1;}
    acvr::GunDraw out;check(gun.draw(p,p,c,1,5,0,out)==ACVR_OK);return out;
}
void renderer() {
    mock=Mock{};acvr::GunGlRenderer r;auto d=device();check(r.initialize(d)==ACVR_OK);
    auto e=eye();auto in=info(e);acvr::GunInstance gun;auto p=packet(gun);
    const auto caps=mock.caps;check(r.draw(in,p)==ACVR_OK && mock.draws==1 && mock.draw_depth[0]);
    check(mock.uploaded.size()==3 && std::abs(mock.uploaded[1].position[0]-.11f)<.0001f);
    check(mock.program==17 && mock.vao==18 && mock.buffer==19 && mock.fbo==20 && mock.pushes==mock.pops);
    for(const auto &[key,value]:caps) check(mock.caps[key]==value);
    check(mock.depth_write==GL_FALSE);
    acvr_result foreign=ACVR_OK;std::thread other([&]{foreign=r.draw(in,p);});other.join();
    check(foreign==ACVR_BAD_STATE && mock.draws==1);
    in.target.depth_image=0;check(r.draw(in,p)==ACVR_UNSUPPORTED && mock.draws==1);in.target.depth_image=44;
    in.target.colour_image=34;check(r.draw(in,p)==ACVR_BAD_ARGUMENT && mock.draws==1 && mock.fbo==20);in.target.colour_image=33;
    mock.error=GL_INVALID_VALUE;check(r.draw(in,p)==ACVR_BAD_STATE && mock.draws==1);
    mock.fail_upload=true;check(r.draw(in,p)==ACVR_ERROR && mock.draws==1 && mock.fbo==20 && mock.pushes==mock.pops);mock.fail_upload=false;
    mock.fail_draw=true;check(r.draw(in,p)==ACVR_ERROR && mock.draws==2 && mock.program==17 && mock.fbo==20 && mock.pushes==mock.pops);mock.fail_draw=false;
    p.laser_mode=ACVR_LASER_DOT;check(r.draw(in,p)==ACVR_UNSUPPORTED);p.laser_mode=0;
    gun.model.asset.materials[0].base_colour[3]=.5f;
    check(r.draw(in,p)==ACVR_OK && mock.draw_depth.back()==GL_FALSE);
    check(r.shutdown()==ACVR_OK && r.draw(in,p)==ACVR_BAD_STATE);
    mock=Mock{};mock.compile=false;acvr::GunGlRenderer failed;check(failed.initialize(device())==ACVR_ERROR && mock.deletes>0);
    mock=Mock{};mock.major=2;check(failed.initialize(device())==ACVR_UNSUPPORTED);
    auto unsupported=device();unsupported.flags=0;check(failed.initialize(unsupported)==ACVR_UNSUPPORTED);
    unsupported.flags=ACVR_DEVICE_GLES;check(failed.initialize(unsupported)==ACVR_UNSUPPORTED);
}
void packing() {
    acvr::GunInstance gun;auto p=packet(gun);auto e=eye();std::vector<acvr::GunGlVertex> vertices;uint32_t opaque=999;
    p.body[0]=.25f;check(acvr::pack_gun_vertices(p,e,vertices,opaque)==ACVR_OK && opaque==3 && vertices[0].colour[0]==.125f);
    p.visible=false;check(acvr::pack_gun_vertices(p,e,vertices,opaque)==ACVR_OK && vertices.empty());p.visible=true;
    p.lod=1;check(acvr::pack_gun_vertices(p,e,vertices,opaque)==ACVR_OK && vertices[1].position[0]==.1f);
    p.lod=0;gun.model.asset.materials[0].base_colour[3]=.5f;gun.model.asset.primitives[1].lod=0;p.scene_from_node[5][14]=-2;
    check(acvr::pack_gun_vertices(p,e,vertices,opaque)==ACVR_OK && opaque==0 && vertices.size()==6);
    check(vertices[0].position[2]==-2 && vertices[3].position[2]==0); // translucent triangles sorted far to near per eye
    p.lod=0;gun.model.asset.primitives[0].indices[0]=999;check(acvr::pack_gun_vertices(p,e,vertices,opaque)==ACVR_BAD_ARGUMENT);
}
int main() {try {renderer();packing();} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}std::cout<<checks<<" GL mock checks passed (no GPU)\n";}
