"""Bounded, silent Windows runner for a hash-pinned synthetic stereo probe.

All outputs belong in a private lane. This never invokes the live launcher or XR.
The caller supplies an already built probe and a private launch manifest/plan.
"""
import argparse
import hashlib
import json
import math
import os
import re
from pathlib import Path
import shutil
import subprocess
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def below(path, root):
    return Path(path).resolve().is_relative_to(Path(root).resolve())


def observation_time(value):
    value = float(value)
    if not math.isfinite(value) or value < 5:
        raise argparse.ArgumentTypeError('--observe-until must be finite and at least 5 emulated seconds')
    return value


def copy_settings(live, seed, destination):
    """Copy graphics/input configuration and save data, never NAND title content.

    Dolphin.ini is generated instead of copied: account/network configuration is
    irrelevant to an offline measurement. Only existing per-title data folders
    and the Mii database are copied from the explicitly selected test seed.
    """
    destination.mkdir(parents=True, exist_ok=False)
    (destination / 'Config').mkdir()
    (destination / 'Logs').mkdir()
    for name in ('GFX.ini', 'WiimoteNew.ini', 'GCPadNew.ini'):
        src = live / 'Config' / name
        if src.is_file():
            shutil.copy2(src, destination / 'Config' / name)
    for name in ('GameSettings', 'GameSettingsVR', 'Config/Profiles'):
        src = live / name
        if src.is_dir():
            shutil.copytree(src, destination / name)
    for data in (seed / 'Wii/title').glob('*/*/data'):
        if data.is_dir():
            shutil.copytree(data, destination / data.relative_to(seed))
    mii = seed / 'Wii/shared2/menu/FaceLib/RFL_DB.dat'
    if mii.is_file():
        target = destination / mii.relative_to(seed)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(mii, target)
    (destination / 'Config/Dolphin.ini').write_text(
        '[Core]\nGFXBackend = D3D\n[Analytics]\nEnabled = False\nPermissionAsked = True\n'
        '[AutoUpdate]\nUpdateTrack = \n[General]\nISOPaths = 0\n'
        '[Interface]\nUsePanicHandlers = False\nConfirmStop = False\n'
        'OnScreenDisplayMessages = False\n', encoding='utf-8')
    (destination / 'Config/Logger.ini').write_text(
        '[Options]\nWriteToFile = True\nWriteToConsole = False\nWriteToWindow = False\n'
        'Verbosity = 3\n[Logs]\nVIDEO = True\n', encoding='utf-8')


