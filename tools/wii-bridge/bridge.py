#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Live SteamVR -> DSU bridge, with visible pointer/motion status and calibration."""
import argparse
import ctypes
import json
import queue
import subprocess
import threading
import time
import tkinter as tk
import sys
from pathlib import Path
from dsu import Pad, Server
from motion import Mapper


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pose-reader', type=Path, required=True)
    parser.add_argument('--port', type=int, default=26760)
    parser.add_argument('--seconds', type=float, default=0)
    args = parser.parse_args()
    server = Server(args.port)
    mapper = Mapper()
    window = tk.Tk()
    window.title('AladdinsCastle — Wii controller bridge')
    window.geometry('900x650')
    window.configure(bg='#132030')
    tk.Label(window,text='Wii Remotes · live SteamVR motion',font=('Segoe UI',22),fg='white',bg='#132030').pack(pady=12)
    tk.Label(window,text='Face the centre of your Dolphin screen, then click Centre pointer plane.\nRight hand = Remote 1 · Left hand = Remote 2 / Nunchuk',font=('Segoe UI',12),fg='white',bg='#132030').pack()
    canvas = tk.Canvas(window,width=820,height=360,bg='#0b111a',highlightthickness=0)
    canvas.pack(pady=12)
    for x,y in [(410,180),(30,30),(790,30),(30,330),(790,330)]:
        canvas.create_oval(x-8,y-8,x+8,y+8,outline='#567080',width=2)
    label=tk.Label(window,font=('Consolas',11),fg='#bdebdc',bg='#132030',justify='left')
    label.pack()
    samples=queue.Queue(maxsize=1)
    errors=queue.Queue()
    process=subprocess.Popen([str(args.pose_reader.resolve()),str(Path(__file__).with_name('actions.json').resolve())],
        stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)

    def read():
        for line in process.stdout:
            try:
                sample=json.loads(line)
            except (ValueError, TypeError):
                continue
            try: samples.get_nowait()
            except queue.Empty: pass
            samples.put(sample)
        errors.put(process.stderr.read().strip() or 'Tracking reader stopped')
    threading.Thread(target=read,daemon=True).start()
    frame=None
    last_sample=0
    started=time.monotonic()
    fallback=None
    reader_error=''
    def center():
        if frame and frame['head_valid']:
            mapper.recenter(frame['head'])
    tk.Button(window,text='Centre pointer plane',command=center,font=('Segoe UI',13)).pack(pady=5)
    # Temporary app-owned hotkeys keep accessibility motions usable while the
    # game has focus. No hooks, persistent settings, or global key recording.
    hotkeys=[]
    user32=None
    if sys.platform=='win32':
        from ctypes import wintypes
        user32=ctypes.windll.user32
        for ident,key in [(0xAC01,'R'),(0xAC02,'V')]:
            if user32.RegisterHotKey(None,ident,0x4003,ord(key)):
                hotkeys.append(ident)
        hotkey_message=wintypes.MSG()
    tk.Label(window,text='Ctrl+Alt+R = reload · Ctrl+Alt+V = melee · R/V also work in this window · Close stops bridge',fg='white',bg='#132030').pack()
    def gesture(kind):
        nonlocal fallback
        fallback=(kind,time.monotonic())
    window.bind('<r>',lambda e:gesture('reload'))
    window.bind('<v>',lambda e:gesture('melee'))
    alive=True
    def close():
        nonlocal alive
        alive=False
        if user32:
            for ident in hotkeys:user32.UnregisterHotKey(None,ident)
        process.terminate()
        try: process.wait(timeout=3)
        except subprocess.TimeoutExpired: process.kill(); process.wait()
        server.close()
        window.destroy()
    window.protocol('WM_DELETE_WINDOW',close)
    def tick():
        nonlocal frame,last_sample,fallback,reader_error
        now=time.monotonic()
        if user32:
            while user32.PeekMessageW(ctypes.byref(hotkey_message),None,0x312,0x312,1):
                if hotkey_message.wParam in hotkeys:
                    gesture('reload' if hotkey_message.wParam==0xAC01 else 'melee')
        try:
            frame=samples.get_nowait()
            last_sample=now
            mapper.update(frame)
        except queue.Empty: pass
        pads=mapper.pads if now-last_sample < .15 else [Pad(),Pad()]
        if fallback and pads[0].connected:
            import dataclasses,math
            kind,start=fallback
            t=now-start
            if t>.45: fallback=None
            else:
                # Explicit accessibility motion on same sensor path; never add
                # a second Dolphin Shake/Swing binding. Owner tuning is pending.
                p=dataclasses.replace(pads[0])
                if kind=='reload': p.accel=(p.accel[0]+3*math.sin(t*math.tau*8),*p.accel[1:])
                else: p.gyro=(p.gyro[0],p.gyro[1]+350*math.sin(t*math.pi/.45),p.gyro[2])
                pads=[p,pads[1]]
        server.tick(pads,now)
        canvas.delete('hands')
        lines=[]
        for i,p in enumerate(pads):
            x,y=410+p.pointer[0]*400,180-p.pointer[1]*170
            color=('#ffcf5b','#72cfff')[i]
            if p.connected:
                canvas.create_oval(x-10,y-10,x+10,y+10,fill=color,tags='hands')
                canvas.create_text(x+16,y,text=str(i+1),fill=color,tags='hands')
            lines.append(f'Remote {i+1}: {"TRACKED" if p.connected else "not tracked":11}  pointer {p.pointer[0]:+.2f},{p.pointer[1]:+.2f}  accel '+','.join(f'{v:+.2f}' for v in p.accel)+' g')
        lines.append(f'DSU 127.0.0.1:{args.port} · {len(server.subscriptions)} subscriptions · {server.sent} packets · plane '+('centred' if mapper.center else 'needs centring'))
        if not errors.empty():
            reader_error=errors.get_nowait()
        if reader_error:
            lines.append(reader_error)
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
