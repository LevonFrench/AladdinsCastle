# Arcade 3D racing games: MAME / Supermodel / TeknoParrot / decomp status

> Collected 2026-10-08 by research agents from primary listings (MAME driver `GAME()` lines, TeknoParrot GameProfiles, DemulShooter, Supermodel Games.xml, emulator source). Status flags are snapshots and some cells are unverified (`?`). Corrections welcome via pull request.

Legend: MAME flag "ok" = no MACHINE_NOT_WORKING or MACHINE_IMPERFECT flags on the GAME() line. "NW" = MACHINE_NOT_WORKING. "IMP" = an IMPERFECT flag only. "decomp" = namco22-decompile native playable. "TP" = TeknoParrot. "Supermodel" = listed in Supermodel Config/Games.xml (listing only, not a compatibility grade). Years come from the GAME() line; "?" means not read.

Supermodel note: Games.xml lists Model 3 titles only (Scud Race, Scud Race Plus, Daytona USA 2, Daytona USA 2 Power Edition, Dirt Devils, Harley-Davidson and L.A. Riders, Le Mans 24). Its presence is a listing, not a status grade. The only status claim retained for Scud Race is the first-pass "almost perfectly with sound" (gametechwiki, earlier pass, unverified here).

## Catalog

| Game | Year | Maker | Board | Best route (MAME set + flags / Supermodel / Model2Emu / TeknoParrot / native decomp / PC port) | Control set (wheel, shifter, pedals, extras) | Notes |
|---|---|---|---|---|---|---|
| Virtua Racing | 1992 | Sega | Model 1 | MAME `vr` (ok, no flags). Layout `layout_vr`. | ? (wheel/shifter/pedals not read from a primary source) | Earliest polygon racer in this list. Simplest true-3D candidate. |
| Daytona USA (Rev A) | 1994 | Sega | Model 2 | MAME `daytona` (ok). Also `daytona93` (1993, ok), `daytonase` (SE, ok), `daytonas` (ok). TP `daytona`, TeknoModel2. | Wheel (TP Analog0). Shifter ?. | Turbo hacks and To The MAXX / GTX are NW hacks. |
| Sega Rally Championship (Twin/DX, Rev C) | 1995 | Sega | Model 2 | MAME `srallyc` and variants (NW). TP `srallyc`, TeknoModel2. | Wheel (TP Analog0). | Driver NW in MAME; TP route is the cleaner path. |
| Manx TT Superbike (DX/Twin, Rev D) | 1995 | Sega | Model 2 | MAME `manxtt` (NW). `manxttdx` (NW). | Motorcycle (handlebars / seat). Exact controls ? | Motorcycle racer. |
| Indy 500 (Twin, Rev A) | 1995 | Sega | Model 2 | MAME `indy500` (NW, `layout_vr`). | ? | Layout exists in MAME for VR-style cabinet. |
| Motor Raid (Twin) | 1997 | Sega | Model 2 | MAME `motoraid` (IMP sound). `motoraiddx` (NW). | ? | Bike/vehicle, Model 2B-era. |
| Sega Touring Car Championship | 1996 | Sega | Model 2 | MAME `stcco` / `stcc` / `stcca` / `stccb` (NW, `layout_vr`). | ? | Touring car. |
| Super GT 24h / Scud Race (combined driver) | 1996 | Sega | Model 3 | MAME `sgt24h` (Model 2, NW). Scud: MAME `scud` (NW, IMP), `scuddx` (NW), `scuddxo` (NW), `scudau` (NW, IMP). | Twin and DX cabinets. Wheel/shifter ? | See Scud Race row. |
| Scud Race (Twin/DX) | 1996 | Sega | Model 3 Step 1 | MAME `scud` (NW, IMP). Supermodel `scud`. TP `scud`, TeknoModel3. | Wheel (TP Analog0). Twin/DX dual-seat. | Earlier pass says "almost perfectly with sound" (gametechwiki). Not re-verified. |
| Scud Race Plus | 1997 | Sega | Model 3 | MAME `scudplus` (NW). Supermodel `scudplus`. TP `scudplus`, TeknoModel3. | Wheel (TP Analog0). | Successor to Scud Race. |
| Le Mans 24 (Rev B) | 1997 | Sega | Model 3 | MAME `lemans24` (NW, IMP graphics). Supermodel listed. | ? | Model 3 sports car. |
| Harley-Davidson and L.A. Riders (Rev B) | 1997 | Sega | Model 3 | MAME `harley` (NW). `harleya` (NW). Supermodel `harley` (input type `harley`). TP `Harley`, ElfLdr2 emulator (not TeknoModel3). | Handlebar-style riding control (Supermodel input type "harley"). TP profile lists Wheel. | Motorcycle riding. Different control path per emulator. |
| Daytona USA 2: Battle on the Edge (Rev A) | 1998 | Sega | Model 3 | MAME `daytona2` (NW). Supermodel `daytona2`. TP `daytona2`, TeknoModel3. | Wheel (TP Analog0). | |
| Daytona USA 2: Power Edition | 1998 | Sega | Model 3 | MAME `dayto2pe` (NW, IMP). Supermodel `dayto2pe`. | Wheel (TP Analog0). | |
| Sega Rally 2 (Deluxe) | 1998 | Sega | Model 3 | MAME `srally2`, `srally2dx` (NW). TP `srally2`, TeknoModel3. | Wheel (TP Analog0). | Model 3 successor to Sega Rally. |
| Dirt Devils | 1998 | Sega | Model 3 | MAME `dirtdvls` family (NW, IMP). Supermodel `dirtdvls` family. | ? | Off-road vacuum-themed driver. |
| Magical Truck Adventure | 1998 | Sega | Model 3 | MAME `magtruck` (NW, IMP). | ? | Truck driving. |
| Emergency Call Ambulance | 1999 | Sega | Model 3 | MAME `eca`, `ecaj`, `ecau` (NW, IMP). | ? | Ambulance driving. |
| Sega Ski Super G | 1996 | Sega | Model 2 | MAME `skisuprg` (NW, unemulated protection). | ? | Skiing, not driving. |
| Initial D4 (Rev D) | 2007 | Sega | Lindbergh | MAME `initiad4` (NW). Lindbergh driver. | Wheel (per first pass). | Steering sensitivity issue noted in first pass (Batocera). |
| OutRun 2 SP SDX | 2006 | Sega | Lindbergh | MAME `outr2sdx` (NW). | ? (shifter/pedals not read). | First-pass listing says Lindbergh. |
| Sega Race TV (Export) | 2007 | Sega | Lindbergh | MAME `segartv` (NW). TP `segartv` (Lindbergh emulator), TP `segartvelf2`. | Wheel (TP Analog0). | |
| Hummer Extreme | 2009 | Sega | Lindbergh | MAME `hummerxt` (NW). TP `hummerextreme` and `Hummer` (ElfLdr2). | Genre wheel (per first pass, Batocera). | |
| Crazy Taxi | 2000 (general knowledge, not from MAME GAME line) | Sega | NAOMI | MAME `naomi.cpp` contains `crzytaxi` INPUT_PORTS (driver present). GAME() line not captured by this pass. | Drive gear button on P1 (per `crzytaxi` input port name "Drive Gear"). | Status ?. Verify GAME line in naomi.cpp before relying on it. |
| Ridge Racer (World RR2 / Japan RR1) | 1993 | Namco | System 22 | MAME `ridgerac`, `ridgeracb`, `ridgeracc`, `ridgeracj` (IMP graphics). TP `ridgerac`, TeknoS22. | Wheel (TP Analog0). | Full Scale and 3-monitor variants NW. |
| Ridge Racer 2 (World RRS2 / Japan RRS1) | 1994 | Namco | System 22 | MAME `ridgera2`, `ridgera2j`, `ridgera2ja`, `ridgera28` (IMP). TP `ridgera2j`, TeknoS22. | Wheel (TP Analog0). | |
| Rave Racer (World RV2 Ver.B) | 1995 | Namco | System 22 | MAME `raverace` (IMP). TP `raverace`, TeknoS22. **Native decomp: playable, online up to 8 players.** | Wheel (TP Analog0, force feedback per TP hint). | Best open-source native route in this table. |
| Ace Driver (World AD2) | 1994 | Namco | System 22 | MAME `acedrive` (IMP). TP `acedrive`, TeknoS22. **Native decomp: playable, force feedback, developer screens.** | Wheel with force feedback (decomp README). | Native decomp route is open-source. |
| Ace Driver: Victory Lap (World ADV2) | 1996 | Namco | System 22 | MAME `victlap`, `victlapa`, `victlapj` (IMP). | ? | Native decomp does not list Victory Lap. |
| Alpine Racer (World AR2 Ver.D) | 1994 | Namco | System 22 | MAME `alpinerd`, `alpinerc`, `alpinerjc` (IMP). TP `AlpineRacer`, TeknoS22, AnalogJoystick input. | Ski poles/joystick (TP AnalogJoystick). | Not in the namco22-decompile list. |
| Alpine Racer 2 (World ARS2 Ver.B) | 1996 | Namco | System 22 | MAME `alpinr2b`, `alpinr2a` (IMP). | ? (ski-style joystick per Alpine Racer family, unverified here) | Not in namco22-decompile list. |
| Cyber Cycles (World CB2 Ver.C) | 1995 | Namco | System 22 | MAME `cybrcycc`, `cybrcyccj` (IMP). | Cycle handlebars? (unverified) | Not in namco22-decompile list. |
| Dirt Dash (World DT2 Ver.C) | 1995 | Namco | System 22 | MAME `dirtdash`, `dirtdasha`, `dirtdashb`, `dirtdashj` (IMP). **Native decomp: playable, all five stages.** | ? (decomp README gives no control detail) | Off-road racer. Strong open-source route. |
| Prop Cycle (World PR2 Ver.A) | 1996 | Namco | System 22 | MAME `propcycl`, `propcyclj` (IMP). **Native decomp: playable start to finish.** | ? | Unusual vehicle concept, controls not verified. |
| Armadillo Racing | 1997 | Namco | System 22 | MAME `adillor`, `adillorj` (IMP). | ? | |
| Motocross Go! (World MG3 Ver.A) | 1997 | Namco | System 23 | MAME `motoxgo`, `motoxgov2a`, `motoxgov1a` (ok flags). | Motocross bike (handlebars?, unverified) | Namco System 23. |
| Downhill Bikers | 1997 | Namco | System 23 | MAME `downhill`, `downhillu` (ok flags). | Mountain bike (handlebars?, unverified) | Namco System 23. |
| Race On! (World RO2 Ver.A) | 1998 | Namco | System 23 | MAME `raceon`, `raceonj` (ok flags). TP `raceonj`, TeknoS23. | Wheel (TP Analog0). | |
| Tokyo Wars | 1996 | Namco | System 22 | MAME `tokyowar` (not captured in this pass). **Native decomp: playable.** TP `tokyowar`, TeknoS22. | Tank controls (not a racer; decomp). | Not a racer, listed as requested. |
| Pocket Racer (Japan PKR1) | 1996 | Namco | System 11 | MAME `pocketrc` (ok flags). | ? | |
| Wangan Midnight Maximum Tune (MR) | 2000s (not read) | Namco | (not read) | TP `wanganmr`, emulator `pcsx2x6` (PS2-based route). `wanganmd` TP profile present. | Wheel (TP Analog0). | Route is a PS2 emulation path, not a native arcade emulator. |
| Thrill Drive (GE713UFB et al.) | 1998 | Konami | Hornet | MAME `thrilldgeu` and 7 other sets (NW, IMP). TP `thrilldgeu`, `thrilld`. | Wheel (TP profile). | |
| Thrill Drive 2 | 2001 | Konami | Viper | MAME `thrild2`, `thrild2j`, `thrild2a`, `thrild2c` (NW). | ? | |
| Thrill Drive 4 | ? | Konami | ? | TP `ThrillDrive4` profile present. | Wheel (TP Analog0). | Year and board not read. |
| GTI Club: Rally Cote D'Azur | 1996 | Konami | Konami GTI Club board (MAME gticlub.cpp; board name not read) | MAME `gticlub`, `gticlubu`, `gticluba`, `gticlubj` (IMP graphics). TP `gticlub`, TeknoGClub. | Wheel (TP Analog0). | |
| GTI Club: Corso Italiano | 2000 | Konami | Viper | MAME `gticlub2`, `gticlub2ea` (NW). TP `gticlub2`, TeknoViper. | Wheel (TP Analog0). | |
| GTI Club 3 | ? | Konami | (not read) | TP `GtiClub3`, OpenParrot emulator. | Wheel (TP Analog0). | |
| Midnight Run: Road Fighter 2 | 1995 | Konami | ZR107 | MAME `midnrun`, `midnrunj`, `midnruna`, `midnruna2` (IMP graphics). | ? | |
| Winding Heat | 1996 | Konami | ZR107 | MAME `windheat`, `windheatu`, `windheatj`, `windheata` (IMP graphics). | ? | |
| Battle Gear (VER.2.40A) | 1999 | Taito | Taito Type X (taitotz driver) | MAME `batlgear` (ok flags). `batlgr2` (ok), `batlgr2a` (ok, Side by Side conversion). TP `BattleGear4`, `BattleGear4Tuned`, OpenParrot. | Wheel, shifter and pedals (TP Analog Wheel tag). | Battle Gear 2 and Side by Side conversion both listed. |
| Landing Gear | 1995 | Taito | taitojc | MAME `landgear`, `landgearj`, `landgeara`, `landgearja` (ok flags). | Aircraft yoke (general knowledge, unverified). | Plane, not car. Listed as requested. |
| Cruis'n Blast | 2015-ish (not read) | Raw Thrills | (not read) | TP `CruisnBlast`, ElfLdr2 emulator. | Wheel (TP Analog0). | Not found in MAME driver search. |
| Hydro Thunder | 1999 (not read) | Midway | (not read) | TP `HydroThunder`, TeknoParrot. | Boat wheel (TP Analog0 Wheel tag). | Not found in MAME driver search. |
| San Francisco Rush | (not read) | Midway / Atari | (not read) | TP `sfrush`, `sfrushrk`, TeknoVegas emulator. | Wheel (TP Analog14). | Not found in MAME driver search. |
| Hot Wheels | (not read) | Sega / Raw Thrills | (not read) | TP `HotWheels` profile present. | ? | |
| Star Wars Racer Arcade | 2000 | Sega / LucasArts | Hikaru | MAME `swracer` (no NW flag; `hikaru.cpp`). | ? | Hikaru board, not Model 3. |
| Cruis'n USA / World / Exotica | (not read) | Midway | (not read) | Not located in this pass. | ? | |
| Fast & Furious (Raw Thrills) | (not read) | Raw Thrills | (not read) | Not located in this pass. | ? | |
| Initial D Arcade Stage 4/5, Hummer, Sega Race TV, Harley | see first-pass table | Sega | Lindbergh | MAME `initiad4`, `hummerxt`, `segartv` (NW). Listing only. | See first-pass table | |

