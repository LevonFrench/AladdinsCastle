"""Producer capability audit using synthetic packets/poses; no sockets or runtime."""
import configparser
import hashlib
import sys
import unittest
from wii_shell_stick import ROOT,source,render_profile,render_producer_profile,scripted_bridge_profile,CANONICAL_LIVE_SOURCE_TESTED
from wii_controls import compile_library
sys.path.insert(0,str(ROOT/'tools/wii-bridge'))
from dsu import Pad,payload
from motion import Mapper
from scripted import BUTTONS,Timeline

IDENTITY=[1,0,0,0,0,1,0,0,0,0,1,0]
def read_profile(data):
    p=configparser.ConfigParser(interpolation=None);p.optionxform=str;p.read_string(data.decode());return p['Wiimote1']
def legacy(left=(),right=(),offscreen='edge',outside=False):
    mapper=Mapper(offscreen=offscreen);mapper.recenter(IDENTITY)
    def hand(pressed):
        pose=IDENTITY[:]
        if outside:pose[3]=10
        return dict(valid=True,pose=pose,velocity=[0,0,0],omega=[0,0,0],stick=[0,0],
            buttons=[name in pressed for name in ('trigger','grip','primary','secondary','menu','click')])
    return mapper.update(dict(time=1,hands=[hand(left),hand(right)]))

