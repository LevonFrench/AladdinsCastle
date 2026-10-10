"""Original con-revolver-chunky parameter set. GPL-3.0-only; built model CC0-1.0."""
from pathlib import Path
import sys

# Our own design dimensions; these are not measurements of a branded peripheral.
PARAMETERS = {
    'length_mm': 230,
    'barrel_width_mm': 51,
    'grip_angle_deg': 10,
}

if __name__ == '__main__':
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from common import run
    run('con-revolver-chunky', PARAMETERS)
