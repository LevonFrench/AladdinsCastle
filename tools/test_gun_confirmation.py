"""Confirmation-index evidence boundaries and artifact-reference integrity."""
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from gun_confirmation import ROOT,compile_index,artifact,markdown

class ConfirmationIndexTests(unittest.TestCase):
    def test_every_authored_game_stays_a_question_with_proposed_choices(self):
        index=compile_index()
        self.assertEqual({g['game_id'] for g in index['games']},{p.stem for p in (ROOT/'data/controls/wii/games').glob('*.toml')})
        self.assertEqual(index['state'],'owner-confirmation-pending')
        for game in index['games']:
            self.assertEqual(game['proposal']['state'],'proposed');self.assertEqual(game['hold']['state'],'provisional')
            self.assertEqual(game['reload']['state'],'question');self.assertFalse(game['reload']['enabled'])
            self.assertEqual(game['layout']['state'],'question')
            for part in game['parts']:
                if not part['id'].endswith('-aim'):self.assertEqual(part['state'],'question')
        self.assertIn('not an actual headset frame',markdown(index))

    def test_game_model_and_case_references_resolve_to_hash_bound_artifacts(self):
        index=compile_index();artifacts=index['artifacts']
        for game in index['games']:
            for name in game['sources']:self.assertIn(name,artifacts)
            for mid in game['review_models']:self.assertIn(mid,index['models'])
        for mid,model in index['models'].items():
            for name in model['sources']+model['sight_checks']+[model['review_sheet']]:self.assertIn(name,artifacts)
            self.assertEqual({c['primary_hand'] for c in model['review_cases'] if c['hold']=='primary'},{'left','right'})
            for case in model['review_cases']:
                for image in case['images']:self.assertEqual(artifacts[image],artifact(ROOT,image,'preview'))
        self.assertFalse(index['models']['con-stick-grip']['sight_checks'])
        self.assertEqual(len(index['models']['con-remote-shell']['sight_checks']),2)

    def test_visible_hashes_and_model_sheet_links_are_in_the_owner_worksheet(self):
        index=compile_index();page=markdown(index)
        for name,item in index['artifacts'].items():self.assertIn(item['sha256'],page)
        for mid,model in index['models'].items():
            if mid in {g['proposal']['model'] for g in index['games']}:self.assertIn(mid+'-review-sheet.png',page)
        self.assertNotIn('- [x]',page)

    def test_missing_or_outside_artifacts_refuse_instead_of_guessing(self):
        directory=ROOT/'.local/confirmation-fixtures';directory.mkdir(parents=True,exist_ok=True)
        with TemporaryDirectory(dir=directory) as temp:
            with self.assertRaisesRegex(ValueError,'Missing existing artifact'):artifact(temp,'missing.png','preview')
            with self.assertRaisesRegex(ValueError,'escapes'):artifact(temp,'../outside.png','preview')

    def test_repository_text_hashes_are_portable_but_binary_bytes_are_exact(self):
        directory=ROOT/'.local/confirmation-fixtures';directory.mkdir(parents=True,exist_ok=True)
        with TemporaryDirectory(dir=directory) as temp:
            path=Path(temp)/'sample';path.write_bytes(b'line\r\n')
            text=artifact(temp,'sample','source');binary=artifact(temp,'sample','preview')
            path.write_bytes(b'line\n')
            self.assertEqual(text,artifact(temp,'sample','source'))
            self.assertNotEqual(binary['sha256'],artifact(temp,'sample','preview')['sha256'])
            self.assertEqual(text['hash_basis'],'repository-lf');self.assertEqual(binary['hash_basis'],'file-bytes')

if __name__=='__main__':unittest.main()
