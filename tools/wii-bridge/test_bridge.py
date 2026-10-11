# SPDX-License-Identifier: GPL-3.0-only
import copy
import hashlib
import math
import json
import socket
import struct
import tempfile
import time
import unittest
import zlib
from pathlib import Path
from dsu import DATA, PORTS, VERSION, Pad, Server, axis, mac, packet, parse, payload
from motion import Mapper, MotionLatch
from policy import CanonicalPolicy

IDENTITY = [1,0,0,0, 0,1,0,0, 0,0,1,0]


def hand():
    return dict(valid=True,pose=IDENTITY[:],velocity=[0,0,0],omega=[0,0,0],stick=[0,0],buttons=[False]*6)


ROOT = Path(__file__).resolve().parents[2]


class CanonicalInput(unittest.TestCase):
    def admitted_data(self, game='wii-dead-space-extraction', primary='right'):
        path = ROOT / 'data/controls/wii/generated/games' / game / f'scripted-dsu-{primary}.json'
        data = json.loads(path.read_text(encoding='utf-8'))
        data['producer_policy'] = {
            'schema': 'canonical-dsu/1',
            'expression_tokens': sorted(('click', 'down', 'grip', 'left', 'primary',
                                         'right', 'secondary', 'trigger', 'up')),
            'slots': {'remote': 0, 'nunchuk': 1}, 'unsupported_actions': ['home'],
            'offscreen_hide_supported': False,
            'recenter': {'chord': ['right.click', 'left.click'],
                         'suppress_game_buttons': True, 'shared_theatre_calibration': True},
            'producers': {'scripted_dsu': {'supported': True, 'tracked_recenter': False},
                          'legacy_steamvr': {'supported': False, 'reason': 'synthetic legacy refusal'},
                          'canonical_live': {'supported': True, 'state': 'source-tested'}},
        }
        data['live_bridge_compatible'] = True
        data['recenter_supported'] = False
        for player in data['players']:
            for action in player['actions']:
                action['supported'] = action['script']['supported']
        return data

    def profile(self, primary='right'):
        return CanonicalPolicy(self.admitted_data(primary=primary), 'synthetic-admitted')

    def wire_mask(self, slot, pad):
        return struct.unpack_from('<H', packet(DATA, payload(slot, pad, 1)), 36)[0]

    def test_generated_hash_game_identity_and_refusal(self):
        policy = self.profile()
        self.assertEqual((policy.remote_hand, policy.stick_hand), ('right', 'left'))
        with self.assertRaises(ValueError):
            CanonicalPolicy.load(ROOT, '../escape', 'right')
        unready = self.admitted_data()
        unready['live_bridge_compatible'] = False
        unready['producer_policy']['producers']['canonical_live'] = {
            'supported': False, 'state': 'consumer-implementation-pending'}
        with self.assertRaisesRegex(ValueError, 'not approved'):
            CanonicalPolicy(unready, 'synthetic-refused')
        scratch = Path(__file__).resolve().parent / '.local'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temp:
            base = Path(temp) / 'data/controls/wii/generated'
            target = base / 'games/wii-dead-space-extraction'
            target.mkdir(parents=True)
            source = ROOT / 'data/controls/wii/generated'
            manifest = json.loads((source / 'generated-files.json').read_text(encoding='utf-8'))
            product = Path(temp) / 'data/product.json'
            product.write_bytes((ROOT / 'data/product.json').read_bytes())
            for extension in ('.json', '.ini'):
                name = 'scripted-dsu-right' + extension
                (target / name).write_bytes((source / 'games/wii-dead-space-extraction' / name).read_bytes())
            path = target / 'scripted-dsu-right.json'
            path.write_text(json.dumps(self.admitted_data()), encoding='utf-8')
            rel = 'games/wii-dead-space-extraction/scripted-dsu-right.json'
            manifest['files'][rel] = hashlib.sha256(path.read_bytes()).hexdigest()
            (base / 'generated-files.json').write_text(json.dumps(manifest), encoding='utf-8')
            self.assertEqual(CanonicalPolicy.load(temp, 'wii-dead-space-extraction', 'right').primary_hand, 'right')
            path.write_bytes(path.read_bytes() + b' ')
            with self.assertRaisesRegex(ValueError, 'hash mismatch'):
                CanonicalPolicy.load(temp, 'wii-dead-space-extraction', 'right')
            path.write_text(json.dumps(self.admitted_data()), encoding='utf-8')
            (target / 'scripted-dsu-right.ini').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'hash mismatch'):
                CanonicalPolicy.load(temp, 'wii-dead-space-extraction', 'right')
        wrong = self.admitted_data()
        wrong['device_slots']['remote'] = 1
        with self.assertRaises(ValueError): CanonicalPolicy(wrong, 'synthetic')
        wrong['device_slots']['remote'] = 0
        wrong['players'][0]['actions'][0]['hand'] = 'left'
        with self.assertRaises(ValueError): CanonicalPolicy(wrong, 'synthetic')

    def test_every_generated_game_role_profile(self):
        source = ROOT / 'data/controls/wii/generated'
        manifest = json.loads((source / 'generated-files.json').read_text())
        games = {path.split('/')[1] for path in manifest['files']
                 if path.startswith('games/') and path.endswith('/scripted-dsu-right.json')}
        self.assertGreaterEqual(len(games), 5)
        for game in sorted(games):
            for primary in ('right', 'left'):
                with self.subTest(game=game, primary=primary):
                    rel = f'games/{game}/scripted-dsu-{primary}.json'
                    self.assertEqual(hashlib.sha256((source / rel).read_bytes()).hexdigest(), manifest['files'][rel])
                    policy = CanonicalPolicy(self.admitted_data(game, primary), 'synthetic-admitted')
                    self.assertEqual(policy.remote_hand, primary)
                    self.assertEqual(policy.stick_hand, 'left' if primary == 'right' else 'right')

    def test_right_remote_and_left_nunchuk_wire_actions(self):
        mapper = Mapper(policy=self.profile())
        frame = dict(time=1., hands=[hand(), hand()])
        frame['hands'][1]['buttons'][0] = True # fire
        frame['hands'][1]['buttons'][2] = True # interact Cross
        self.assertEqual(self.wire_mask(0, mapper.update(frame)[0]), 1 << 14) # no calibration, no fire
        self.assertTrue(mapper.recenter(IDENTITY))
        frame['hands'][0]['buttons'][0] = True # Nunchuk Z
        frame['hands'][0]['buttons'][1] = True # Nunchuk C
        frame['hands'][0]['stick'] = [.8, -.7]
        pads = mapper.update(frame)
        self.assertEqual(self.wire_mask(0, pads[0]), (1 << 9) | (1 << 14))
        self.assertEqual(self.wire_mask(1, pads[1]), (1 << 8) | (1 << 13))
        self.assertEqual(pads[0].stick, (0., 0.))
        self.assertEqual(pads[1].stick, (.8, -.7))
        frame['hands'][1]['buttons'][1] = True # grip modifier changes Cross to Options
        pads = mapper.update(frame)
        self.assertEqual(self.wire_mask(0, pads[0]), 1 << 3)
        frame['hands'][1]['buttons'][2] = False
        frame['hands'][1]['buttons'][3] = True # grip + secondary = Share (minus)
        frame['hands'][1]['stick'] = [0, .9] # grip + up = Square (Remote 1)
        pads = mapper.update(frame)
        self.assertEqual(self.wire_mask(0, pads[0]), (1 << 0) | (1 << 15))
        frame['hands'][1]['buttons'][1] = False
        frame['hands'][1]['buttons'][0] = False
        frame['hands'][1]['stick'] = [0, 0]
        pads = mapper.update(frame)
        self.assertEqual(self.wire_mask(0, pads[0]), 1 << 11) # secondary fallback R1, not automatic reload
        frame['hands'][1]['buttons'][3] = False
        frame['hands'][1]['buttons'][5] = True
        pads = mapper.update(frame)
        self.assertEqual(self.wire_mask(0, pads[0]), 1 << 10) # solo click swing fallback
        with self.assertRaises(ValueError): Mapper(policy=self.profile(), offscreen='hide')

    def test_left_primary_role_tracking_and_recenter_suppression(self):
        mapper = Mapper(policy=self.profile('left'));mapper.recenter(IDENTITY)
        frame = dict(time=1., hands=[hand(), hand()])
        frame['hands'][0]['buttons'][0] = True
        frame['hands'][1]['buttons'][0] = True
        pads = mapper.update(frame)
        self.assertEqual(self.wire_mask(0, pads[0]), 1 << 9)
        self.assertEqual(self.wire_mask(1, pads[1]), 1 << 8)
        frame['hands'][0]['buttons'][1] = True
        frame['hands'][0]['buttons'][5] = True # grip+click asks for unsupported Home
        pads = mapper.update(frame)
        self.assertEqual(self.wire_mask(0, pads[0]), 0)
        frame['hands'][1]['buttons'][5] = True # both clicks reserve theatre recenter
        frame['hands'][1]['stick'] = [0, 1]
        pads = mapper.update(frame)
        self.assertTrue(mapper.recenter_requested)
        self.assertEqual([self.wire_mask(i, p) for i, p in enumerate(pads)], [0, 0])
        self.assertEqual(pads[1].stick, (0., 0.))
        frame['hands'][0]['valid'] = False
        pads = mapper.update(frame)
        self.assertFalse(mapper.tracked[0])
        self.assertEqual(self.wire_mask(0, pads[0]), 0)
        self.assertTrue(mapper.tracked[1])
        latch = MotionLatch()
        latch.publish(mapper.pads, mapper.tracked, mapper.dynamic_g, 10.)
        served, tracked = latch.consume(10.01)
        self.assertEqual(tracked, [False, True])
        self.assertEqual([self.wire_mask(i, p) for i, p in enumerate(served)], [0, 1 << 8])
        self.assertEqual(served[0].accel, Pad().accel)

    def test_nonfinite_or_malformed_physical_hand_neutral_before_policy(self):
        mapper = Mapper(policy=self.profile());mapper.recenter(IDENTITY)
        frame = dict(time=1., hands=[hand(), hand()])
        frame['hands'][1]['buttons'][0] = True
        self.assertEqual(self.wire_mask(0, mapper.update(frame)[0]), 1 << 9)
        frame['time'] = 1.01
        frame['hands'][1]['stick'][0] = float('nan')
        frame['hands'][1]['buttons'][5] = True
        frame['hands'][0]['buttons'][5] = True
        pads = mapper.update(frame)
        self.assertFalse(mapper.tracked[0])
        self.assertEqual(self.wire_mask(0, pads[0]), 0)
        self.assertFalse(mapper.recenter_requested)
        frame['time'] = 1.02
        frame['hands'][1] = hand()
        frame['hands'][1]['buttons'][0] = 1 # integer is not a digital bool
        self.assertEqual(self.wire_mask(0, mapper.update(frame)[0]), 0)
        self.assertFalse(mapper.tracked[0])


