# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic table and VPS identity cases; no collection data or network access."""
import json, pathlib, subprocess, sys, tempfile, unittest
sys.dont_write_bytecode = True  # Shared fixtures must leave the source tree clean for packaging.
from pinball_fixtures import table

HUB = sys.argv.pop(1)

class IdentityChecks(unittest.TestCase):
    def scan(self, entries, tables):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);library=root/'tables';library.mkdir()
            for stem,title,author,version,*script in tables:
                table(library/(stem+'.vpx'),title,author,version,script[0] if script else '')
            database=root/'vps.json';database.write_text(json.dumps(entries),encoding='utf-8')
            process=subprocess.run([HUB,'scan-pinball',str(library),str(database)],capture_output=True,text=True)
            self.assertEqual(process.returncode,0,process.stderr)
            return {pathlib.Path(row['path']).stem:row for row in json.loads(process.stdout)['tables']}

    def test_filename_identity_beats_unrelated_embedded_template_and_noise(self):
        db=[dict(id='synthetic-a',name='Orbit',manufacturer='Williams',year=1999,type='SS',players=4,
                 theme='Space',vpsUrl='https://virtualpinballspreadsheet.com/game/synthetic-a',
                 tableFiles=[dict(tableFormat='VPX',authors=['Different Author'],version='9',features=['VR'])])]
        stem='Orbit_(Williams_1999)_(VPW)_v1.2'
        row=self.scan(db,[(stem,'Template (Stern 2020)','Fixture Author','1.0')])[stem]
        self.assertEqual((row['machine'],row['maker'],row['year']),('Orbit','Williams',1999))
        self.assertEqual(row['title'],'Orbit (Williams 1999)')
        self.assertEqual((row['vpsId'],row['vpsName']),('synthetic-a','Orbit'))
        self.assertEqual(row['vpsUrl'],'https://virtualpinballspreadsheet.com/game/synthetic-a')
        self.assertEqual((row['type'],row['players'],row['theme']),('SS',4,'Space'))
        self.assertNotIn('features',row);self.assertFalse(row['vrReady'])

    def test_maker_only_year_only_and_canonical_alias(self):
        db=[dict(id='synthetic-b',name='Comet',manufacturer='Gottlieb',year=1985),
            dict(id='synthetic-c',name='Nova',manufacturer='Bally',year=1978)]
        rows=self.scan(db,[('Comet_(Premier)_(Mod)','Comet','Fixture','1'),
                           ('Nova_(1978)','Nova','Fixture','1')])
        self.assertEqual((rows['Comet_(Premier)_(Mod)']['maker'],rows['Comet_(Premier)_(Mod)']['year']),('Gottlieb',1985))
        self.assertEqual(rows['Nova_(1978)']['vpsId'],'synthetic-c')

    def test_narrow_maker_aliases_keep_other_manufacturers_distinct(self):
        db=[dict(id='spooky',name='Phantom',manufacturer='Spooky Pinball',year=2020),
            dict(id='gottlieb',name='Comet',manufacturer='Gottlieb',year=1985)]
        rows=self.scan(db,[('Phantom_(Spooky_2020)','Phantom','Fixture','1'),
                           ('Phantom_(Spooky)','Phantom','Fixture','1'),
                           ('Comet_(Gottileb_1985)','Comet','Fixture','1'),
                           ('Comet_(Electromatic_1985)','Comet','Fixture','1')])
        self.assertEqual(rows['Phantom_(Spooky_2020)']['vpsId'],'spooky')
        self.assertEqual(rows['Phantom_(Spooky)']['vpsId'],'spooky')
        self.assertEqual(rows['Comet_(Gottileb_1985)']['vpsId'],'gottlieb')
        self.assertNotIn('vpsId',rows['Comet_(Electromatic_1985)'])

    def test_ambiguous_remakes_and_conflicting_identity_do_not_match(self):
        db=[dict(id='synthetic-1',name='Orbit',manufacturer='Williams',year=1980),
            dict(id='synthetic-2',name='Orbit',manufacturer='Williams',year=1990),
            dict(id='synthetic-3',name='Meteor',manufacturer='Bally',year=1980)]
        rows=self.scan(db,[('Orbit','Orbit','Fixture','1'),
                           ('Orbit_(1980)','Orbit','Fixture','1'),
                           ('Orbit_(Stern_1980)','Orbit','Fixture','1'),
                           ('Meteor_(Williams_1980)','Meteor','Fixture','1')])
        self.assertNotIn('vpsId',rows['Orbit'])
        self.assertEqual(rows['Orbit_(1980)']['vpsId'],'synthetic-1')
        self.assertNotIn('vpsId',rows['Orbit_(Stern_1980)'])
        self.assertNotIn('vpsId',rows['Meteor_(Williams_1980)'])

    def test_edition_stays_distinct_and_vr_requires_exact_table_file(self):
        db=[dict(id='synthetic-regular',name='Meteor',manufacturer='Stern',year=2020),
            dict(id='synthetic-le',name='Meteor LE',manufacturer='Stern',year=2020,
                 tableFiles=[dict(tableFormat='VPX',authors=['Fixture Author'],version='2',features=['VR'])])]
        rows=self.scan(db,[('Meteor_LE_(Stern_2020)_VR','Meteor LE','Fixture Author','2'),
                           ('Meteor_(Stern_2020)','Meteor','Fixture Author','2')])
        self.assertEqual(rows['Meteor_LE_(Stern_2020)_VR']['vpsId'],'synthetic-le')
        self.assertTrue(rows['Meteor_LE_(Stern_2020)_VR']['vrReady'])
        self.assertEqual(rows['Meteor_(Stern_2020)']['vpsId'],'synthetic-regular')
        self.assertFalse(rows['Meteor_(Stern_2020)']['vrReady'])

    def test_untrusted_url_is_not_exposed(self):
        db=[dict(id='synthetic',name='Nova',manufacturer='Bally',year=1978,
                 url='https://not-vps.example/game/synthetic')]
        row=self.scan(db,[('Nova_(Bally_1978)','Nova','Fixture','1')])['Nova_(Bally_1978)']
        self.assertEqual(row['vpsId'],'synthetic')
        self.assertEqual(row['vpsUrl'],'https://virtualpinballspreadsheet.com/?game=synthetic')

    def test_legacy_idless_fixture_and_blank_format_keep_file_features(self):
        db=[dict(name='Comet',manufacturer='Gottlieb',year=1985,
                 tableFiles=[dict(authors=['Fixture Author'],version='1.0',features=['VR'])])]
        row=self.scan(db,[('Comet_(Gottlieb_1985)','Comet','Fixture Author','1.0')])['Comet_(Gottlieb_1985)']
        self.assertTrue(row['vpsMatched']);self.assertNotIn('vpsId',row)
        self.assertTrue(row['vrReady']);self.assertIn('VR',row['features'])

    def test_suffixes_and_title_numeral(self):
        db=[dict(id='synthetic-a',name='Orbit 2000',manufacturer='Williams',year=1989),
            dict(id='synthetic-b',name='Orbit',manufacturer='Williams',year=1999)]
        rows=self.scan(db,[('Orbit_2000_v2','Orbit 2000','Fixture','1'),
                           ('Orbit_(Williams_1999)_VR_Room_MOD_VPX6_v2','Orbit','Fixture','1')])
        self.assertEqual(rows['Orbit_2000_v2']['vpsId'],'synthetic-a')
        self.assertEqual(rows['Orbit_2000_v2']['year'],1989)
        self.assertEqual(rows['Orbit_(Williams_1999)_VR_Room_MOD_VPX6_v2']['vpsId'],'synthetic-b')

    def test_author_mod_fallback_needs_unique_exact_vps_prefix(self):
        db=[dict(id='regular',name='Meteor',manufacturer='Stern',year=2020),
            dict(id='edition',name='Meteor LE',manufacturer='Stern',year=2020)]
        rows=self.scan(db,[('Meteor_LE_Balutito_MOD_v2','Template','Fixture','1'),
                           ('Meteor_Balutito_MOD_v2','Template','Fixture','1')])
        self.assertEqual(rows['Meteor_LE_Balutito_MOD_v2']['vpsId'],'edition')
        self.assertEqual(rows['Meteor_Balutito_MOD_v2']['vpsId'],'regular')

    def test_attached_version_preserves_exact_file_features(self):
        db=[dict(id='synthetic',name='Comet',manufacturer='Bally',year=1962,
                 tableFiles=[dict(tableFormat='VPX',authors=['Fixture Author'],version='1.04',features=['VR'])])]
        stem='Comet_(Bally_1962)v1.04'
        row=self.scan(db,[(stem,'Comet','Fixture Author','1.04')])[stem]
        self.assertEqual(row['vpsId'],'synthetic');self.assertTrue(row['vrReady'])

    def test_leading_vr_marker_needs_unique_maker_year_match(self):
        db=[dict(id='synthetic',name='Comet',manufacturer='Bally',year=1962),
            dict(id='other',name='Comet',manufacturer='Williams',year=1962)]
        rows=self.scan(db,[('VR_Room_Comet_(Bally_1962)','VR Room Comet','Fixture','1'),
                           ('VR_Comet_(1962)','VR Comet','Fixture','1')])
        self.assertEqual(rows['VR_Room_Comet_(Bally_1962)']['vpsId'],'synthetic')
        self.assertNotIn('vpsId',rows['VR_Comet_(1962)'])

    def test_trailing_the_only_reorders_a_unique_machine(self):
        db=[dict(id='synthetic',name='The Comet',manufacturer='Bally',year=1962),
            dict(id='other',name='The Meteor',manufacturer='Bally',year=1962),
            dict(id='ambiguous',name='The Meteor',manufacturer='Williams',year=1962)]
        rows=self.scan(db,[('Comet_The_(Bally_1962)','Comet The','Fixture','1'),
                           ('Meteor_The_(1962)','Meteor The','Fixture','1')])
        self.assertEqual(rows['Comet_The_(Bally_1962)']['vpsId'],'synthetic')
        self.assertNotIn('vpsId',rows['Meteor_The_(1962)'])

    def test_embedded_identity_requires_a_corroborated_version(self):
        db=[dict(id='synthetic',name='Comet',manufacturer='Bally',year=1982),
            dict(id='different',name='Nova',manufacturer='Bally',year=1982)]
        rows=self.scan(db,[('Cometv1.04','Comet (Bally 1982)','Fixture','1.04'),
                           ('Novav1.04','Comet (Bally 1982)','Fixture','1.04')])
        self.assertEqual(rows['Cometv1.04']['vpsId'],'synthetic')
        self.assertNotIn('vpsId',rows['Novav1.04'])

    def test_vpw_is_not_a_machine_maker_but_conflicts_remain(self):
        db=[dict(id='unique',name='Comet',manufacturer='Bally',year=1962),
            dict(id='ambiguous-a',name='Meteor',manufacturer='Bally',year=1962),
            dict(id='ambiguous-b',name='Meteor',manufacturer='Williams',year=1962)]
        rows=self.scan(db,[('Comet_(VPW_1962)','Comet','Fixture','1'),
                           ('Meteor_(VPW_1962)','Meteor','Fixture','1'),
                           ('Comet_(Williams_1962)','Comet','Fixture','1')])
        self.assertEqual(rows['Comet_(VPW_1962)']['vpsId'],'unique')
        self.assertNotIn('vpsId',rows['Meteor_(VPW_1962)'])
        self.assertNotIn('vpsId',rows['Comet_(Williams_1962)'])

    def test_embedded_metadata_fills_only_missing_filename_fields(self):
        db=[dict(id='williams',name='Orbit',manufacturer='Williams',year=1995),
            dict(id='bally',name='Orbit',manufacturer='Bally',year=1995)]
        rows=self.scan(db,[('Orbit_(1995)','Orbit (Williams 1995)','Fixture','1'),
                           ('Orbit_(Williams)','Orbit (Williams 1995)','Fixture','1')])
        for stem in ('Orbit_(1995)','Orbit_(Williams)'):
            self.assertEqual((rows[stem]['maker'],rows[stem]['year'],rows[stem]['vpsId']),
                             ('Williams',1995,'williams'))

    def test_embedded_metadata_does_not_override_filename_conflicts(self):
        db=[dict(id='williams',name='Orbit',manufacturer='Williams',year=1995)]
        rows=self.scan(db,[('Orbit_(1996)','Orbit (Williams 1995)','Fixture','1'),
                           ('Orbit_(Bally_1995)','Orbit (Williams 1995)','Fixture','1')])
        self.assertEqual(rows['Orbit_(1996)']['year'],1996)
        self.assertNotIn('maker',rows['Orbit_(1996)'])
        self.assertNotIn('vpsId',rows['Orbit_(1996)'])
        self.assertEqual(rows['Orbit_(Bally_1995)']['maker'],'Bally')
        self.assertNotIn('vpsId',rows['Orbit_(Bally_1995)'])

    def test_numeral_ampersand_and_article_spellings_match_one_machine(self):
        db=[dict(id='roman',name='Orbit II',manufacturer='Williams',year=1999),
            dict(id='amp',name='Rock & Roll',manufacturer='Bally',year=1980),
            dict(id='article',name='The Comet',manufacturer='Bally',year=1962)]
        rows=self.scan(db,[('Orbit_2_(Williams_1999)','Orbit 2','Fixture','1'),
                           ('Rock_and_Roll_(Bally_1980)','Rock and Roll','Fixture','1'),
                           ('Comet_(Bally_1962)','Comet','Fixture','1')])
        self.assertEqual(rows['Orbit_2_(Williams_1999)']['vpsId'],'roman')
        self.assertEqual(rows['Orbit_2_(Williams_1999)']['machine'],'Orbit II')
        self.assertEqual(rows['Rock_and_Roll_(Bally_1980)']['vpsId'],'amp')
        self.assertEqual(rows['Comet_(Bally_1962)']['vpsId'],'article')

    def test_near_miss_titles_and_other_machines_do_not_match(self):
        db=[dict(id='roman',name='Orbit II',manufacturer='Williams',year=1999),
            dict(id='plain',name='Meteor',manufacturer='Bally',year=1980),
            dict(id='long',name='Meteor Storm',manufacturer='Bally',year=1980),
            dict(id='amb-a',name='Zeta II',manufacturer='Williams',year=1990),
            dict(id='amb-b',name='Zeta II',manufacturer='Bally',year=1991)]
        rows=self.scan(db,[('Orbit_3_(Williams_1999)','Orbit 3','Fixture','1'),
                           ('Orbit_(Williams_1999)','Orbit','Fixture','1'),
                           ('Orbit_2_(Bally_1999)','Orbit 2','Fixture','1'),
                           ('Meteor_(Bally_1980)','Meteor','Fixture','1'),
                           ('Zeta_2','Zeta 2','Fixture','1')])
        self.assertNotIn('vpsId',rows['Orbit_3_(Williams_1999)'])
        self.assertNotIn('vpsId',rows['Orbit_(Williams_1999)'])
        self.assertNotIn('vpsId',rows['Orbit_2_(Bally_1999)'])
        self.assertEqual(rows['Meteor_(Bally_1980)']['vpsId'],'plain')
        self.assertNotIn('vpsId',rows['Zeta_2'])  # two machines called Zeta II: no guess

    # The ported VPin Studio distance matcher: asked only when no machine has the table's name, and
    # its find taken only under the Hub's guards. Its games need a VPX table file, as VPS lists them.
    def vpx(self,**game):
        game.setdefault('tableFiles',[dict(tableFormat='VPX',authors=['Somebody'],version='9')])
        return game

    def test_distance_matcher_finds_a_name_with_extra_words(self):
        db=[self.vpx(id='bally',name='Star Trek',manufacturer='Bally',year=1979),
            self.vpx(id='stern',name='Star Trek',manufacturer='Stern',year=2013)]
        stem='JPs_Star_Trek_(Bally_1979)'
        row=self.scan(db,[(stem,'Template','Fixture','1')])[stem]
        self.assertEqual(row['vpsId'],'bally')
        self.assertEqual(row['vpsFound'],'by its file name')
        self.assertEqual(row['title'],'Star Trek (Bally 1979)')

    def test_exact_names_say_how_they_were_found(self):
        db=[self.vpx(id='nova',name='Nova',manufacturer='Bally',year=1978)]
        row=self.scan(db,[('Nova_(Bally_1978)','Nova','Fixture','1')])['Nova_(Bally_1978)']
        self.assertEqual(row['vpsFound'],'by its name')

    def test_distance_matcher_keeps_numbers_editions_makers_and_years_apart(self):
        db=[self.vpx(id='orbit2',name='Orbit II',manufacturer='Williams',year=1999),
            self.vpx(id='meteor',name='Meteor',manufacturer='Stern',year=2020),
            self.vpx(id='trek',name='Star Trek',manufacturer='Bally',year=1979)]
        rows=self.scan(db,[('JPs_Orbit_3_(Williams_1999)','Template','Fixture','1'),
                           ('JPs_Orbit_(Williams_1999)','Template','Fixture','1'),
                           ('Meteor_LE_JPs_(Stern_2020)','Template','Fixture','1'),
                           ('JPs_Star_Trek_(Williams_1979)','Template','Fixture','1'),
                           ('JPs_Star_Trek_(Bally_1980)','Template','Fixture','1')])
        for stem,row in rows.items():
            self.assertNotIn('vpsId',row,stem)

    def test_distance_matcher_takes_no_tie(self):
        db=[self.vpx(id='bally',name='Comet',manufacturer='Bally',year=1962),
            self.vpx(id='williams',name='Comet',manufacturer='Williams',year=1962)]
        row=self.scan(db,[('JPs_Comet','Template','Fixture','1')])['JPs_Comet']
        self.assertNotIn('vpsId',row)

    def test_distance_matcher_finds_by_the_scripts_rom(self):
        db=[self.vpx(id='tz',name='Twilight Zone',manufacturer='Bally',year=1993,romFiles=[dict(version='tz_94h')]),
            self.vpx(id='other',name='Comet',manufacturer='Bally',year=1962,romFiles=[dict(version='other_rom')])]
        script='Const cGameName="tz_94h"\nLoadVPM "01560000","sys80.vbs",3.26\nController.GameName=cGameName'
        rows=self.scan(db,[('tz_94h_mod','Template','Fixture','1',script),
                           ('qq_94h_mod','Template','Fixture','1',script)])
        self.assertEqual(rows['tz_94h_mod']['vpsId'],'tz')
        self.assertEqual(rows['tz_94h_mod']['vpsFound'],'by its ROM')
        self.assertNotIn('vpsId',rows['qq_94h_mod'])  # the file name says nothing of Twilight Zone

    def test_distance_matcher_finds_by_the_table_info_name(self):
        db=[self.vpx(id='afm',name='Attack from Mars',manufacturer='Bally',year=1995)]
        row=self.scan(db,[('AFM_mars_2_0','Attack from Mars (Bally 1995)','Fixture','2.0')])['AFM_mars_2_0']
        self.assertEqual(row['vpsId'],'afm')
        self.assertEqual(row['vpsFound'],'by the name in its table info')

    def test_table_file_features_with_the_matchers_author_and_version_tolerance(self):
        files=[dict(tableFormat='VPX',authors=['ClarkKent'],version='1.1.3',features=['VR']),
               dict(tableFormat='VPX',authors=['Somebody'],version='2.0',features=['VR'])]
        db=[dict(id='comet',name='Comet',manufacturer='Bally',year=1962,tableFiles=files)]
        rows=self.scan(db,[('Comet_(Bally_1962)','Comet','Clark Kent','1.13'),
                           ('Comet_(Bally_1962)_b','Comet','Clark Kent','1.2'),
                           ('Comet_(Bally_1962)_c','Comet','Nobody','1.13')])
        row=rows['Comet_(Bally_1962)']
        self.assertTrue(row['vrReady']);self.assertIn('VR',row['features'])
        self.assertEqual(row['vpsFileFound'],'by its authors and version')
        for stem in ('Comet_(Bally_1962)_b','Comet_(Bally_1962)_c'):
            self.assertEqual(rows[stem]['vpsId'],'comet',stem)
            self.assertNotIn('features',rows[stem],stem);self.assertFalse(rows[stem]['vrReady'],stem)

if __name__=='__main__':unittest.main()
