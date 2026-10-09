# Art pipeline: where the Hub's pictures come from

*Design proposal, 2026-10-08. Sources checked 2026-10-08; unverified items are marked. Builds on the rules in [frontend.md](frontend.md) §5, [game-packages.md](game-packages.md) and [legal.md](legal.md).*

This page covers the art the Hub shows for the 413 catalogued games (cards, banners, detail pages) and the Steam library art for SteamVR shortcuts. It sets which source comes first, how assets are matched to games, where credentials live, how the Hub stays within each service's limits, what goes in each provenance sidecar, what the generated fallback looks like, and what may ship in a pack.

## 1. Rules

1. **No third-party art in the repo or in packs.** Packs may ship only art they have the right to distribute (section 10).
2. **Fetching happens on the user's machine,** with the user's own account where a service needs one. Fetched art goes to `user/art/<id>/`, which is gitignored (`user/` is already in `.gitignore`).
3. **Every asset has a `*.source.toml` sidecar** beside it (section 7).
4. **Generated fallback** means no card, banner, hero or shortcut is ever blank (section 8).
5. **Unknown licence means user-side only.** Nothing with an unread licence is redistributed.

## 2. Asset types and sizes per surface

Sizes are the 2x targets from [docs/ui/02-game-cards.md](ui/02-game-cards.md) §3.3 and [docs/ui/01-shell-and-filters.md](ui/01-shell-and-filters.md), with Steam's own sizes for the shortcut art. The card has no art at rest, so it needs no tile file.

| Surface | Spec | File | Target (px) | Ratio | Minimum (px) | Notes |
|---|---|---|---|---|---|---|
| Card hover art strip | 02 §1.9, §2.4 | `banner.png` | 920 x 430 | about 2.14:1 | 460 x 215 | Strip is 175 x 80 card px (2.19:1), UniformToFill. Keep the subject central. |
| Featured banner (list) | 01 §1.6 | `banner.png` | 920 x 430 | about 2.14:1 | 460 x 215 | Frame 140 / 156 / 174 px tall (S / M / L). Art fits by height on the right. |
| Library banner | 01 §1.9 | `banner.png` | 920 x 430 | about 2.14:1 | 460 x 215 | Same heights as the featured banner. |
| Explore banner | 01 (Explore) | `banner.png` | 920 x 430 | about 2.14:1 | 460 x 215 | Heights 184 / 200 / 224 px. |
| Library tile (portrait) | 01 §1.9 | `portrait.png` | 600 x 900 | 2:3 | 300 x 450 | Tiles are 220 x 330, 250 x 375 and 275 x 413 px. |
| Detail hero | 03 (hero) | `banner.png`, cover-cropped | 920 x 430 source | 2.14:1 shown at 2.89:1 | 460 x 215 | Hero is 360 px tall, frozen at 1040 px wide above that. Cropping loses about a quarter of the height. See proposal below. |
| Detail hero (proposed) | 03 | `hero.png` | 2080 x 720 | 2.89:1 | 1040 x 360 | Proposal: sharp at 2x for the 1040 x 360 frame. |
| Detail hero fallback | 03 | `marquee.png`, centre-cropped | original, at least 1024 wide | original | 512 wide | Then the generated card (section 8). |
| Similar-games thumbnail | 03 | `banner.png`, centre-cropped to 160 x 75 | 920 x 430 source | 2.13:1 | 460 x 215 | 03 names `tile.png`. See section 11. |
| Detail marquee | 02 §3.3 | `marquee.png` | original | original | 512 wide | The generator crops it to banner ratio. |
| Flyer | 02 §2.4, §3.3 | `flyer.jpg` | original | original | none | Detail page only (§3.3). The hover chain also names it (§2.4). See section 11. |
| Steam capsule (vertical) | Steam library assets | `<id>p.png` | 600 x 900 | 2:3 | 300 x 450 | Auto-generated half size exists. Graphic-led, title only as text. |
| Steam header (wide) | Steam library assets | `<id>.png` | 920 x 430 | about 2.14:1 | | Falls back to the store header if unset. |
| Steam hero | Steam library assets | `<id>_hero.png` | 3840 x 1240 | about 3.1:1 | 1920 x 620 auto | No text. Keep content inside the centre 860 x 380 safe area. |
| Steam logo | Steam library assets | `<id>_logo.png` | 1280 wide and/or 720 tall | | | PNG with transparent background. Logotype only. |
| Steam app icon | Steam community icons | `<id>_icon.jpg` (name unverified) | 184 x 184 | 1:1 | | JPG, used in the library list view. |
| Shortcut icon | Steam community icons | ICO or PNG | 256 or 512 | 1:1 | | Desktop shortcuts only. Not used for library shortcuts. |

