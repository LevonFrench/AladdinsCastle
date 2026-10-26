"""Reseeding an existing DolphinXR user folder with the universal controls (temporary folders only)."""
import tempfile
import unittest
from pathlib import Path

import reseed_controls as rc
import universal_controls as uc

NEW_RESET = uc.HOTKEYS['VR/Reset VR Position']
NEW_EXIT = uc.HOTKEYS['General/Exit']
OLD_RESET = NEW_RESET.replace(', 0.15)', ', 0.25)')

USER_BINDINGS = (
    '[Hotkeys]\n'
    'Device = DInput/0/Keyboard Mouse\n'
    'General/Toggle Pause = @(F10)\n'
    'General/Take Screenshot = @(F9)\n'
    'Load State/Load State Slot 1 = @(Shift+F1)\n'
)


def value(text, key):
    for line in text.replace('\r\n', '\n').split('\n'):
        left, eq, right = line.partition('=')
        if eq and left.strip() == key:
            return right.strip()
    return None


class MergeHotkeys(unittest.TestCase):
    def test_new_file_text_gets_the_universal_values_exactly(self):
        text, changes = rc.merge_hotkeys_text('[Hotkeys]\n')
        self.assertEqual(text, uc.hotkeys_text())
        self.assertEqual([k for k, _ in changes], ['Device', 'VR/Reset VR Position', 'General/Exit'])

    def test_old_file_with_the_250ms_window_is_upgraded(self):
        old = f'[Hotkeys]\nDevice = DInput/0/Keyboard Mouse\nVR/Reset VR Position = {OLD_RESET}\n'
        text, changes = rc.merge_hotkeys_text(old)
        self.assertEqual(value(text, 'VR/Reset VR Position'), NEW_RESET)
        self.assertEqual(value(text, 'General/Exit'), NEW_EXIT)
        self.assertEqual(dict(changes), {'VR/Reset VR Position': 'upgraded', 'General/Exit': 'added'})
        self.assertNotIn('0.25', text)

    def test_already_new_file_is_unchanged(self):
        text, changes = rc.merge_hotkeys_text(uc.hotkeys_text())
        self.assertEqual((text, changes), (uc.hotkeys_text(), []))

    def test_user_bindings_stay_and_missing_keys_are_added(self):
        text, changes = rc.merge_hotkeys_text(USER_BINDINGS)
        for key, want in (('General/Toggle Pause', '@(F10)'), ('General/Take Screenshot', '@(F9)'),
                          ('Load State/Load State Slot 1', '@(Shift+F1)')):
            self.assertEqual(value(text, key), want)
        self.assertEqual(value(text, 'VR/Reset VR Position'), NEW_RESET)
        self.assertEqual(value(text, 'General/Exit'), NEW_EXIT)
        self.assertEqual(len(changes), 2)

    def test_users_own_exit_key_is_kept_and_the_chord_is_or_ed_in(self):
        mine = '[Hotkeys]\nDevice = DInput/0/Keyboard Mouse\nGeneral/Exit = @(Alt+F4)\nVR/Reset VR Position = @(Ctrl+R)\n'
        text, changes = rc.merge_hotkeys_text(mine)
        self.assertEqual(value(text, 'General/Exit'), f'(@(Alt+F4)) | ({rc.EXIT_CHORD})')
        self.assertEqual(value(text, 'VR/Reset VR Position'), f'(@(Ctrl+R)) | ({rc.RESET_CHORD})')
        self.assertEqual({v for _, v in changes}, {'chord added'})
        # Running it again changes nothing.
        self.assertEqual(rc.merge_hotkeys_text(text), (text, []))

    def test_empty_binding_counts_as_missing(self):
        text, _ = rc.merge_hotkeys_text('[Hotkeys]\nDevice = DInput/0/Keyboard Mouse\nGeneral/Exit = \n')
        self.assertEqual(value(text, 'General/Exit'), NEW_EXIT)
        self.assertEqual(text.count('General/Exit'), 1)

    def test_other_sections_comments_and_order_survive(self):
        old = '# mine\n[Hotkeys]\nDevice = DInput/0/Keyboard Mouse\nGeneral/Toggle Pause = @(F10)\n\n[Other]\nKeep = 1\n'
        text, _ = rc.merge_hotkeys_text(old)
        self.assertTrue(text.startswith('# mine\n[Hotkeys]\n'))
        self.assertTrue(text.endswith('\n\n[Other]\nKeep = 1\n'))
        # New keys land inside [Hotkeys], not after [Other].
        self.assertLess(text.index('General/Exit'), text.index('[Other]'))

    def test_crlf_is_kept(self):
        text, _ = rc.merge_hotkeys_text(USER_BINDINGS.replace('\n', '\r\n'))
        self.assertEqual(text.count('\n'), text.count('\r\n'))
        self.assertTrue(text.endswith('\r\n'))

    def test_file_without_a_final_newline_stays_without_one(self):
        text, _ = rc.merge_hotkeys_text('[Hotkeys]\nDevice = DInput/0/Keyboard Mouse')
        self.assertFalse(text.endswith('\n'))
        self.assertEqual(value(text, 'General/Exit'), NEW_EXIT)

    def test_empty_file(self):
        text, _ = rc.merge_hotkeys_text('')
        self.assertEqual(text, uc.hotkeys_text())

    def test_current_values_match_the_template(self):
        template = (uc.HERE.parent / 'templates/Hotkeys.ini').read_text(encoding='utf-8')
        text, changes = rc.merge_hotkeys_text(template)
        self.assertEqual(changes, [])


