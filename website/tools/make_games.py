"""Writes website/games.html from the per-system tables below.

Sources: docs/status/current.md (counts, evidence levels) and the Hub's own game tables in
hub/src/core/*/*.cpp. Edit SYSTEMS when the project changes, then run from the repo root:
    python website/tools/make_games.py website
"""
import html, sys
from pathlib import Path

E3, E1 = ('ok', 'E3 accepted'), ('no', 'E1 early')
SYSTEMS = [
 ('model3', 'Sega Model 3', '#9bdc3c', '11 games · 54 views', 'Supermodel, patched. Open frame, inside the game with your head tracked, and an opt-in two-handed wheel.',
  ['Scud Race', 'Daytona USA 2', 'Sega Rally 2', ('Sega Rally 2 DX', E3, 'inside, with the wheel'), 'Star Wars Trilogy', 'Harley-Davidson & L.A. Riders', 'Dirt Devils', 'Virtua Fighter 3', 'Spikeout', 'Fighting Vipers 2', 'Virtua Striker 2'],
  'Also tried in a headset by the developer: Scud Race, Daytona USA 2, Sega Rally 2, Star Wars Trilogy, Fighting Vipers 2 and Dirt Devils.'),
 ('mame', 'Arcade guns and motion (MAME)', '#f6c744', '26 games · 34 views', 'MAME, patched, with our viewer: a big screen in the room with the gun in your hand, open frame where the game draws it, and opt-in body motion.',
  ['Point Blank 2', 'Lethal Enforcers', 'Lethal Enforcers II: Gun Fighters', 'Beast Busters', ('Time Crisis', E3, 'on the big screen'), 'Terminator 2: Judgment Day', 'Operation Wolf 3', 'Zombie Raid', 'Rail Chase', 'Police Trainer', 'Alien3: The Gun', 'Rapid Fire', 'Prop Cycle', 'Alpine Racer', 'Alpine Racer 2', 'Cyber Cycles', 'Aqua Jet', 'Air Combat 22'],
  'Eight more are listed in the Hub by their arcade set names only. The six motion cabinets (Prop Cycle to Air Combat 22) run in 16:9 open frame.'),
 ('model2', 'Sega Model 2', '#4ff3ff', '14 games · 25 views', 'MAME 0.289, patched: real 3D and open frame, with the game\'s own sound since 21 October.',
  ['Virtua Cop', 'Virtua Cop 2', 'The House of the Dead', 'Gunblade NY', 'Behind Enemy Lines', ('Sky Target', None, 'firing unproven'), 'Daytona USA', 'Sega Rally Championship', 'INDY 500', 'Motor Raid', 'Super GT 24h', 'Desert Tank', 'Rail Chase 2', ('Virtua Fighter 2', None, 'view only')],
  'The Hub marks the controls of several of these as new and untested.'),
 ('system22', 'Namco System 22', '#ff4fa8', '1 game · 5 views', 'Time Crisis VR, a third-party port built on your PC, started by the Hub directly.',
  ['Time Crisis'], None),
 ('atomiswave', 'Atomiswave', '#f28a2e', '5 games · 10 views', 'Flycast (hotd2-vr), patched: real 3D, with the world around the frame part of it.',
  ['Sports Shooting USA', 'Ranger Mission', 'Extreme Hunting', 'Extreme Hunting 2', 'Sega Clay Challenge'],
  'Title and menu screens still show stray art around the frame.'),
 ('n64', 'Nintendo 64', '#9bdc3c', '22 games · 47 views', 'acvr-ps1 with mupen64plus: real 3D by intercepting the game\'s drawing, with the HUD on a panel.',
  ['GoldenEye 007', ('Mario Kart 64', E3, 'real 3D'), 'F-Zero X', 'Star Fox 64', 'Wave Race 64', 'Perfect Dark', 'Diddy Kong Racing', 'Beetle Adventure Racing!', '1080° Snowboarding', "Cruis'n USA", "Cruis'n World", "Cruis'n Exotica", 'Hydro Thunder', 'San Francisco Rush: Extreme Racing', 'Rush 2: Extreme Racing USA', 'San Francisco Rush 2049', 'Star Wars Episode I: Racer', 'Super Mario 64', 'The Legend of Zelda: Ocarina of Time', "The Legend of Zelda: Majora's Mask", 'Banjo-Kazooie', 'Turok: Dinosaur Hunter'],
  'Stick clicks only, the same in every game: Left stick click = Start. Right stick click = Select or Coin. Click both sticks together = put the screen in front of you again. Hold both stick clicks for 3 seconds = quit and save. The left Menu button belongs to SteamVR. Not yet checked in the headset.'),
 ('ps1', 'PlayStation', '#4ff3ff', '22 games · 37 views', 'acvr-ps1 with SwanStation or Beetle: a big screen in the room with the gun in your hand.',
  ['Time Crisis', 'Time Crisis: Project Titan', 'Point Blank', 'Point Blank 2', 'Point Blank 3', 'Point Blank 2 (arcade, MAME)', 'Ghoul Panic', 'Oh! Bakyuuun (Japan)', 'Mighty Hits Special', 'Mighty Hits Special (Japan)', 'Gunfighter: The Legend of Jesse James', 'Judge Dredd', 'Extreme Ghostbusters: The Ultimate Invasion', 'Die Hard Trilogy', 'Die Hard Trilogy 2: Viva Las Vegas', 'Maximum Force', ('Resident Evil Survivor', None, 'no gun support'), 'Elemental Gearbolt', 'Rescue Shot', 'Rescue Shot Bubibo (Japan)', 'Project: Horned Owl', 'Horned Owl (Japan)'],
  'Open frame is not possible on PlayStation: these games cut everything outside 4:3.'),
 ('ps2', 'PlayStation 2', '#f28a2e', '12 games · 27 views', 'PCSX2 VR: a screen in the room, or inside the game with real depth.',
  ['Time Crisis II', 'Time Crisis 3', 'Crisis Zone', 'Vampire Night', 'Virtua Cop: Elite Edition', 'Ninja Assault', 'OutRun 2', 'OutRun 2006: Coast 2 Coast', 'Wipeout Fusion', 'Wipeout Pulse', 'XGIII: Extreme G Racing', 'XGRA: Extreme G Racing Association'],
  'Depth is measured for Virtua Cop: Elite Edition and OutRun 2006, experimental for the others.'),
 ('saturn', 'Saturn', '#ff4fa8', '5 games · 5 views', 'acvr-ps1 with Beetle Saturn: a big screen in the room with the revolver in your hand.',
  ['Virtua Cop', 'Virtua Cop 2', 'The House of the Dead', 'Area 51', 'Mechanical Violator Hakaider: Last Judgement'], None),
 ('dreamcast', 'Dreamcast', '#4ff3ff', '5 games · 7 views', 'Flycast (hotd2-vr), patched: real 3D and open frame.',
  ['The House of the Dead 2', ('Confidential Mission', None, 'rebuilt after headset issues'), ('Death Crimson OX', E1), ('Death Crimson 2', E1), ('Virtua Cop 2', E1)], None),
 ('wii', 'Nintendo Wii', '#9bdc3c', '19 games · 26 views', 'DolphinXR: real 3D with the pointer or motion controls, pointer aim within 1.3%.',
  ['Resident Evil: The Umbrella Chronicles', 'Resident Evil: The Darkside Chronicles', 'The House of the Dead: Overkill', 'Gunblade NY & L.A. Machineguns', 'Sin & Punishment: Star Successor', 'Dead Space: Extraction', "Link's Crossbow Training", ('Metroid Prime Trilogy', None, 'drops frames'), 'Donkey Kong Country Returns', 'Mario Strikers Charged', 'New Super Mario Bros. Wii', 'Punch-Out!!'],
  'Menus and HUDs sit on a screen in the room in Dead Space, Overkill, Gunblade, New Super Mario Bros. Wii and Punch-Out!!; several others still show them on your face.'),
 ('gamecube', 'GameCube', '#f6c744', '1 game · 2 views', 'DolphinXR: real 3D with the HUD on a panel.', ['F-Zero GX'], None),
]