## Native and open-source route summary

- Namco native decomp (playable, open source): Rave Racer (1995), Ace Driver (1994), Dirt Dash (1995), Prop Cycle (1996), Tokyo Wars (tank, 1996). Source: namco22-decompile README.
- Supermodel (Model 3, open source): Scud Race, Scud Race Plus, Daytona USA 2, Daytona USA 2 Power Edition, Dirt Devils, Harley-Davidson and L.A. Riders, Le Mans 24. Listing only in Games.xml.
- MAME (open source, GAME() flags): Virtua Racing and Daytona USA families (Model 1 / Model 2) have no NW or IMP flag on the working set lines. Most Model 2 and Model 3 racers are NW.
- TeknoParrot (closed-source emulation route, profile metadata only here): most Namco, Sega and Konami racers have profiles.

## Ranked shortlist: first true-3D VR slice (open-source, working, distinct controls)

Ranking criteria: MAME or native decomp working status with no NW flag, open-source route, true polygon 3D, and a control set that the VR slice can exercise. Controls beyond "Wheel" are unverified and need a second pass.

1. Virtua Racing (Sega Model 1, 1992): MAME `vr`, no NW or IMP flag. Simplest real 3D polygon racer. Wheel and pedals likely (unverified).
2. Daytona USA (Sega Model 2, 1994): MAME `daytona`, no NW or IMP flag on the Rev A line. Wheel and shifter likely (unverified). Strongly cabinet-representative.
3. Rave Racer (Namco System 22, 1995): native decomp, playable, online up to 8. Wheel with force feedback per TeknoParrot hint. Source code makes VR integration feasible.
4. Ace Driver (Namco System 22, 1994): native decomp, playable, force feedback and developer screens. Wheel with force feedback.
5. Scud Race (Sega Model 3, 1996): Supermodel route, Games.xml listed. Twin/DX dual-seat cabinet. Earlier pass reported near-perfect status (unverified here).
6. Dirt Dash (Namco System 22, 1995): native decomp, playable, all five stages. Off-road racer. Control set not verified from the decomp README.

