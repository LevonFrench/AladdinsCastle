"""Original con-pistol-slim parameter set. GPL-3.0-only; built model CC0-1.0."""
from pathlib import Path
import sys

# Our own design dimensions; these are not measurements of a branded peripheral.
PARAMETERS = {
    'length_mm': 240,
    'barrel_width_mm': 34,
    'grip_angle_deg': 16,
}

if __name__ == '__main__':
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from common import run
    run('con-pistol-slim', PARAMETERS)
