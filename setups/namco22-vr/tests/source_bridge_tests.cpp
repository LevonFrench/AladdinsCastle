// SPDX-License-Identifier: GPL-3.0-only
#include "n22_upstream_bridge.hpp"
#include "n22_source_hooks.h"
#include <iostream>
#include <stdexcept>
extern "C" {
void n22_source_fixture_entry(void);
void n22_source_fixture_input(unsigned);
unsigned n22_source_fixture_inputs(void);
unsigned n22_source_fixture_presses(void);
void n22_source_fixture_mutate(void);
}
namespace {int checks=0;void check(bool b,const char *s) {++checks;if(!b) throw std::runtime_error(s);}}
int main() {
    try {
        n22::Worker worker([](n22::Worker &w) {return n22::run_source_entry(w,n22_source_fixture_entry,
            [](n22::NativeInput in) {n22_source_fixture_input(in.trigger);});});
        std::shared_ptr<const n22::VideoSnapshot> first,second;
        check(worker.request({1,ACVR_INPUT_HELD|ACVR_INPUT_PRESSED,.5f,.5f,1},first)==ACVR_OK,"actual source-header bridge publication");
        check(n22_source_fixture_inputs()==1 && n22_source_fixture_presses()==1 && first->output_bits==1,"native input/output exactly once");
        n22_source_fixture_mutate();
        for(size_t i=0;i<n22::BankCount;++i)
            check(first->banks[i].size()==n22::bank_sizes[i] && first->banks[i][0]==1,"source bank copied, not borrowed");
        check(first->polygon_words[0]==1 && first->czram[0][0]==1 && first->spot_words[0]==1,"all pointer-indirect host word banks copied");
        const uint16_t *retained_spot=nullptr;const uint8_t *retained_pal=nullptr;
        n22::with_ss22_regs(*first,[&](const ss22_regs &r) {
            check(r.poly_word(0)==1 && r.poly_word(0x8000)==1,"owner-thread polygon callback reads leased copy");
            check(r.czattr[0]==1 && r.tilemapattr[0]==1 && r.vics_ctl[0]==1,"register scalar arrays retained");
            retained_spot=r.spotram;retained_pal=r.pal;
            check(r.spotram==first->spot_words.data() && r.pal==first->banks[n22::Palette].data(),"replay-retained pointers refer to lease-owned storage");
            bool nested=false;try {n22::with_ss22_regs(*first,[](const ss22_regs &) {});} catch(const std::invalid_argument &) {nested=true;}
            check(nested,"nested global polygon-reader binding rejected");
        });
        check(retained_spot[0]==1 && retained_pal[0]==1,"replay pointer lifetime extends past preparation callback");
        check(worker.release(first)==ACVR_OK && worker.request({2,ACVR_INPUT_HELD,.5f,.5f,1},second)==ACVR_OK,"second native boundary permit");
        check(n22_source_fixture_inputs()==2 && n22_source_fixture_presses()==1 && second->output_bits==2,"held trigger does not replay pressed edge");
        check(first->banks[n22::Palette][0]==1 && second->banks[n22::Palette][0]==2 && retained_spot[0]==1,"new source frame leaves old copy unchanged");
        geo_quad quad{};geo_view view{};quad.v[0].sx16=160;quad.rv[0].z=2;quad.ndv=3;
        quad.rv[0].u=19;quad.rv[0].v=27;quad.rv[0].bri=128;quad.rv[0].uf=123;quad.rv[0].vf=456;quad.rv[0].bf=789;
        quad.color=0x128000;quad.texbank=7;quad.cmode=13;quad.objectflags=3;quad.cz_type=2;quad.cz_adjust=0x801234;
        quad.zsort=0x123456;quad.order=17;quad.uvbox[0]=12;quad.clip[0]=9;
        view.zoom_mant=1545;view.zoom_shift=1;view.vx=7;view.vy=-4;
        auto captured=n22::copy_geo_quad(quad,&view);quad.v[0].sx16=0;view.zoom_mant=0;
        check(captured.has_camera && captured.focal==772.5f && captured.cx==327 && captured.cy==236,"exact emitted camera copied before sorting");
        check(captured.quad.v[0].sx16==160 && captured.quad.rv[0].z==2 && captured.quad.ndv==3,"all raw/clipped/guard-band quad data retained");
        check(captured.quad.rv[0].u==19 && captured.quad.rv[0].v==27 && captured.quad.rv[0].bri==128 &&
              captured.quad.rv[0].uf==123 && captured.quad.rv[0].vf==456 && captured.quad.rv[0].bf==789,"UV brightness and pre-truncation fields survive capture");
        check(captured.quad.color==0x128000 && captured.quad.texbank==7 && captured.quad.cmode==13 && captured.quad.objectflags==3 &&
              captured.quad.cz_type==2 && captured.quad.cz_adjust==0x801234,"palette/addressing/fog/solid metadata retained without interpretation");
        check(captured.quad.zsort==0x123456 && captured.quad.order==17 && captured.quad.uvbox[0]==12 && captured.quad.clip[0]==9,"sort order and stable UV/clip seam retained");
        quad.direct=1;check(!n22::copy_geo_quad(quad,&view).has_camera,"direct polygons are not assigned an invented camera");
        quad.direct=0;view.zoom_mant=1545;
        auto emitted=n22::prepare_with_capture(*second,[&](const ss22_regs &r) {
            check(r.poly_word(0)==2,"capture preparation uses newest leased banks");
            acvr_ss22_capture_quad(&quad,&view);quad.direct=1;
            acvr_ss22_capture_quad(&quad,nullptr);quad.v[0].sx16=999;
        });
        check(emitted.size()==2 && emitted[0].has_camera && !emitted[1].has_camera &&
              emitted[0].quad.v[0].sx16==0,"actual source hook callback path copies world and direct records");
        worker.stop();
        std::cout<<checks<<" pinned source-header snapshot checks passed\n";return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