Steam's library header is 920 x 430 on its library asset page. Some older material gives 460 x 215 and 616 x 353; do not use those for library art without checking.

## 3. Source priority

Each source is off unless the user has enabled it. Defaults are a proposal (section 12).

### 3.1 Arcade games (`id` = MAME set name)

| Order | Source | Gives | Match | Default |
|---|---|---|---|---|
| 1 | Files already in `user/art/<id>/` | Any | Set name | On |
| 2 | ScreenScraper, user's own account | `marquee`, `screenmarquee`, `box-2D`, `wheel`, `flyer`, `ss`, `fanart` | `romnom` (set name) with `romtaille`, plus CRC or SHA-1 when the scanner has them | On when the user has an account |
| 3 | Front-end media folders the user already has (read in place with consent) | Marquees, flyers, cabinets | Set name | On when found |
| 4 | Arcade Database (user-side) | Marquees, flyers, cabinets, control panels | Set name | Off until its terms are confirmed |
| 5 | progettosnaps packs the user downloads | Snaps, marquees, flyers, cabinets | Set name | Off until its licence is known |
| 6 | libretro-thumbnails `Named_Boxarts` | Box art | Display name, mapped from `title` | Off |
| 7 | Generated (section 8) | Banner, portrait, hero, logo, icon | n/a | Always the last step |

### 3.2 Console and PC releases (`id` = `<platform>-<slug>`)

| Order | Source | Gives | Match | Default |
|---|---|---|---|---|
| 1 | Files in `user/art/<id>/` | Any | Id | On |
| 2 | Front-end media folders the user already has | Any | Hash, serial, then title with confirmation | On when found |
| 3 | ScreenScraper, user's own account | Box, marquee, wheel, flyer, screenshots, `steamgrid` | System id (to be fetched) with CRC, MD5 or SHA-1 of the user's file, or `romnom` | On when the user has an account |
| 4 | libretro-thumbnails, per system | Box art, snaps, titles | Display name, mapped from `title` | Off |
| 5 | Title search on SteamGridDB, IGDB or TheGamesDB | Text metadata first; images only if enabled | Title, always user-confirmed | Text on, images off |
| 6 | Generated (section 8) | Any | n/a | Always the last step |

Disc games need the scanner's hashes. The `serial` field exists in the schema but no `game.toml` fills it yet (section 4).

### 3.3 PC ports with a Steam app id

1. Files in `user/art/<id>/`.
2. Steam's header and portrait CDN chain, cache first, as the Hub does today. Source `steam-cdn`.
3. Generated (section 8).

Requires the proposed optional `steam_app_id` in `game.toml` (section 11).

### 3.4 SteamVR library shortcuts

A shortcut needs five images: wide (`<id>.png`), portrait (`<id>p.png`), hero (`<id>_hero.png`), logo (`<id>_logo.png`) and icon (`<id>_icon.jpg`, name unverified).

1. Files in `user/art/<id>/` already in the right shape.
2. SteamGridDB, by Steam app id for PC ports (`type = steam`) or by name search with user confirmation (`type = game`). Uses the user's own key. Off until the SteamGridDB terms are read.
3. ScreenScraper `steamgrid` (jpg), if the user has an account and the type is confirmed on the live list.
4. Generated set (section 8) for any shape still missing.

## 4. Matching keys

