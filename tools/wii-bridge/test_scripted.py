# SPDX-License-Identifier: GPL-3.0-only
import copy
import io
import os
import json
import socket
import struct
import threading
import time
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from dsu import DATA, packet
from scripted import Timeline, run, fresh_receipt


def fixture():
    return dict(version=1,seconds=.3,steps=[dict(at=0,pads=[{'buttons':['Cross'],'pointer':[.25,-.5]},{}]),
        dict(at=.15,pads=[{}, {'buttons':['L2']}])])


class Scripted(unittest.TestCase):
    def test_bound_ready_identifies_current_producer_and_input(self):
        class Idle:
            sent=0
            subscriptions={}
            socket=type('Socket',(),{'getsockname':lambda self:('127.0.0.1',12345)})()
            def tick(self,*args):pass
            def close(self):pass
        output=io.StringIO(); receipt=io.StringIO()
        with patch('scripted.Server',return_value=Idle()), patch('scripted.time.sleep'), patch('sys.stdout',output):
            run(Timeline(fixture()),12345,0,receipt,'a'*64,'b'*32)
        ready=json.loads(output.getvalue().splitlines()[0]); result=json.loads(receipt.getvalue())
        for data in (ready,result):
            self.assertEqual(data['producer_pid'],os.getpid())
            self.assertEqual(data['script_sha256'],'a'*64)
            self.assertEqual(data['session_token'],'b'*32)
        self.assertTrue(ready['ready']); self.assertEqual(ready['port'],12345)

    def test_invalid_session_identity_fails_before_socket(self):
        with patch('scripted.Server') as server:
            for token,digest in [('x'*32,'a'*64),('a'*31,'b'*64),('a'*32,None),('a'*32,'bad')]:
                with self.subTest(token=token,digest=digest),self.assertRaises(ValueError):
                    run(Timeline(fixture()),12345,0,io.StringIO(),digest,token)
            server.assert_not_called()

    def test_traversal_refused_and_existing_receipt_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            with self.assertRaises(ValueError):fresh_receipt(root/'.local'/ '..'/'public.json')
            self.assertFalse((root/'public.json').exists())
            target=root/'.local'/'private.json'
            with fresh_receipt(target) as output:output.write('sentinel')
            with self.assertRaises(FileExistsError):fresh_receipt(target)
            self.assertEqual(target.read_text(),'sentinel')

    def test_transport_failure_still_closes_and_writes_receipt(self):
        class Broken:
            sent=0
            subscriptions={}
            closed=False
            socket=type('Socket',(),{'getsockname':lambda self:('127.0.0.1',12345)})()
            def tick(self,*args):raise OSError('synthetic failure')
            def close(self):self.closed=True
        server=Broken();receipt=io.StringIO()
        with patch('scripted.Server',return_value=server),self.assertRaises(OSError):
            run(Timeline(fixture()),12345,1,receipt)
        self.assertTrue(server.closed)
        record=json.loads(receipt.getvalue())
        self.assertEqual(record['outcome'],'error');self.assertFalse(record['neutral_release'])

    def test_complete_states_boundaries_and_no_mutation(self):
        timeline=Timeline(fixture())
        self.assertEqual(timeline.sample(-1,1)[0].buttons,0)
        self.assertEqual(timeline.sample(0,1)[0].buttons,1<<14)
        self.assertEqual(timeline.sample(.15,2)[0].buttons,0)
        self.assertEqual(timeline.sample(.15,2)[1].buttons,1<<8)
        sample=timeline.sample(.15,3);sample[1].buttons=123
        self.assertEqual(timeline.sample(.15,4)[1].buttons,1<<8)
        self.assertEqual(timeline.sample(.3,5)[1].buttons,0)

    def test_invalid_scripts_fail_before_socket(self):
        cases=[]
        for value in (float('nan'),float('inf'),-1,1801,True):
            d=fixture();d['seconds']=value;cases.append(d)
        d=fixture();d['steps'][1]['at']=0;cases.append(d)
        d=fixture();d['steps'][0]['pads'][0]['buttons']=['Typo'];cases.append(d)
        d=fixture();d['steps'][0]['pads'][0]['pointer']=[2,0];cases.append(d)
        d=fixture();d['steps'][0]['pads'][0]['keyboard']='X';cases.append(d)
        for d in cases:
            with self.subTest(d=d),self.assertRaises(ValueError):Timeline(d)

    def test_real_udp_buttons_release_and_receipt(self):
        # Reserve an ephemeral loopback port for this synthetic-only service.
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
            reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
        receipt=io.StringIO();results=[]
        worker=threading.Thread(target=lambda:results.append(run(Timeline(fixture()),port,2,receipt)))
        worker.start()
        observed=[set(),set()]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as client:
            client.bind(('127.0.0.1',0));client.settimeout(.05)
            deadline=time.monotonic()+3
            while worker.is_alive() and time.monotonic()<deadline:
                client.sendto(packet(DATA,bytes(8),42,b'DSUC'),('127.0.0.1',port))
                try:
                    data=client.recv(1024)
                    observed[data[20]].add(struct.unpack_from('<H',data,36)[0])
                except socket.timeout:pass
        worker.join(3)
        self.assertFalse(worker.is_alive());self.assertEqual(results,[0])
        self.assertTrue({0,1<<14}<=observed[0]);self.assertTrue({0,1<<8}<=observed[1])
        record=json.loads(receipt.getvalue())
        self.assertEqual(record['outcome'],'completed');self.assertGreater(record['sent'],0)


if __name__=='__main__':unittest.main()
