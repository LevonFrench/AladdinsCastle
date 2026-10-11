# Chronicles Phase 1 preparation — 2026-10-10

The frame-rate lane branched from next 9f3e4d0. It verified the installed v0.4
executable against the pinned hash and inspected only the two disc headers:
RBUP08 and SBDP08 are PAL revision 0. Six private profiles are prepared, three per
game: baseline, candidate 60 code, and that code with a 1.5 VBI override targeting
90. The code candidates and graphics settings remain unmeasured. No Dolphin
process was launched and no frame-rate number was collected. The lead reports a
Windows Security firewall permission prompt during its capture attempt. No
security UI action or workaround is authorized; both turns remain withheld.

The harness verifies the pinned v0.4 executable hash, reads only the selected
disc's 32-byte header at preparation and again at launch, records revision, and
creates fresh private settings without changing shipped profiles. The collector
requires an exact-run, expiring grant for both turns, rejects active build/render
or Dolphin processes, logs logical-core and available GPU engine counters,
records other running process names, and closes its own process. Code and
override runs require a reviewed predecessor with the same scene, disc revision,
build, normal speed, warmed shaders and CPU headroom. Twelve synthetic checks
passed; PowerShell syntax, seven lease/predecessor checks and 28 mocked active-build
refusals passed without executable access or application launch. The build gate
covers MSVC, GCC workers, make, Clang/linker variants and compiler name prefixes.
The public lease uses a neutral lead
marker; the exact coordination identity and grant provenance stay private.

Pinned source 85c4266 confirms that render_times.txt and vblank_times.txt contain
milliseconds between counter events. The FPS counter counts host presentations,
including duplicates; neither that counter nor its interval log establishes the
rate of unique animated frames by itself. The interval summaries preserve this
limit. Umbrella's candidate matches Dolphin's upstream game-settings entry.
Darkside's direct source page returned a challenge, so its brief-transcribed code
remains explicitly unverified. Neither candidate has been tried on these PAL
revisions.

The attract/opening scene and warmup, FPS/VPS/speed
confirmation, the three timed runs, game-speed classification, audio/cutscene
sync, CPU/GPU load under emulation, busy fights, QTE, reload/slash and pointer
checks are not reached. Stereo bloom, headset pacing and all other refresh rates
are also not reached. There is no measured headset preset or recommendation to
remove a per-refresh gun/camera pass. The lane is stopped at preparation pending
owner resolution of the desktop blocker and both lead grants; exact receipts
remain private. No system settings,
downloads, Steam writes, headset or account activity occurred.
