"""Copy the ASCII animations the site uses from a checkout of ascii-magic-art (MIT).

Usage: python website/tools/sync_ascii.py <path to ascii-magic-art checkout>

Only the runtime, its helper kit and the pieces listed below are copied, into website/ascii/,
keeping the package layout (runtime/, lib/, pieces/) that runtime/ascii.js expects. The two
photo-based pieces under the Unsplash License (ocean-sunset, starry-ridge) are never copied.
"""
import shutil
import subprocess
import sys
from pathlib import Path

# cabinet attract screens in the arcade walk
CABINETS = [
    'alien-wave', 'rock-blaster', 'vector-space-duel', 'maze-chase', 'brick-breaker',
    'pipe-flyer', 'block-drop', 'paddle-tennis', 'frog-hop-crossing', 'snake-autoplay',
    'dino-runner', 'coin-run-platformer', 'gem-cascade', 'circuit-lap-racer', 'rainbow-warp-road',
]
# page headers, dividers and the 404
PAGES = ['synthwave-grid', 'tv-static', 'wave-divider', 'circuit-trace', 'stars-divider', 'pulse-line']
RUNTIME = ['runtime/ascii.js', 'runtime/player.js', 'runtime/glyphs.js', 'runtime/canvas.js', 'lib/kit.js']
NEVER = {'ocean-sunset', 'starry-ridge'}


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    src = Path(sys.argv[1])
    dst = Path(__file__).resolve().parent.parent / 'ascii'
    if dst.exists():
        shutil.rmtree(dst)
    for rel in RUNTIME + ['LICENSE']:
        (dst / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src / rel, dst / rel)
    (dst / 'pieces').mkdir(parents=True, exist_ok=True)
    for slug in CABINETS + PAGES:
        assert slug not in NEVER, slug
        shutil.copy2(src / 'pieces' / f'{slug}.js', dst / 'pieces' / f'{slug}.js')
    try:  # record which upstream revision this is (THIRD-PARTY-NOTICES.md points here)
        rev = subprocess.run(['git', '-C', str(src), 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
        (dst / 'UPSTREAM.txt').write_text(f'https://github.com/kailashsr38/ascii-magic-art\ncommit {rev}\n', encoding='utf-8')
    except (OSError, subprocess.CalledProcessError):
        print('note: not a git checkout, so no ascii/UPSTREAM.txt was written')
    print(f'copied {len(RUNTIME)} runtime files and {len(CABINETS) + len(PAGES)} pieces to {dst.name}/')


if __name__ == '__main__':
    main()
