# SPDX-License-Identifier: GPL-3.0-only
"""High scores read from PinMAME .nv files. Synthetic bytes only: the real ROM set names come from the
Hub's own score maps, every other case from a small test map given through AC_TEST_PINBALL_SCORE_MAPS."""
import json, os, pathlib, subprocess, sys, tempfile, unittest
sys.dont_write_bytecode = True  # Shared fixtures must leave the source tree clean for packaging.

HUB = sys.argv.pop(1)
OWN = json.loads((pathlib.Path(__file__).resolve().parents[2]/'data/pinball-score-maps.json').read_text(encoding='utf-8'))

def bcd(value, length):
    digits = str(value).rjust(length * 2, '0')
    return bytes(int(digits[i:i + 2], 16) for i in range(0, len(digits), 2))

PLATFORMS = {
    'eight': {'endian': 'big', 'nvram': [{'address': 0, 'size': 64, 'nibble': 'both'}]},
    'split': {'endian': 'little', 'nvram': [{'address': 0x100, 'size': 16, 'nibble': 'low'}, {'address': 0x200, 'size': 16, 'nibble': 'both'}]},
}
MAPS = {'platforms': PLATFORMS, 'roms': {'fixture_a': 0, 'fixture_b': 1, 'fixture_c': 2}, 'maps': [
    {'platform': 'eight', 'scores': [
        {'kind': 'high', 'label': 'Grand Champion', 'short': 'GC', 'initials': {'encoding': 'ch', 'start': 0, 'count': 3}, 'score': {'encoding': 'bcd', 'start': 3, 'count': 4}},
        {'kind': 'high', 'label': 'Unused', 'short': '1st', 'initials': {'encoding': 'ch', 'start': 7, 'count': 3}, 'score': {'encoding': 'bcd', 'start': 10, 'count': 4}},
        {'kind': 'champion', 'label': 'Loop Champion', 'short': 'Loops', 'initials': {'encoding': 'ch', 'start': 14, 'count': 3, 'null': 'terminate'},
         'score': {'encoding': 'int', 'start': 17, 'count': 2, 'scale': 10, 'suffix': ' Loops'}},
        {'kind': 'high', 'label': 'Out of range', 'short': 'X', 'score': {'encoding': 'bcd', 'start': 0x400, 'count': 4}},
    ]},
    {'platform': 'split', 'scores': [
        {'kind': 'high', 'label': 'High Score', 'short': 'HS', 'score': {'encoding': 'bcd', 'addresses': [0x100, 0x101, 0x102, 0x103, 0x104, 0x105]}},
        {'kind': 'high', 'label': 'Second', 'short': '2nd', 'initials': {'encoding': 'ch', 'start': 0x106, 'count': 8},
         'score': {'encoding': 'bcd', 'start': 0x200, 'count': 3}},
    ]},
    {'platform': 'eight', 'charMap': ' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789', 'scores': [
        {'kind': 'high', 'label': 'First', 'short': '1st', 'initials': {'encoding': 'ch', 'start': 0, 'count': 3}, 'score': {'encoding': 'bcd', 'start': 3, 'count': 2}},
    ]},
]}