class ProfileAuditTests(unittest.TestCase):
    def test_current_packet_writer_cannot_emit_home(self):
        self.assertEqual(payload(0,Pad(buttons=0xffff),1)[18],0)
        self.assertNotIn('PS',BUTTONS)
        with self.assertRaisesRegex(ValueError,'Unknown DSU button'):
            Timeline(dict(version=1,seconds=1,steps=[dict(at=0,pads=[dict(buttons=['PS']),{}])]))
        for mode in ('split','docked','two-remotes'):
            for hand in ('left','right'):
                self.assertEqual(read_profile(render_profile('flat-dolphin',mode,hand))['Buttons/Home'],'')
        self.assertTrue(read_profile(render_profile())['Buttons/Home'])

    def test_legacy_primary_bit13_is_circle_not_documented_cross(self):
        pads=legacy(right=['primary'])
        self.assertEqual(pads[0].buttons,1<<BUTTONS['Circle'])
        self.assertFalse(pads[0].buttons & (1<<BUTTONS['Cross']))
        self.assertEqual(next(a['wire'] for a in source()['action'] if a['id']=='interact'),'Cross')

    def test_legacy_offhand_and_chord_do_not_adopt_canonical_policy(self):
        self.assertEqual(legacy(left=['trigger'])[1].buttons,1<<BUTTONS['R2'])
        self.assertEqual(legacy(left=['grip'])[1].buttons,1<<BUTTONS['L2'])
        self.assertNotEqual(legacy(left=['grip'])[1].buttons,1<<BUTTONS['Circle'])
        self.assertEqual(legacy(right=['click'])[0].buttons,0)
        self.assertNotEqual(legacy(left=['click'],right=['click','trigger'])[0].buttons,0)
        self.assertEqual(legacy(offscreen='hide',outside=True)[0].buttons,1<<BUTTONS['Square'])

    def test_producer_selection_refuses_legacy_and_unknown(self):
        gid='wii-resident-evil-umbrella-chronicles'
        from wii_shell_stick import render_scripted_profile
        for producer in ('legacy-steamvr','automatic'):
            with self.assertRaises(ValueError):render_producer_profile(gid,producer)
        if CANONICAL_LIVE_SOURCE_TESTED:self.assertEqual(render_producer_profile(gid,'canonical-live','left'),render_scripted_profile(gid,'left'))
        else:
            with self.assertRaisesRegex(ValueError,'source review'):render_producer_profile(gid,'canonical-live')
        self.assertEqual(render_producer_profile(gid,'native-openxr'),render_profile())
        self.assertEqual(render_producer_profile(gid,'scripted-dsu'),render_scripted_profile(gid))
        self.assertEqual(hashlib.sha256(render_profile()).hexdigest(),'edfcccec10a8226ee5366b0fedf500a57a3674dd7b91b72c6bf200e55fc27ebe')

    def test_descriptor_roles_requirements_and_unsupported_home(self):
        for game in compile_library()['games']:
            for hand in ('left','right'):
                d=scripted_bridge_profile(game['game_id'],hand);p=d['producer_policy']
                self.assertEqual(p['schema'],'canonical-dsu/1');self.assertEqual(p['slots'],{'remote':0,'nunchuk':1})
                self.assertEqual(p['unsupported_actions'],['home']);self.assertFalse(p['offscreen_hide_supported'])
                self.assertEqual(d['live_bridge_compatible'],CANONICAL_LIVE_SOURCE_TESTED);self.assertFalse(d['recenter_supported'])
                self.assertFalse(p['producers']['legacy_steamvr']['supported']);self.assertTrue(p['producers']['scripted_dsu']['supported'])
                self.assertTrue(p['recenter']['suppress_game_buttons']);self.assertTrue(p['recenter']['shared_theatre_calibration'])
                self.assertEqual(p['producers']['canonical_live']['supported'],CANONICAL_LIVE_SOURCE_TESTED)
                home=next(a for a in d['players'][0]['actions'] if a['id']=='home')
                self.assertFalse(home['supported']);self.assertFalse(home['script']['supported'])
                remote=d['players'][0]['remote_hand'];other=d['players'][0]['stick_hand']
                self.assertEqual(remote,hand);self.assertNotEqual(remote,other)
                for action in d['players'][0]['actions']:
                    self.assertEqual(action['hand'],remote if action['role']=='remote' else other)
                    self.assertEqual(action['slot'],0 if action['role']=='remote' else 1)

    def test_aim_and_offhand_axes_swap_but_scripted_slots_are_roles(self):
        server=source()['dsu']['server']
        for hand,remote,other in (('right',0,1),('left',1,0)):
            p=read_profile(render_profile('flat-dolphin','split',hand))
            self.assertEqual(p['Device'],f'DSUClient/{remote}/{server}')
            self.assertEqual(p['IR/Right'],'`Right X+`')
            self.assertEqual(p['Nunchuk/Stick/Right'],f'`DSUClient/{other}/{server}:Left X+`')
            self.assertEqual(p['Nunchuk/Buttons/Z'],f'`DSUClient/{other}/{server}:L2`')
            self.assertEqual(p['Nunchuk/Buttons/C'],f'`DSUClient/{other}/{server}:Circle`')

    def test_actions_reload_and_confirm_back_remain_questions(self):
        for game in compile_library()['games']:
            for route,data in game['route_variants'].items():
                self.assertEqual(data['layout']['state'],'question');self.assertEqual(data['reload']['state'],'question')
                self.assertFalse(data['reload']['enabled'])
                for action in data['element']:
                    if not action['id'].endswith('-aim'):self.assertEqual(action['game_action_state'],'question')
                home=next(a for a in data['element'] if a['id']=='p1-home')
                if route=='flat-dolphin':
                    self.assertEqual(home['availability']['state'],'unavailable');self.assertIn('PS/Home',home['availability']['reason'])
                else:self.assertEqual(home['availability']['state'],'configured')

    def test_selected_view_profile_uses_canonical_role_slots_on_both_hands(self):
        from wii_controls import resolve_game
        for game in compile_library()['games']:
            for hand in ('left','right'):
                data=resolve_game(ROOT,game['game_id'],hand)['data']
                self.assertEqual(data['transport']['device_slots'],{'remote':0,'nunchuk':1})
                self.assertTrue(data['profiles']['dsu'].endswith(f'/scripted-dsu-{hand}.ini'))
                p=read_profile((ROOT/data['profiles']['dsu']).read_bytes())
                self.assertEqual(p['Device'],'DSUClient/0/'+source()['dsu']['server'])
                native=resolve_game(ROOT,game['game_id'],hand,route='dolphinxr')['data']
                self.assertTrue(native['profiles']['native'].endswith(f'/dolphinxr-{hand}.ini'))
                self.assertEqual(native['transport']['pointer'],'native_hand_pose')

    def test_manifest_never_admits_generic_physical_profiles_as_canonical_live(self):
        import json
        from wii_shell_stick import generated_files,bridge_profile
        manifest=json.loads(generated_files()['generated-files.json'])['capabilities']
        self.assertFalse(manifest['flat_dolphin']['live_bridge_compatible'])
        self.assertEqual(manifest['canonical_live']['single_player'],CANONICAL_LIVE_SOURCE_TESTED)
        for feature in ('two_remotes','home','recenter'):self.assertFalse(manifest['canonical_live'][feature])
        for hand in ('left','right'):self.assertFalse(bridge_profile(primary_hand=hand)['live_bridge_compatible'])

if __name__=='__main__':unittest.main()