| Kind | Key | Source in the repo or scanner | Used for |
|---|---|---|---|
| Arcade | MAME set name (= `id`) | `games/<id>/game.toml` `[[media]] set` | ScreenScraper `romnom`, Arcade Database, progettosnaps, scanner |
| Arcade file | ROM file name, size, CRC and SHA-1 | Scanner on the user's machine; the ROMs are never shipped | ScreenScraper `romnom`, `romtaille`, `crc`, `sha1` |
| Disc | Serial (`[[media]] serial`) | [game-schema.md](game-schema.md) §2. **Populated in 0 of 413 files.** | Matching and display |
| Disc or ROM image | CRC32, MD5, SHA-1 | Scanner | ScreenScraper `crc`, `md5`, `sha1` |
| PC port | Steam app id (`steam_app_id`, proposed) | `game.toml`, new optional field | Steam CDN, SteamGridDB `steam` type |
| Any title | `title`, `alt_titles`, `manufacturer`, `year`, `hardware` | `game.toml` and `data/vocab/` | Title search (always confirmed by the user); libretro name mapping |

Counts from the catalog: 224 of 413 entries list a `mame-romset` item, 110 list a `disc` and 9 list a `pc-game`. An entry can list more than one kind.

## 5. Credentials and where they live

| Item | Where it lives | Notes |
|---|---|---|
| ScreenScraper user name and password (`ssid`, `sspassword`) | OS credential store (Windows Credential Manager on Windows, the secret service on Linux) | Entered by the user in Settings. Never in a file. |
| ScreenScraper developer id, password and software name (`devid`, `devpassword`, `softname`) | Not in the repo. See section 12. | Open decision. |
| SteamGridDB API key | OS credential store | Sent as `Authorization: Bearer`. |
| Twitch Client ID and Client Secret (IGDB) | OS credential store | The secret is never shown in the UI or written to logs. |
| TheGamesDB API key | OS credential store | |
| Enabled flags, quota snapshot, last-success times, source order | `user/services.toml` | No secrets and no account names. |

- ScreenScraper sends its credentials as query parameters on each call. **Logs must redact query strings** for that host.
- The diagnostics zip ([frontend.md](frontend.md) §1) excludes `user/services.toml` and anything from the credential store.
- The Hub never signs in for the user. The user pastes their own key or password into Settings.
- Credentials go only to the service's own API host, never to a host named in fetched content.

## 6. Rate limits and caching

- **One request queue per service.** ScreenScraper and TheGamesDB run one request at a time by default. SteamGridDB starts at two, with backoff. Concurrency is a setting, not a constant.
- **Read quotas before each batch.** ScreenScraper: its user-info endpoint, which returns per-minute and per-day quota fields. TheGamesDB: the remaining allowance in each response. IGDB: the token and the limit (secondhand, about 4 requests per second).
- **ScreenScraper errors.** 429 (thread limit) and 430 (daily quota) stop the batch and show the time the quota resets, if the response gives one. 431 (too many unrecognised ROMs) stops and reports. 401 (closed or overloaded), 423 (API closed) and 426 (client blacklisted) stop the service for the session and show the service's message. 404 is recorded in the negative cache.
- **Negative cache.** A "not found" result is kept for 30 days (proposal), with a manual "retry" action.
- **Positive cache.** JSON responses go to `user/cache/<service>/`, keyed by request parameters with credentials stripped. Images are written as `.tmp` and moved into place, as the Hub already does, so a half-written file is never a cache hit.
- **User-initiated batches only.** A batch shows progress and a cancel button. The Hub never scrapes the whole catalogue in the background. Start-up only reads existing files (02 §3.2).
- **No automated crawling** of Arcade Database or progettosnaps. The user downloads their packs.
- **libretro-thumbnails:** a single user-chosen repository, downloaded on request.

## 7. Provenance sidecar (`*.source.toml`)

Field names follow [docs/ui/02-game-cards.md](ui/02-game-cards.md) §3.2 (`url`, `scraper`, `retrieved`, `licence_note`, `generated`). The rest are added here. One sidecar per asset, beside it, for example `user/art/timecris/marquee.png.source.toml`.