class MergeDolphin(unittest.TestCase):
    def test_missing_keys_are_added_in_their_sections(self):
        old = '[Core]\nGFXBackend = D3D\n[General]\nISOPaths = 0\n[Input]\nBackgroundInput = True\n'
        text, changes = rc.merge_dolphin_text(old)
        self.assertEqual(value(text, 'ConfirmStop'), 'False')
        self.assertEqual(value(text, 'HotkeysRequireFocus'), 'False')
        self.assertEqual(value(text, 'GFXBackend'), 'D3D')
        self.assertEqual(value(text, 'BackgroundInput'), 'True')
        self.assertLess(text.index('HotkeysRequireFocus'), text.index('[Input]'))
        self.assertEqual(len(changes), 2)

    def test_confirm_stop_true_becomes_false_and_other_values_stay(self):
        old = '[Interface]\nUsePanicHandlers = False\nConfirmStop = True\n[General]\nHotkeysRequireFocus = True\n'
        text, changes = rc.merge_dolphin_text(old)
        self.assertEqual(value(text, 'ConfirmStop'), 'False')
        self.assertEqual(value(text, 'UsePanicHandlers'), 'False')
        # The user's own focus choice is not overridden.
        self.assertEqual(value(text, 'HotkeysRequireFocus'), 'True')
        self.assertEqual(changes, [('Interface/ConfirmStop', 'set to False')])

    def test_already_new_is_unchanged(self):
        new = '[General]\nHotkeysRequireFocus = False\n[Interface]\nConfirmStop = False\n'
        self.assertEqual(rc.merge_dolphin_text(new), (new, []))


class HubCopy(unittest.TestCase):
    def test_the_hub_carries_the_same_expressions(self):
        text = (uc.ROOT / 'hub/src/core/wii/Wii.cpp').read_text(encoding='utf-8')
        self.assertIn('R"x(' + rc.RESET_CHORD + ')x"', text)
        self.assertIn('R"x(' + rc.STALE_RESET_CHORDS[0] + ')x"', text)
        self.assertIn('R"x(' + rc.EXIT_CHORD + ')x"', text)
        self.assertIn('QStringLiteral("@(Shift+R)")', text)
        self.assertIn(rc.BACKUP_SUFFIX, text)
        self.assertEqual(rc.HOTKEY_DEVICE, 'DInput/0/Keyboard Mouse')
        self.assertIn('"DInput/0/Keyboard Mouse"', text)


