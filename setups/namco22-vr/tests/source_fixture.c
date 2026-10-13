/* SPDX-License-Identifier: GPL-3.0-only
 * Synthetic banks only, compiled against the actual pinned ss22_regs header.
 */
#include "ss22_gl.h"
#include "n22_source_hooks.h"
#include <string.h>
static unsigned step_count,input_count,press_count;
static uint8_t pal[0x18000],mixer[0x400],cg[0x1e000],text[0x2000],sprites[0x30000],vics[0x10000];
static uint16_t spot[0x800],cz[4][256];
static uint32_t poly[0x8000];
static uint32_t poly_word(int i) {return poly[(unsigned)i&0x7fff];}
void n22_source_fixture_input(unsigned trigger) {++input_count;if(trigger&ACVR_INPUT_PRESSED) ++press_count;}
unsigned n22_source_fixture_inputs(void) {return input_count;}
unsigned n22_source_fixture_presses(void) {return press_count;}
void n22_source_fixture_mutate(void) {
    pal[0]=99;mixer[0]=99;cg[0]=99;text[0]=99;sprites[0]=99;vics[0]=99;spot[0]=99;cz[0][0]=99;poly[0]=99;
}
void n22_source_fixture_entry(void) {
    for(;;) {
        ss22_regs r;
        unsigned i,b;
        acvr_ss22_check_stop();++step_count;
        memset(&r,0,sizeof r);
        memset(pal,(int)step_count,sizeof pal);memset(mixer,(int)step_count,sizeof mixer);
        memset(cg,(int)step_count,sizeof cg);memset(text,(int)step_count,sizeof text);
        memset(sprites,(int)step_count,sizeof sprites);memset(vics,(int)step_count,sizeof vics);
        for(i=0;i<0x800;++i) spot[i]=(uint16_t)step_count;
        for(i=0;i<0x8000;++i) poly[i]=step_count;
        for(b=0;b<4;++b) {for(i=0;i<256;++i) cz[b][i]=(uint16_t)(step_count+b);r.czram[b]=cz[b];}
        r.walk=true;r.poly_word=poly_word;r.pal=pal;r.mixer=mixer;r.cgram=cg;r.textram=text;
        r.spriteram=sprites;r.spriteram_size=sizeof sprites;r.vics=vics;r.vics_size=sizeof vics;
        r.spotram=spot;r.spot_enabled=true;r.czattr[0]=(uint16_t)step_count;
        r.tilemapattr[0]=(uint16_t)step_count;r.vics_ctl[0]=step_count;
        acvr_ss22_output_bits((uint16_t)step_count);
        acvr_ss22_video_snapshot(&r);
        acvr_ss22_frame_boundary();
    }
}
