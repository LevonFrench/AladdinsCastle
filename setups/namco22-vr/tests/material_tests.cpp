// SPDX-License-Identifier: GPL-3.0-only
#include "n22_cpu_backend.hpp"
#include "n22_texture_plan.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks=0;
void check(bool b,const char *what) {++checks;if(!b) throw std::runtime_error(what);}
n22::MaterialPacket packet() {
    n22::MaterialPacket p;p.addressing=n22::TileAddressing::Fixed16;
    p.palette.resize(0x8000);for(uint32_t i=0;i<256;++i) p.palette[i]=i;
    p.materials.emplace_back();p.cells.push_back({0,0,0});p.tiles.emplace_back();
    for(size_t i=0;i<256;++i) p.tiles[0].pens[i]=static_cast<uint8_t>(i);
    return p;
}
uint32_t sample(const n22::MaterialPacket &p,double u=2.5,double v=5.5,double brightness=64) {
    uint32_t rgb=0;check(n22::sample_material(p,0,u,v,brightness,rgb)==ACVR_OK,"owned packet sample");return rgb;
}
void address_and_palette() {
    auto even=n22::decode_texture_cell(0,0x34,0x12,0xa5),odd=n22::decode_texture_cell(1,0x78,0x56,0xa5);
    check(even.tile==0x1234 && even.attribute==10 && odd.tile==0x5678 && odd.attribute==5,"little-endian tile and high-first attribute nibble");
    std::vector<uint8_t> planar(0x18000);planar[0x312]=0x12;planar[0x8312]=0x34;planar[0x10312]=0x56;
    std::vector<uint32_t> palette;
    check(n22::decode_planar_palette(planar,palette)==ACVR_OK && palette[0x312]==0x123456,"three planar colour banks");
    planar.pop_back();check(n22::decode_planar_palette(planar,palette)==ACVR_BAD_ARGUMENT && palette[0x312]==0x123456,"truncated palette does not mutate output");
    auto p=packet();check(n22::validate_material_packet(p)==ACVR_OK,"bounded synthetic native packet");
    constexpr uint32_t transformed[16]={82,82,162,162,93,93,173,173,37,37,42,42,213,213,218,218};
    for(uint8_t attr=0;attr<16;++attr) {p.cells[0].attribute=attr;check(sample(p)==transformed[attr],"native flip-before-transpose addressing");}
    p.addressing=n22::TileAddressing::Extended17;p.cells[0].attribute=1;p.palette[0]=0x123456;
    check(sample(p)==0x123456,"extended tile beyond pin's 16 MiB limit resolves opaque pen0");
    p.addressing=n22::TileAddressing::Fixed16;p.cells[0].attribute=0;
    check(sample(p,4098.5,4101.5)==82 && sample(p,-4093.5,-4090.5)==82,"12-bit UV wrap");
    p.cells.push_back({0x10000,1,0});n22::TextureTile bank;bank.index=1;bank.pens.fill(17);p.tiles.push_back(bank);p.materials[0].texbank=1;
    check(sample(p)==17,"V bank selects distinct map region");
    p.materials[0].colour_word=3<<8;p.palette[3*256+17]=0xaabbcc;
    check(sample(p)==0xaabbcc,"raw colour chooses palette group rather than RGB");
}
void modes_and_shade() {
    auto p=packet();p.tiles[0].pens.fill(0xb6);
    constexpr uint32_t pens[16]={182,182,235,230,238,239,237,238,182,182,251,246,254,255,253,254};
    for(uint8_t cmode=0;cmode<16;++cmode) {p.materials[0].cmode=cmode;check(sample(p)==pens[cmode],"cmode 8/4/2-bit pen decode");}
    p.materials[0].cmode=0;p.palette[182]=0x502814;
    check(sample(p)==0x502814 && sample(p,2.5,5.5,128)==0xa05028,"neutral64 and over-bright shade");
    p.palette[182]=0xc08040;check(sample(p,2.5,5.5,255)==0xffffff,"shade saturates channels after multiplying");
    p.materials[0].objectflags=1;p.materials[0].colour_word=3<<8;p.materials[0].cz_adjust=0x120000;p.palette[3*256+18]=0x804020;
    check(sample(p,0,0,32)==0x402010,"objectflags solid selects native pen and remains shaded");
    p.materials[0].objectflags=6;p.materials[0].cz_adjust=0x1234;p.palette[0x1234]=0x123456;
    check(sample(p,0,0,1)==0x123456,"solid flags select cz_adjust pen without shading");
}
void malformed() {
    auto p=packet();auto bad=p;bad.addressing=n22::TileAddressing::Unknown;
    check(n22::validate_material_packet(bad)==ACVR_UNSUPPORTED,"board addressing must be explicit");
    bad=p;bad.cells.push_back(bad.cells[0]);check(n22::validate_material_packet(bad)==ACVR_BAD_ARGUMENT,"duplicate cell rejected");
    bad=p;bad.tiles.push_back(bad.tiles[0]);check(n22::validate_material_packet(bad)==ACVR_BAD_ARGUMENT,"duplicate tile rejected");
    bad=p;bad.cells[0].index=0x100000;check(n22::validate_material_packet(bad)==ACVR_BAD_ARGUMENT,"map index bounded");
    bad=p;bad.tiles[0].index=0x10000;check(n22::validate_material_packet(bad)==ACVR_BAD_ARGUMENT,"tile capacity bounded to source pin");
    bad=p;bad.materials[0].cmode=16;check(n22::validate_material_packet(bad)==ACVR_BAD_ARGUMENT,"cmode nibble bound");
    bad=p;bad.palette.pop_back();check(n22::validate_material_packet(bad)==ACVR_BAD_ARGUMENT,"incomplete palette rejected");
    bad=p;bad.tiles.resize(4097);check(n22::validate_material_packet(bad)==ACVR_BAD_ARGUMENT,"packet memory bound");
    uint32_t rgb=0;
    p.cells.clear();check(n22::sample_material(p,0,0,0,64,rgb)==ACVR_BAD_ARGUMENT,"missing map cell has no fabricated fallback");
    p=packet();p.tiles.clear();check(n22::sample_material(p,0,0,0,64,rgb)==ACVR_BAD_ARGUMENT,"missing referenced tile fails");
    p=packet();check(n22::sample_material(p,0,std::numeric_limits<double>::infinity(),0,64,rgb)==ACVR_BAD_ARGUMENT,"nonfinite UV rejected");
    check(!n22::valid_material_vertex({0,0,-1}),"negative shade rejected");
}
n22::SceneInput scene() {
    n22::SceneInput in;in.cameras.push_back(n22::synthetic_camera());in.materials=packet();
    for(uint32_t i=0;i<256;++i) in.materials.palette[i]=i<<16;
    n22::Polygon p;p.material=0;
    // With desktop_eye's focal2: NDC (-.8,-.8),(.8,-.8),(0,.8),
    // but different depths. Perspective sample must differ from affine UV.
    p.vertices={n22::ProjectedVertex{128*16,432*16,1},{512*16,432*16,4},{320*16,48*16,2}};
    p.attributes={n22::MaterialVertex{.5f,.5f,64},{15.5f,.5f,64},{.5f,.5f,64}};
    in.polygons.push_back(p);return in;
}
void per_eye_and_ownership() {
    auto in=scene();n22::Frame frame;check(n22::prepare(in,1,frame)==ACVR_OK,"prepare copies material packet");
    auto eye=n22::desktop_eye(0,0,4,64,64);n22::Image image(128,64);
    check(n22::draw_cpu(frame,eye,image)==ACVR_OK,"per-eye textured CPU scene");
    const size_t probe=32*image.width+32;
    // Barycentric weights at (32.5,31.5) in bottom-left pixels are
    // (0.2451171875,0.2646484375,0.490234375). Inverse depths 1,.25,.5
    // give U=2.283... (pen2). Affine U=4.469... would incorrectly pick4.
    check(image.rgb[probe]==0x020000,"new eye perspective-correct UV sample differs from affine");
    const auto original=image.rgb;in.materials.palette.assign(0x8000,0xffffff);in.materials.tiles[0].pens.fill(0);
    check(n22::draw_cpu(frame,eye,image)==ACVR_OK && image.rgb==original,"lease owns tile and same-tick palette after source mutation");
    n22::Frame next;check(n22::prepare(in,2,next)==ACVR_OK && n22::draw_cpu(next,eye,image)==ACVR_OK && image.rgb[probe]==0xffffff,"next frame receives changed palette without altering old lease");
    auto right=n22::desktop_eye(1,.12f,4,64,64);
    check(n22::draw_cpu(frame,right,image)==ACVR_OK,"second eye uses same owned material packet");
    size_t different=0;for(size_t y=0;y<64;++y) for(size_t x=0;x<64;++x) if(original[y*128+x]!=image.rgb[y*128+x+64]) ++different;
    check(different>50,"stereo pose changes textured projection");
    auto bad=scene();bad.polygons[0].material=1;
    check(n22::prepare(bad,3,next)==ACVR_BAD_ARGUMENT && next.id==2,"bad material binding does not replace previous frame");
    bad=scene();bad.polygons[0].attributes[0].u=std::numeric_limits<float>::quiet_NaN();
    check(n22::prepare(bad,3,next)==ACVR_BAD_ARGUMENT,"invalid vertex material data rejected before publication");
    // Pen zero and black still occlude a rear surface; no texture transparency.
    auto black=scene();black.materials.tiles[0].pens.fill(0);black.materials.palette[0]=0;
    auto rear=black.polygons[0];rear.material=n22::NoMaterial;rear.rgb=0xff0000;
    for(auto &v:rear.vertices) v.depth*=2;
    black.polygons.push_back(rear);
    check(n22::prepare(black,4,next)==ACVR_OK && n22::draw_cpu(next,eye,image)==ACVR_OK && image.rgb[probe]==0 && image.depth[probe]<1,"opaque black pen0 retains depth and occludes rear polygon");
}
void clipped_attributes() {
    auto in=scene();in.polygons[0].vertices[0].depth=.05f;
    n22::Frame frame;check(n22::prepare(in,1,frame)==ACVR_OK,"near-clipped textured input");
    auto eye=n22::desktop_eye(0,0,4,64,64);n22::Image clipped(128,64),manual(128,64);
    check(n22::draw_cpu(frame,eye,clipped)==ACVR_OK,"clip interpolates owned UV attributes");
    const auto triangle=frame.triangles[0];const auto a=triangle.vertices[0];
    auto intersect=[&](size_t k) {
        const auto b=triangle.vertices[k];const float t=(-.1f-a.z)/(b.z-a.z);
        return std::pair<n22::Vec3,n22::MaterialVertex>{{a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),-.1f},
            {triangle.attributes[0].u+t*(triangle.attributes[k].u-triangle.attributes[0].u),.5f,64}};
    };
    const auto edge2=intersect(2),edge1=intersect(1);
    auto first=triangle,second=triangle;
    first.vertices={edge2.first,edge1.first,triangle.vertices[1]};first.attributes={edge2.second,edge1.second,triangle.attributes[1]};
    second.vertices={edge2.first,triangle.vertices[1],triangle.vertices[2]};second.attributes={edge2.second,triangle.attributes[1],triangle.attributes[2]};
    frame.triangles={first,second};
    check(n22::draw_cpu(frame,eye,manual)==ACVR_OK,"independently pre-clipped textured fan");
    size_t mismatch=0,coloured=0;for(size_t i=0;i<clipped.rgb.size();++i) {mismatch+=clipped.rgb[i]!=manual.rgb[i];coloured+=clipped.rgb[i]!=0;}
    check(coloured>100 && mismatch<8,"clipped UV agrees with explicitly interpolated near-plane polygon");
}
void backend_lease() {
    acvr_backend_api api{};ACVR_INIT(&api);check(acvr_backend_query(1,&api)==ACVR_OK,"synthetic factory unchanged");
    acvr_open_info open{};ACVR_INIT(&open);ACVR_INIT(&open.graphics);open.game_id_utf8="synthetic-system22";
    acvr_backend_info meta{};ACVR_INIT(&meta);acvr_backend *b=nullptr;
    check(api.game_open(&open,&b,&meta)==ACVR_OK,"open no-content harness");
    auto in=n22::synthetic_material_cube();check(n22::stage_cpu_scene(b,in)==ACVR_OK,"stage owned synthetic texture packet");
    in.materials.palette.assign(0x8000,0);
    acvr_inputs input{};ACVR_INIT(&input);input.tick_id=1;
    acvr_step_info step{};ACVR_INIT(&step);step.tick_id=1;acvr_frame_info info{};ACVR_INIT(&info);acvr_frame *f=nullptr;
    check(api.game_set_inputs(b,&input)==ACVR_OK && api.game_step(b,&step,&f,&info)==ACVR_OK,"publish unchanged libacvr lease");
    n22::Image image(128,64);const auto eye=n22::desktop_eye(0,0,4,64,64);
    check(n22::draw_cpu_frame(b,f,eye,image)==ACVR_OK && std::count_if(image.rgb.begin(),image.rgb.end(),[](uint32_t rgb){return rgb!=0;})>50,"leased material frame survives original palette mutation");
    const auto first=image.rgb;check(n22::draw_cpu_frame(b,f,eye,image)==ACVR_OK && image.rgb==first,"same lease replays identical material bytes");
    check(n22::stage_cpu_scene(b,in)==ACVR_BAD_STATE,"leased packet cannot be replaced");
    api.game_release_frame(b,f);api.game_close(b);
}
void texture_admission() {
    n22::Frame frame;check(n22::prepare(n22::synthetic_material_cube(),1,frame)==ACVR_OK,"bounded GL preparation fixture");
    n22::TexturePlan plan;check(n22::prepare_texture_plan(frame,plan)==ACVR_OK && plan.rectangles.size()==6 && plan.bytes==6*16*16*4,"exact whole-lease texture plan and duplicate-key reuse");
    auto bad=frame;bad.triangles[0].attributes[0].u=-65536;bad.triangles[0].attributes[1].u=65536;
    check(n22::prepare_texture_plan(bad,plan)==ACVR_UNSUPPORTED && plan.rectangles.size()==6,"large checked UV extent rejects without replacing plan");
    bad=frame;bad.triangles[0].attributes[0].u=std::numeric_limits<float>::infinity();
    check(n22::prepare_texture_plan(bad,plan)==ACVR_BAD_ARGUMENT,"nonfinite attributes reject before integer extent conversion");
    bad=frame;bad.materials.materials.resize(257);bad.triangles.resize(257,frame.triangles[0]);
    for(size_t i=0;i<257;++i) {bad.triangles[i].material=static_cast<uint32_t>(i);bad.materials.materials[i].objectflags=6;}
    check(n22::prepare_texture_plan(bad,plan)==ACVR_UNSUPPORTED,"whole-lease texture count checked before pixel sampling");
    bad=frame;bad.materials.materials.resize(9);bad.triangles.resize(9,frame.triangles[0]);
    for(size_t i=0;i<9;++i) {bad.triangles[i].material=static_cast<uint32_t>(i);bad.triangles[i].attributes={n22::MaterialVertex{.5f,.5f,64},{1023.5f,.5f,64},{.5f,1023.5f,64}};}
    // Sparse cells deliberately insufficient: budget must reject first, with
    // no allocation of these nine 4 MiB rectangles and no sample fallback.
    check(n22::prepare_texture_plan(bad,plan)==ACVR_UNSUPPORTED,"complete byte budget checked before any texture pixels allocated");
    bad=frame;bad.triangles.resize(n22::MaxGlTriangles+1);
    check(n22::prepare_texture_plan(bad,plan)==ACVR_UNSUPPORTED,"triangle count checked before per-triangle binding allocation");
    bad=frame;bad.materials.tiles.clear();check(n22::prepare_texture_plan(bad,plan)==ACVR_BAD_ARGUMENT && plan.rectangles.size()==6,"missing sparse input has no partial plan publication");
}
}
int main() {
    try {address_and_palette();modes_and_shade();malformed();per_eye_and_ownership();clipped_attributes();backend_lease();texture_admission();std::cout<<checks<<" material packet checks passed (synthetic CPU only)\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
