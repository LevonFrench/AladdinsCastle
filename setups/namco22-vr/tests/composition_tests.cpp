// SPDX-License-Identifier: GPL-3.0-only
// Independent tiny literal/rational images, never game pixels or geometry.
#include "n22_cpu_backend.hpp"
#include "n22_worker.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
int checks=0;
void check(bool ok,const char *what) {++checks;if(!ok) throw std::runtime_error(what);}
template<class T> T record() {T r{};ACVR_INIT(&r);return r;}
n22::CompositionItem item(uint32_t id,n22::CompositionKind kind,uint32_t priority=0,uint32_t order=0) {
    n22::CompositionItem i;i.id=id;i.tick=1;i.kind=kind;i.priority=priority;i.order=order;
    i.stage=kind==n22::CompositionKind::Polygon?n22::CompositionStage::PolygonPostShadeFogPolyfade:kind==n22::CompositionKind::Sprite?n22::CompositionStage::SpritePostFogPreOwnFade:n22::CompositionStage::TextPreOwnFadeAlphaResolved;
    i.order_provided=kind==n22::CompositionKind::Polygon;return i;
}
n22::SceneInput scene(const std::vector<n22::CompositionItem> &items={}) {
    n22::SceneInput s;auto &p=s.composition;p.policy=n22::CompositionPolicy::Super22SourceOrder;p.tick=p.mixer.tick=1;
    p.mixer.background={40,40,40};p.items=items;p.declarations.fill(n22::CompositionDeclaration::Empty);
    for(const auto &i:items) p.declarations[static_cast<size_t>(i.kind)]=n22::CompositionDeclaration::Complete;
    return s;
}
n22::CompositionSpan span(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint8_t c,uint8_t a=255) {
    n22::CompositionSpan s;s.x=x;s.y=y;s.width=w;s.height=h;s.pixels.resize(size_t(w)*h,{{c,c,c},a});return s;
}
n22::CompositionEyeSpans payload(const n22::CompositionPlan &p,const acvr_eye &e) {
    n22::CompositionEyeSpans x;x.frame_id=p.tick;x.eye=e;
    for(const auto &i:p.items) {n22::CompositionEyeItem v;v.id=i.id;v.tick=i.tick;v.stage=i.stage;x.items.push_back(v);}return x;
}
n22::Frame prepared(const n22::SceneInput &in) {n22::Frame f;check(n22::prepare(in,1,f)==ACVR_OK,"owned complete composition plan publication");return f;}
uint32_t run(const n22::SceneInput &in,const std::vector<n22::CompositionSpan> &samples) {
    auto f=prepared(in);auto e=n22::desktop_eye(0,0,4,4,4);auto x=payload(f.composition,e);
    check(samples.size()==x.items.size(),"fixture provides each item exactly once");for(size_t i=0;i<samples.size();++i) x.items[i].spans={samples[i]};
    n22::Image out(4,4);out.depth[0]=.37f;check(n22::compose_cpu(f,e,x,out)==ACVR_OK && out.depth[0]==.37f,"RGB-only scalar composition");return out.rgb[0];
}
void mixer_and_plan() {
    n22::VideoSnapshot source;source.tick=1;source.banks[n22::Mixer].resize(1024);
    for(size_t k=0;k<3;++k) {source.banks[n22::Mixer][8+k]=static_cast<uint8_t>(10+k);source.banks[n22::Mixer][0x16+k]=static_cast<uint8_t>(20+k);source.banks[n22::Mixer][(k+1)*256+17]=static_cast<uint8_t>(30+k);}
    source.banks[n22::Mixer][0x19]=127;source.banks[n22::Mixer][0x1a]=3;n22::CompositionMixer m;
    check(n22::copy_super22_composition_mixer(source,m)==ACVR_OK && m.background==std::array<uint8_t,3>{10,11,12} && m.fade==std::array<uint8_t,3>{20,21,22} && m.factor==127 && m.flags==3 && m.gamma[2][17]==32,"exact owned mixer offsets and three LUT channels");
    source.banks[n22::Mixer].assign(1024,0);check(m.gamma[2][17]==32 && m.background[0]==10,"snapshot mutation leaves copied mixer unchanged");
    source.tick=0;check(n22::copy_super22_composition_mixer(source,m)==ACVR_BAD_ARGUMENT && m.tick==1,"zero copy tick fails atomically");source.tick=1;
    for(size_t n:{size_t{0},size_t{10},size_t{1023},size_t{1025}}) {source.banks[n22::Mixer].resize(n);check(n22::copy_super22_composition_mixer(source,m)==ACVR_BAD_ARGUMENT && m.gamma[2][17]==32,"exact mixer length checked before replacing owned state");}
    auto in=scene();auto f=prepared(in);in.composition.mixer.background={99,99,99};check(f.composition.mixer.background[0]==40,"Frame owns plan/mixer, not staging source");
    auto invalid=[&](n22::SceneInput bad,acvr_result expected) {check(n22::prepare(bad,1,f)==expected && f.composition.mixer.background[0]==40,"invalid plan leaves previous published frame untouched");};
    auto bad=scene();bad.composition.tick=2;invalid(bad,ACVR_BAD_ARGUMENT);
    bad=scene();bad.composition.mixer.tick=2;invalid(bad,ACVR_BAD_ARGUMENT);
    bad=scene();bad.composition.declarations[1]=n22::CompositionDeclaration::Missing;invalid(bad,ACVR_UNSUPPORTED);
    bad=scene();bad.composition.declarations[1]=n22::CompositionDeclaration::Complete;invalid(bad,ACVR_BAD_ARGUMENT);
    bad=scene();bad.composition.policy=static_cast<n22::CompositionPolicy>(99);invalid(bad,ACVR_UNSUPPORTED);
    bad=scene();bad.composition.mixer.flags=4;invalid(bad,ACVR_UNSUPPORTED);
    bad=scene({item(1,n22::CompositionKind::Polygon),item(1,n22::CompositionKind::Sprite,2)});invalid(bad,ACVR_BAD_ARGUMENT);
    bad=scene({item(1,n22::CompositionKind::Polygon)});bad.composition.items[0].stage=n22::CompositionStage::SpritePostFogPreOwnFade;invalid(bad,ACVR_UNSUPPORTED);
    bad=scene({item(1,n22::CompositionKind::Polygon)});bad.composition.items[0].priority=0x1000000;invalid(bad,ACVR_BAD_ARGUMENT);
    bad=scene({item(1,n22::CompositionKind::Sprite,4),item(2,n22::CompositionKind::Sprite,4)});invalid(bad,ACVR_UNSUPPORTED);
    bad=scene({item(1,n22::CompositionKind::Polygon,4,2),item(2,n22::CompositionKind::Polygon,4,2)});invalid(bad,ACVR_BAD_ARGUMENT);
    bad=scene({item(1,n22::CompositionKind::Text),item(2,n22::CompositionKind::Text)});invalid(bad,ACVR_UNSUPPORTED);
    bad=scene();bad.composition.items.resize(n22::MaxCompositionItems+1);invalid(bad,ACVR_UNSUPPORTED);
    n22::Image out(4,4);out.rgb.assign(16,0x123456);out.depth.assign(16,.37f);const auto eye=n22::desktop_eye(0,0,4,4,4);const auto rgb=out.rgb;
    check(n22::draw_cpu(f,eye,out)==ACVR_UNSUPPORTED && out.rgb==rgb && out.depth[0]==.37f,"legacy CPU refuses present complete plan before mutation");
    check(n22::draw_cpu(f,eye,out,true)==ACVR_UNSUPPORTED && out.rgb==rgb,"HUD-only legacy CPU also cannot silently drop plan");
    n22::Frame absent;n22::prepare(n22::SceneInput{},1,absent);auto empty=payload(absent.composition,eye);
    check(n22::compose_cpu(absent,eye,empty,out)==ACVR_UNSUPPORTED && out.rgb==rgb,"composition entry never invents an absent plan");
    check(n22::draw_cpu(absent,eye,out)==ACVR_OK && out.rgb[0]==0,"absent plan preserves earlier polygon-only path");
}
void ordering_fades_gamma() {
    auto p1=item(1,n22::CompositionKind::Polygon,100,1),p2=item(2,n22::CompositionKind::Polygon,100,2),s=item(3,n22::CompositionKind::Sprite,100),t=item(4,n22::CompositionKind::Text);
    check(run(scene({p1,p2}),{span(0,0,1,1,70),span(0,0,1,1,90)})==0x464646,"reverse polygon emission tie makes first emitted70 topmost");
    check(run(scene({p1,s}),{span(0,0,1,1,70),span(0,0,1,1,90)})==0x464646,"sprite first at polygon tie, polygon covers it");
    s.priority=50;check(run(scene({p1,s}),{span(0,0,1,1,70),span(0,0,1,1,90)})==0x5a5a5a,"near sprite interleaves AFTER polygon by native priority");
    s.priority=150;check(run(scene({p1,s}),{span(0,0,1,1,70),span(0,0,1,1,90)})==0x464646,"far sprite interleaves BEFORE polygon, not a flattened all-sprites overlay");
    auto s2=s;s2.id=5;s.order_provided=s2.order_provided=true;s.order=0;s2.order=1;
    check(run(scene({s,s2}),{span(0,0,1,1,70),span(0,0,1,1,90)})==0x5a5a5a,"explicit actual sprite tie order consumed without inventing stability");
    s=item(3,n22::CompositionKind::Sprite,50);s.prioverchar=true;
    check(run(scene({s,t}),{span(0,0,1,1,90),span(0,0,1,1,200)})==0x5a5a5a,"prioverchar coverage masks later text");
    s.prioverchar=false;check(run(scene({s,t}),{span(0,0,1,1,90),span(0,0,1,1,200)})==0xc8c8c8,"ordinary sprite allows text over it");
    s.prioverchar=true;check(run(scene({s,t}),{span(0,0,1,1,90,0),span(0,0,1,1,200)})==0xc8c8c8,"transparent sprite contributes no text mask");
    auto in=scene({p1,t});in.composition.mixer.factor=127;in.composition.mixer.flags=1;in.composition.mixer.fade={20,20,20};
    for(auto &lut:in.composition.mixer.gamma) {lut[60]=7;lut[200]=19;lut[110]=23;lut[130]=29;}
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200)})==0x131313,"text follows global fade, gamma maps text200 to19 not faded110 to23");
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200,0)})==0x070707,"world100 fades to60 before nonlinear gamma7");
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200,128)})==0x1d1d1d && (200*128+60*127)/255==130,"partial text alpha composes before gamma130->29, not gamma-first13");
    in=scene({p1,t});in.composition.mixer.factor=1;in.composition.mixer.flags=3;
    check(run(in,{span(0,0,1,1,255),span(0,0,1,1,255)})==0xfefefe,"text own /255 gives254 while world screen /256 gives253");
    check(run(in,{span(0,0,1,1,255),span(0,0,1,1,255,0)})==0xfdfdfd,"white world screen factor1 gives253");
    in=scene({s});for(uint8_t flags=0;flags<4;++flags) for(bool own:{false,true}) {
        in.composition.mixer.factor=1;in.composition.mixer.flags=flags;in.composition.items[0].sprite_fade=own;
        const bool pre=(flags&2)||own,post=(flags&1)!=0;const uint32_t expected=pre&&post?251:pre||post?253:255;
        check(run(in,{span(0,0,1,1,255)})==expected*0x010101,"sprite bit1/item own fade precedes alpha; bit0 global fade can apply a second time");
    }
    in.composition.mixer.factor=0;in.composition.mixer.flags=3;check(run(in,{span(0,0,1,1,255)})==0xffffff,"zero factor bypasses both fade gates, retaining white255");
    in=scene();auto f=prepared(in);auto e=n22::desktop_eye(0,0,4,4,4);auto x=payload(f.composition,e);n22::Image out(4,4);
    check(n22::compose_cpu(f,e,x,out)==ACVR_OK && out.rgb[0]==0x282828,"all-zero gamma bypass is the pin's uninitialized policy, not black mapping");
    f.composition.mixer.gamma[0][40]=77;check(n22::compose_cpu(f,e,x,out)==ACVR_OK && out.rgb[0]==0x4d0000,"any-byte enables all channels, including zero-filled green/blue LUTs");
}
void independent_review_regressions() {
    // Literal witnesses from the independent Guns review. No production
    // colour helpers are used to compute expected pixels.
    const auto p=item(1,n22::CompositionKind::Polygon,100,1),t=item(4,n22::CompositionKind::Text);
    auto masked=item(2,n22::CompositionKind::Sprite,100);masked.prioverchar=true;
    const auto covering=item(1,n22::CompositionKind::Polygon,50,1);
    check(run(scene({masked,covering,t}),{span(0,0,1,1,90),span(0,0,1,1,70),span(0,0,1,1,200)})==0x464646,"later polygon70 covers masked sprite90 but cannot clear its text mask");
    check(run(scene({masked,covering,t}),{span(0,0,1,1,90,1),span(0,0,1,1,70),span(0,0,1,1,200)})==0x464646,"sprite alpha1 still masks text after polygon overdraw");
    check(run(scene({masked,covering,t}),{span(0,0,1,1,90,0),span(0,0,1,1,70),span(0,0,1,1,200)})==0xc8c8c8,"sprite alpha0 leaves no mask, permitting text200 after polygon70");
    auto ordinary=item(3,n22::CompositionKind::Sprite,10);
    check(run(scene({masked,covering,ordinary,t}),{span(0,0,1,1,90),span(0,0,1,1,70),span(0,0,1,1,70),span(0,0,1,1,200)})==0x464646,"later ordinary sprite70 cannot clear an earlier prioverchar mask");

    auto in=scene({p,t});in.composition.mixer.fade={20,20,20};in.composition.mixer.factor=127;
    const std::array<uint32_t,4> expected{0x969696,0x828282,0x696969,0x555555}; //150,130,105,85
    for(uint8_t flags=0;flags<4;++flags) {
        in.composition.mixer.flags=flags;
        check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200,128)})==expected[flags],"partial text alpha with flags0/1/2/3 distinguishes both fade gates and their order");
    }
    check(21700/255==85 && (130*128+20*127)/255==75,"text-own110 before alpha over screen60 gives85; wrong fade-after-alpha gives75");
    auto sprite=item(3,n22::CompositionKind::Sprite,50);sprite.sprite_fade=true;
    in=scene({p,sprite});in.composition.mixer.fade={20,20,20};in.composition.mixer.factor=127;
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200,128)})==0x696969,"sprite item own fade before partial alpha gives105, not swapped85");
    in.composition.items[1].sprite_fade=false;in.composition.mixer.flags=2;
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200,128)})==0x696969,"sprite mixer bit1 independently gives the same own-before-alpha105");
    check((110*128+100*127)/255==105 && (150*128+20*128)/256==85,"independent sprite own-before-alpha105 differs from alpha-before-own85");

    in=scene({p});in.composition.mixer.fade={20,20,20};in.composition.mixer.factor=255;in.composition.mixer.flags=1;
    check(run(in,{span(0,0,1,1,100)})==0x141414,"global factor255 reaches fade20 exactly");
    in=scene({p,sprite});in.composition.mixer.fade={20,20,20};in.composition.mixer.factor=255;in.composition.mixer.flags=2;
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200)})==0x141414,"sprite own factor255 reaches fade20 exactly");
    in=scene({p,t});in.composition.mixer.fade={20,20,20};in.composition.mixer.factor=255;in.composition.mixer.flags=3;
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200)})==0x141414,"text own factor255 reaches fade20 over globally faded20");
    in.composition.mixer.flags=1;
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200)})==0xc8c8c8,"flags1 factor255 still leaves later opaque text200 unfaded");
    in.composition.mixer.flags=3;in.composition.mixer.factor=0;
    check(run(in,{span(0,0,1,1,100),span(0,0,1,1,200,128)})==0x969696,"factor0 flags3 bypasses both fades with partial text alpha150");
}
void atomic_payload_limits_replay() {
    auto f=prepared(scene({item(7,n22::CompositionKind::Polygon,4,1)}));auto eye=n22::desktop_eye(0,0,4,4,4);eye.rect_x=2;eye.rect_y=1;
    auto good=payload(f.composition,eye);good.items[0].spans={span(0,0,2,2,90)};
    n22::Image out(10,8);out.rgb.assign(80,0xabcdef);for(size_t i=0;i<out.depth.size();++i) out.depth[i]=static_cast<float>(i)/100;
    auto prior=out.rgb;const auto depth=out.depth;
    auto reject=[&](const n22::CompositionEyeSpans &x,acvr_result expected) {check(n22::compose_cpu(f,eye,x,out)==expected && out.rgb==prior && out.depth==depth,"invalid eye payload fails atomically without RGB/depth mutation");};
    auto bad=good;bad.frame_id=2;reject(bad,ACVR_BAD_STATE);
    bad=good;bad.eye.eye_index=1;reject(bad,ACVR_BAD_STATE);
    bad=good;bad.eye.view_from_scene[12]=.01f;reject(bad,ACVR_BAD_STATE);
    bad=good;bad.eye.projection_from_view[8]=.01f;reject(bad,ACVR_BAD_STATE);
    bad=good;bad.eye.rect_x=3;reject(bad,ACVR_BAD_STATE);
    bad=good;bad.items.clear();reject(bad,ACVR_BAD_ARGUMENT);
    bad=good;bad.items[0].id=99;reject(bad,ACVR_BAD_ARGUMENT);
    bad=good;bad.items[0].tick=2;reject(bad,ACVR_BAD_ARGUMENT);
    bad=good;bad.items[0].stage=n22::CompositionStage::Unspecified;reject(bad,ACVR_UNSUPPORTED);
    bad=good;bad.items[0].spans[0].width=0;reject(bad,ACVR_BAD_ARGUMENT);
    bad=good;bad.items[0].spans[0].x=UINT32_MAX;reject(bad,ACVR_BAD_ARGUMENT);
    bad=good;bad.items[0].spans[0].width=UINT32_MAX;reject(bad,ACVR_BAD_ARGUMENT);
    bad=good;bad.items[0].spans[0].pixels.pop_back();reject(bad,ACVR_BAD_ARGUMENT);
    bad=good;bad.items[0].spans.push_back(bad.items[0].spans[0]);reject(bad,ACVR_BAD_ARGUMENT);
    bad=good;bad.items[0].spans[0].pixels[0].alpha=128;reject(bad,ACVR_UNSUPPORTED);
    bad=good;bad.items[0].spans.resize(n22::MaxCompositionSpans+1);reject(bad,ACVR_UNSUPPORTED);
    bad=good;bad.items[0].spans[0].pixels.reserve(n22::MaxCompositionInputBytes/sizeof(n22::CompositionPixel)+1);reject(bad,ACVR_UNSUPPORTED);
    // Duplicate/missing IDs with an unchanged overall payload count.
    auto two=prepared(scene({item(7,n22::CompositionKind::Polygon,4,1),item(8,n22::CompositionKind::Sprite,3)}));
    auto twice=payload(two.composition,eye);twice.items[1].id=7;
    check(n22::compose_cpu(two,eye,twice,out)==ACVR_BAD_ARGUMENT && out.rgb==prior && out.depth==depth,"duplicate ID cannot conceal missing item in count-equal payload");
    auto cumulative=payload(two.composition,eye);
    for(auto &i:cumulative.items) {i.spans={span(0,0,1,1,90)};i.spans[0].pixels.reserve(n22::MaxCompositionInputBytes/(2*sizeof(n22::CompositionPixel)));}
    check(n22::compose_cpu(two,eye,cumulative,out)==ACVR_UNSUPPORTED && out.rgb==prior && out.depth==depth,"individually bounded item storage exceeds cumulative byte budget together");
    auto huge=eye;huge.rect_width=4097;auto giant=payload(f.composition,huge);n22::Image wide(8192,1);huge.rect_y=0;huge.rect_height=1;giant.eye=huge;
    check(n22::compose_cpu(f,huge,giant,wide)==ACVR_UNSUPPORTED && wide.rgb.front()==0,"edge budget checked before working pixel allocation");
    huge.rect_x=0;huge.rect_width=4096;huge.rect_height=1025;giant.eye=huge;n22::Image area(4096,1025);
    check(n22::compose_cpu(f,huge,giant,area)==ACVR_UNSUPPORTED && area.rgb.front()==0,"cumulative eye pixel/work budget checked before allocation");
    check(n22::compose_cpu(f,eye,good,out)==ACVR_OK && out.depth==depth,"valid item RGB composes with every caller depth unchanged");
    bool exact=true;for(uint32_t y=0;y<8;++y) for(uint32_t x=0;x<10;++x) {
        const bool inside=x>=2 && x<6 && y>=3 && y<7,covered=inside && x<4 && y<5;
        exact=exact && out.rgb[size_t(y)*10+x]==(covered?0x5a5a5au:inside?0x282828u:0xabcdefu);
    }
    check(exact,"top-left eye-local spans map nonzero lower-left eye rect without touching neighbours");prior=out.rgb;
    check(n22::compose_cpu(f,eye,good,out)==ACVR_OK && out.rgb==prior && out.depth==depth,"same-pose lease replay is stable");
    auto moved=eye;moved.view_from_scene[12]=-.2f;
    check(n22::compose_cpu(f,moved,good,out)==ACVR_BAD_STATE && out.rgb==prior,"old decoded spans cannot be reused at a new pose");good.eye=moved;
    check(n22::compose_cpu(f,moved,good,out)==ACVR_OK && out.rgb==prior,"newly bound decoded spans use same immutable mixer/plan");
    auto right=moved;right.eye_index=1;right.rect_x=6;good.eye=right;
    check(n22::compose_cpu(f,right,good,out)==ACVR_OK && out.depth==depth && out.rgb[3*10+2]==0x5a5a5a,"paired eye commits only its rectangle and retains left output/depth");
}
void callbacks() {
    auto api=record<acvr_backend_api>();acvr_backend_query(1,&api);check(api.supported_graphics==0,"complete synthetic composition does not admit native factory/graphics");
    auto open=record<acvr_open_info>();ACVR_INIT(&open.graphics);open.game_id_utf8="synthetic-system22";auto meta=record<acvr_backend_info>();acvr_backend *b=nullptr;api.game_open(&open,&b,&meta);
    auto in=scene({item(7,n22::CompositionKind::Polygon,4,1)});const auto frozen=in.composition;
    check(n22::stage_cpu_scene(b,in)==ACVR_OK,"existing staging owns production-intended plan");in.composition.items.clear();in.composition.mixer.background.fill(0);
    auto inputs=record<acvr_inputs>();inputs.tick_id=1;api.game_set_inputs(b,&inputs);auto step=record<acvr_step_info>();step.tick_id=1;auto info=record<acvr_frame_info>();acvr_frame *lease=nullptr;
    check(api.game_step(b,&step,&lease,&info)==ACVR_OK,"usual immutable composition lease publication");
    auto eye=n22::desktop_eye(0,0,4,4,4);auto x=payload(frozen,eye);x.items[0].spans={span(0,0,1,1,90)};n22::Image image(8,4);image.depth.assign(32,.4f);
    check(n22::compose_cpu_frame(b,lease,eye,x,image)==ACVR_OK && image.rgb[0]==0x5a5a5a && image.rgb[1]==0x282828 && image.depth[0]==.4f,"callback consumes frozen plan despite mutated producer");
    check(n22::stage_cpu_scene(b,scene())==ACVR_BAD_STATE,"active composition lease cannot be replaced");
    const auto rgb=image.rgb;check(n22::draw_cpu_frame(b,lease,eye,image)==ACVR_UNSUPPORTED && image.rgb==rgb,"old callback cannot drop complete composition silently");
    eye.eye_index=1;eye.rect_x=4;x.eye=eye;check(n22::compose_cpu_frame(b,lease,eye,x,image)==ACVR_OK && image.rgb[4]==0x5a5a5a && image.rgb[0]==0x5a5a5a,"real callback paired replay retains first eye");
    api.game_release_frame(b,lease);check(n22::compose_cpu_frame(b,nullptr,eye,x,image)==ACVR_BAD_STATE,"released lease cannot compose");
    in=scene();in.composition.tick=in.composition.mixer.tick=2;in.composition.mixer.background={60,60,60};n22::stage_cpu_scene(b,in);
    inputs.tick_id=2;api.game_set_inputs(b,&inputs);step.tick_id=2;step.simulation_time_ns=static_cast<int64_t>(1000000000000ULL/59906ULL);api.game_step(b,&step,&lease,&info);
    x=payload(in.composition,eye);check(n22::compose_cpu_frame(b,lease,eye,x,image)==ACVR_OK && image.rgb[4]==0x3c3c3c,"successor lease receives new owned composition state");api.game_release_frame(b,lease);api.game_close(b);
}
}
int main() {
    try {mixer_and_plan();ordering_fades_gamma();independent_review_regressions();atomic_payload_limits_replay();callbacks();std::cout<<checks<<" owned composition checks passed (synthetic CPU only)\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