def card(item):
    if isinstance(item, str):
        title, state, note = item, None, ''
    else:
        title, state, *rest = item
        note = rest[0] if rest else ''
    out = f'<li class="game"><span class="t">{html.escape(title)}</span>'
    if note:
        out += f'<span class="n">{html.escape(note)}</span>'
    if state:
        out += f'<span class="state {state[0]}">{state[1]}</span>'
    return out + '</li>'


def section(sid, name, colour, tally, intro, games, note):
    parts = [f'  <section class="pipe" id="{sid}" style="--accent: {colour}">',
             f'    <h2>{html.escape(name)} <span class="tally">{tally}</span></h2>',
             f'    <p class="pipe-intro">{html.escape(intro)}</p>',
             '    <ul class="titles">', *('      ' + card(g) for g in games), '    </ul>']
    if note:
        parts.append(f'    <p class="also">{html.escape(note)}</p>')
    parts.append('  </section>')
    return '\n'.join(parts)


HEAD = Path(__file__).with_name('games_head.html').read_text(encoding='utf-8')
FOOT = Path(__file__).with_name('games_foot.html').read_text(encoding='utf-8')
out = Path(sys.argv[1]) / 'games.html'
jump = '\n'.join(f'      <a href="#{s[0]}">{html.escape(s[1].split(" (")[0])}</a>' for s in SYSTEMS)
body = '\n\n'.join(section(*s) for s in SYSTEMS)
out.write_text(HEAD.replace('{{JUMP}}', jump) + body + '\n\n' + FOOT, encoding='utf-8')
print('wrote', out, sum(len(s[5]) for s in SYSTEMS), 'titles')
