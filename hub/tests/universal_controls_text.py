# SPDX-License-Identifier: GPL-3.0-only
"""Universal controls (docs/controls.md U1-U4): the Hub text, docs, website and control data say one thing.

CPU only, no build needed. Run from anywhere: python hub/tests/universal_controls_text.py
"""
from pathlib import Path
import re
import sys
import tomllib
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / 'hub/src/core/launch/UniversalControls.h'

# Wording that belonged to the rules the contract retired (left Y quit and select, grips recentre, A and B as the
# only Start and Coin, Home as recentre). Pinball has its own page text and is lane F's, so its core folder is skipped.
RETIRED = [
    r'hold (left )?Y for', r'Hold (left )?Y for', r'tap (of )?left Y', r'Y exits', r'Home = ', r'Home recentres',
    r'A = [Ss]tart', r'B = [Cc]oin', r'Both thumbsticks pressed', r'both grips for 1 s', r'[Rr]ecentre.{0,20}both grips',
]
TEXT_DIRS = ['hub/src', 'hub/qml', 'hub/PINBALL.md', 'hub/MODEL3.md', 'hub/N64.md', 'hub/PS1.md', 'hub/Saturn.md', 'website']
TEXT_SUFFIX = {'.cpp', '.h', '.qml', '.md', '.html', '.py'}


def universal_sentence():
    text = HEADER.read_text(encoding='utf-8')
    body = text[text.index('inline QString controls()'):]
    body = body[:body.index('}')]
    return ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', body))


def files(entry):
    path = ROOT / entry
    if path.is_file():
        yield path
    else:
        for item in sorted(path.rglob('*')):
            if item.is_file() and item.suffix in TEXT_SUFFIX:
                yield item


class UniversalControlsText(unittest.TestCase):
    def test_sentence_states_u1_to_u4(self):
        sentence = universal_sentence()
        for part in ('Left stick click = Start', 'Right stick click = Select or Coin',
                     'Click both sticks together', 'Hold both stick clicks for 3 seconds'):
            self.assertIn(part, sentence)

    def test_docs_and_website_repeat_the_sentence(self):
        sentence = universal_sentence()
        for rel in ('docs/controls.md', 'website/index.html', 'website/games.html', 'website/tools/make_games.py'):
            self.assertIn(sentence, (ROOT / rel).read_text(encoding='utf-8'), rel)

    def test_no_retired_wording_in_player_text(self):
        bad = []
        for entry in TEXT_DIRS:
            for path in files(entry):
                rel = path.relative_to(ROOT).as_posix()
                if rel.startswith('hub/src/core/pinball/') or rel.endswith('UniversalControls.h') or rel == 'hub/tests/universal_controls_text.py':
                    continue
                text = path.read_text(encoding='utf-8', errors='replace')
                for pattern in RETIRED:
                    if re.search(pattern, text):
                        bad.append(rel + ': ' + pattern)
        self.assertEqual(bad, [])

    def test_every_core_card_uses_the_shared_sentence(self):
        for name in ('ps1/Ps1', 'n64/N64', 'saturn/Saturn', 'libretro/Libretro', 'ps2/Ps2', 'model2/Model2', 'model3/Model3', 'system22/System22'):
            text = (ROOT / 'hub/src/core' / (name + '.cpp')).read_text(encoding='utf-8')
            self.assertIn('universal::', text, name)

    def test_cabinet_sets_follow_the_universal_layer(self):
        sets = sorted((ROOT / 'data/controls/cabinets').rglob('*.toml')) + sorted(ROOT.glob('games/*/setup/controls.toml'))
        self.assertGreater(len(sets), 60)
        for path in sets:
            data = tomllib.loads(path.read_text(encoding='utf-8'))
            for element in data.get('element', []):
                source, eid = element.get('source', ''), element['id']
                self.assertNotIn('menu', source, path.name + ' ' + eid)
                self.assertNotIn('oculus', source, path.name + ' ' + eid)
                if source.endswith('thumbstick_click'):
                    self.assertTrue(eid.split('-')[-1] in ('start', 'coin'), path.name + ' ' + eid + ' binds a stick click')
                if eid in ('start', 'p1-start'):
                    self.assertEqual(source, 'left_thumbstick_click', path.name + ' ' + eid)
                if eid in ('coin', 'p1-coin'):
                    self.assertEqual(source, 'right_thumbstick_click', path.name + ' ' + eid)

    def test_gun_sets_follow_the_universal_layer(self):
        sets = sorted((ROOT / 'data/controls').glob('gun-*.toml'))
        self.assertGreater(len(sets), 25)
        for path in sets:
            data = tomllib.loads(path.read_text(encoding='utf-8'))
            for element in data.get('element', []):
                binding, eid = element['binding'], element['id']
                self.assertNotEqual(binding['control'], 'menu_chord', path.name + ' ' + eid)
                if binding['control'] == 'thumbstick_click':
                    want = {'p1-start': 'left', 'p1-coin': 'right'}.get(eid)
                    self.assertEqual(binding['hand'], want, path.name + ' ' + eid)
                if eid.endswith('laser'):
                    self.assertEqual((binding['hand'], binding['control']), ('slot', 'secondary'), path.name + ' ' + eid)
                self.assertNotEqual(eid, 'p2-coin', path.name)


if __name__ == '__main__':
    sys.exit(unittest.main(verbosity=1))