```toml
asset          = "marquee.png"
game_id        = "timecris"
kind           = "marquee"          # marquee | banner | portrait | hero | logo | icon | flyer | snap | fanart | steamgrid
url            = "https://..."      # where it came from; "" when generated
scraper        = "screenscraper"    # screenscraper | steamgriddb | arcade-database | progettosnaps | libretro-thumbnails
                                    # | igdb | thegamesdb | launchbox | steam-cdn | user-file | pack | generated
source_id      = "..."              # the service's id for the game or media, when there is one
media_type     = "marquee"          # the service's own media type name
match_key      = "romnom"           # romnom | crc | md5 | sha1 | serial | steam_app_id | title-confirmed | none
user_confirmed = true               # true when the user confirmed a name-based match
retrieved      = 2026-10-08T12:00:00Z
licence_note   = "unverified"       # what the source says, or "none stated", "user-owned", "unverified"
terms_url      = ""                 # the terms page checked, when there is one
generated      = false
generator      = ""                 # for generated art: "fallback/v1"
sha256         = "..."
width          = 920
height         = 430
```

- The sidecar never holds an account name, a key or a token.
- `licence_note = "unverified"` is the default for any scraped asset. It is changed only when the source's terms have been read.

## 8. Generated fallback

**Inputs** from `games/<id>/game.toml`: `title`, `manufacturer` (label from `data/vocab/manufacturers.toml`), `year`, `hardware` (label from `data/vocab/hardware.toml`), `[hub].colour` (background), `[hub].accent` (accent and glow), and `controls.type` (for the glyph: gun, wheel, bike or a generic mark on the 24-unit grid in 02 §6 item 7).

**Outputs** per game, all from the same inputs:

| File | Size (px) | Content |
|---|---|---|
| `banner.png` | 920 x 430 | Title, manufacturer and year. Glow in the accent colour. |
| `portrait.png` | 600 x 900 | Marquee-style plate with the title stacked, hardware label at the foot. |
| `hero.png` (or `<id>_hero.png` for Steam) | 2080 x 720 (proposed) and 3840 x 1240 | Gradient in colour and accent, glyph. **No text** on the Steam hero. |
| `logo.png` (or `<id>_logo.png`) | 1280 x 720 | Title logotype on transparency. |
| `icon.png` (or `<id>_icon.jpg`) | 184 x 184 | Glyph on the accent. |

- **No third-party logos or trademarks.** Manufacturer names are plain text from the vocabulary.
- **Deterministic.** Identical inputs and generator version give identical files. The sidecar records a hash of the inputs, so the file regenerates when `game.toml` changes.
- **Font.** Must be licensed for embedding and redistribution in generated images (for example SIL OFL). Not chosen yet (section 12).

## 9. Licence matrix

Terms are as found on 2026-10-08. "Unverified" means the source was not readable or the terms were not on the page. Ship means in the repo or in a pack.

