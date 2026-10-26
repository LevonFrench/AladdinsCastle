# Artwork from the internet (optional)

Decision D66 in [docs/workshop.md](../docs/workshop.md). The owner's words: "no
art by default. make it a option to enable it and download it. pick the best
sources."

## Off by default

Settings has one switch, **Download artwork from the internet**. While it is
off the Hub makes no request for art of any kind. The switch is read again
before every request, so turning it off part-way stops the next one. Pictures
already on the PC keep showing either way.

Nothing downloads by itself. With the switch on:

- **Get artwork** on a game's page fetches that game's pictures.
- **Get artwork · <system>** in Settings fetches pictures for every game of that
  system on the VR games page.

Pinball tables keep the pictures made from their own backglass files; no
service has pictures for them.

## What each card and banner shows

Every card has the same 16:9 frame as the pinball tables. A picture close to
that shape is cropped, as a backglass is. A flyer, box or marquee is shown
whole over a blurred, darkened copy of itself. Nothing is stretched. The blur is
the picture decoded tiny and scaled up, so it needs no graphics effects.

| | Card | Banner across the top |
|---|---|---|
| Pinball | the table's own `.directb2s` backglass | the backglass |
| Arcade (Model 2, Model 3, System 22, NAOMI) | flyer, then title screen, then a gameplay shot (libretro's arcade box art is mostly flyers) | a hero picture (yours, a local folder's, then SteamGridDB's), then a gameplay shot, otherwise the card picture blurred |
| Console (Dreamcast, PS2) | box cover, then title screen | the same as arcade |

For each picture the Hub looks in this order:

1. The user's own pictures for that game.
2. Folders of artwork already on the PC.
3. Downloaded pictures.

## Folders of artwork you already have

Settings has a list of folders: marquee, flyer or box-art packs already on the
PC. The Hub indexes them in the background when the list changes. It reads the
pictures where they are and keeps only names and paths. Nothing is copied,
nothing leaves the PC, and it needs no internet, so it works with the download
switch off.

A picture matches a game by:

- the set name (`scud`, `vcop2`, `timecris`);
- the disc's own file or folder name;
- the title, loosely, so `Time Crisis 3 (USA).png` fits Time Crisis 3.

"Loosely" means the same title written another way: case, accents, punctuation, `&` or
`and`, `The` or `, The`, a region or revision tag such as `(USA)` or `[!]`, and roman or
arabic numerals (`Time Crisis II` is `Time Crisis 2`). It never means a different game: an
extra or missing word or another number does not match, so `Ridge Racer` is not
`Ridge Racer V` and `Star Fox` is not `Star Fox Adventures`. One shared matcher
(`src/core/match/TitleMatch.cpp`) does this for artwork, library search and pinball table
names. It compares titles only; a file name never says which disc a file is.

What a picture is comes from its folders. `flyers`, `marquees`, `box` or `covers`,
`titles` and `snaps` or `screenshots` say what they hold. `Named_Boxarts`,
`Named_Titles` and `Named_Snaps` count too. A folder you chose called
`MAME Flyers` holds flyers. Wheels and logos are skipped. A picture in a folder
that says nothing comes after the known kinds.

## Sources, in order

1. **libretro thumbnails**, `https://thumbnails.libretro.com/<system>/Named_Snaps|Named_Titles|Named_Boxarts/<name>.png`.
   The folders used are `Sega - Dreamcast`, `Sony - PlayStation 2` and `MAME`
   (the arcade boards). File names are display names, with each of
   `` & * / : ` < > ? \ | " `` replaced by `_`. MAME names each game by its full
   description, not its set name. The Hub reads each folder's list once and
   matches the disc's own file name exactly, then the title. For a title it
   prefers USA, then World, Europe and Japan, and never takes a beta, prototype,
   demo or bootleg when there is another. MAME has pictures for few Model 2 and
   Model 3 games.
2. **SteamGridDB**, only with the user's own API key (Settings, under the switch).
   The key is kept protected with Windows data protection as
   `steamGridDbKeyProtected`: only the same Windows account on the same PC can
   read it back, and the page never shows it again, only that one is saved and
   a button to remove it. A key an earlier Hub saved in the clear
   (`steamGridDbKey`) is protected at the next start and the clear copy removed.
   It is asked for every game's hero
   (the banner). A card picture is asked for only when libretro had nothing. The Hub searches by title and takes only an exact title
   match. It then takes one 920×430 or 460×215 grid for the card and one hero
   for the banner, still pictures only, with adult, joke and flashing pictures
   filtered out. The key goes in the `Authorization` header to
   `www.steamgriddb.com` and nowhere else. Pictures are fetched from
   SteamGridDB's own servers without it.

ScreenScraper is not used.

## Being a good client

- One request a second at most. Every request has a User-Agent naming the Hub
  and its source repository.
- Never fetched twice:
  - a picture already downloaded;
  - a folder list younger than 30 days;
  - a "not there" answer younger than 30 days.
- A refused SteamGridDB key stops SteamGridDB for the rest of that run.
- Answers are checked: only real pictures under 20 MB are kept. Each is saved
  as a PNG of at most 1920 pixels wide.
- libretro publishes no rate limit or terms for its thumbnail server. Its
  thumbnails repository says the art belongs to each game's developers and
  publishers.
- SteamGridDB's terms allow personal, non-commercial use and forbid heavy load.
  The Hub uses SteamGridDB's published API with the user's own key, and only on
  request.

## Where pictures go

Downloaded pictures go in `user/art-downloaded/<game id>/` in the Hub's folder:
`snap`, `title` and `boxart` from libretro, and `grid` and `hero` from
SteamGridDB. `user/art-downloaded/index.json` records, for each picture, its
source, its address and when it was fetched. It also records the "not there"
answers and SteamGridDB's id for each game. Folder lists are in `listings/`.
Delete the folder to start again.

**The user's own pictures always win.** `user/art/<game id>/` is used before
anything else. A card takes `banner`, then the kinds above (`flyer`, `boxart`,
`title`, `marquee`, `snap`). A banner takes `hero` or `fanart`, then `snap`. Any
of `.png`, `.jpg`, `.jpeg`, `.webp` or `.bmp` works. Catalog games keep their
local art order ([docs/art-pipeline.md](../docs/art-pipeline.md) §0).

## Privacy and copyright

- **Sent:** the requests carry game titles and disc file names, nothing else.
- **Not sent:** paths, the library, or anything that identifies the PC.
- **Ownership:** the pictures belong to the games' makers and to SteamGridDB's
  uploaders. They are fetched for the user's own personal use.
- **Never shared:** they are never committed, shipped, put in a pack or shared
  by the Hub. `user/` is ignored by git.

## Code and tests

- Code: `src/core/art/Download.cpp`.
- Wiring: `UiController::getArtwork` and `HubServices`.
- Tests: `tests/ArtDownloadTest.cpp`, with a stand-in network. They cover:
  - name matching, including the substitutions;
  - the switch, which makes no request while off, even part-way through;
  - the cache, so a second run makes no request;
  - the key reaching SteamGridDB's API only;
  - the key kept protected (`keyIsKeptProtected`, and
    `UiTest::steamGridKeyIsKeptProtected` for the move from a clear key);
  - local pictures winning.
- `IntegrationTest::artworkAsksNothingWhileOff` checks the same switch through
  the Hub itself.
