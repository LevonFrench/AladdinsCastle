// SPDX-License-Identifier: GPL-3.0-only
#include "n22_cpu_backend.hpp"
#include "n22_texture_plan.hpp"
#include "n22_worker.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool ok,const char *name) {++checks;if(!ok) throw std::runtime_error(name);}
n22::SceneInput fixture(bool fog=true) {
    n22::SceneInput in;in.cameras={n22::synthetic_camera()};
    in.materials.addressing=n22::TileAddressing::Fixed16;in.materials.materials.emplace_back();
    in.materials.palette.resize(32768,0xc82850);in.materials.cells.push_back({0,0,0});
    n22::TextureTile tile;tile.pens.fill(1);in.materials.tiles.push_back(tile);
    n22::Polygon p{};p.material=0;p.vertices={{{1920,5760,2},{8320,5760,2},{5120,1920,2}}};
    for(auto &a:p.attributes) a.brightness=128;
    if(fog) {
        in.fog.policy=n22::FogPolicy::Super22Table;in.fog.tick=1;in.fog.attributes[4]=4;
        in.fog.rgb={40,40,40};in.fog.tables[0].fill(128);
        p.fog.provided=p.fog.has_native_depth=true;p.fog.tick=1;p.fog.native_depth.fill(256);
    }
    in.polygons.push_back(p);in.polygon_fade.policy=n22::PolygonFadePolicy::Super22InputFold;
    in.polygon_fade.tick=1;in.polygon_fade.rgb={128,128,128};
    in.background.policy=n22::BackgroundPolicy::Super22Mixer;in.background.tick=1;in.background.rgb={17,34,51};return in;
}
uint32_t centre(const n22::SceneInput &in) {
    n22::Frame f;check(n22::prepare(in,1,f)==ACVR_OK,"owned polygon-fade fixture");
    n22::Image image(64,64);const auto e=n22::desktop_eye(0,0,4,64,64);
    check(n22::draw_cpu(f,e,image)==ACVR_OK && image.depth[31*64+32]<1,"covered CPU fade fragment");return image.rgb[31*64+32];
}
void source_gates_ownership() {
    n22::VideoSnapshot source;source.tick=1;source.banks[n22::Mixer].resize(1024,200);
    source.banks[n22::Mixer][0]=192;source.banks[n22::Mixer][1]=128;source.banks[n22::Mixer][2]=255;
    source.banks[n22::Mixer][0x1a]=0; // screen-fade flag does not gate polyfade
    n22::PolygonFadeState state;
    check(n22::copy_super22_polygon_fade(source,state)==ACVR_OK && state.tick==1 && state.rgb==std::array<uint8_t,3>{192,128,255},"only already copied mixer bytes0..2 feed explicit source policy");
    check(n22::polygon_fade_active(state) && n22::polygon_fade_factors(state)==std::array<double,3>{.75,.5,255./256},"global enable gives byte/256 to EVERY channel including255");
    auto in=fixture();in.polygon_fade=state;n22::Frame f;n22::prepare(in,1,f);
    in.polygon_fade.rgb={0,0,0};source.banks[n22::Mixer].assign(1024,0);
    check(f.polygon_fade.rgb==std::array<uint8_t,3>{192,128,255} && state.rgb==f.polygon_fade.rgb,"snapshot/input mutation cannot change owned frame fade");
    auto bad=source;bad.tick=0;check(n22::copy_super22_polygon_fade(bad,state)==ACVR_BAD_ARGUMENT && state.rgb==f.polygon_fade.rgb,"zero source tick cannot replace prior fade");
    for(size_t size:{size_t{0},size_t{2},size_t{1023},size_t{1025}}) {
        bad=source;bad.banks[n22::Mixer].resize(size);
        check(n22::copy_super22_polygon_fade(bad,state)==ACVR_BAD_ARGUMENT && state.rgb==f.polygon_fade.rgb,"bad exact mixer length rejects atomically");
    }
    check(n22::validate_polygon_fade(state,0)==ACVR_BAD_ARGUMENT && n22::validate_polygon_fade(state,2)==ACVR_BAD_ARGUMENT,"present fade requires matching nonzero lease tick");
    auto disabled=state;disabled.rgb={255,255,255};check(!n22::polygon_fade_active(disabled) && n22::polygon_fade_factors(disabled)==std::array<double,3>{1,1,1},"all255 sentinel bypasses normalization entirely");
    disabled=state;disabled.policy=n22::PolygonFadePolicy::Absent;
    check(n22::validate_polygon_fade(disabled,0)==ACVR_OK && n22::polygon_fade_factors(disabled)==std::array<double,3>{1,1,1},"absent policy is identity despite unused non255 bytes");
    in=fixture();in.polygon_fade.tick=2;
    check(n22::prepare(in,1,f)==ACVR_BAD_ARGUMENT && f.polygon_fade.rgb==state.rgb,"bad input tick preserves previous published fade frame");
    in.polygon_fade.policy=static_cast<n22::PolygonFadePolicy>(99);check(n22::prepare(in,1,f)==ACVR_UNSUPPORTED,"unknown input order cannot become a guessed fade");
}
void rational_witnesses() {
    auto in=fixture();
    // Independent integer rational values: 27960/255 and 36225/255.
    check((centre(in)>>16)==109 && (200*127+20*128)/255==109,"D128 source fold witness109, not after-clamp73 or shade-only119");
    in.polygon_fade.rgb={192,192,192};
    check((centre(in)>>16)==142 && (255*127+30*128)/255==142,"D192 source fold witness142, not after-clamp110 or final-only164");
    in=fixture(false);std::fill(in.materials.palette.begin(),in.materials.palette.end(),0x404040);
    for(auto &a:in.polygons[0].attributes) a.brightness=64;
    in.polygon_fade.rgb={255,255,255};check(centre(in)==0x404040,"UNSATURATED all255 identity64 cannot pass accidental255/256 darkening63");
    in.polygon_fade.rgb={255,128,255};check(centre(in)==0x3f203f,"enabled mixed channels include255/256, not per-channel sentinel");
    in=fixture();std::fill(in.materials.palette.begin(),in.materials.palette.end(),0x010101);
    for(auto &a:in.polygons[0].attributes) a.brightness=127.5f;
    in.fog.rgb={10,10,10};check(centre(in)==0x030303 && 39245/13056==3,"final-only floor preserves rational3; premature shade byte gives2");
    in=fixture();in.fog.tables[0].fill(255);in.fog.rgb={80,80,80};check(centre(in)==0x282828,"full fog scales raw constant by fade without prequantization");
    in=fixture();in.polygons[0].fog.colour_word=0x8000;check((centre(in)>>16)==200,"colour bit15 disables fog, never polygon fade");
    in=fixture();in.polygons[0].fog.cz_adjust=0x800000;check((centre(in)>>16)==200,"adjust bit23 disables fog, never polygon fade");
    in=fixture(false);check((centre(in)>>16)==200,"active fade works with explicitly absent fog");
    in.polygon_fade={};check((centre(in)>>16)==255,"absent fade preserves prior saturated shade");
}
void depth_background_admission() {
    auto in=fixture(false);in.polygon_fade.rgb={0,0,0};n22::Frame f;n22::prepare(in,1,f);
    auto plain=in;plain.polygon_fade={};n22::Frame unfaded;n22::prepare(plain,1,unfaded);
    auto eye=n22::desktop_eye(0,0,4,64,64);n22::Image a(64,64),b(64,64);n22::draw_cpu(f,eye,a);n22::draw_cpu(unfaded,eye,b);
    bool exact=true;size_t covered=0;for(size_t i=0;i<a.rgb.size();++i) {
        if(a.depth[i]<1) {exact=exact && a.rgb[i]==0;++covered;}else exact=exact && a.rgb[i]==0x112233 && b.rgb[i]==a.rgb[i];
    }
    check(exact && covered>0 && a.depth==b.depth,"zero polygon RGB still writes same depth and never fades uncovered background");
    acvr_ray ray{};ACVR_INIT(&ray);ray.direction_scene[2]=-1;ray.max_distance_scene=100;acvr_hit h1{},h2{};ACVR_INIT(&h1);ACVR_INIT(&h2);
    check(n22::raycast(f,ray,h1)==ACVR_OK && n22::raycast(unfaded,ray,h2)==ACVR_OK && h1.found && h1.distance_scene==h2.distance_scene && h1.screen_x==h2.screen_x,"fade does not change gun/raycast geometry");
    for(auto layer:{n22::Layer::Hud,n22::Layer::Backdrop,n22::Layer::GunFlash}) {
        auto bad=f;bad.triangles[0].layer=layer;const auto before=a.rgb;
        check(n22::draw_cpu(bad,eye,a)==ACVR_UNSUPPORTED && a.rgb==before && a.depth==b.depth,"active unsupported layer rejects before clear without silently skipping fade");
        bad.polygon_fade.rgb={255,255,255};check(n22::validate_polygon_fade_draw(bad)==ACVR_OK,"inactive all255 policy retains prior layer admission");
    }
    n22::TexturePlan plan;plan.bytes=17;auto bad=f;bad.triangles[0].material=n22::NoMaterial;
    check(n22::prepare_texture_plan(bad,plan)==ACVR_UNSUPPORTED && plan.bytes==17,"active flat fade rejects before texture bytes/target clear");
    bad=f;bad.materials.materials[0].objectflags=6;check(n22::validate_polygon_fade_draw(bad)==ACVR_UNSUPPORTED,"active solid fade unsupported even with no fog");
    bad=f;bad.fog.policy=n22::FogPolicy::System22Constant;check(n22::validate_polygon_fade_draw(bad)==ACVR_UNSUPPORTED,"Super22 fold never silently processes System22 constant policy");
    bad=f;bad.polygon_fade.tick=2;const auto before=a.rgb;
    check(n22::draw_cpu(bad,eye,a)==ACVR_BAD_ARGUMENT && a.rgb==before,"stale prepared fade rejects before draw clear");
    bad=f;bad.polygon_fade.policy=static_cast<n22::PolygonFadePolicy>(99);
    check(n22::draw_cpu(bad,eye,a)==ACVR_UNSUPPORTED && a.rgb==before,"unknown prepared order rejected before clear");
    in.polygons.clear();n22::prepare(in,1,bad);check(n22::draw_cpu(bad,eye,a)==ACVR_OK && a.rgb.front()==0x112233,"active fade with empty world leaves existing background unchanged");
}
}
int main() {
    try {source_gates_ownership();rational_witnesses();depth_background_admission();std::cout<<checks<<" polygon fade input-fold checks passed (synthetic CPU only)\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
