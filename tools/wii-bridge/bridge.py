#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Live SteamVR -> DSU bridge, with visible pointer/motion status and calibration."""
import argparse
import ctypes
import dataclasses
import json
import math
import queue
import subprocess
import threading
import time
import tkinter as tk
import sys
from pathlib import Path
from dsu import Pad, Server
from motion import Mapper, MotionLatch
from policy import CanonicalPolicy


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pose-reader', type=Path, required=True)
    route = parser.add_mutually_exclusive_group(required=True)
    route.add_argument('--control-set', help='Exact generated scripted-DSU catalog slug')
    route.add_argument('--legacy-input', action='store_true', help='Explicit historical button mapping')
    parser.add_argument('--primary-hand', choices=('right', 'left'),
                        help='Physical firing hand for the generated scripted-DSU profile')
    parser.add_argument('--port', type=int, default=26760)
    parser.add_argument('--seconds', type=float, default=0)
    parser.add_argument('--head-file', type=Path, help='Shared calibration emitted by the SBS theatre')
    parser.add_argument('--offscreen-pointer', choices=('edge', 'hide'), default='edge',
                        help='Per-game pointer policy: edge clamp, or Dolphin Point/Hide via DSU Square')
    args = parser.parse_args()
    product = json.loads((Path(__file__).resolve().parents[2] / 'data/product.json').read_text(encoding='utf-8'))
    if product.get('schema') != 1 or not isinstance(product.get('display_name'), str):
        raise ValueError('Invalid product identity')
    policy = None
    if args.control_set:
        if args.primary_hand is None:
            parser.error('--primary-hand is required with --control-set')
        policy = CanonicalPolicy.load(Path(__file__).resolve().parents[2], args.control_set, args.primary_hand)
        if (policy.data['server']['name'] != product.get('dsu_server_name') or
                policy.data['server']['port'] != args.port):
            raise ValueError('generated DSU server identity/port mismatch')
    elif args.primary_hand is not None:
        parser.error('--primary-hand applies only to --control-set')
    mapper = Mapper(offscreen=args.offscreen_pointer, policy=policy)
    if policy:
        print(f'Canonical DSU control-set {args.control_set} {args.primary_hand}: '
              f'generated JSON SHA256 {policy.source_hash}', file=sys.stderr, flush=True)
    latch = MotionLatch()
    state_lock = threading.Lock()
    gesture_state = [None]
    server_state = [0, 0]
    service_stop = threading.Event()
    service_ready = threading.Event()
    service_start_error = [None]

    def serve():
        server = None
        try:
            server = Server(args.port)
            service_ready.set()
            next_tick = time.monotonic()
            while not service_stop.is_set():
                now = time.monotonic()
                with state_lock:
                    pads, tracked = latch.consume(now)
                    fallback = gesture_state[0]
                    chord = mapper.recenter_requested
                if fallback and tracked[0] and not chord:
                    kind, start = fallback
                    t = now-start
                    if t <= .45:
                        pad = dataclasses.replace(pads[0])
                        if kind == 'reload':
                            pad.accel = (pad.accel[0]+3*math.sin(t*math.tau*8), *pad.accel[1:])
                        else:
                            pad.gyro = (pad.gyro[0], pad.gyro[1]+350*math.sin(t*math.pi/.45), pad.gyro[2])
                        pads[0] = pad
                server.tick(pads, now)
                with state_lock:
                    server_state[:] = [len(server.subscriptions), server.sent]
                    if fallback and gesture_state[0] is fallback and now-fallback[1] > .45:
                        gesture_state[0] = None
                next_tick += 1/120
                delay = next_tick-time.monotonic()
                if delay < -.05:
                    next_tick = time.monotonic()
                elif delay > 0:
                    service_stop.wait(delay)
        except Exception as exc:
            service_start_error[0] = exc
            print(f'DSU service stopped: {exc}', file=sys.stderr, flush=True)
            service_ready.set()
        finally:
            if server is not None:
                server.close()

    window = tk.Tk()
    service_thread = threading.Thread(target=serve, daemon=True)
    service_thread.start()
    if not service_ready.wait(timeout=2) or service_start_error[0]:
        service_stop.set()
        service_thread.join(timeout=2)
        window.destroy()
        raise RuntimeError(f'DSU service could not start: {service_start_error[0]}')
    window.title(product['display_name'] + ' — Wii controller bridge')
    window.geometry('900x650')
    window.configure(bg='#132030')
    tk.Label(window,text='Wii Remotes · live SteamVR motion',font=('Segoe UI',22),fg='white',bg='#132030').pack(pady=12)
    centre_help = ('Use Ctrl+Alt+C in the theatre to centre the screen and pointer together.'
                   if args.head_file else
                   'Face the centre of your Dolphin screen, then click Centre pointer plane.')
    role_help = (f'{policy.remote_hand.title()} hand = Remote · {policy.stick_hand.title()} hand = Nunchuk'
                 if policy else 'Legacy: right hand = Remote 1 · left hand = Remote 2')
    tk.Label(window,text=centre_help+'\n'+role_help,font=('Segoe UI',12),fg='white',bg='#132030').pack()
    canvas = tk.Canvas(window,width=820,height=360,bg='#0b111a',highlightthickness=0)
    canvas.pack(pady=12)
    for x,y in [(410,180),(30,30),(790,30),(30,330),(790,330)]:
        canvas.create_oval(x-8,y-8,x+8,y+8,outline='#567080',width=2)
    label=tk.Label(window,font=('Consolas',11),fg='#bdebdc',bg='#132030',justify='left')
    label.pack()
    errors=queue.Queue()
    try:
        process=subprocess.Popen([str(args.pose_reader.resolve()),str(Path(__file__).with_name('actions.json').resolve())],
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
    except OSError:
        service_stop.set()
        service_thread.join(timeout=2)
        window.destroy()
        raise

    frame=None
    last_sample=0
    def read():
        nonlocal frame,last_sample
        for line in process.stdout:
            try:
                sample=json.loads(line)
            except (ValueError, TypeError):
                continue
            try:
                with state_lock:
                    mapper.update(sample)
                    sample_time=time.monotonic()
                    latch.publish(mapper.pads, mapper.tracked, mapper.dynamic_g, sample_time)
                    frame=sample
                    last_sample=sample_time
            except (KeyError, IndexError, TypeError, ValueError):
                continue
        errors.put(process.stderr.read().strip() or 'Tracking reader stopped')
    reader_thread=threading.Thread(target=read,daemon=True)
    reader_thread.start()
    started=time.monotonic()
    reader_error=''
    calibration_key=None
    last_peak_log=started
    def center():
        if args.head_file:
            return
        with state_lock:
            if frame and frame['head_valid']:
                mapper.recenter(frame['head'])
    tk.Button(window,text='Use Ctrl+Alt+C in theatre to centre' if args.head_file else 'Centre pointer plane',command=center,font=('Segoe UI',13),state='disabled' if args.head_file else 'normal').pack(pady=5)
    # Temporary app-owned hotkeys keep accessibility motions usable while the
    # game has focus. No hooks, persistent settings, or global key recording.
    hotkeys=[]
    hotkey_events=queue.Queue()
    hotkey_ready=threading.Event()
    hotkey_thread=None
    hotkey_thread_id=None
    user32=None
    if sys.platform=='win32':
        from ctypes import wintypes
        user32=ctypes.windll.user32
        def pump_hotkeys():
            nonlocal hotkey_thread_id
            hotkey_thread_id=ctypes.windll.kernel32.GetCurrentThreadId()
            message=wintypes.MSG()
            # A dedicated message loop keeps Tk from consuming WM_HOTKEY.
            user32.PeekMessageW(ctypes.byref(message),None,0,0,0)
            for ident,key in [(0xAC01,'R'),(0xAC02,'V')]:
                if user32.RegisterHotKey(None,ident,0x4003,ord(key)):
                    hotkeys.append(ident)
            hotkey_ready.set()
            while user32.GetMessageW(ctypes.byref(message),None,0,0)>0:
                if message.message==0x312 and message.wParam in hotkeys:
                    hotkey_events.put('reload' if message.wParam==0xAC01 else 'melee')
            for ident in hotkeys:
                user32.UnregisterHotKey(None,ident)
        hotkey_thread=threading.Thread(target=pump_hotkeys,daemon=True)
        hotkey_thread.start()
        hotkey_ready.wait(timeout=2)
    tk.Label(window,text='Player 1 only: Ctrl+Alt+R = reload · Ctrl+Alt+V = swing\nR/V also work in this window · Player 2 uses physical motion · Close stops bridge',fg='white',bg='#132030').pack()
    def gesture(kind):
        with state_lock:
            gesture_state[0]=(kind,time.monotonic())
    window.bind('<r>',lambda e:gesture('reload'))
    window.bind('<v>',lambda e:gesture('melee'))
    alive=True
    def close():
        nonlocal alive
        alive=False
        if user32 and hotkey_thread_id:
            user32.PostThreadMessageW(hotkey_thread_id,0x12,0,0)
            hotkey_thread.join(timeout=2)
        if process.poll() is None:
            process.terminate()
        try: process.wait(timeout=3)
        except subprocess.TimeoutExpired: process.kill(); process.wait()
        reader_thread.join(timeout=2)
        service_stop.set()
        service_thread.join(timeout=2)
        if service_thread.is_alive():
            print('DSU service did not stop within 2 seconds',file=sys.stderr,flush=True)
        window.destroy()
    window.protocol('WM_DELETE_WINDOW',close)
    def tick():
        nonlocal reader_error,calibration_key,last_peak_log
        now=time.monotonic()
        if args.head_file:
            try:
                record=json.loads(args.head_file.read_text(encoding='utf-8'))
                key=(record.get('revision'), tuple(record.get('head', [])))
                if key != calibration_key:
                    with state_lock:
                        if mapper.theatre_calibration(record):
                            calibration_key=key
            except (OSError, ValueError, TypeError):
                pass
        while True:
            try: gesture(hotkey_events.get_nowait())
            except queue.Empty: break
        with state_lock:
            fresh=now-last_sample < .15
            pads=mapper.pads[:] if fresh else [Pad(),Pad()]
            tracked=mapper.tracked[:] if fresh else [False,False]
            on_plane=mapper.on_plane[:] if fresh else [False,False]
            centred=bool(mapper.center)
            subscriptions,sent=server_state
            chord=mapper.recenter_requested if fresh else False
            peaks=mapper.take_peaks() if now-last_peak_log >= 1 else None
        if peaks is not None:
            print('motion peak (1 s): '+', '.join(
                f'{("Remote" if i == 0 else "Nunchuk") if policy else "remote " + str(i+1)} accel={peak:.2f} g speed={speed:.2f} m/s'
                for i,(peak,speed) in enumerate(peaks)),file=sys.stderr,flush=True)
            last_peak_log=now
        canvas.delete('hands')
        lines=[]
        for i,p in enumerate(pads):
            x,y=410+p.pointer[0]*400,180-p.pointer[1]*170
            color=('#ffcf5b','#72cfff')[i]
            if tracked[i]:
                canvas.create_oval(x-10,y-10,x+10,y+10,fill=color,tags='hands')
                canvas.create_text(x+16,y,text=str(i+1),fill=color,tags='hands')
            role = ('Remote' if i == 0 else 'Nunchuk') if policy else f'Remote {i+1}'
            lines.append(f'{role}: {"TRACKED" if tracked[i] else "not tracked":11}  {"on" if on_plane[i] else "off"}-screen  pointer {p.pointer[0]:+.2f},{p.pointer[1]:+.2f}  accel '+','.join(f'{v:+.2f}' for v in p.accel)+' g')
        lines.append(f'DSU 127.0.0.1:{args.port} · {subscriptions} subscriptions · {sent} packets · plane '+('centred' if centred else 'needs centring'))
        if policy:
            lines.append(f'Canonical {args.control_set} · {args.primary_hand} · source {policy.source_hash[:12]} · theatre chord unimplemented')
        if policy and chord:
            lines.append('Both clicks reserved; use theatre Ctrl+Alt+C to recenter screen and pointer together.')
        if not errors.empty():
            reader_error=errors.get_nowait()
        if reader_error:
            lines.append(reader_error)
        if service_start_error[0]:
            lines.append(f'DSU service stopped: {service_start_error[0]}')
        if user32 and len(hotkeys)!=2:
            lines.append('Some global hotkeys unavailable; R/V still work while this window has focus.')
        label.config(text='\n'.join(lines))
        if args.seconds and now-started>=args.seconds:
            close();return
        if alive:window.after(8,tick)
    tick()
    window.mainloop()


if __name__=='__main__':
    main()
