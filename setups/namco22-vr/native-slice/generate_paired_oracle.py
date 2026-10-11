"""CPU source generation from the reviewed exact oracle, never a native build."""
from pathlib import Path
import paired_proof_protocol as proof


def render():
    o=proof.oracle()
    def array(name,values):
        return 'static const uint32_t '+name+'[] = {'+','.join('0x'+v+'u' for v in values)+'};\n'
    text='/* Generated from exact reviewed numeric oracle; do not hand edit. */\n#include <stdint.h>\n'
    text+=array('proof_anchor_bits',o['anchor_f32'])
    for phase,s in enumerate(o['markers']['poses']):
        text+=array('proof_head_'+str(phase),s['head_f32'])
        text+=array('proof_projection_'+str(phase),s['projection_f32'])
        for eye,e in enumerate(s['eyes']):
            text+=array('proof_view_'+str(phase)+'_'+str(eye),e['view_f32'])
            if phase==0:text+=array('proof_eye_'+str(eye),e['eye_from_head_f32'])
    s=o['cz'];text+=array('proof_head_cz',s['head_f32'])+array('proof_projection_cz',s['projection_f32'])
    for eye,e in enumerate(s['samples']):text+=array('proof_view_cz_'+str(eye),e['view_f32'])
    text+='static const int32_t proof_cz_xyz[4][3] = {'+','.join('{'+','.join(map(str,v))+'}' for v in s['xyz'])+'};\n'
    text+='static const int32_t proof_cz_uv[4][2] = {'+','.join('{'+','.join(map(str,v))+'}' for v in s['uv_q16'])+'};\n'
    return text


if __name__=='__main__':
    Path(__file__).with_name('paired_oracle.h').write_bytes(render().encode('ascii'))
