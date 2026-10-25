"""Copies the site into a clean folder for `wrangler pages deploy`, leaving out what must not be served.

Cloudflare Pages publishes every file in the folder it is given, so deploying website/ itself would
put tools/ (build scripts and templates) and every .md (README, notices source) on the public site.
Run from the repo root, then deploy the staged folder:
    python website/tools/stage_deploy.py <empty or new folder>
    wrangler pages deploy <that folder> --project-name ac-vr-hub --branch preview
"""
import shutil, sys
from pathlib import Path

SKIP_DIRS = {'tools', '__pycache__', 'node_modules'}
SKIP_SUFFIX = {'.md', '.py', '.pyc'}

src = Path(__file__).resolve().parent.parent
if len(sys.argv) != 2:
    sys.exit(__doc__)
dst = Path(sys.argv[1]).resolve()
if src == dst or src in dst.parents:
    sys.exit('the staging folder must be outside website/')
if dst.exists() and any(dst.iterdir()):
    sys.exit(f'{dst} is not empty; give a new or empty folder')

count = 0
for path in sorted(src.rglob('*')):
    rel = path.relative_to(src)
    if path.is_dir() or SKIP_DIRS & set(rel.parts) or path.suffix.lower() in SKIP_SUFFIX or rel.name.startswith('.'):
        continue
    (dst / rel).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, dst / rel)
    count += 1
# the page that replaces the served notices must be there
for need in ('index.html', '404.html', '_headers', 'notices.html', 'privacy.html'):
    assert (dst / need).is_file(), need
leaked = [p for p in dst.rglob('*') if p.suffix.lower() in SKIP_SUFFIX or 'tools' in p.relative_to(dst).parts]
assert not leaked, leaked
print(f'staged {count} files in {dst}')
