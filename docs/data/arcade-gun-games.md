# Arcade 3D light gun games: MAME / TeknoParrot / DemulShooter status

> Collected 2026-10-08 by research agents from primary listings (MAME driver `GAME()` lines, TeknoParrot GameProfiles, DemulShooter, Supermodel Games.xml, emulator source). Status flags are snapshots and some cells are unverified (`?`). Corrections welcome via pull request.

## Method and caveats
- MAME status comes from the `GAME(` macro flags in each driver file. `MACHINE_NOT_WORKING` = MAME marks the set as not working. `MACHINE_IMPERFECT_GRAPHICS` alone = playable with graphics faults. No flag = MAME marks it working. Where a driver uses a local `GAME_FLAGS` macro, the expanded flags are given (namcos23: `NOT_WORKING | IMPERFECT_GRAPHICS | SUPPORTS_SAVE`; naomi: `IMPERFECT_GRAPHICS | IMPERFECT_SOUND | NOT_WORKING`).
- "Not in MAME" means the title does not appear in the MAME driver files pulled (`namco/namcos22.cpp`, `namco/namcos23.cpp`, `sega/model2.cpp`, `sega/model3.cpp`, `sega/naomi.cpp`, `sega/chihiro.cpp`, `sega/lindbergh.cpp`, `konami/hornet.cpp`, `konami/viper.cpp`). It is not evidence that the board is unsupported elsewhere.
- TeknoParrot column = a profile file exists in `TeknoParrotUi.Common/GameProfiles`. Profile filenames are not title-verified; `<GameName>` is not a field in these XMLs, so titles below are inferred from filename and marked accordingly. `GunGame` = profile contains `<GunGame>true`.
- DemulShooter column = a `Games/Game_*.cs` class exists. Class names are abbreviations; titles are inferred and marked.
- "3D?" = polygon status not confirmed by a primary source here.

## Catalog (gap rows)

