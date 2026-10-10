"""Original mnt-mg-heavy parameter set. GPL-3.0-only; built model CC0-1.0."""
from pathlib import Path
import sys

# Our own design dimensions; these are not measurements of a branded peripheral.
PARAMETERS = {
    'length_mm': 650,
    'barrel_width_mm': 156,
    'grip_angle_deg': 0,
}

if __name__ == '__main__':
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from common import run
    run('mnt-mg-heavy', PARAMETERS)