class ReseedFolders(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.root = Path(self._tmp.name)

    def folder(self, name, hotkeys=None, dolphin='[Core]\nGFXBackend = D3D\n', bom=False):
        config = self.root / name / 'Config'
        config.mkdir(parents=True)
        if hotkeys is not None:
            (config / 'Hotkeys.ini').write_bytes(hotkeys.encode('utf-8'))
        if dolphin is not None:
            (config / 'Dolphin.ini').write_bytes((b'\xef\xbb\xbf' if bom else b'') + dolphin.encode('utf-8'))
        # Files that must never be touched.
        (config / 'GCPadNew.ini').write_text('[GCPad1]\nButtons/A = `Right Button A`\n', encoding='utf-8')
        (self.root / name / 'GameSettings').mkdir()
        (self.root / name / 'GameSettings' / 'GALE01.ini').write_text('[Core]\nCPUCore = 1\n', encoding='utf-8')
        return self.root / name

    def snapshot(self, user, names):
        return {n: (user / n).read_bytes() for n in names}

    def test_dry_run_writes_nothing(self):
        user = self.folder('dry')
        before = sorted(p.name for p in (user / 'Config').iterdir())
        result = rc.reseed_user_folder(user, apply=False)
        self.assertEqual(result['Hotkeys.ini'][0], 'created')
        self.assertEqual(result['Dolphin.ini'][0], 'changed')
        self.assertEqual(sorted(p.name for p in (user / 'Config').iterdir()), before)

    def test_apply_creates_hotkeys_and_backs_up_dolphin_once(self):
        user = self.folder('fresh')
        original = (user / 'Config/Dolphin.ini').read_bytes()
        rc.reseed_user_folder(user, apply=True)
        self.assertEqual((user / 'Config/Hotkeys.ini').read_text(encoding='utf-8'), uc.hotkeys_text())
        self.assertEqual((user / ('Config/Dolphin.ini' + rc.BACKUP_SUFFIX)).read_bytes(), original)
        self.assertFalse((user / ('Config/Hotkeys.ini' + rc.BACKUP_SUFFIX)).exists())  # nothing to back up
        self.assertIn('ConfirmStop = False', (user / 'Config/Dolphin.ini').read_text(encoding='utf-8'))
        self.assertEqual(sorted(p.name for p in (user / 'Config').glob('*.tmp-reseed')), [])

    def test_second_run_changes_nothing_and_keeps_the_first_backup(self):
        user = self.folder('twice', hotkeys=USER_BINDINGS)
        original = (user / 'Config/Hotkeys.ini').read_bytes()
        rc.reseed_user_folder(user, apply=True)
        after = self.snapshot(user, ['Config/Hotkeys.ini', 'Config/Dolphin.ini'])
        result = rc.reseed_user_folder(user, apply=True)
        self.assertEqual({v[0] for v in result.values()}, {'unchanged'})
        self.assertEqual(self.snapshot(user, ['Config/Hotkeys.ini', 'Config/Dolphin.ini']), after)
        self.assertEqual((user / ('Config/Hotkeys.ini' + rc.BACKUP_SUFFIX)).read_bytes(), original)

    def test_backup_is_never_overwritten(self):
        user = self.folder('keep', hotkeys=f'[Hotkeys]\nVR/Reset VR Position = {OLD_RESET}\n')
        backup = user / ('Config/Hotkeys.ini' + rc.BACKUP_SUFFIX)
        backup.write_text('first original\n', encoding='utf-8')
        rc.reseed_user_folder(user, apply=True)
        self.assertEqual(backup.read_text(encoding='utf-8'), 'first original\n')

    def test_other_files_are_never_touched(self):
        user = self.folder('other', hotkeys=USER_BINDINGS)
        names = ['Config/GCPadNew.ini', 'GameSettings/GALE01.ini']
        before = self.snapshot(user, names)
        rc.reseed_user_folder(user, apply=True)
        self.assertEqual(self.snapshot(user, names), before)

    def test_byte_order_mark_is_kept(self):
        user = self.folder('bom', hotkeys='[Hotkeys]\n', bom=True)
        rc.reseed_user_folder(user, apply=True)
        self.assertTrue((user / 'Config/Dolphin.ini').read_bytes().startswith(b'\xef\xbb\xbf'))

    def test_folder_without_dolphin_ini_is_skipped_for_that_file(self):
        user = self.folder('nodolphin', dolphin=None)
        result = rc.reseed_user_folder(user, apply=True)
        self.assertEqual(result['Dolphin.ini'], ('missing', []))
        self.assertFalse((user / 'Config/Dolphin.ini').exists())
        self.assertTrue((user / 'Config/Hotkeys.ini').is_file())

    def test_folder_without_config_is_an_error(self):
        with self.assertRaises(FileNotFoundError):
            rc.reseed_user_folder(self.root / 'nothing', apply=True)

    def test_command_line_dry_run_then_apply(self):
        user = self.folder('cli')
        self.assertEqual(rc.main([str(user)]), 0)
        self.assertFalse((user / 'Config/Hotkeys.ini').exists())
        self.assertEqual(rc.main(['--apply', str(user)]), 0)
        self.assertTrue((user / 'Config/Hotkeys.ini').is_file())
        self.assertEqual(rc.main([]), 2)


if __name__ == '__main__':
    unittest.main()
