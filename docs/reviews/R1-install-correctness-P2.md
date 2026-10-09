# R1 install correctness (P2)

Scope: R1 items 17–23. All validation uses synthetic temporary files, a loopback HTTPS fixture and the approved existing toolchain. No emulator, SteamVR, headset, external release download, owner configuration or GPU run is part of this review.

| Item | Change | Regression |
| --- | --- | --- |
| 17 | Updates omit `user-media-link` files from staging and recreate the declared link. Install, staging, backups and rollback copies stream in 128 KiB chunks. | `mediaLinkUpdateAndRemovalWithoutBackup`, `explicitMediaCopyStreamsAcrossUpdate`, existing update/WAL fault boundaries |
| 18 | File validators accompany range resumes only; missing parts discard stale validators. Promotion and wrong pins remove validators. HTTP 304 cannot succeed for a file download. | `validatorLifecycleAndWrongPin` |
| 19 | Unavailable GitHub metadata produces a warning and proceeds with the exact artifact pin. Available metadata still checks the exact asset and digest. Warnings enter the install log. | `githubCrosscheckUnavailableUsesPin`, `githubMalformedCrosscheckAndContradictoryDigest`, existing release/cache/rate-limit tests |
| 20 | Managed keys remerge into the current configuration bytes while retaining each original prior value. Unmanaged edits remain intact. | `configRemergeKeepsUnmanagedEditsAndOriginalPrior` |
| 21 | Uninstall journals media-link identity and its original source without a byte backup. Recovery restores a link only to the same original file identity. | `mediaLinkUpdateAndRemovalWithoutBackup`, `unlinkRecoveryRestoresOriginalIdentityWithoutBackup` |
| 22 | `hubtool --settings <JSON>` populates `Request.settings` and rejects non-object settings. Steps without ids receive stable collision-free ids. `require-media` supports `none`, `name` and default `hash`; copy-media still requires hash verification. The shipped HotD 2 PCVR recipe plans without network access. | `hubtoolSettingsAreAvailableToRecipe`, `mediaVerificationModesAndGeneratedIds`, `hotdSelfContainedDryRun` |
| 23, partial | Failed runs record `failed`, `previous_state` and `last_error` while retaining the restored prior installed version and files. Interrupted recovery records its own error code. | `failedUpdateRecordsPriorStateAndError`, existing recovery boundaries |

Deferred portions of item 23:

- Repair when both ownership-state generations are corrupt remains blocked. Reconstructing ownership from disk or an unauthenticated journal can adopt user files incorrectly. A trusted ownership reconstruction and explicit recovery review are needed; existing files remain preserved.
- Old `.previous-<run>` generation pruning remains deferred. The retention policy is launch-once or seven days, and deletion must use the old ownership manifest while preserving unowned and edited files. The current implementation lacks a durable per-generation ownership/retention receipt; retaining these folders is safer than guessing what to delete.

The real HotD release expansion-cap and `.bin` checks are deferred because a fresh external download requires separate owner approval. The planning regression makes no request. Runtime, headset and GPU acceptance gates remain open.

Verified locally: the Windows CPU InstallTest suite passed all 105 cases, with zero failures or skips. The public-privacy checker passed its four regression cases and the tracked-file audit. Linux acceptance is left to the integration CI run.
