// SPDX-License-Identifier: GPL-3.0-only
#include "n22_cpu_backend.hpp"
#include "n22_worker.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool ok,const char *name) {++checks;if(!ok) throw std::runtime_error(name);}
template<class T> T record() {T value{};ACVR_INIT(&value);return value;}
n22::VideoSnapshot snapshot(uint64_t tick=1) {
    n22::VideoSnapshot v;v.tick=tick;v.banks[n22::Mixer].resize(n22::bank_sizes[n22::Mixer],200);
    v.banks[n22::Mixer][8]=17;v.banks[n22::Mixer][9]=34;v.banks[n22::Mixer][10]=51;return v;
}
n22::SceneInput scene() {
    n22::SceneInput in;in.cameras={n22::synthetic_camera()};const auto source=snapshot();
    check(n22::copy_super22_background(source,in.background)==ACVR_OK,"explicit copied mixer background");return in;
}
void decoder_binding() {
    auto source=snapshot();n22::BackgroundState owned;
    check(n22::copy_super22_background(source,owned)==ACVR_OK && owned.policy==n22::BackgroundPolicy::Super22Mixer && owned.tick==1 && owned.rgb==std::array<uint8_t,3>{17,34,51},"only mixer bytes08..0a become owned RGB");
    source.banks[n22::Mixer].assign(n22::bank_sizes[n22::Mixer],0);
    check(n22::background_rgb(owned)==0x112233,"snapshot mutation cannot alter copied background");
    auto bad=snapshot();bad.tick=0;check(n22::copy_super22_background(bad,owned)==ACVR_BAD_ARGUMENT && n22::background_rgb(owned)==0x112233,"zero snapshot tick rejects atomically");
    for(size_t size:{size_t{0},size_t{10},n22::bank_sizes[n22::Mixer]-1,n22::bank_sizes[n22::Mixer]+1}) {
        bad=snapshot();bad.banks[n22::Mixer].resize(size);
        check(n22::copy_super22_background(bad,owned)==ACVR_BAD_ARGUMENT && owned.tick==1 && n22::background_rgb(owned)==0x112233,"invalid exact mixer length cannot replace output");
    }
    check(n22::validate_background(owned,0)==ACVR_BAD_ARGUMENT && n22::validate_background(owned,2)==ACVR_BAD_ARGUMENT,"present background needs matching nonzero frame tick");
    auto absent=owned;absent.policy=n22::BackgroundPolicy::Absent;
    check(n22::validate_background(absent,0)==ACVR_OK && n22::background_rgb(absent)==0,"explicit absent policy preserves black even with unused bytes");
    auto in=scene();n22::Frame f;check(n22::prepare(in,1,f)==ACVR_OK,"background-only empty frame publication");
    in.background.tick=2;check(n22::prepare(in,1,f)==ACVR_BAD_ARGUMENT && f.id==1 && n22::background_rgb(f.background)==0x112233,"mismatched input tick leaves prior frame unchanged");
    in.background.policy=static_cast<n22::BackgroundPolicy>(42);
    check(n22::prepare(in,1,f)==ACVR_UNSUPPORTED && f.id==1,"unknown background policy never silently paints");
}
bool in_eye(uint32_t x,uint32_t y,const acvr_eye &e) {
    return x>=static_cast<uint32_t>(e.rect_x) && x<static_cast<uint32_t>(e.rect_x)+e.rect_width &&
           y>=static_cast<uint32_t>(e.rect_y) && y<static_cast<uint32_t>(e.rect_y)+e.rect_height;
}
void rectangles_replay() {
    auto in=scene();n22::Frame f;n22::prepare(in,1,f);in.background.rgb={0,0,0};
    n22::Image image(128,80);std::fill(image.rgb.begin(),image.rgb.end(),0xabcdef);std::fill(image.depth.begin(),image.depth.end(),.25f);
    auto left=n22::desktop_eye(0,-.032f,4,48,32),right=n22::desktop_eye(1,.032f,4,48,32);
    left.rect_x=12;right.rect_x=70;left.rect_y=right.rect_y=8;
    check(n22::draw_cpu(f,left,image)==ACVR_OK && n22::draw_cpu(f,right,image)==ACVR_OK,"both nonzero eye rectangles use same leased background");
    bool exact=true;for(uint32_t y=0;y<80;++y) for(uint32_t x=0;x<128;++x) {
        const bool inside=in_eye(x,y,left)||in_eye(x,y,right);const size_t p=size_t(79-y)*128+x;
        exact=exact && image.rgb[p]==(inside?0x112233u:0xabcdefu) && image.depth[p]==(inside?1.f:.25f);
    }
    check(exact,"all uncovered eye pixels and depth are exact; neighbours untouched");
    const auto pixels=image.rgb;const auto depth=image.depth;left.view_from_scene[12]=-.3f;
    check(n22::draw_cpu(f,left,image)==ACVR_OK && image.rgb==pixels && image.depth==depth,"moved-pose replay uses immutable clear without touching other eye");
    check(n22::draw_cpu(f,left,image,true)==ACVR_OK,"standalone HUD-only CPU path remains available");
    exact=true;for(uint32_t y=8;y<40;++y) for(uint32_t x=12;x<60;++x) exact=exact && image.rgb[size_t(79-y)*128+x]==0;
    check(exact,"HUD-only image still clears black rather than mixer background");
    auto plain=scene();plain.background={};n22::Frame absent;n22::prepare(plain,1,absent);
    check(n22::draw_cpu(absent,left,image)==ACVR_OK && image.rgb[size_t(79-8)*128+12]==0,"absent background clears black");
    for(bool unknown:{false,true}) {
        auto bad=f;if(unknown) bad.background.policy=static_cast<n22::BackgroundPolicy>(99);else bad.background.tick=2;
        const auto prior=image.rgb;const auto prior_depth=image.depth;
        check(n22::draw_cpu(bad,left,image)==(unknown?ACVR_UNSUPPORTED:ACVR_BAD_ARGUMENT) && image.rgb==prior && image.depth==prior_depth,"invalid prepared background rejects before any CPU clear");
    }
}
void geometry_depth() {
    auto in=scene();n22::Polygon p{};p.rgb=0xfedcba;
    p.vertices={{{240*16,320*16,2},{400*16,320*16,2},{320*16,160*16,2}}};in.polygons.push_back(p);
    auto plain=in;plain.background={};n22::Frame colour,black;n22::prepare(in,1,colour);n22::prepare(plain,1,black);
    auto eye=n22::desktop_eye(0,0,4,64,64);n22::Image a(64,64),b(64,64);
    check(n22::draw_cpu(colour,eye,a)==ACVR_OK && n22::draw_cpu(black,eye,b)==ACVR_OK,"same polygon over coloured/black clear");
    bool correct=true;size_t covered=0,uncovered=0;
    for(size_t i=0;i<a.rgb.size();++i) {
        if(a.depth[i]<1) {correct=correct && a.rgb[i]==0xfedcba && b.rgb[i]==a.rgb[i];++covered;}
        else {correct=correct && a.rgb[i]==0x112233 && b.rgb[i]==0;++uncovered;}
    }
    check(correct && covered>0 && uncovered>0 && a.depth==b.depth,"geometry replaces clear colour while every depth remains unchanged");
    auto ray=record<acvr_ray>();ray.direction_scene[2]=-1;ray.max_distance_scene=100;
    auto h1=record<acvr_hit>(),h2=record<acvr_hit>();
    check(n22::raycast(colour,ray,h1)==ACVR_OK && n22::raycast(black,ray,h2)==ACVR_OK && h1.found && h1.distance_scene==h2.distance_scene && h1.screen_x==h2.screen_x,"background never changes gun/raycast geometry");
}
void callback_lease() {
    auto api=record<acvr_backend_api>();acvr_backend_query(1,&api);check(api.supported_graphics==0,"no background factory/graphics admission");
    auto open=record<acvr_open_info>();ACVR_INIT(&open.graphics);open.game_id_utf8="synthetic-system22";
    auto meta=record<acvr_backend_info>();acvr_backend *b=nullptr;api.game_open(&open,&b,&meta);
    auto in=scene();check(n22::stage_cpu_scene(b,in)==ACVR_OK,"private backend stages owned same-tick background");in.background.rgb={0,0,0};
    auto input=record<acvr_inputs>();input.tick_id=1;api.game_set_inputs(b,&input);
    auto step=record<acvr_step_info>();step.tick_id=1;auto fi=record<acvr_frame_info>();acvr_frame *f=nullptr;
    check(api.game_step(b,&step,&f,&fi)==ACVR_OK,"usual background-only lease");
    auto eye=n22::desktop_eye(0,0,4,32,32);n22::Image image(32,32);
    check(n22::draw_cpu_frame(b,f,eye,image)==ACVR_OK && image.rgb.front()==0x112233,"callback replay reads the prepared background, not producer");
    check(n22::stage_cpu_scene(b,in)==ACVR_BAD_STATE,"active lease cannot be replaced");
    api.game_release_frame(b,f);check(n22::draw_cpu_frame(b,nullptr,eye,image)==ACVR_BAD_STATE,"released background cannot draw");
    check(n22::stage_cpu_scene(b,in)==ACVR_BAD_ARGUMENT,"next tick rejects an old mixer background before staging");
    in.background.tick=2;in.background.rgb={68,85,102};
    check(n22::stage_cpu_scene(b,in)==ACVR_OK,"new tick stages a distinct owned background");
    input.tick_id=2;api.game_set_inputs(b,&input);step.tick_id=2;step.simulation_time_ns=static_cast<int64_t>(1000000000000ULL/59906ULL);
    check(api.game_step(b,&step,&f,&fi)==ACVR_OK && n22::draw_cpu_frame(b,f,eye,image)==ACVR_OK && image.rgb.front()==0x445566,"new lease receives new RGB without retaining previous background");
    api.game_release_frame(b,f);api.game_close(b);
}
}
int main() {
    try {decoder_binding();rectangles_replay();geometry_depth();callback_lease();std::cout<<checks<<" owned background checks passed (synthetic CPU only)\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
