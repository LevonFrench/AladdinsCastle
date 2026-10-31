# SPDX-License-Identifier: GPL-3.0-only
"""ROM sets checked file by file against a PinMAME file list. Synthetic bytes and set names only; the
list comes from AC_TEST_PINMAME_ROM_LIST, so VPinMAME is never asked."""
import json, os, pathlib, subprocess, sys, tempfile, unittest, zipfile, zlib
sys.dont_write_bytecode = True  # Shared fixtures must leave the source tree clean for packaging.
from pinball_fixtures import table

HUB = sys.argv.pop(1)
SCRIPT = 'Const cGameName="{name}"\nLoadVPM "01560000","sys80.vbs",3.26\nController.GameName=cGameName'
CPU, DISPLAY, SPEECH = b'synthetic cpu bytes', b'synthetic display bytes', b'synthetic speech bytes'

def rom(name, data, flags=0, crc=None):
    return dict(name=name, length=len(data), crc=zlib.crc32(data) if crc is None else crc, flags=flags)

LIST = {'sets': {
    'fixture_a': {'cloneOf': '', 'roms': [rom('cpu.u6', CPU), rom('display.u2', DISPLAY), rom('speech.u7', SPEECH, flags=0x20)]},
    'fixture_b': {'cloneOf': 'fixture_a', 'roms': [rom('cpu_b.u6', CPU + b'b'), rom('display.u2', DISPLAY)]},
    'fixture_c': {'cloneOf': '', 'roms': [rom('cpu.u6', CPU), rom('nodump.u9', b'', crc=0)]},
}}

class Checks(unittest.TestCase):
    def scan(self, zips, name, rom_list=LIST):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp); tables = root/'tables'; roms = root/'roms'; tables.mkdir(); roms.mkdir()
            table(tables/'fixture.vpx', 'Fixture (Synthetic 2000)', 'Test', '1', SCRIPT.format(name=name))
            for zip_name, files in zips.items():
                with zipfile.ZipFile(roms/(zip_name + '.zip'), 'w') as archive:
                    for member, data in files.items(): archive.writestr(member, data)
            env = dict(os.environ)
            if rom_list is not None:
                (root/'list.json').write_text(json.dumps(rom_list), encoding='utf-8'); env['AC_TEST_PINMAME_ROM_LIST'] = str(root/'list.json')
            else:
                env.pop('AC_TEST_PINMAME_ROM_LIST', None)
            result = json.loads(subprocess.check_output([HUB, 'scan-pinball', str(tables), '', str(roms), '', ''], env=env))
            return result['tables'][0], result

    def test_complete_set_by_name_and_by_crc_under_another_name(self):
        row, _ = self.scan({'fixture_a': {'cpu.u6': CPU, 'display.u2': DISPLAY, 'speech.u7': SPEECH}}, 'fixture_a')
        self.assertEqual((row['romState'], row['romCheck'], row['romCheckSource']), ('ready', 'complete', 'VPinMAME'))
        row, _ = self.scan({'fixture_a': {'renamed_cpu.bin': CPU, 'DISPLAY.U2': DISPLAY, 'speech.u7': SPEECH}}, 'fixture_a')
        self.assertEqual((row['romState'], row['romCheck']), ('ready', 'complete'))

    def test_missing_program_file_makes_the_rom_missing(self):
        row, _ = self.scan({'fixture_a': {'cpu.u6': CPU, 'speech.u7': SPEECH}}, 'fixture_a')
        self.assertEqual((row['romState'], row['romCheck'], row['romMissing']), ('ROM missing', 'files missing', ['display.u2']))
        self.assertIn('display.u2', row['romReason'])

    def test_a_differing_file_leaves_it_unknown(self):
        row, _ = self.scan({'fixture_a': {'cpu.u6': CPU + b'changed', 'display.u2': DISPLAY, 'speech.u7': SPEECH}}, 'fixture_a')
        self.assertEqual((row['romState'], row['romCheck'], row['romDiffer']), ('unknown', 'files differ', ['cpu.u6']))

    def test_missing_sound_files_still_play(self):
        row, _ = self.scan({'fixture_a': {'cpu.u6': CPU, 'display.u2': DISPLAY}}, 'fixture_a')
        self.assertEqual((row['romState'], row['romCheck'], row['romSoundMissing']), ('ready', 'sound files missing', ['speech.u7']))
        self.assertIn('without sound', row['romReason'])

    def test_a_clone_takes_files_from_its_parent_zip(self):
        parent = {'cpu.u6': CPU, 'display.u2': DISPLAY, 'speech.u7': SPEECH}
        row, _ = self.scan({'fixture_a': parent, 'fixture_b': {'cpu_b.u6': CPU + b'b'}}, 'fixture_b')
        self.assertEqual((row['romState'], row['romCheck']), ('ready', 'complete'))
        row, _ = self.scan({'fixture_b': {'cpu_b.u6': CPU + b'b'}}, 'fixture_b')
        self.assertEqual((row['romState'], row['romMissing']), ('ROM missing', ['display.u2']))

    def test_a_file_with_no_known_dump_is_not_needed(self):
        row, _ = self.scan({'fixture_c': {'cpu.u6': CPU}}, 'fixture_c')
        self.assertEqual((row['romState'], row['romCheck']), ('ready', 'complete'))

    def test_sets_pinmame_does_not_list_and_no_list_keep_the_name_check(self):
        row, _ = self.scan({'fixture_x': {'cpu.u6': CPU}}, 'fixture_x')
        self.assertEqual(row['romState'], 'ready'); self.assertNotIn('romCheck', row)
        row, result = self.scan({'fixture_a': {'cpu.u6': CPU}}, 'fixture_a', rom_list=None)
        self.assertEqual(row['romState'], 'ready'); self.assertNotIn('romCheck', row)
        self.assertIn('romCheckNote', result)  # no VPinMAME folder was given: said, not hidden

if __name__ == '__main__': unittest.main()
