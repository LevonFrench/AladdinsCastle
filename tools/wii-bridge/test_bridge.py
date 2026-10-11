# SPDX-License-Identifier: GPL-3.0-only
import copy
import math
import socket
import struct
import time
import unittest
import zlib
from dsu import DATA, PORTS, VERSION, Pad, Server, axis, mac, packet, parse, payload
from motion import Mapper, MotionLatch

IDENTITY = [1,0,0,0, 0,1,0,0, 0,0,1,0]


def hand():
    return dict(valid=True,pose=IDENTITY[:],velocity=[0,0,0],omega=[0,0,0],stick=[0,0],buttons=[False]*6)


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