| Source | What it holds | Terms found | Ship? | Hub use |
|---|---|---|---|---|
| ScreenScraper | Game media (box, marquee, wheel, flyer, screenshots, fanart, video, `steamgrid`) and metadata | The API reference limits API use to fully free, distributed applications unless ScreenScraper authorises otherwise. The site footer states CC BY-NC-SA 4.0 for site content. Media rights for uploads are unclear. | No | User-side, user's own account. Written confirmation needed before any release that uses it. |
| SteamGridDB | Grids, heroes, logos, icons | API key per account. Terms page not readable. Uploads are by users. | No (assumed) | User-side, user's own key, opt-in. |
| Steam library assets | Steam's own shortcut art | Steam's sizes are specified. Non-Steam use not addressed. | No | Steam CDN art for owned PC ports, as the Hub does today. Generated art for shortcuts. |
| Arcade Database | Marquees, flyers, cabinets, control panels, bezels from external sources | No terms page. Trademark notice only. Acknowledges third-party owners. No API document. | No | User-side, opt-in, after maintainer confirms. |
| progettosnaps | Snaps, marquees, flyers, cabinets, control panels, titles | Licence not readable. The site does not host ROMs. | No | User downloads packs. Off until the licence is known. |
| libretro-thumbnails | Box art, snaps, titles per system | No licence stated. Credits MobyGames, Fandom and volunteers. Art belongs to developers and publishers. | No | Optional, user-chosen, off by default. |
| MAME history.dat and mameinfo.dat | Game descriptions | No primary licence found. Distributors treat history.dat as non-free. MAME code licence needs re-checking (section 11). | No | User supplies the file. Or write our own descriptions. |
| IGDB (Twitch) | Metadata and covers | Secondhand: free for non-commercial use under the Twitch agreement, commercial use by partnership, attribution expected. Official docs not readable. | No | Text metadata, user's own credentials, off for images. |
| TheGamesDB | Metadata and images | API key needed. No image terms found. | No | User-side, user's own key, off for images. |
| LaunchBox Games DB | Images and metadata | No licence stated. No public API identified. | No | Only the user's own LaunchBox media folder. |
| Generated (ours) | Banner, portrait, hero, logo, icon | Ours, under the repo licence. Font licence to be checked. | Yes | Default fallback. |
| Pack art (author's own) | Any | The pack's own licence in `pack.toml` ([config-spec.md](config-spec.md) §9). | Yes, if the licence allows | Shipped with sidecars. |

## 10. What packs may ship

- **Allowed:** art the pack author made; art under CC0 or CC BY 4.0, with the attribution kept in the sidecar and the pack's README; generated art from section 8.
- **Not allowed:** anything from a row marked "No" in section 9; anything under `user/`; any file whose sidecar `scraper` is not `generated` or `pack`; any image without a sidecar.
- **Validator.** Extends the ROM check in [config-spec.md](config-spec.md) §9. It rejects scraper-sourced sidecars, images without sidecars, and ROM names and hashes.

## 11. Drift found while writing this page

1. **Similar-games thumbnail.** [03](ui/03-detail-and-install-flow.md) names `art/tile.png`. [02](ui/02-game-cards.md) §3.3 says no tile file is needed. Proposal: centre-crop `banner.png` and drop the `tile.png` mention.
2. **Flyer on hover.** 02 §2.4 puts the flyer in the hover fallback chain. 02 §3.3 says the flyer is detail-only. Pick one.
3. **Detail hero shape.** 920 x 430 cropped to 1040 x 360 loses about a quarter of its height. Proposal: add `hero.png` at 2080 x 720 (section 2).
4. **`steam_app_id`.** Named in 02 §3.2 and 02 §6 item 5 as optional. Not in [game-schema.md](game-schema.md) §2. Add it.
5. **`serial`.** Defined in game-schema §2. No `game.toml` fills it (0 of 413). Disc matching depends on hashes until it is filled.
6. **MAME licence.** [legal.md](legal.md) says MAME is GPL-2.0-or-later. Secondary sources show an older MAME licence with a non-commercial clause. Check the MAME `LICENSE` file before the licence matrix in section 9 is treated as final.
7. **Steam icon name.** `<id>_icon.jpg` is unverified (community sources only). Check it on a real Steam install.

## 12. Open decisions

1. **ScreenScraper developer key.** Request one from ScreenScraper for this software name. Decide how it reaches the app without being published in this GPL repo (a per-user file, or a build-time secret kept outside git).
2. **ScreenScraper terms.** The API rule ("fully free, distributed applications") and the site's CC BY-NC-SA footer both need written confirmation before any release that uses ScreenScraper.
3. **Defaults.** Proposed: ScreenScraper on (user account); SteamGridDB, Arcade Database, progettosnaps, libretro-thumbnails, IGDB images, TheGamesDB images off; LaunchBox only through the user's own files.
4. **Detail hero.** Add `hero.png` (2080 x 720), or accept a cover-crop of `banner.png`.
5. **Steam grid writes.** Write files directly into the Steam grid folder, or use Steam's custom-artwork route. Test with Steam closed. Confirm `_icon.jpg` and how non-Steam shortcut ids are generated on a real install.
6. **Fields.** Add `steam_app_id` (optional) to [game-schema.md](game-schema.md). Decide who fills `serial`, and when.
7. **Generated-art font.** Choose a font whose licence allows embedding and redistribution.
8. **Negative cache.** Confirm the 30-day proposal and the retry UI.
9. **Arcade descriptions.** Ship none, let the user supply history.dat, or write our own.
10. **Maintainer questions.** Ask ScreenScraper, Arcade Database, progettosnaps, IGDB and TheGamesDB, and the libretro-thumbnails maintainers, in writing, whether a free desktop front end may use their data on the proposed terms.
11. **Drift.** Resolve section 11 items 1 to 3 in the UI docs.