class Protocol(unittest.TestCase):
    def test_wire_layout_crc_and_sensor_units(self):
        pad=Pad(True,(1,-1),(-1,1),(1<<9)|(1<<13),(1.25,-1,0.5),(90,-45,180),123456)
        data=packet(DATA,payload(1,pad,42),77)
        self.assertEqual(len(data),100)
        self.assertEqual(struct.unpack_from('<HH',data,4),(1001,84))
        crc=struct.unpack_from('<I',data,8)[0]
        self.assertEqual(crc,zlib.crc32(data[:8]+bytes(4)+data[12:]))
        self.assertEqual(data[20:24],bytes([1,2,2,1]))
        self.assertEqual(data[40:44],bytes([0,255,255,0]))
        self.assertEqual(data[50],255) # Cross analog
        self.assertEqual(data[54],255) # R2 analog
        self.assertEqual(struct.unpack_from('<Q6f',data,68),(123456,1.25,-1,.5,90,-45,180))

    def test_bad_requests_and_axis_extremes(self):
        good=packet(VERSION,b'',99,b'DSUC')
        self.assertEqual(parse(good),(99,VERSION,b''))
        self.assertIsNone(parse(good[:-1]))
        self.assertIsNone(parse(good[:-1]+b'X'))
        self.assertEqual([axis(v) for v in [-2,-1,0,1,2,float('nan')]],[0,0,128,255,255,128])

    def test_two_live_udp_clients_are_isolated_and_expire(self):
        server=Server(0)
        clients=[socket.socket(socket.AF_INET,socket.SOCK_DGRAM) for _ in range(2)]
        try:
            for i,client in enumerate(clients):
                client.bind(('127.0.0.1',0));client.settimeout(.2)
                client.sendto(packet(DATA,bytes([1,i])+bytes(6),10+i,b'DSUC'),server.socket.getsockname())
            pads=[Pad(True,(-1,0)),Pad(True,(1,0))]
            time.sleep(.02) # Allow the OS to deliver loopback datagrams.
            server.tick(pads,100)
            for i,client in enumerate(clients):
                data=client.recv(1024)
                self.assertEqual(data[20],i)
                self.assertEqual(data[42],i*255)
            server.tick(pads,106)
            self.assertFalse(server.subscriptions)
            for client in clients:
                with self.assertRaises(socket.timeout):client.recv(1024)
            clients[0].sendto(packet(DATA,bytes([2,0])+mac(1),10,b'DSUC'),server.socket.getsockname())
            time.sleep(.02)
            server.tick(pads,107)
            self.assertEqual(clients[0].recv(1024)[20],1)
        finally:
            server.close()
            for client in clients:client.close()

    def test_untracked_slots_stay_discoverable_and_neutral(self):
        server=Server(0)
        client=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        try:
            client.bind(('127.0.0.1',0));client.settimeout(.2)
            client.sendto(packet(PORTS,struct.pack('<i2B',2,0,1),13,b'DSUC'),server.socket.getsockname())
            time.sleep(.02)
            server.tick([Pad(),Pad()],1)
            for slot in range(2):
                data=client.recv(1024)
                self.assertEqual(data[20:24],bytes([slot,2,2,1]))
            client.sendto(packet(DATA,bytes([0,0])+bytes(6),13,b'DSUC'),server.socket.getsockname())
            time.sleep(.02)
            server.tick([Pad(),Pad()],2)
            for slot in range(2):
                data=client.recv(1024)
                self.assertEqual(data[20:24],bytes([slot,2,2,1]))
                self.assertEqual(struct.unpack_from('<H',data,36)[0],0)
                self.assertEqual(struct.unpack_from('<6f',data,76),(0,-1,0,0,0,0))
        finally:
            server.close();client.close()

    def test_transient_survives_newest_packet_drain(self):
        server=Server(0)
        client=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        mapper=Mapper();mapper.recenter(IDENTITY);latch=MotionLatch()
        try:
            client.bind(('127.0.0.1',0));client.settimeout(.2)
            client.sendto(packet(DATA,bytes([1,0])+bytes(6),31,b'DSUC'),server.socket.getsockname())
            time.sleep(.02)
            server.tick([Pad(),Pad()],100)
            client.recv(1024)
            frame=dict(time=1,hands=[hand(),hand()])
            for sample_time,velocity in [(101.0,0),(101.008,.0980665),(101.016,0)]:
                frame['time']=1+sample_time-101
                frame['hands'][1]['velocity'][0]=velocity
                frame['hands'][1]['buttons'][0]=velocity>0
                mapper.update(frame)
                latch.publish(mapper.pads,mapper.tracked,mapper.dynamic_g,sample_time)
            # The UI has not ticked. Dolphin drains both packets and keeps the
            # newest; the 25 ms hold must leave the peak in that newest packet.
            for send_time in (101.017,101.024):
                pads,_=latch.consume(send_time)
                server.tick(pads,send_time)
            client.recv(1024)
            newest=client.recv(1024)
            self.assertLess(struct.unpack_from('<f',newest,76)[0],-1)
            self.assertEqual(struct.unpack_from('<H',newest,36)[0],0)
            # The transient expires; it cannot turn into a sustained input.
            pads,_=latch.consume(101.034)
            server.tick(pads,101.034)
            expired=client.recv(1024)
            self.assertEqual(struct.unpack_from('<f',expired,76)[0],0)
        finally:
            server.close();client.close()


