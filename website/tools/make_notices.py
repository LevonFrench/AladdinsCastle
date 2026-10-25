"""Writes website/notices.html from website/THIRD-PARTY-NOTICES.md.

The .md stays the source (and is not served: tools/stage_deploy.py leaves every .md out of the
deploy folder), so the page the site links to is this generated HTML. Run from the repo root:
    python website/tools/make_notices.py website
"""
import html, re, sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else 'website')
lines = (root / 'THIRD-PARTY-NOTICES.md').read_text(encoding='utf-8').splitlines()


def inline(text):
    out, pos = '', 0
    for m in re.finditer(r'`([^`]+)`|\[([^\]]+)\]\(([^)]+)\)|(https://[^\s)|]+)', text):
        out += html.escape(text[pos:m.start()])
        if m.group(1) is not None:
            out += f'<code>{html.escape(m.group(1))}</code>'
        elif m.group(2) is not None:
            out += f'<a href="{html.escape(m.group(3))}" rel="noopener">{html.escape(m.group(2))}</a>'
        else:
            out += f'<a href="{html.escape(m.group(4))}" rel="noopener">{html.escape(m.group(4))}</a>'
        pos = m.end()
    return out + html.escape(text[pos:])


body, para, i = [], [], 0


def flush():
    global para
    if para:
        body.append(f'<p>{inline(" ".join(para))}</p>')
        para = []


while i < len(lines):
    ln = lines[i]
    if ln.startswith('```'):
        flush()
        j = i + 1
        while not lines[j].startswith('```'):
            j += 1
        body.append('<pre>' + html.escape('\n'.join(lines[i + 1:j])) + '</pre>')
        i = j
    elif ln.startswith('## '):
        flush()
        body.append(f'<h2>{inline(ln[3:])}</h2>')
    elif ln.startswith('# '):
        flush()
    elif ln.startswith('|'):
        flush()
        rows = []
        while i < len(lines) and lines[i].startswith('|'):
            cells = [c.strip() for c in lines[i].strip('|').split('|')]
            if not all(re.fullmatch(r'-+', c) for c in cells):
                rows.append(cells)
            i += 1
        i -= 1
        head, rest = rows[0], rows[1:]
        t = '<table><thead><tr>' + ''.join(f'<th>{inline(c)}</th>' for c in head) + '</tr></thead><tbody>'
        t += ''.join('<tr>' + ''.join(f'<td>{inline(c)}</td>' for c in r) + '</tr>' for r in rest)
        body.append(t + '</tbody></table>')
    elif ln.startswith('- '):
        flush()
        items = []
        while i < len(lines) and lines[i].startswith('- '):
            items.append(f'<li>{inline(lines[i][2:])}</li>')
            i += 1
        i -= 1
        body.append('<ul>' + ''.join(items) + '</ul>')
    elif not ln.strip():
        flush()
    else:
        para.append(ln.strip())
    i += 1
flush()

page = '''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Notices · AC VR Hub</title>
<meta name="description" content="Licences and credits for the AC VR Hub website: Three.js, the ASCII animations, the fonts, the models and the music.">
<meta name="theme-color" content="#120c1f">
<link rel="icon" href="token.svg" type="image/svg+xml">
<link rel="preload" href="fonts/PressStart2P-Regular.woff2" as="font" type="font/woff2" crossorigin>
<link rel="preload" href="fonts/CinzelDecorative-Black.woff2" as="font" type="font/woff2" crossorigin>
<link rel="preload" href="fonts/SpaceGrotesk-Variable.woff2" as="font" type="font/woff2" crossorigin>
<link rel="stylesheet" href="style.css?v={VER}">
<link rel="stylesheet" href="flow.css?v={VER}">
<script src="music.js?v={VER}" defer></script>
</head>
<body class="flow-page">
<header class="hud">
  <a class="brand" href="index.html" aria-label="AC VR Hub, back to the arcade">
    <img src="token.svg" alt="" width="34" height="34">
    <span>AC VR Hub</span>
  </a>
  <nav class="hud-links" aria-label="Pages">
    <a href="index.html">The arcade</a>
    <a href="how-it-works.html">How it works</a>
    <a href="games.html">Game list</a>
    <a href="https://github.com/LevonFrench/AladdinsCastle" rel="noopener">GitHub</a>
  </nav>
</header>

<main class="flow-main">
  <header class="intro">
    <h1>Notices</h1>
    <p class="lede">Licences and credits for the parts of this website that are not the project's own code. This page is generated from <code>website/THIRD-PARTY-NOTICES.md</code> in the repository.</p>
  </header>

  <div class="prose">
{BODY}
  </div>

  <footer class="outro">
    <p class="actions">
      <a class="btn" href="index.html">Back to the arcade</a>
      <a class="btn btn-ghost" href="privacy.html">Privacy</a>
    </p>
  </footer>
</main>
</body>
</html>
'''
ver = re.search(r'style\.css\?v=(\w+)', (root / 'privacy.html').read_text(encoding='utf-8')).group(1)
(root / 'notices.html').write_text(page.replace('{VER}', ver).replace('{BODY}', '\n'.join(body)), encoding='utf-8', newline='\n')
print('wrote', root / 'notices.html')
