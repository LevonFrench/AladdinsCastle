# U1 UX review: panel content, reading order, scannability

Lens: UX architect, with the UX researcher's scanning and reading lens. Review only; nothing edited except this report.
Files reviewed (in `website/`): `index.html`, `games.html`, `how-it-works.html`, `style.css`, `flow.css`; captures d0, d1, d4, d5, d8, p4 in `docs/reviews/2026-10-22-studio-experts/captures/`; facts checked against `docs/status/current.md`.
Held: 3D backdrop with panels in front (panels on one side, ~56% width, so each panel gets narrower and taller), pixel headings, honest E-levels, title AC VR Hub.

Verdict: the facts are accurate and the status honesty is good (counts reconcile: 143 games = 128 named + 8 + 7 unnamed; 279 views). The problems are vocabulary and density, not truth. A fan landing on the page cannot tell in the first screen what they need, what the terms mean, or what "E2" means.

## Findings, ranked

### 1. HIGH: "open frame" is used about 14 times and never defined; "theatre", "diorama", "surround view" are undefined too
- `index.html:172, 173, 174, 175, 176, 188, 192, 195`; `games.html:62, 81, 102, 107, 150, 180, 205, 242`; `how-it-works.html:169, 186, 241`.
- Effect: the page's core selling vocabulary (real 3D, inside the game, open frame, big screen) is what a visitor cannot decode. `games.html:40` names four views but does not explain them. "Theatre" appears only in a pill (`index.html:173`) while every sentence says "big screen" (current.md calls it "Theatre screen").
- Remedy: add one glossary panel to stop 1, "Four ways to see a game": Real 3D (the game's world drawn in true depth); Inside the game (you sit in the game's camera, head tracked); Big screen (a flat screen in the room, gun in hand); Open frame (the picture is not boxed to a screen; the game's world continues past its edges). The open-frame and diorama definitions are my reading of current.md, so the owner should confirm the wording. Use "Big screen" everywhere; replace "theatre" in `index.html:173`. Remove or define "surround view" and "diorama" (`index.html:191, 192`).

### 2. HIGH: E-pills do not read without the legend, and the legend scrolls away
- Only legend is inside stop 4's head (`index.html:169`); pills at `172-177`, `191-196`, `244`; CSS `style.css:225-233`.
- Effect: stop 5 and stop 8 show bare "E2" / "E2 · three games E1" with no legend in view (dot-nav jumps skip stop 4). Pills are 11px all caps, so "E3 SEGA RALLY 2 DX · REST E2" reads as a shout. Amber marks E2, the normal state of about 140 games, so it reads as "warning/unfinished". "Desk or headset" (`index.html:177`) is not a status but wears the amber status pill.
- Remedy: put the meaning in the pill: "E3 played in headset", "E2 offline-proven", "E1 early"; sentence case, drop `text-transform` for pills. Add a one-line legend (three small pills) at the top of stops 4, 5 and 8, or one fixed strip under the HUD. Give non-status pills (Pinball) a neutral class, not `.wip`. Split mixed pills: "E3 Sega Rally 2 DX" plus "Others E2".

### 3. HIGH: the first screen does not tell a visitor what they need to know
- `index.html:67-68, 76-79`; capture d0 (hero buttons on the fold at 800x500, stat row below it).
- Effect: "front door", "panel inside SteamVR", "inside the game" and "real 3D" in 59 words. That it is Windows-only and needs SteamVR, and that no games are included, appear only at stops 6-7 (`index.html:211, 223`). The stat row (143 "in the Hub library", 561 pinball tables, 279 views) states the developer's PC counts as product claims and shows no accepted count; the honest number (3 accepted) is only on the games page (`games.html:41`). "12 systems, from Model 3 to Wii" is wrong in shape (it includes MAME and ends elsewhere).
- Remedy: lede to about 35 words: "AC VR Hub is a Windows app that starts your own arcade and console light gun and racing games in VR through SteamVR: real 3D, inside the game, or on a big screen in the room. It ships no games." Move line 68 to the footer. Stats: "143 games supported", "12 systems, arcade boards and consoles", "3 accepted in the headset", "279 views (ways to see them)". Move 561 into the Visual Pinball panel, worded "found on the developer's PC".

### 4. MEDIUM: reading order puts "what is the Hub" eighth of nine stops
- Stops at `index.html:85, 110, 148, 164, 183, 202, 218, 234`.
- Effect: a first-time reader meets controls, guns and ghost controls before learning there is a launcher; licences (stop 6) sit between consoles and the Hub.
- Remedy: ideal order is Welcome, The Hub, How you play, Guns, Ghost controls, Arcade, Consoles, Where it stands, Open source with links. Caveat: stop numbers are wired to 3D exhibits (`arcade.js:838-850` use `stop: 2`, `stop: 3`), so a reorder is a scene change. Cheaper alternative: keep the order and carry the Hub essentials plus "no game content" in the hero (finding 3).

### 5. MEDIUM: panels mix release-note detail with visitor value
- `index.html:103` (rules panel, ~50 words), `115` (59-word head with "four-specialist studio review"), `174` ("Aim error cut from 7.3% to 0.3%"), `192`, `193`, `194`, `195`.
- Effect: stops 4 and 5 are about 250 words each across six panels, the densest on the page. Line 174 also overstates: current.md gives 7.3% to 0.3% for House of the Dead only, not for all 14 Model 2 games.
- Remedy: each system panel = title, route line, one benefit sentence of at most 18 words, one pill, one "still open" fragment. Move 7.3%, 1.3%, 5x and 4x to how-it-works notes. Fix 174: "Real 3D and open frame, with the game's own sound. House of the Dead aim error fell from 7.3% to 0.3% of the screen." Split the rules panel (103): "Buttons the headset keeps" (left Menu, right headset button) stays; Start/Plus/Home moves to how-it-works. Stop 2's head: keep one sentence on own models; put "nine of twelve are new, no headset verdict yet" into a status panel with an E-pill.

### 6. MEDIUM: panel titles are topic labels or counts, not specific
- `index.html:95, 99, 102, 140, 158, 210, 226-228`.
- Remedy (about 5 words each): "Light guns" to "Aim, shoot, reload off-screen"; "Racing" to "Racing: stick and triggers"; "Rules from real headset sessions" to "Buttons you cannot use, and what replaces them"; "Sixteen control models" to "Wheels, pedals and sticks you can grab"; "Built on the people doing this work" to "Built on existing emulators".

### 7. MEDIUM: two status scales share one colour set across pages
- `how-it-works.html:33-37` (Working / In progress / Not yet, with E3/E2 attached), steps at `53-155`, route heads at `166-226`; `games.html:34-36`.
- Effect: green means "E3 accepted" on route heads but "Working (covered by tests)" on every flow step, so "Overlay mode: Working" looks headset-accepted. "Not yet" (grey) and E1 "early" (grey) are different meanings. "Clean exit: Not yet" and "Controls: In progress" (`155`, `149`) are not traceable to current.md; verify before keeping.
- Remedy: flow steps use different wording ("Built", "Being finished", "Not yet") in a neutral outline; reserve green/amber/grey pills for E3/E2/E1 only, with the same key text on both pages.

### 8. MEDIUM: Game List accent colours collide with status colours; caution notes are quiet
- `games.html:60, 148, 252` green accents, `79, 272` amber; notes `flow.css:204` (13px grey): "drops frames", "no gun support", "view only", "firing unproven". `games.html:76` "Also tried in a headset by the developer" maps to no key level.
- Effect: a green left border on Model 3, N64 and Wii looks like E3; the most important per-game caveats are the least visible; "tried" is a fourth unexplained status.
- Remedy: accent borders from non-status colours (cyan, pink, violet). Style caution notes as small outline chips. Reword 76 to an E-level statement or drop it.

### 9. LOW-MEDIUM: Saturn and GameCube merged; route lines are builder jargon
- `index.html:196` ("Beetle Saturn · 5 games · DolphinXR · F-Zero GX"); route lines `172-195` ("acvr-ps1", "mupen64plus", "PCSX2 VR").
- Remedy: two panels (Saturn 5 games, 5 views; GameCube 1 game, 2 views), or at least "Saturn 5 games · GameCube 1 game". Route line becomes "22 games · 47 views · mupen64plus", numbers first.

### 10. LOW: duplication and stale phrasing
- `index.html:172-173` body sentence repeats the pill ("Time Crisis on the big screen is accepted"). `153` "the racer behind this panel" is wrong once panels move aside: say "beside this panel". CC0 is stated three times (`123, 158, 212`). "27 designs planned" and "sixteen control models" (`141, 158`) were not found in current.md; verify before keeping.

### 11. LOW: headings and landmarks
- Heading order is sound (h1, h2, h3). `index.html:33` HUD nav has no `aria-label` beside the labelled `40` (add "Site"); no skip link; sections lack `aria-labelledby`. Dot labels (`42-50`) differ from eyebrows ("Ghost controls" vs "Sit-down racers", "Open source" vs "Prize counter", "Where it stands" vs "Scoreboard"): make the dot label match the h2 topic. Games page: 13 jump chips (`games.html:44-56`) against "12 systems"; group them as Arcade / Consoles / Pinball.

## Game List scannability (128 titles)
Structure is good: one h2 per system with a tally, a 230px card grid, exceptions in-card. Improve: tally says "11 games" first with views second (views undefined), a sticky E-key, and an "Accepted in the headset" band of the three E3 titles at the top (matches the stat at `games.html:41`).

## First-time visitor summary
Understands: VR, arcade/console light gun and racing, opened from a panel. Does not understand: open frame, theatre, views, what E2 means in stop 5, that it needs Windows and SteamVR, that no games are included, how few titles are headset-accepted.

## Smallest change set
1. Glossary panel (1) and pill wording (2). 2. Hero rewrite and stat swap (3). 3. Trim the six arcade and six console panels to about 35 words, fix the Model 2 claim (5). 4. Separate flow-step wording from E-pills (7). 5. Remove status-coloured accents (8).
