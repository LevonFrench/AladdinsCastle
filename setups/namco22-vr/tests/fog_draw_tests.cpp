// SPDX-License-Identifier: GPL-3.0-only
// Analytic ray/triangle expectations, independent of raster clipping/weights.
#include "n22_scene.hpp"
#include "n22_texture_plan.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool ok,const char *name) {++checks;if(!ok) throw std::runtime_error(name);}
n22::SceneInput fixture() {
    n22::SceneInput in;in.cameras={n22::synthetic_camera()};
    in.materials.addressing=n22::TileAddressing::Fixed16;
    in.materials.materials.emplace_back();in.materials.palette.resize(32768,0xc81164);
    in.materials.cells.push_back({0,0,0});n22::TextureTile tile;tile.pens.fill(1);in.materials.tiles.push_back(tile);
    in.fog.policy=n22::FogPolicy::Super22Table;in.fog.tick=1;in.fog.attributes[4]=4;
    in.fog.rgb={10,80,30};in.fog.tables[0].fill(128);
    n22::Polygon p{};p.material=0;p.vertices={{{1920,5760,2},{8320,5760,2},{5120,1920,2}}};
    p.fog.provided=p.fog.has_native_depth=true;p.fog.tick=1;p.fog.native_depth={0,256,512};
    for(auto &a:p.attributes) a.brightness=128;
    in.polygons.push_back(p);return in;
}
uint32_t render_centre(n22::SceneInput in,n22::Frame *owned=nullptr) {
    n22::Frame f;check(n22::prepare(in,1,f)==ACVR_OK,"owned synthetic fog frame");
    const auto eye=n22::desktop_eye(0,0,4,64,64);n22::Image image(64,64);
    check(n22::draw_cpu(f,eye,image)==ACVR_OK,"CPU fog reference draws");
    if(owned) *owned=f;
    const size_t centre=31*64+32;check(image.depth[centre]<1,"centre covered");return image.rgb[centre];
}
void endpoints_rounding_depth() {
    auto in=fixture();in.fog.tables[0].fill(0);check(render_centre(in)==0xff22c8,"unfogged endpoint uses shaded material");
    in.fog.tables[0].fill(255);check(render_centre(in)==0x0a501e,"full fog uses unshaded RGB");
    in.fog.tables[0].fill(128);check(render_centre(in)==0x843972,"partial fog clamps high shade BEFORE fog");
    for(auto &a:in.polygons[0].attributes) a.brightness=127.5f;
    std::fill(in.materials.palette.begin(),in.materials.palette.end(),0x010101);in.fog.rgb={10,10,10};
    check(render_centre(in)==0x060606,"float shade 1.9921875 survives until final floor; integer sampler would produce 5");
    auto nofog=in;nofog.fog={};nofog.polygons[0].fog={};n22::Frame a,b;
    n22::prepare(in,1,a);n22::prepare(nofog,1,b);n22::Image before(64,64),after(64,64);auto eye=n22::desktop_eye(0,0,4,64,64);
    check(n22::draw_cpu(a,eye,after)==ACVR_OK && n22::draw_cpu(b,eye,before)==ACVR_OK && before.depth==after.depth && before.rgb!=after.rgb,"fog changes RGB and preserves every written depth");
    acvr_ray ray{};ACVR_INIT(&ray);ray.direction_scene[2]=-1;ray.max_distance_scene=100;
    acvr_hit h1{},h2{};ACVR_INIT(&h1);ACVR_INIT(&h2);
    check(n22::raycast(a,ray,h1)==ACVR_OK && n22::raycast(b,ray,h2)==ACVR_OK && h1.found && h1.distance_scene==h2.distance_scene && h1.screen_x==h2.screen_x,"fog does not alter ray/gun geometry");
    // Submit front then rear; fog must not let the later rear draw cover it.
    auto rear=in.polygons[0];for(auto &v:rear.vertices) v.depth=4;rear.fog.colour_word=0x8000;
    in.polygons.push_back(rear);check(render_centre(in)==0x060606,"depth-separated later rear polygon remains occluded");
    std::reverse(in.polygons.begin(),in.polygons.end());check(render_centre(in)==0x060606,"front wins after earlier rear regardless of synthetic submission order");
}
struct D3 {double x,y,z;};
D3 sub(D3 a,D3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
D3 cross(D3 a,D3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(D3 a,D3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
D3 convert(n22::Vec3 a) {return {a.x,a.y,a.z};}
bool analytic(const n22::Triangle &t,const acvr_eye &e,int x,int y,std::array<double,3> &weights,double &z) {
    const double nx=2*(x+.5)/e.rect_width-1,ny=2*(y+.5)/e.rect_height-1;
    D3 origin{-e.view_from_scene[12],0,0},direction{(nx+e.projection_from_view[8])/e.projection_from_view[0],(ny+e.projection_from_view[9])/e.projection_from_view[5],-1};
    const auto v=convert(t.vertices[0]),e1=sub(convert(t.vertices[1]),v),e2=sub(convert(t.vertices[2]),v);
    const auto p=cross(direction,e2);const double determinant=dot(e1,p);if(std::abs(determinant)<1e-12) return false;
    const auto s=sub(origin,v),q=cross(s,e1);const double u=dot(s,p)/determinant,w=dot(direction,q)/determinant,d=dot(e2,q)/determinant;
    // Keep expectations away from raster edges and the actual near boundary.
    if(u<.03 || w<.03 || u+w>.97 || d<.101 || d>99) return false;
    weights={1-u-w,u,w};z=d;return true;
}
void interpolation_clip_replay(bool active_fade) {
    auto in=fixture();in.fog.tables[0][0]=0;in.fog.tables[0][1]=128;in.fog.tables[0][2]=255;
    if(active_fade) {
        in.polygon_fade.policy=n22::PolygonFadePolicy::Super22InputFold;in.polygon_fade.tick=1;in.polygon_fade.rgb={192,128,255};
    }
    const std::array<double,3> fade=active_fade?std::array<double,3>{.75,.5,255./256}:std::array<double,3>{1,1,1};
    // Same native raster at very different reconstructed depths; the first
    // vertex is outside the eye near plane but inside the source packet.
    const std::array<n22::Vec3,3> positions{{{-.015f,-.012f,-.05f},{.65f,-.45f,-2},{-.2f,.5f,-1}}};
    const auto &camera=in.cameras[0];
    for(size_t k=0;k<3;++k) {
        const auto p=positions[k];const float depth=-p.z;
        in.polygons[0].vertices[k]={(camera.centre_x_px+p.x*camera.focal_x_px/depth)*16,(camera.centre_y_px-p.y*camera.focal_y_px/depth)*16,depth};
        in.polygons[0].attributes[k].brightness=static_cast<float>(64+k*32);
    }
    n22::Frame f;check(n22::prepare(in,1,f)==ACVR_OK,"near-clipped analytic packet");
    check(f.triangles[0].fog_samples.alpha==std::array<uint8_t,3>{255,127,0},"once-per-lease weights use native depths, not scene Z");
    const auto frozen=f.triangles[0].fog_samples.alpha;
    in.fog.tables[0].fill(0);in.polygons[0].fog.native_depth.fill(0); // mutate producer only
    in.polygon_fade.rgb={0,0,0}; // owned factors must also survive producer mutation
    size_t compared=0;bool witnessed_perspective=false;std::vector<uint32_t> first;
    for(float eye_x:{0.f,.025f,-.025f}) {
        auto e=n22::desktop_eye(0,eye_x,4,64,64);n22::Image image(64,64);
        check(n22::draw_cpu(f,e,image)==ACVR_OK,"new-eye clipped draw uses immutable fog packet");
        if(eye_x==0) first=image.rgb;else check(image.rgb!=first,"moving eyes change coverage/interpolation");
        for(int y=8;y<56;y+=8) for(int x=8;x<56;x+=8) {
            std::array<double,3> w{};double distance=0;if(!analytic(f.triangles[0],e,x,y,w,distance)) continue;
            const auto &t=f.triangles[0];double alpha=0,brightness=0;
            for(size_t k=0;k<3;++k) {alpha+=w[k]*frozen[k]/255.;brightness+=w[k]*t.attributes[k].brightness;}
            const auto rgb=image.rgb[static_cast<size_t>(63-y)*64+static_cast<size_t>(x)];
            for(size_t channel=0;channel<3;++channel) {
                const unsigned shift=static_cast<unsigned>((2-channel)*8);
                const double texel=(0xc81164>>shift)&255;
                const int expected=static_cast<int>(std::clamp(texel*brightness/64.*fade[channel],0.,255.)*alpha+f.fog.rgb[channel]*fade[channel]*(1-alpha));
                check(std::abs(int((rgb>>shift)&255)-expected)<=1,"ray/triangle analytic fog agrees through eye clipping within one byte");
            }
            const double expected_depth=(-e.projection_from_view[10]+e.projection_from_view[14]/distance+1)*.5;
            check(std::abs(image.depth[static_cast<size_t>(63-y)*64+static_cast<size_t>(x)]-expected_depth)<2e-6,"analytic shared depth independent of fog");
            // Surface barycentrics differ substantially from screen barycentrics.
            double sum=0;for(size_t k=0;k<3;++k) sum+=w[k]*-t.vertices[k].z;
            double screen_alpha=0;for(size_t k=0;k<3;++k) screen_alpha+=w[k]*-t.vertices[k].z/sum*frozen[k]/255.;
            witnessed_perspective=witnessed_perspective || std::abs(screen_alpha-alpha)>.1;++compared;
        }
        check(f.triangles[0].fog_samples.alpha==frozen,"pose replay never mutates/redecodes native fog weights");
    }
    check(compared>=12 && witnessed_perspective,"bounded analytic coverage distinguishes screen-linear from perspective interpolation");
}
void admission_budgets() {
    auto in=fixture();n22::Frame f;n22::prepare(in,1,f);auto eye=n22::desktop_eye(0,0,4,64,64);
    for(auto layer:{n22::Layer::Hud,n22::Layer::Backdrop,n22::Layer::GunFlash}) {
        auto bad=f;bad.triangles[0].layer=layer;n22::Image image(64,64);image.rgb[0]=123;image.depth[0]=.4f;
        check(n22::draw_cpu(bad,eye,image)==ACVR_UNSUPPORTED && image.rgb[0]==123 && image.depth[0]==.4f,"unsupported enabled fog layer rejects before CPU clear");
    }
    auto bad=f;bad.triangles[0].material=n22::NoMaterial;n22::TexturePlan plan;plan.bytes=123;
    check(n22::prepare_texture_plan(bad,plan)==ACVR_UNSUPPORTED && plan.bytes==123,"flat fog rejects before pixels and preserves output plan");
    bad=f;bad.materials.materials[0].objectflags=1;check(n22::validate_fog_draw(bad)==ACVR_UNSUPPORTED,"solid fog not silently admitted");
    in.fog.policy=n22::FogPolicy::System22Constant;in.polygons[0].fog.constant_provided=true;in.polygons[0].fog.constant_alpha=128;
    n22::prepare(in,1,bad);check(n22::validate_fog_draw(bad)==ACVR_UNSUPPORTED,"constant fog remains state-only");
    bad=f;bad.triangles[0].fog.tick=2;check(n22::validate_fog_draw(bad)==ACVR_BAD_ARGUMENT,"mismatched fog binding fails before draws");
    check(n22::prepare_texture_plan(f,plan)==ACVR_OK && plan.rectangles.size()==2 && plan.bytes==8 && plan.fog_texture==1 && plan.rectangles[1].rgba==std::vector<uint8_t>{255,255,255,255},"dummy counted in existing count/byte budget");
    bad=f;bad.triangles.resize(n22::MaxLeaseTextures,f.triangles[0]);
    for(size_t i=0;i<bad.triangles.size();++i) for(auto &a:bad.triangles[i].attributes) a.u=static_cast<float>(i);
    const auto old_bytes=plan.bytes;
    check(n22::prepare_texture_plan(bad,plan)==ACVR_UNSUPPORTED && plan.bytes==old_bytes,"256 material rectangles plus dummy exceed count before sparse sampling");
    bad=f;bad.materials.materials.resize(8,bad.materials.materials[0]);bad.triangles.resize(8,f.triangles[0]);
    for(size_t i=0;i<8;++i) {bad.triangles[i].material=static_cast<uint32_t>(i);bad.triangles[i].attributes={{{0,0,64},{1023,0,64},{0,1023,64}}};}
    check(n22::prepare_texture_plan(bad,plan)==ACVR_UNSUPPORTED && plan.bytes==old_bytes,"32MiB material extents plus dummy exceed byte budget before allocating pixels");
}
}
int main() {
    try {endpoints_rounding_depth();interpolation_clip_replay(false);interpolation_clip_replay(true);admission_budgets();std::cout<<checks<<" synthetic analytic fog draw checks passed (CPU only)\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
