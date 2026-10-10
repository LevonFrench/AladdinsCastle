// SPDX-License-Identifier: GPL-3.0-only
#include "n22_cpu_backend.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks=0;
void check(bool condition,const char *message) {
    ++checks; if(!condition) throw std::runtime_error(message);
}
bool near(float a,float b,float eps=1e-5f) { return std::abs(a-b)<eps; }
template<class T> T record() { T t{}; ACVR_INIT(&t); return t; }
n22::Polygon triangle(float depth,uint32_t rgb,n22::Layer layer=n22::Layer::World) {
    n22::Polygon p; p.rgb=rgb; p.layer=layer;
    p.vertices={n22::ProjectedVertex{240*16,320*16,depth},
                n22::ProjectedVertex{400*16,320*16,depth},
                n22::ProjectedVertex{320*16,160*16,depth}};
    return p;
}
acvr_ray ray(float x=0,float y=0,float z=-1) {
    auto r=record<acvr_ray>(); r.direction_scene[0]=x; r.direction_scene[1]=y;
    r.direction_scene[2]=z; r.max_distance_scene=100; return r;
}
std::array<float,2> project_eye(n22::Vec3 v,const acvr_eye &e) {
    // Independent formula for the generated translation-only test cameras.
    float x=v.x+e.view_from_scene[12],y=v.y,z=v.z;
    return {(e.projection_from_view[0]*x+e.projection_from_view[8]*z)/-z,
            e.projection_from_view[5]*y/-z};
}
void reconstruction() {
    n22::Frame f; auto in=n22::synthetic_cube();
    check(n22::prepare(in,1,f)==ACVR_OK && f.triangles.size()==12,"cube preparation");
    for(const auto &t:f.triangles) for(const auto &v:t.vertices)
        check(near(std::abs(v.x),.5f) && near(std::abs(v.y),.5f) && (near(v.z,-2) || near(v.z,-3)),"cube reconstructed dimensions");
    auto c=n22::synthetic_camera(); c.focal_x_px=777; c.focal_y_px=555;
    c.centre_x_px=290; c.centre_y_px=230;
    for(float nx:{0.f,.1f,.5f,.9f,1.f}) for(float ny:{0.f,.1f,.5f,.9f,1.f}) {
        n22::ProjectedVertex v{nx*640*16,ny*480*16,6};
        float x,y; bool off;
        check(n22::project_gun(n22::unproject(v,c),c,x,y,off)==ACVR_OK && near(x,nx) && near(y,ny) && !off,"asymmetric focal gun round trip");
    }
    const auto before=f.triangles;
    in.polygons[0].vertices[0].depth=std::numeric_limits<float>::quiet_NaN();
    check(n22::prepare(in,2,f)==ACVR_BAD_ARGUMENT && f.id==1 && f.triangles.size()==before.size(),"invalid prepare preserves old frame");
    in=n22::synthetic_cube(); in.cameras[0].view_from_scene[12]=1;
    check(n22::prepare(in,2,f)==ACVR_BAD_ARGUMENT,"rejects non-view-local camera transform");
    float x,y;bool off;
    check(n22::project_gun({0,0,1},c,x,y,off)==ACVR_BAD_ARGUMENT && x==.5f && y==.5f && off,"behind-camera finite neutral aim");
    c.viewport_px[0]=100;c.viewport_px[2]=440;
    check(n22::project_gun(n22::unproject({50*16,240*16,2},c),c,x,y,off)==ACVR_OK && off && near(x,50.f/640),"native viewport offscreen retains diagnostics");
    check(n22::time_crisis_adc(0,0)==std::array<uint32_t,2>{68,43},"TC top left ADC");
    check(n22::time_crisis_adc(1,1)==std::array<uint32_t,2>{694,284},"TC bottom right ADC");
    check(n22::time_crisis_adc(.5f,.5f)==std::array<uint32_t,2>{381,163},"TC centre floors ADC");
    check(n22::time_crisis_adc(-1,2)==std::array<uint32_t,2>{68,284},"TC clamp at device boundary");
}
void stereo_and_raster() {
    auto l=n22::desktop_eye(0,-.032f,4,320,240),r=n22::desktop_eye(1,.032f,4,320,240);
    auto a=project_eye({0,.2f,-4},l),b=project_eye({0,.2f,-4},r);
    check(near(a[0],b[0]) && near(a[1],b[1]),"off-axis convergence and no vertical disparity");
    check(l.view_from_scene[0]==1 && r.view_from_scene[0]==1 && l.view_from_scene[8]==0 && r.view_from_scene[8]==0,"parallel cameras never toe in");
    auto near_l=project_eye({0,0,-2},l),near_r=project_eye({0,0,-2},r);
    auto far_l=project_eye({0,0,-8},l),far_r=project_eye({0,0,-8},r);
    check(near_l[0]>near_r[0] && far_l[0]<far_r[0],"disparity changes sign across convergence plane");
    n22::SceneInput in;in.cameras.push_back(n22::synthetic_camera());
    in.polygons={triangle(5,0x0000ff),triangle(2,0xff0000)};
    n22::Frame f;check(n22::prepare(in,1,f)==ACVR_OK,"overlap scene");
    auto mono=n22::desktop_eye(0,0,4,320,240);
    n22::Image im(640,240); std::fill(im.rgb.begin(),im.rgb.end(),0xabcdef);
    check(n22::draw_cpu(f,mono,im)==ACVR_OK,"CPU draw");
    check(im.rgb[120*640+160]==0xff0000,"nearest polygon wins depth test");
    check(im.rgb[120*640+480]==0xabcdef,"draw clears only supplied eye rectangle");
    auto pixels=im.rgb;
    check(n22::draw_cpu(f,mono,im)==ACVR_OK && im.rgb==pixels && f.id==1,"same lease replay stable");
    std::reverse(in.polygons.begin(),in.polygons.end()); n22::Frame g;
    check(n22::prepare(in,1,g)==ACVR_OK && n22::draw_cpu(g,mono,im)==ACVR_OK && im.rgb==pixels,"depth independent of submission order");
    auto translated=mono; translated.view_from_scene[12]=-.3f;
    check(n22::draw_cpu(f,translated,im)==ACVR_OK && im.rgb!=pixels && f.id==1,"latest pose redraw changes image without simulation");
    n22::Image pair(640,240);
    check(n22::draw_cpu(f,l,pair)==ACVR_OK,"left eye draw");
    auto left=pair.rgb;
    check(n22::draw_cpu(f,r,pair)==ACVR_OK,"right eye draw");
    bool left_same=true,right_present=false;
    for(size_t y=0;y<240;++y) for(size_t x=0;x<320;++x) {
        left_same=left_same && pair.rgb[y*640+x]==left[y*640+x];
        right_present=right_present || pair.rgb[y*640+x+320]!=0;
    }
    check(left_same && right_present && f.id==1,"paired eyes share frame and preserve opposite rectangle");
    auto bad=mono;bad.rect_x=-1; pixels=im.rgb;
    check(n22::draw_cpu(f,bad,im)==ACVR_BAD_ARGUMENT && pixels==im.rgb,"invalid rectangle draws nothing");
    // A polygon crossing near and side planes must clip, not disappear.
    in.polygons={triangle(.2f,0x11ff22)};
    in.polygons[0].vertices[0]={-1000*16,400*16,.05f};
    check(n22::prepare(in,2,g)==ACVR_OK && n22::draw_cpu(g,mono,im)==ACVR_OK &&
          std::count(im.rgb.begin(),im.rgb.end(),0x11ff22u)>0,"homogeneous near/side clipping");
}
void layers_and_aim() {
    n22::SceneInput in;in.cameras.push_back(n22::synthetic_camera());
    in.polygons={triangle(3,0x123456),triangle(20,0xfedcba,n22::Layer::Hud),triangle(1,0xffffff,n22::Layer::GunFlash)};
    n22::Frame f;check(n22::prepare(in,1,f)==ACVR_OK && f.triangles.size()==2,"explicit flash discarded");
    check(near(f.triangles[1].vertices[0].z,-2),"HUD flattened to configured depth");
    auto h=record<acvr_hit>();auto aim=ray();aim.origin_scene[0]=.2f;
    // A displaced gun pointed straight forward hits x=.2, not the camera centre.
    check(n22::raycast(f,aim,h)==ACVR_OK && h.found && near(h.position_scene[0],.2f) && near(h.distance_scene,3),"nearest world hit excludes HUD/flash");
    check(h.flags==ACVR_HIT_GUN_COORDS && near(h.screen_x,.55f) && near(h.screen_y,.5f),"v0.1 hit returns normalized native coordinates");
    auto old_hit=record<acvr_hit>();old_hit.size=ACVR_HIT_V1_SIZE;
    old_hit.flags=0xfefefefe;old_hit.screen_x=123;old_hit.screen_y=456;
    check(n22::raycast(f,aim,old_hit)==ACVR_OK && old_hit.found && old_hit.flags==0xfefefefe &&
          old_hit.screen_x==123 && old_hit.screen_y==456,"old hit prefix leaves entire tail untouched");
    old_hit.size=ACVR_HIT_V1_SIZE+sizeof(uint32_t);
    check(n22::raycast(f,aim,old_hit)==ACVR_OK && old_hit.flags==0xfefefefe && old_hit.screen_x==123,"partial hit tail remains untouched");
    old_hit.size=ACVR_HIT_V1_SIZE-1;
    check(n22::raycast(f,aim,old_hit)==ACVR_BAD_VERSION,"undersized hit rejected");
    old_hit.version=99;old_hit.size=sizeof(old_hit);
    check(n22::raycast(f,aim,old_hit)==ACVR_BAD_VERSION,"unknown hit version rejected");
    struct ExtendedHit { acvr_hit hit; uint64_t sentinel; } extended_hit{};
    ACVR_INIT(&extended_hit.hit);extended_hit.hit.size=sizeof(extended_hit);extended_hit.sentinel=1234567;
    check(n22::raycast(f,aim,extended_hit.hit)==ACVR_OK && extended_hit.sentinel==1234567 &&
          extended_hit.hit.size==sizeof(extended_hit),"hit preserves unknown appended bytes and caller size");
    float x,y;bool off;
    check(n22::project_gun({h.position_scene[0],h.position_scene[1],h.position_scene[2]},f.cameras[0],x,y,off)==ACVR_OK && near(x,.55f) && near(y,.5f),"displaced gun parallax round trip");
    aim.max_distance_scene=2;
    check(n22::raycast(f,aim,h)==ACVR_OK && !h.found && h.camera_id==ACVR_NO_CAMERA && h.distance_scene==0 &&
          h.flags==0 && h.screen_x==.5f && h.screen_y==.5f,"ray max range miss clears old hit including tail");
    check(n22::project_gun({.2f,0,-100},f.cameras[0],x,y,off)==ACVR_OK && near(x,.5015f) && near(y,.5f),"far-along-ray miss projection for runtime fallback");
    aim=ray(0,0,-2);
    check(n22::raycast(f,aim,h)==ACVR_BAD_ARGUMENT,"non-unit ray rejected");
    n22::Image world(320,240),hud(320,240);auto e=n22::desktop_eye(0,0,4,320,240);
    check(n22::draw_cpu(f,e,world)==ACVR_OK && n22::draw_cpu(f,e,hud,true)==ACVR_OK &&
          world.rgb[120*320+160]==0x123456 && hud.rgb[120*320+160]==0xfedcba,"HUD/world separation");
    // Two source cameras retain the camera associated with the nearest polygon.
    in.cameras.push_back(in.cameras[0]);in.cameras[1].camera_id=1;in.cameras[1].focal_x_px=400;
    in.polygons={triangle(3,0x123456),triangle(1,0x654321)};in.polygons[1].camera_id=1;
    check(n22::prepare(in,2,f)==ACVR_OK && n22::raycast(f,ray(),h)==ACVR_OK && h.camera_id==1,"nearest triangle camera provenance");
    in.polygons={triangle(3,0x2222ff,n22::Layer::Backdrop)};
    check(n22::prepare(in,3,f)==ACVR_OK,"backdrop prepare");
    e=n22::desktop_eye(0,0,4,320,240);e.projection_from_view[8]=0;
    check(n22::draw_cpu(f,e,world)==ACVR_OK,"backdrop draw");auto pixels=world.rgb;
    e.view_from_scene[12]=100; e.view_from_scene[13]=20;
    check(n22::draw_cpu(f,e,world)==ACVR_OK && world.rgb==pixels,"infinity backdrop ignores translation");
    check(n22::raycast(f,ray(),h)==ACVR_OK && !h.found,"backdrops excluded from aim");
}
void abi_lifecycle() {
    auto api=record<acvr_backend_api>();
    check(acvr_backend_query(99,&api)==ACVR_BAD_VERSION,"factory rejects ABI mismatch");
    api.size=sizeof(api)-1;
    check(acvr_backend_query(ACVR_ABI_VERSION,&api)==ACVR_BAD_VERSION,"factory rejects undersized callback table");
    api=record<acvr_backend_api>();
    check(acvr_backend_query(ACVR_ABI_VERSION,&api)==ACVR_OK && !api.supported_graphics && api.game_draw_eye,"CPU query cannot be accepted as graphics backend");
    auto in=record<acvr_open_info>();ACVR_INIT(&in.graphics);in.game_id_utf8="timecris";
    acvr_backend *backend=reinterpret_cast<acvr_backend *>(1);auto info=record<acvr_backend_info>();
    check(api.game_open(&in,&backend,&info)==ACVR_UNSUPPORTED && !backend,"real game requests rejected without content access");
    in.game_id_utf8="synthetic-system22";
    check(api.game_open(&in,&backend,&info)==ACVR_OK && backend && info.gun_count==0,"synthetic-only backend open");
    auto input=record<acvr_inputs>();input.tick_id=1;
    check(api.game_set_inputs(backend,&input)==ACVR_OK,"complete neutral synthetic input");
    auto scene=n22::synthetic_cube();
    check(n22::stage_cpu_scene(backend,scene)==ACVR_OK,"stage owned scene");
    scene.polygons.clear(); // Mutation after stage must not change the frame.
    auto step=record<acvr_step_info>();step.tick_id=1;
    auto frame_info=record<acvr_frame_info>();acvr_frame *frame=nullptr;
    check(api.game_step(backend,&step,&frame,&frame_info)==ACVR_OK && frame_info.frame_id==1,"step publishes lease");
    acvr_frame *second=frame;
    check(api.game_step(backend,&step,&second,&frame_info)==ACVR_BAD_STATE && !second,"outstanding lease forbids stepping");
    check(n22::stage_cpu_scene(backend,scene)==ACVR_BAD_STATE,"outstanding lease forbids mutation");
    auto c=record<acvr_game_camera>();check(api.game_camera(backend,frame,0,&c)==ACVR_OK,"leased camera");
    auto h=record<acvr_hit>();auto r=ray();
    check(api.game_raycast(backend,frame,&r,&h)==ACVR_OK && h.found,"staging made deep copy");
    n22::Image image(640,240);auto e=n22::desktop_eye(0,-.032f,4,320,240);
    check(n22::draw_cpu_frame(backend,frame,e,image)==ACVR_OK,"draw lease CPU bridge");auto pixels=image.rgb;
    auto event=record<acvr_output_event>();auto drain=record<acvr_outputs>();
    drain.capacity=1;drain.stride=sizeof(event);drain.events=&event;
    check(api.game_poll_outputs(backend,&drain)==ACVR_OK && drain.count==0 && drain.dropped==0,"synthetic output queue empty before draw");
    check(api.game_draw_eye(backend,frame,nullptr)==ACVR_UNSUPPORTED && pixels==image.rgb,"GPU callback unsupported before drawing");
    check(api.game_poll_outputs(backend,&drain)==ACVR_OK && drain.count==0 && drain.dropped==0,"replay does not generate outputs");
    check(api.game_pause(backend,1)==ACVR_OK && n22::draw_cpu_frame(backend,frame,e,image)==ACVR_OK && image.rgb==pixels,"paused lease remains replayable");
    api.game_release_frame(backend,frame);api.game_pause(backend,0);
    input.tick_id=2;check(api.game_set_inputs(backend,&input)==ACVR_OK,"resume neutral tick");
    step.tick_id=2;step.simulation_time_ns=1000000000000LL/59906;
    check(api.game_step(backend,&step,&frame,&frame_info)==ACVR_OK && frame_info.camera_count==1,"empty frame retains last camera");
    check(api.game_camera(backend,frame,0,&c)==ACVR_OK && (c.flags&ACVR_CAMERA_RETAINED),"retained camera tagged");
    check(api.game_raycast(backend,frame,&r,&h)==ACVR_OK && !h.found,"empty frame does not retain stale geometry");
    api.game_release_frame(backend,frame);api.game_close(backend);
    struct Extended { acvr_backend_api api; uint64_t sentinel; } extended{};
    ACVR_INIT(&extended.api);extended.api.size=sizeof(extended);extended.sentinel=0x123456789abcdef0ULL;
    check(acvr_backend_query(ACVR_ABI_VERSION,&extended.api)==ACVR_OK && extended.api.size==sizeof(extended) &&
          extended.sentinel==0x123456789abcdef0ULL,"ABI output preserves caller prefix and unknown tail");
}
}
int main() {
    try { reconstruction();stereo_and_raster();layers_and_aim();abi_lifecycle();
        std::cout<<checks<<" synthetic checks passed (CPU only)\n"; return 0;
    } catch(const std::exception &e) { std::cerr<<"Failed after "<<checks<<" checks: "<<e.what()<<"\n"; return 1; }
}