| Game | Year | Maker | Board | MAME set + status flags | TeknoParrot profile? | DemulShooter? | Notes |
|---|---|---|---|---|---|---|---|
| Time Crisis (World TS2 Ver.B) | 1996 | Namco | Super System 22 | `timecris` (also `timecrisa`, `timecrisj`); flags `IMPERFECT_GRAPHICS`, no NOT_WORKING | `timecris.xml` (TeknoS22) | no | MAME: playable with imperfect graphics. 3D yes. |
| Time Crisis II (World TSS2 Ver.B) | 1997 | Namco | System 23 | `timecrs2`, `timecrs2v2b`, `timecrs2v1b`, `timecrs2v5a`; `GAME_FLAGS` = NOT_WORKING | `timecrs2v5a.xml` (TeknoS23) | no | MAME: not working. 3D yes. |
| Gunmen Wars (GM1 Ver.B) | 1998 | Namco | System 23 | `gunwars`, `gunwarsa`; `GAME_FLAGS` = NOT_WORKING | not found by filename | no | MAME: not working. 3D? (System 23 is polygon). |
| Crisis Zone (World CSZO4 Ver.B) | 1999 | Namco | System 23 Evolution (per driver `crszone`) | `crszone` + 7 variants; `GAME_FLAGS` = NOT_WORKING | `crszone.xml` (TeknoS23) | no | MAME: not working. 3D yes. |
| Silent Scope (UAD 1.33 etc.) | 1999 | Konami | Hornet | `sscope`, `sscopee`, `sscopea`, plus ~20 region/version sets and Voodoo 2 variants; all `MACHINE_NOT_WORKING` | `sscope.xml` (TeknoHornet) | no | MAME: not working in every set listed. 3D yes. |
| Silent Scope 2: Dark Silhouette / Fatal Judgement / Innocent Sweeper | 2000 | Konami | Hornet (`sscope2`) and Hornet+Voodoo 1 (`sscope2vd1` etc.) | `sscope2`, `sscope2e`, `sscope2j`, `sscope2a` + variants; `MACHINE_NOT_WORKING` | `sscope2.xml` (TeknoHornet) | no | MAME: not working. 3D yes. |
| Silent Scope EX | 2001 | Konami | Viper | `sscopex`, `sscopexu`; `MACHINE_NOT_WORKING` | `sscopex.xml` (TeknoViper) | no | MAME: not working. |
| Silent Scope Fortune Hunter (EAA) | 2002 | Konami | Viper | `sscopefh`; `MACHINE_NOT_WORKING` (driver comment: "UK only?") | `sscopefh.xml` (TeknoViper) | no | MAME: not working. |
| Silent Scope: Bone-Eater | 2014 | Konami | not stated in profile name | not in MAME | `SilentScopeBoneEater.xml` (TeknoMacaw) | no | Not in MAME. Profile GunGame=true. |
| Police 911 (UAD) | 2000 | Konami | Viper (fullbody) | `p911ud` (note: driver title "Police 24/7 (ver EAD)" is `p911ed`); `MACHINE_NOT_WORKING` | `p911ud.xml` (TeknoViper) | no | MAME: not working. |
| Police 911 2 | 2001 | Konami | Viper (fullbody dongle) | `p9112`; `MACHINE_NOT_WORKING` | `p9112.xml` (TeknoViper) | no | MAME: not working. |
| Police Trainer 2 | ? | ? | ElfLdr2 profile | not in MAME | `PoliceTrainer2.xml` (ElfLdr2), DemulShooter `PpmPoliceTrainer2` | yes (class name) | Title/year not confirmed here. Profile GunGame=true. |
| Jurassic Park III | 2001 | Konami | Viper | `jpark3`, `jpark3u`; `MACHINE_NOT_WORKING` | `jpark3u.xml` (TeknoViper) | no | MAME: not working. Gun status not confirmed here. |
| Lethal Enforcers 3 | 2004 | Konami | not stated | not in MAME | `LethalEnforcers3.xml` (TeknoParrot), DemulShooter `KonamiLethalEnforcer3` | yes | Not in MAME. Profile GunGame=true. |
| Evil Night | ? | ? | TeknoM2 profile (Model 2-class) | not in MAME | `evilngt.xml` (TeknoM2) | no | Title/year/maker not confirmed by any primary source here. Profile name only. |
| Hell Night | ? | ? | TeknoM2 profile | not in MAME | `hellngt.xml` (TeknoM2) | no | Same caveat as Evil Night. |
| Golgo 13 (series, g13jnr / g13knd / golgo13) | 1999-2000 | ? | TeknoS11 profiles | not in MAME | `golgo13.xml`, `g13jnr.xml`, `g13knd.xml` (TeknoS11) | no | Profile names only. Not confirmed polygon. |
| Dead Eye | ? | ? | TeknoS11 (`kdeadeye`) | not in MAME | `kdeadeye.xml` (TeknoS11) | no | Profile name only. |
| Point Blank 2 / 3 | ? | Namco | TeknoS11 (`ptblank2`, `ptblank3`) | not in MAME | `ptblank2.xml`, `ptblank3.xml` (TeknoS11) | no | Profile names only. Board per profile emulator type, not verified. |
| Point Blank X | ? | Namco | ES4 (per existing table) | not in MAME | `PointBlankX.xml` (TeknoParrot), DemulShooter `Es4PointBlankX` | yes | Not in MAME. Profile GunGame=true. |
| Time Crisis 3 | 2002 | Namco | pcsx2x6 profile (not PS2-hardware-verified here) | not in MAME | `timecrs3.xml` (EmulatorType pcsx2x6) | no | Not in MAME. Profile name only. 3D yes (per existing table). |
| Time Crisis 4 | 2006 | Namco | pcsx2x6 profile | not in MAME | `timecrs4.xml` (EmulatorType pcsx2x6) | no | Not in MAME. Profile name only. |
| Time Crisis 5 | 2015 | Namco | TeknoParrot (`TC5.xml`) | not in MAME | `TC5.xml` (TeknoParrot), GunGame=true | DemulShooter ES3 page per existing table; no Game_*.cs class found | Not in MAME. |
| Vampire Night | 2000 | Namco / Sega / Wow Entertainment | pcsx2x6 profile | not in MAME | `vnight.xml` (EmulatorType pcsx2x6) | no | Not in MAME. Profile name only. |
| Razing Storm | 2009 | Namco | RPCS3 profile | not in MAME | `RazingStorm.xml` (RPCS3), GunGame=true | no | Not in MAME. |
| Dead Storm Pirates | 2010 | Namco | not stated | not in MAME | no profile found by filename | no | Not found. |
| Ninja Assault (NJA1/2/3/4 Ver.A) | 2000 | Namco | NAOMI (naomim2) | `ninjasltj`, `ninjaslt`, `ninjasltu`, `ninjaslta`; `GAME_FLAGS` = NOT_WORKING | not found by filename | DemulShooter via Demul/NAOMI route (per existing table) | MAME: not working. 3D yes. |
| Confidential Mission (GDS-0001) | 2000 | Sega | NAOMI GD-ROM (naomigd) | `confmiss`; `GAME_FLAGS` = NOT_WORKING | not found | DemulShooter via Demul/NAOMI route (per existing table) | MAME: not working. |
| The Maze of the Kings (GDS-0022) | 2002 | Sega | NAOMI GD-ROM | `mok`; `GAME_FLAGS` = NOT_WORKING | not found | DemulShooter via Demul/NAOMI route (per existing table) | MAME: not working. |
| Death Crimson OX | 2000 | Ecole Software (per MAME) | NAOMI M2 (`naomim2`) | `deathcoxo`, `deathcoxj`, `deathcox`; `GAME_FLAGS` = NOT_WORKING | not found | DemulShooter via Demul/NAOMI route (per existing table) | MAME lists publisher as Ecole Software, not Sega. |
| The House of the Dead 2 | 1998 | Sega | NAOMI (`naomim2_gun`) | `hotd2`, `hotd2o`, `hotd2e`, `hotd2p`; `GAME_FLAGS` = NOT_WORKING | not found | yes (`DemulNaomi`/`Demul` route, per existing table) | MAME: not working. Requires `hod2bios`. |
| The House of the Dead (Model 2) | 1997 | Sega | Model 2C | `hotd`, `hotdo`, `hotdp`; `MACHINE_NOT_WORKING` | `hotd.xml` (TeknoModel2) | `Model2Hotd` | MAME: not working. |
| Virtua Cop (Rev B) | 1994 | Sega | Model 2 | `vcop` (Rev B), `vcopa` (Rev A); flags 0 | `vcop.xml` (TeknoModel2) | `Model2Vcop` | MAME: working (no flags). |
| Virtua Cop 2 | 1995 | Sega | Model 2 | `vcop2`; flags 0 | `vcop2.xml` (TeknoModel2) | `Model2Vcop2` | MAME: working (no flags). |
| Gunblade NY (Rev A) | 1995 | Sega | Model 2B | `gunblade`; flags 0 | `gunblade.xml` (TeknoModel2) | `Model2Gunblade` | MAME: working (no flags). |
| Ghost Squad (GDX-0012 / Rev A) | 2004 | Sega | Chihiro | `ghostsqo` (GDX-0012), `ghostsqu` (Rev A); `MACHINE_NO_SOUND | MACHINE_NOT_WORKING` | not found by filename | `CxbxGsquad` (Cxbx route) | MAME: not working. |
| Ghost Squad Evolution | 2007 | Sega | Lindbergh | `ghostsev`; `MACHINE_NOT_WORKING | UNEMULATED_PROTECTION` | `GSEVO.xml` (Lindbergh), `GSEVOELF2.xml` (ElfLdr2); GunGame=true | `LindberghGsquadEvo` | MAME: not working. |
| The House of the Dead 4 (Export Rev B) | 2005 | Sega | Lindbergh | `hotd4`, `hotd4a`; `MACHINE_NOT_WORKING | UNEMULATED_PROTECTION` | `HOTD4.xml` (Lindbergh) and `HOTD4ELF2.xml`; GunGame=true | `LindberghHotd4` | MAME: not working. |
| The House of the Dead 4 Special | 2006 | Sega | Lindbergh | not in MAME | `HOTD4SP.xml` (Lindbergh), `HOTD4SPELF2.xml`; GunGame=true | `LindberghHotd4Sp` | Not in MAME. |
| The House of the Dead EX | 2008 | Sega | Lindbergh | `hotdex`; `MACHINE_NOT_WORKING | UNEMULATED_PROTECTION` | `HOTDEX.xml` (ElfLdr2); GunGame=true | `LindberghHotdEx` | MAME: not working. |
| Let's Go Jungle (Export) | 2006 | Sega | Lindbergh | `letsgoju`; `MACHINE_NOT_WORKING | UNEMULATED_PROTECTION` | `LGJ.xml` (ElfLdr2), `LGJS.xml` (Lindbergh), `LGJSElf2.xml` | `LindberghLgj`, `LindberghLgjsp` | MAME: not working. |
| Rambo (Export) | 2008 | Sega | Lindbergh | `rambo`; `MACHINE_NOT_WORKING | UNEMULATED_PROTECTION` | `Rambo.xml` (Lindbergh), `RamboElf2.xml` (ElfLdr2); GunGame=true | `LindberghRambo` | MAME: not working. |
| The Lost World: Jurassic Park (Rev A) | 1997 | Sega | Model 3 | `lostwsga` (Rev A), `lostwsgp` (location test); `GAME_FLAGS`-style = NOT_WORKING | `lostwsga.xml` (TeknoModel3) | no | MAME: not working. |
| The Ocean Hunter | 1998 | Sega | Model 3 (5881 variant) | `oceanhun`, `oceanhuna`; NOT_WORKING | `oceanhun.xml` (TeknoModel3) | no | MAME: not working. |
| L.A. Machineguns | 1998 | Sega | Model 3 (per profile) | not in MAME | `lamachin.xml` (TeknoModel3) | no | Not in MAME. Profile name only. |
| Transformers (arcade) | ? | Sega | TeknoParrot profile | not in MAME | `Transformers.xml` (TeknoParrot), `TransformersShadowsRising.xml`; GunGame=true | DemulShooter `RwTransformers`, `Re2Transformers2` | Not in MAME. Title/year for the Sega release not confirmed here. |
| Alien Extermination | ? | ? | TeknoParrot | not in MAME | `AliensExtermination.xml` (TeknoParrot); GunGame=true | no | Not in MAME. |
| Aliens Armageddon | 2014 | Raw Thrills | ElfLdr2 profile | not in MAME | `AliensArmageddon.xml` (ElfLdr2); GunGame=true | `RtAliensArmageddon` | Not in MAME. |
| Jurassic Park Arcade | 2014 | Raw Thrills | ElfLdr2 profile | not in MAME | `JurassicPark.xml` (ElfLdr2); GunGame=true | `RtJurassicPark` | Not in MAME. |
| Terminator Salvation | 2010 | Raw Thrills | ElfLdr2 profile | not in MAME | `Terminator.xml` (ElfLdr2); GunGame=true | `RtTerminatorSalvation` | Not in MAME. |
| Halo: Fireteam Raven | 2019 | Raw Thrills | ElfLdr2 profile | not in MAME | `Halo.xml` (ElfLdr2); GunGame=true | no | Not in MAME. Filename-only match. |
| Target Terror (Gold) | ? | Raw Thrills | ElfLdr2 profile | not in MAME | `TargetTerrorGold.xml` (ElfLdr2); GunGame=true | `RtTargetTerror` | Not in MAME. |
| The Walking Dead | ? | Raw Thrills / Play Mechanix? (not verified) | ElfLdr2 profile | not in MAME | `WalkingDead.xml` (ElfLdr2); GunGame=true | `RtWalkingDead` | Not in MAME. Maker not verified. |
| NERF (arcade) | ? | ? | TeknoParrot | not in MAME | `NERF.xml` (TeknoParrot); GunGame=true | no | Not in MAME. |
| Wild West Shootout | ? | ? | TeknoParrot | not in MAME | `WildWestShootout.xml` (TeknoParrot); GunGame=true | `WndColtWildWestShootout` | Not in MAME. |
| Pull the Trigger | ? | ? | TeknoParrot | not in MAME | `PullTheTrigger.xml` (TeknoParrot); GunGame=true | no | Not in MAME. |
| Medaru No Gunman | ? | ? | TeknoParrot | not in MAME | `MedaruNoGunman.xml` (TeknoParrot); GunGame=true | `RwGunman` (maybe) | Not in MAME. |
| Music Gun Gun 2 | ? | ? | OpenParrot | not in MAME | `MusicGunGun2.xml` (OpenParrot); GunGame=true | `TtxGungun2` | Not in MAME. |
| Operation Tiger | 1998 | Taito | TeknoTPJC profile (`optiger`, `optigersm`) | not in MAME | `optiger.xml`, `optigersm.xml` (TeknoTPJC) | no | Not in MAME. Board name not confirmed here. |
| Gun Busters | ? | Taito (not verified) | not found | not in MAME | not found | no | Not found. |
| Sega Golden Gun | ? | Sega | RingWide per existing table | not in MAME | no TeknoParrot profile by filename | `RwSGG` | Not in MAME. |
| Transformers: Human Alliance | ? | Sega | RingWide per existing table | not in MAME | no profile by filename | `RwTransformers` (probable) | Not in MAME. Mapping unverified. |
| Big Buck Hunter (series) | 2000-2020 | Incredible Technologies / Raw Thrills | not stated | not in MAME | `BBHHDWild.xml`, `BBHHome.xml`, `BBHPro.xml`, `BBHWorld.xml` (ElfLdr2); GunGame not set on these | no | Only the BBH profile names seen. Not confirmed gun-based in profile. |
| Ghost Squad (Chihiro) | 2004 | Sega | Chihiro | see row above | none | `CxbxGsquad` | duplicate row kept for clarity. |

