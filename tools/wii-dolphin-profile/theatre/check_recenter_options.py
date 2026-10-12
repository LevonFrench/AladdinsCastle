"""Exercise actual CLI rejection paths before capture, VR or keyboard setup."""
import subprocess
import sys

executable = sys.argv[1]
cases = [
    ([], "Use --pid"),
    (["--desktop-recenter-hotkey", "maybe"], "Desktop recenter hotkey requires on or off"),
    (["--desktop-recenter-hotkey"], "Options need values"),
    (["--desktop-recenter-hotkey", "off", "--desktop-recenter-hotkey", "on"],
     "Desktop recenter hotkey may be specified only once"),
    (["--capture-only", "--desktop-recenter-hotkey", "on"],
     "Desktop recenter hotkey is only available in live mode"),
    (["--capture-only", "--desktop-recenter-hotkey", "off"], "Use --pid"),
]
for arguments, expected in cases:
    result = subprocess.run([executable, *arguments], capture_output=True, text=True, timeout=5)
    if result.returncode == 0 or expected not in result.stdout + result.stderr:
        raise SystemExit(f"CLI guard failed for {arguments}: {result.returncode}, {result.stdout}, {result.stderr}")
print(f"{len(cases)} actual CLI checks passed before capture/VR/hotkey initialization")