def run(args):
    if os.name != 'nt':
        raise ValueError('This runner requires Windows hidden-process support.')
    observe_until = getattr(args, 'observe_until', None)
    live_input = getattr(args, 'live_input', False)
    if live_input and observe_until is None:
        raise ValueError('--live-input requires --observe-until; navigation is not a timing pass.')
    if observe_until is not None:
        try:
            observe_until = observation_time(observe_until)
        except (ValueError, argparse.ArgumentTypeError) as error:
            raise ValueError(str(error)) from error
    marker = Path(args.marker).resolve()
    if marker.exists():
        raise RuntimeError(f'Headset marker present: {marker}')
    exe = Path(args.exe).resolve(strict=True)
    if digest(exe).lower() != args.exe_sha256.lower():
        raise ValueError('Probe executable hash does not match the reviewed build.')
    root = Path(args.out).resolve()
    if '.local' not in root.parts:
        raise ValueError('Private output must be inside a .local directory.')
    manifest_path = Path(args.manifest).resolve(strict=True)
    manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    plan = json.loads(Path(args.plan).read_text(encoding='utf-8-sig'))
    if isinstance(plan, dict) and 'games' in plan:
        plan = plan['games']
    if isinstance(plan, list):
        plan = {p['game_key']: p for p in plan}
    item = plan[args.game]
    game = manifest[args.game]
    view_id = args.view or next(iter(game['views']))
    view = game['views'][view_id]
    live = Path(view['user']).resolve(strict=True)
    seed = Path(item['seed_user']).resolve(strict=True)
    disc = Path(view['disc']).resolve(strict=True)
    original_script = Path(item['script']).resolve(strict=True)
    if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.tag):
        raise ValueError('Run tag must contain only letters, numbers, underscore or hyphen.')
    run_dir = root / 'runs' / f'{args.game}-{args.rate}-{args.tag}'
    if not below(run_dir, root):
        raise ValueError('Run output escapes private root.')
    if run_dir.exists():
        raise ValueError(f'Refusing to reuse an existing run: {run_dir}')
    if below(run_dir, live) or below(run_dir, seed) or below(run_dir, disc.parent):
        raise ValueError('Run output overlaps live settings, seed, or game storage.')
    run_dir.mkdir(parents=True)
    user = run_dir / 'User'
    copy_settings(live, seed, user)
    if view.get('gameIni'):
        ini = view['gameIni']
        source = Path(ini['path']).resolve()
        if not below(source, live / 'GameSettings'):
            raise ValueError('View gameIni is outside its declared GameSettings.')
        target = user / source.relative_to(live)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('\n'.join(ini['content']) + '\n', encoding='utf-8')
    start = args.start if args.start is not None else item.get('gameplay_start_s')
    if start is None:
        start = max(0.0, float(item.get('max_emulated_s', 120)) - 60)
    end = start + 80  # enough emulated time to cover a 60s wall interval at normal speed
    script_text = original_script.read_text(encoding='utf-8-sig')
    # Preserve input events; replace old captures with bounded, named checkpoints.
    lines = [line for line in script_text.splitlines() if not line.strip().startswith('shot ')]
    if args.input_until is not None:
        clipped = []
        for line in lines:
            if not line.strip() or line.lstrip().startswith('#'):
                clipped.append(line)
                continue
            values = line.split()
            begin, finish = float(values[0]), float(values[1])
            if begin < args.input_until:
                values[1] = str(min(finish, args.input_until))
                clipped.append(' '.join(values))
        lines = clipped
    shots = set(float(t) for t in item.get('capture_times_s', [8, 20, 40, 60]))
    original_shots = [float(line.split()[1]) for line in script_text.splitlines()
                      if line.strip().startswith('shot ')]
    # Retain a bounded sample of the source route's captures as well as our
    # measurement checkpoints; visual stage attribution is a separate review.
    if original_shots:
        shots.update(original_shots[::max(1, len(original_shots) // 12)])
        shots.add(original_shots[-1])
    shots.update([start, start + 30, start + 65, end])
    if args.capture_policy == 'before-window':
        # PNG readback/compression can interrupt rendering. Keep a gameplay
        # checkpoint ahead of the timing anchor and leave a settling interval.
        # The caller must still review that checkpoint to establish its stage.
        shots = {t for t in shots if 0 <= t <= start - 10}
        if start >= 10:
            shots.add(start - 10)
    if observe_until is not None:
        # Observation runs need a final state capture, then at least five
        # emulated seconds for readback before the owned-process stop.
        last_capture = observe_until - 5
        shots = {t for t in shots if 0 <= t <= last_capture}
        if args.capture_policy == 'sampled' or last_capture <= start - 10:
            shots.add(last_capture)
    for t in sorted(shots):
        lines.append(f'shot {t:.3f} w0-{t:07.2f}')
    script = run_dir / 'input.txt'
    script.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    # Override live settings only inside the child process. Synthetic rendering
    # needs EnableOpenXR=True, but D3DMain's probe guard never calls the loader.
    config = list(view.get('config', []))
    forced = {
        'Dolphin.Core.GFXBackend': 'D3D',
        'Graphics.VR.EnableOpenXR': 'True',
        'Dolphin.Interface.UsePanicHandlers': 'False',
        'Dolphin.Interface.OnScreenDisplayMessages': 'False',
        'Dolphin.Interface.ConfirmStop': 'False',
        'Dolphin.DSP.Backend': 'No Audio Output',
        'Dolphin.DSP.Volume': '0',
        'Dolphin.DSP.Muted': 'True',
        'Graphics.VR.AutoVBIFromHMD': 'False',
        'Dolphin.Core.VIOverclockEnable': 'False',
    }
    # Auto/headset mode has no refresh source offline. Each experiment declares
    # its VI override explicitly; this changes emulation cadence, not XR delivery.
    forced['Graphics.VR.ForcedVBIFrequency'] = '0' if args.rate == 'native' else args.rate
    for option in args.config:
        key, value = option.split('=', 1)
        forced[key] = value
    required = {'Graphics.VR.EnableOpenXR': 'True', 'Dolphin.Core.GFXBackend': 'D3D',
                'Dolphin.Interface.UsePanicHandlers': 'False',
                'Dolphin.Interface.OnScreenDisplayMessages': 'False',
                'Dolphin.Interface.ConfirmStop': 'False',
                'Dolphin.DSP.Backend': 'No Audio Output', 'Dolphin.DSP.Volume': '0',
                'Dolphin.DSP.Muted': 'True'}
    if any(forced.get(k) != v for k, v in required.items()):
        raise ValueError('A config override would weaken the offline guards.')
    if (forced['Graphics.VR.ForcedVBIFrequency'] != ('0' if args.rate == 'native' else args.rate)
            or forced['Graphics.VR.AutoVBIFromHMD'] != 'False'
            or forced['Dolphin.Core.VIOverclockEnable'] != 'False'):
        raise ValueError('A config override would change the declared cadence.')
    config = [c for c in config if c.split('=', 1)[0] not in forced]
    config += [f'{k}={v}' for k, v in forced.items()]
    argv = [str(exe), '-p', 'headless', '-u', str(user)]
    for c in config:
        argv.extend(['-C', c])
    argv.extend(['-e', str(disc)])
    env = os.environ.copy()
    env.pop('DOLPHINXR_GUN', None)
    # Never inherit a trace/capture path or unrelated probe option.
    for key in list(env):
        if key.startswith(('AC_STEREO_', 'DOLPHINXR_INPUT_')):
            env.pop(key)
    missing_runtime = run_dir / 'NO-OPENXR-RUNTIME.json'
    if missing_runtime.exists():
        raise ValueError('Missing-runtime sentinel unexpectedly exists.')
    env.update(AC_STEREO_PROBE='1', AC_STEREO_PROBE_YAW=str(args.yaw),
               AC_STEREO_METRICS_CSV=str(run_dir / 'frames.csv'),
               XR_RUNTIME_JSON=str(missing_runtime),
               DOLPHINXR_INPUT_SCRIPT=str(script),
               DOLPHINXR_INPUT_PROBE=str(run_dir / 'probe.log'))
    inbox = run_dir / 'live-input.txt'
    stop_file = run_dir / 'STOP'
    if live_input:
        inbox.write_text('# Append future timed input or screenshot commands only.\n', encoding='utf-8')
        env['DOLPHINXR_INPUT_APPEND'] = str(inbox)
    receipt = {'game_key': args.game, 'view': view_id, 'rate': args.rate,
               'phase': args.phase, 'stage': 'unverified', 'argv': argv,
               'exe': str(exe), 'exe_sha256': digest(exe),
               'manifest': str(manifest_path), 'manifest_sha256': digest(manifest_path),
               'script_source': str(original_script), 'script_sha256': digest(script),
               'input_until_emulated_s': args.input_until,
               'seed_user': str(seed), 'settings_source': str(live),
               'window_start_emulated_s': start, 'marker': str(marker),
               'observe_until_emulated_s': observe_until,
               'live_input_path': str(inbox) if live_input else None,
               'operator_stop_path': str(stop_file) if live_input else None,
               'capture_policy': args.capture_policy,
               'scheduled_capture_emulated_seconds': sorted(shots),
               'capture_overhead_note': (
                   'Observation only; scheduled captures stop at least 5 emulated seconds '
                   'before observation end. Asynchronous PNG write completion is not instrumented.'
                   if observe_until is not None else
                   'PNG captures may interrupt the measured interval.'
                   if args.capture_policy == 'sampled' else
                   'Captures stop at least 10 emulated seconds before the timing anchor; '
                   'completion of asynchronous PNG writes is not instrumented.'),
               'evidence_level': 'unverified run record', 'real_compositor': 'not measured: no XR runtime',
               'guard_config': forced, 'run_dir': str(run_dir)}
    (run_dir / 'run.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    if marker.exists():
        raise RuntimeError('Headset marker appeared during preparation; no run started.')
    start_wall = time.monotonic()
    reason = 'process_exit'
    window_wall_origin = None
    with (run_dir / 'stdout.txt').open('wb') as stdout, (run_dir / 'stderr.txt').open('wb') as stderr:
        child = subprocess.Popen(argv, cwd=exe.parent, env=env, stdout=stdout, stderr=stderr,
                                 creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS)
        receipt['pid'] = child.pid
        (run_dir / 'run.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
        try:
            while child.poll() is None:
                if marker.exists():
                    reason = 'headset_marker'; break
                if live_input and stop_file.exists():
                    reason = 'operator_stop'; break
                if time.monotonic() - start_wall >= args.seconds:
                    reason = 'wall_deadline'; break
                trace = run_dir / 'frames.csv'
                if trace.is_file():
                    with trace.open('rb') as f:
                        header = f.readline().decode('utf-8').strip().split(',')
                        f.seek(max(f.tell(), trace.stat().st_size - 2048))
                        raw_rows = f.read().decode('utf-8', errors='replace').splitlines()
                    for line in reversed(raw_rows):
                        fields = line.split(',')
                        if len(fields) != len(header):
                            continue
                        row = dict(zip(header, fields))
                        try:
                            emulated = int(row['emulated_ticks']) / int(row['ticks_per_second'])
                            wall = int(row['wall_ns']) / 1e9
                        except (KeyError, ValueError, ZeroDivisionError):
                            continue
                        if observe_until is not None:
                            if emulated >= observe_until:
                                reason = 'observation_end'
                        else:
                            if emulated >= start and window_wall_origin is None:
                                window_wall_origin = wall
                            if window_wall_origin is not None and wall - window_wall_origin >= 65:
                                reason = 'complete_wall_window'
                        break
                    if reason in ('complete_wall_window', 'observation_end'):
                        # Let an in-flight screenshot finish before the bounded
                        # owned-process stop. Observation mode claims no window.
                        time.sleep(3)
                        break
                log = user / 'Logs/dolphin.log'
                if time.monotonic() - start_wall > 30:
                    guard_text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
                    if 'AC stereo probe: OpenXR not initialized; synthetic eyes.' not in guard_text:
                        reason = 'missing_synthetic_guard'; break
                    if live_input:
                        probe_log = run_dir / 'probe.log'
                        probe_text = probe_log.read_text(encoding='utf-8', errors='replace') if probe_log.is_file() else ''
                        if 'AC_INPUT_APPEND ready' not in probe_text:
                            reason = 'missing_live_input_guard'; break
                time.sleep(0.5)
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=20)
    receipt.update(exit_code=child.returncode, stop_reason=reason,
                   wall_duration_s=time.monotonic() - start_wall,
                   finished_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()))
    log = user / 'Logs/dolphin.log'
    text = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
    receipt['synthetic_guard_logged'] = 'AC stereo probe: OpenXR not initialized; synthetic eyes.' in text
    receipt['fifo_error_mentions'] = text.count('Unknown Opcode')
    receipt['screenshots'] = [str(p) for p in (user / 'ScreenShots').rglob('*.png')]
    receipt['measurement_window_complete'] = observe_until is None and reason == 'complete_wall_window'
    receipt['accepted_as_measurement'] = False  # analysis and stage review are required
    if live_input:
        receipt['live_input_sha256'] = digest(inbox)
    (run_dir / 'run.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    print(json.dumps({k: receipt[k] for k in ('game_key', 'rate', 'stop_reason', 'wall_duration_s', 'synthetic_guard_logged', 'run_dir')}), flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for key in ('exe', 'exe-sha256', 'manifest', 'plan', 'out', 'game', 'marker'):
        ap.add_argument('--' + key, required=True)
    ap.add_argument('--view')
    ap.add_argument('--rate', choices=('native', '72', '90'), default='native')
    ap.add_argument('--tag', default='baseline')
    ap.add_argument('--phase', choices=('baseline', 'before', 'after'), default='baseline')
    ap.add_argument('--capture-policy', choices=('sampled', 'before-window'), default='sampled',
                    help='before-window keeps scheduled PNG capture work outside timing windows')
    ap.add_argument('--start', type=float)
    ap.add_argument('--observe-until', type=observation_time,
                    help='stop after this emulated time plus a 3s flush; observation only, no timing-window claim')
    ap.add_argument('--live-input', action='store_true',
                    help='observation-only append inbox for a compatible offline probe; never drives desktop input')
    ap.add_argument('--input-until', type=float, help='omit later input events, such as a scripted pause')
    ap.add_argument('--seconds', type=int, default=600)
    ap.add_argument('--yaw', type=float, default=0)
    ap.add_argument('--config', action='append', default=[])
    args = ap.parse_args()
    if not 1 <= args.seconds <= 1800:
        ap.error('--seconds must be between 1 and 1800')
    run(args)


if __name__ == '__main__':
    main()