## Counts (this pass)

- MAME-driver games found (3D, Namco/Sega/Konami): Time Crisis (working w/ imperfect graphics); Time Crisis II, Gunmen Wars, Crisis Zone (not working); Silent Scope series incl. Bone-Eater (not in MAME), Silent Scope/2/EX/Fortune Hunter (not working); Police 911 and 911 2 (not working); Jurassic Park III (not working); Ninja Assault, Confidential Mission, Maze of the Kings, Death Crimson OX, HotD2 (NAOMI, not working); Ghost Squad (Chihiro, not working); Ghost Squad Evolution, HotD4, HotD EX, Let's Go Jungle, Rambo (Lindbergh, not working); Lost World, Ocean Hunter (Model 3, not working); HotD (Model 2, not working); Virtua Cop, Virtua Cop 2, Gunblade NY (Model 2, working, no flags).
- MAME sets with no NOT_WORKING flag among the gun rows: `timecris` (+ `timecrisa`, `timecrisj`; IMPERFECT_GRAPHICS only), `vcop` (+ `vcopa`), `vcop2`, `gunblade`.
- TeknoParrot rows with GunGame=true in this pass: ~25 profiles (TC5, Lethal Enforcers 3, Point Blank X, Razing Storm, Silent Scope Bone-Eater, Alien Extermination, Terminator, Halo, Target Terror, Walking Dead, HotD4, HotD4 SP, HotD EX, Rambo, GSEVO, Aliens Armageddon, Jurassic Park, NERF, Wild West Shootout, Pull the Trigger, Medaru No Gunman, Music Gun Gun 2, Police Trainer 2, Transformers, Lost Land, etc.). Full GunGame=true list is 175 profile files in the repo at this commit.
- DemulShooter gun game classes: about 70 `Games/Game_*.cs` class files (Model2, Lindbergh, Chihiro/Cxbx, Raw Thrills `Rt*`/`Rw*`, Windows-PC `Wnd*`, Konami Lethal Enforcers 3, ES4 Point Blank X).