class Scores(unittest.TestCase):
    def read(self, files, sets, maps=MAPS):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp); folder = root/'nvram'; folder.mkdir()
            for i, (name, data) in enumerate(files.items()):
                path = folder/(name + '.nv'); path.write_bytes(data); os.utime(path, (1_700_000_000 + i * 60,) * 2)
            env = dict(os.environ)
            if maps is not None:
                (root/'maps.json').write_text(json.dumps(maps), encoding='utf-8'); env['AC_TEST_PINBALL_SCORE_MAPS'] = str(root/'maps.json')
            else:
                env.pop('AC_TEST_PINBALL_SCORE_MAPS', None)
            return json.loads(subprocess.check_output([HUB, 'pinball-scores', str(folder), *sets], env=env))

    def test_scores_initials_scale_and_suffix(self):
        nv = bytearray(64 + 46); nv[0:3] = b'ABC'; nv[3:7] = bcd(12345670, 4); nv[14:16] = b'Z\x00'; nv[17:19] = (25).to_bytes(2, 'big')
        out = self.read({'fixture_a': bytes(nv)}, ['fixture_a'])
        self.assertEqual((out['set'], out['source']), ('fixture_a', 'PinMAME NVRAM'))
        rows = [(s['kind'], s['label'], s['initials'], s['score'], s['text']) for s in out['scores']]
        # The unused place (no initials, score 0) and the entry outside the file's NVRAM are left out.
        self.assertEqual(rows, [('high', 'Grand Champion', 'ABC', 12345670, '12,345,670'), ('champion', 'Loop Champion', 'Z', 250, '250 Loops')])

    def test_nibbles_little_endian_offsets_and_a_second_region(self):
        nv = bytearray(32 + 46)
        for i, digit in enumerate([0, 5, 4, 3, 2, 1]): nv[i] = 0xF0 | digit  # little-endian digits in low nibbles: 123450
        nv[6:14] = bytes([0x04, 0x01, 0x04, 0x02, 0x04, 0x03, 0x04, 0x04])  # "ABCD", high nibble first
        nv[16:19] = bytes([0x50, 0x34, 0x12])  # the second region: 123450 little-endian
        out = self.read({'fixture_b': bytes(nv)}, ['fixture_b'])
        self.assertEqual([(s['label'], s['initials'], s['score']) for s in out['scores']], [('High Score', '', 123450), ('Second', 'ABCD', 123450)])

    def test_char_map_and_bcd_letters_count_as_zero(self):
        nv = bytearray(64); nv[0:3] = bytes([1, 2, 3]); nv[3:5] = bytes([0x1A, 0x34])
        out = self.read({'fixture_c': bytes(nv)}, ['fixture_c'])
        self.assertEqual([(s['initials'], s['score']) for s in out['scores']], [('ABC', 1034)])

    def test_the_newest_file_of_the_possible_sets_counts(self):
        older = bytearray(64 + 46); older[0:3] = b'OLD'; older[3:7] = bcd(100, 4)
        newer = bytearray(64); newer[0:3] = bytes([14, 5, 23]); newer[3:5] = bcd(200, 2)
        out = self.read({'fixture_a': bytes(older), 'fixture_c': bytes(newer)}, ['fixture_a', 'fixture_c'])
        self.assertEqual((out['set'], out['scores'][0]['initials']), ('fixture_c', 'NEW'))

    def test_a_set_the_maps_do_not_cover_says_so(self):
        out = self.read({'fixture_x': bytes(64)}, ['fixture_x'])
        self.assertEqual(out['set'], 'fixture_x'); self.assertNotIn('scores', out); self.assertIn('do not cover', out['note'])

    def test_nothing_saved_unsafe_names_and_large_files_give_nothing(self):
        self.assertEqual(self.read({}, ['fixture_a']), {})
        self.assertEqual(self.read({'fixture_a': bytes(64)}, ['../fixture_a']), {})
        self.assertEqual(self.read({'fixture_a': bytes(1024 * 1024 + 1)}, ['fixture_a']), {})

    def test_the_hubs_own_maps_read_a_real_set(self):
        set_name = 'dw_l2'; m = OWN['maps'][OWN['roms'][set_name]]; platform = OWN['platforms'][m['platform']]
        first = next(e for e in m['scores'] if e['kind'] == 'high' and 'initials' in e and 'start' in e['score'] and 'start' in e['initials'])
        base = platform['nvram'][0]['address']; nv = bytearray(sum(r['size'] for r in platform['nvram']) + 46)
        at = first['initials']['start'] - base; nv[at:at + 3] = b'XYZ'
        at = first['score']['start'] - base; nv[at:at + first['score']['count']] = bcd(987654320, first['score']['count'])
        out = self.read({set_name: bytes(nv)}, [set_name], maps=None)
        self.assertEqual((out['scores'][0]['label'], out['scores'][0]['initials'], out['scores'][0]['score']), (first['label'], 'XYZ', 987654320))

if __name__ == '__main__': unittest.main()
