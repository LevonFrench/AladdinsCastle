# R4 sitting profile seeding

Preparation now selects a native DolphinXR rendering profile by the exact
six-character disc ID and copies its bytes into the managed User's
GameSettingsVR folder. This includes native bare shader-enable names. Missing
profiles do not fall back to another title or region. Existing user tuning is
preserved. The run retains before/after copies of the selected profile alongside
its existing configuration evidence.

Windows PowerShell 5.1.26100.9549 passed 70 synthetic checks. New cases cover
exact bytes in Default, Eager and Desktop modes, preservation of existing tuning,
no cross-title fallback and invalid identity refusal before writes. Combined
prepare-only CLI checks also copy the published PR75 profile byte-for-byte in
desktop and native modes, using synthetic executable/disc inputs. No game,
application or VR runtime was launched.

The matching profile is delivered separately in PR75. The reviewed mission
picture used 1x internal resolution; the sitting instructions explicitly request
Quality Native to reproduce that comparison. The existing Sharp default is not
newly accepted. GPU work remains held, and neither headset appearance nor native
controller aim is accepted by these source checks.
