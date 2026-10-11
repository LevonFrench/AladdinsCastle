# Native DolphinXR first-use settings

Both Chronicles recipes seed the exact generated `split-right.ini` from the
shared shell-and-stick contract, pinned by SHA-256. The installer accepts only
literal split/docked native generated profile paths, bounded to 1 MiB, with an
exact hash. Seeded INI files require `create = true`, `only_if_missing = true`, and
no managed-key merge. The bytes are checked again immediately before writing.
Missing or changed generated assets refuse; no shipped-profile fallback is used.

An existing mapping remains untouched. Repair/update skips an existing seed and
relinquishes prior seed ownership, so later uninstall retains it. New seeds still
use the installer's normal transaction and file guards. User directories retain
their established update protection. These rules do not rewrite prior Source-only
profiles; replacing an existing mapping requires the user's explicit choice.

The recipe creates `User/Logs` through a scoped, journaled `create-directory`
step before launch. Existing directories are reused; rollback removes only newly
created empty directories, and uninstall preserves directories containing logs.
Running a scoped Hub repair creates a missing Logs directory without replacing
existing configuration. No fake log entry is written.

New first-use settings include the shared right-hand companion flag, recenter
hotkeys, logging and the lead's base quality settings. Launch overrides use the
upstream `Graphics` system name, not the ignored `GFX` spelling. This does not
establish button delivery, recenter, sharpness, logs or headset acceptance.
