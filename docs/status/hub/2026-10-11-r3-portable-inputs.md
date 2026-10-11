# Portable inputs — source/synthetic checkpoint

Windows packaging now requires a matching `hub-portable-build/1` receipt for
all four shipping EXEs and canonical product bytes. The producer checks clean
source/cache ownership, uses already-local dependencies with fetch disconnected,
invokes the bounded build, checks source again and records binary hashes.
Windows CI calls it under PowerShell 5.1; Linux build behavior is unchanged.
This records an observed build invocation, not a reproducibility attestation.

Optional native staging requires an explicit executable, reviewed source SHA,
exact binary hash and minimal literal capability receipt. Only tracked authored
gun/hand assets and notices are staged, before ZIP creation. Official CRT,
existing dependency notices and Qt SBOMs are preserved. Default payload adds no
native selection, loader, runtime registration or persisted calibration.
Linked inputs/trees and extra metadata are refused; output/archive must be new.

Verified with synthetic inputs only: four unsafe baseline cases failed first.
Actual PowerShell 5.1 passed eleven case groups, including default payload,
optional bytes inside ZIP, six capability/key/privacy negatives, junctions,
compiler failure refusal and all-target provenance. The producer's reserved
HOME-name error was diagnosed, renamed and passed the sole retry. Final fresh
inert-probe run and four build-policy checks pass. Independent review found
two producer gaps: subproject cache names and writable build descendants.
The preserved pre-fix producer fails both new regressions; the repaired source
passes all ten groups. Fixtures derive names from real FetchContent declarations,
cover override/default cache layouts and reject generated/bin junctions before
any configure call, with external sentinels unchanged. Real-tree planning then
found canonical tracked data/emulators incorrectly refused. Preserved source
8848235 fails the new layout case; a flat tracked catalog exception preserves
hidden/user/installed, other emulator folders and untracked-file refusals.
The first repaired run exposed a wrong ZIP-root expectation in the new test;
its diagnosed sole retry passes all eleven groups. Read-only PowerShell 5.1
admits all 559 actual public metadata files, including 20 emulator entries.
No product compiler, real
native executable, game/Steam/runtime or actual candidate package was run.

Reconciled once with integrated next source 64680ef, without conflicts or
portable-file overlaps; the inherited Hub delta is the reviewed launcher
logging wrapper. Final repair freeze awaits independent closure before a real
candidate build. Real four-EXE lineage and deployed Qt notice
inventory remain execution/release gates. Consumer/calibration final hosted
checks passed and are integrated; the earlier launcher failure remains unknown.
Parent owns candidate approval, coordinated build slot, publication and merges.