Alternates if a control-variety slice is wanted: Alpine Racer (ski joystick, but no open-source route found here; TeknoParrot only), Harley-Davidson and L.A. Riders (handlebar input in Supermodel, NW in MAME).

## Unresolved items (not verified in this pass)

- Crazy Taxi GAME() line in MAME naomi.cpp was not extracted (INPUT_PORTS `crzytaxi` present). Year 2000 is general knowledge.
- Control sets for most titles other than what TeknoParrot profiles state (Wheel, AnalogJoystick, Gas, Analog14 for SF Rush).
- Cruis'n USA, Cruis'n World, Cruis'n Exotica, San Francisco Rush 2049, California Speed, Off Road Thunder, Arctic Thunder, Road Burners, Hydro Thunder MAME presence not confirmed by this pass (search returned no hits).
- Fast & Furious, Big Rigs, H2Overdrive, Winding Heat/Midnight Run control specifics.
- Taito Chase Bombers, Densha de Go, Sega Super GT (Model 3 variant beyond Scud Race combined), Ferrari F355 Challenge, Sega Rally 3, Super GT, Wangan Midnight arcade board.

## Sources

- MAME drivers (GAME() lines fetched via `gh api repos/mamedev/mame/contents/src/mame/<file>`, `Accept: application/vnd.github.raw`):
  - https://github.com/mamedev/mame/blob/master/src/mame/sega/model1.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/sega/model2.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/sega/model3.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/sega/lindbergh.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/sega/naomi.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/sega/hikaru.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos22.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos23.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos11.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos12.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/konami/hornet.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/konami/viper.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/konami/zr107.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/konami/gticlub.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/taito/taitotz.cpp
  - https://github.com/mamedev/mame/blob/master/src/mame/taito/taitojc.cpp
- Supermodel : https://github.com/trzy/Supermodel `Config/Games.xml` (Model 3 listing only)
- TeknoParrot profiles (raw fetch, `TeknoParrotUi.Common/GameProfiles/`): https://github.com/teknogods/TeknoParrotUI/blob/master/TeknoParrotUi.Common/GameProfiles/ (files: AlpineRacer.xml, BattleGear4.xml, CruisnBlast.xml, Daytona3.xml, Daytona3NSE.xml, GtiClub3.xml, Harley.xml, Hummer.xml, HydroThunder.xml, ThrillDrive4.xml, wanganmr.xml, raverace.xml, ridgera2j.xml, ridgerac.xml, scud.xml, scudplus.xml, segartv.xml, sfrush.xml, srally2.xml, srallyc.xml, tokyowar.xml, daytona.xml, daytona2.xml, gticlub.xml, gticlub2.xml, acedrive.xml, aquarush.xml, racedriv.xml, raceonj.xml, racingj2.xml)
- namco22-decompile : https://github.com/spacestate1/namco22-decompile README