class Tracking(unittest.TestCase):
    def test_shared_theatre_calibration_rejects_bad_geometry(self):
        mapper=Mapper()
        record=dict(schema=1,head_valid=True,width=3.2,height=1.8,distance=2.,head=IDENTITY[:])
        self.assertTrue(mapper.theatre_calibration(record))
        center=mapper.center
        record['width']=4
        self.assertFalse(mapper.theatre_calibration(record))
        record['width']=3.2;record['head'][0]=float('nan')
        self.assertFalse(mapper.theatre_calibration(record))
        self.assertEqual(mapper.center,center)

    def test_world_fixed_plane_and_hand_isolation(self):
        mapper=Mapper();mapper.recenter(IDENTITY)
        frame=dict(time=1,hands=[hand(),hand()])
        frame['hands'][1]['pose'][3]=1.6
        frame['hands'][1]['buttons'][0]=True
        pads=mapper.update(frame)
        self.assertAlmostEqual(pads[0].pointer[0],1)
        self.assertAlmostEqual(pads[1].pointer[0],0)
        self.assertTrue(pads[0].buttons&(1<<9))
        self.assertEqual(pads[0].accel,(-0.,-1.,-0.))
        frame['hands'][1]['pose'][3]=2
        self.assertTrue(mapper.update(frame)[0].buttons&(1<<9))
        self.assertEqual(mapper.pads[0].pointer,(1,0))
        self.assertFalse(mapper.on_plane[0])
        frame['hands'][1]['valid']=False
        self.assertTrue(mapper.update(frame)[0].connected)
        self.assertFalse(mapper.tracked[0])
        self.assertEqual(mapper.pads[0].buttons,0)
        self.assertFalse(mapper.velocity[0])

    def test_acceleration_and_rotation_from_real_derivatives(self):
        mapper=Mapper();mapper.recenter(IDENTITY)
        frame=dict(time=1,hands=[hand(),hand()]);mapper.update(frame)
        frame['time']=1.01
        frame['hands'][1]['velocity'][0]=.0980665
        frame['hands'][1]['omega'][0]=3.141592653589793/2
        p=mapper.update(frame)[0]
        self.assertAlmostEqual(p.accel[0],-1)
        self.assertAlmostEqual(p.gyro[0],90)
        # Delayed samples restart derivatives rather than making a stale spike.
        frame['time']=3
        frame['hands'][1]['velocity'][0]=20
        self.assertAlmostEqual(mapper.update(frame)[0].accel[0],0)

    def test_rotated_pose_axes_follow_dolphin_dsu_control_names(self):
        mapper=Mapper();mapper.recenter(IDENTITY)
        frame=dict(time=1,hands=[hand(),hand()])
        # Right-handed 90 degree roll makes world up point along local right.
        # Dolphin names negative DSU X "Accel Right" and negative roll
        # "Gyro Roll Left" (DualShockUDPClient.cpp:562-577).
        frame['hands'][1]['pose']=[0,-1,0,0, 1,0,0,0, 0,0,1,0]
        frame['hands'][1]['omega']=[0,0,math.pi/2]
        pad=mapper.update(frame)[0]
        self.assertAlmostEqual(pad.accel[0],-1)
        self.assertAlmostEqual(pad.accel[1],0)
        self.assertAlmostEqual(pad.gyro[2],-90)
        # Pitch 90 degrees maps world up onto local back (-Z), which
        # Dolphin calls positive "Accel Forward".
        frame['time']=1.01
        frame['hands'][1]['pose']=[1,0,0,0, 0,0,-1,0, 0,1,0,0]
        frame['hands'][1]['omega']=[math.pi/2,0,0]
        pad=mapper.update(frame)[0]
        self.assertAlmostEqual(pad.accel[2],1)
        self.assertAlmostEqual(pad.gyro[0],90)
        # Yawing right is negative OpenVR Y angular velocity. In a 90-degree
        # yawed hand, world-right acceleration is local-back acceleration.
        frame['time']=1.02
        frame['hands'][1]['pose']=[0,0,1,0, 0,1,0,0, -1,0,0,0]
        frame['hands'][1]['omega']=[0,-math.pi/2,0]
        frame['hands'][1]['velocity']=[.0980665,0,0]
        pad=mapper.update(frame)[0]
        self.assertAlmostEqual(pad.gyro[1],90)
        self.assertLess(pad.accel[2],0)

    def test_identity_forward_and_right_push_signs(self):
        mapper=Mapper();frame=dict(time=1,hands=[hand(),hand()]);mapper.update(frame)
        frame['time']=1.01
        frame['hands'][1]['velocity']=[.0980665,0,0]
        self.assertAlmostEqual(mapper.update(frame)[0].accel[0],-1)  # Accel Right
        frame['time']=1.02
        frame['hands'][1]['velocity']=[.0980665,0,-.0980665]
        self.assertGreater(mapper.update(frame)[0].accel[2],0)  # Accel Forward

    def test_short_window_keeps_peak_between_ui_ticks(self):
        mapper=Mapper();mapper.recenter(IDENTITY)
        frame=dict(time=1,hands=[hand(),hand()]);mapper.update(frame)
        frame['time']=1.008
        frame['hands'][1]['velocity'][0]=.0980665
        mapper.update(frame)
        frame['time']=1.016
        frame['hands'][1]['velocity'][0]=0
        mapper.update(frame)
        self.assertGreaterEqual(mapper.take_peaks()[0][0],1)
        self.assertEqual(mapper.take_peaks()[0][0],0)

    def test_motion_latch_clears_on_tracking_loss_and_stale_sample(self):
        mapper=Mapper();latch=MotionLatch()
        frame=dict(time=1,hands=[hand(),hand()]);mapper.update(frame)
        latch.publish(mapper.pads,mapper.tracked,mapper.dynamic_g,10)
        frame['time']=1.008;frame['hands'][1]['velocity'][0]=.0980665
        mapper.update(frame)
        latch.publish(mapper.pads,mapper.tracked,mapper.dynamic_g,10.008)
        self.assertLess(latch.consume(10.009)[0][0].accel[0],-1)
        frame['time']=1.016;frame['hands'][1]['valid']=False
        mapper.update(frame)
        latch.publish(mapper.pads,mapper.tracked,mapper.dynamic_g,10.016)
        pads,tracked=latch.consume(10.017)
        self.assertFalse(tracked[0])
        self.assertEqual(pads[0],Pad())
        frame['time']=1.024;frame['hands'][1]['valid']=True
        mapper.update(frame)
        latch.publish(mapper.pads,mapper.tracked,mapper.dynamic_g,10.024)
        pads,tracked=latch.consume(10.175)
        self.assertFalse(tracked[0])
        self.assertEqual(pads[0],Pad())

    def test_offscreen_hide_policy_keeps_trigger(self):
        mapper=Mapper(offscreen='hide')
        frame=dict(time=1,hands=[hand(),hand()])
        frame['hands'][1]['pose'][3]=2
        frame['hands'][1]['buttons'][0]=True
        self.assertFalse(mapper.update(frame)[0].buttons&(1<<9))
        mapper.recenter(IDENTITY)
        pad=mapper.update(frame)[0]
        self.assertEqual(pad.pointer,(1,0))
        self.assertTrue(pad.buttons&(1<<9))
        self.assertTrue(pad.buttons&(1<<15))
        frame['hands'][1]['pose'][3]=0
        self.assertFalse(mapper.update(frame)[0].buttons&(1<<15))


if __name__=='__main__':unittest.main()