## Sources (exact file URLs)

MAME (`https://github.com/mamedev/mame/blob/master/src/mame/...`):
- https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos22.cpp (Time Crisis, lines ~6440-6442)
- https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos23.cpp (Time Crisis II, Gunmen Wars, Crisis Zone; GAME_FLAGS line 8829)
- https://github.com/mamedev/mame/blob/master/src/mame/konami/hornet.cpp (Silent Scope, Silent Scope 2)
- https://github.com/mamedev/mame/blob/master/src/mame/konami/viper.cpp (Police 911, 911 2, Silent Scope EX, Silent Scope Fortune Hunter, Jurassic Park III)
- https://github.com/mamedev/mame/blob/master/src/mame/sega/model2.cpp (Virtua Cop, Virtua Cop 2, HotD, Gunblade NY)
- https://github.com/mamedev/mame/blob/master/src/mame/sega/model3.cpp (Lost World, Ocean Hunter)
- https://github.com/mamedev/mame/blob/master/src/mame/sega/naomi.cpp (Ninja Assault, Confidential Mission, Maze of the Kings, Death Crimson OX, HotD2; GAME_FLAGS line 10915)
- https://github.com/mamedev/mame/blob/master/src/mame/sega/chihiro.cpp (Ghost Squad)
- https://github.com/mamedev/mame/blob/master/src/mame/sega/lindbergh.cpp (HotD4, HotD EX, Ghost Squad Evolution, Let's Go Jungle, Rambo)

TeknoParrot (cloned `https://github.com/teknogods/TeknoParrotUI`, depth 1, commit at fetch time):
- https://github.com/teknogods/TeknoParrotUI/tree/master/TeknoParrotUi.Common/GameProfiles

DemulShooter: https://github.com/argonlefou/DemulShooter (`DemulShooter/Games/` (Game_*.cs class list)  and `DemulShooterX64/Games/`).

