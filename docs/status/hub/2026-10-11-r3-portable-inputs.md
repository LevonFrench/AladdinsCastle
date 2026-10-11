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
Actual PowerShell 5.1 passed nine case groups, including default payload,
optional bytes inside ZIP, six capability/key/privacy negatives, junctions,
compiler failure refusal and all-target provenance. The producer's reserved
HOME-name error was diagnosed, renamed and passed the sole retry. Final fresh
inert-probe run and four build-policy checks pass. No product compiler, real
native executable, game/Steam/runtime or actual candidate package was run.

Independent source review is pending. Real four-EXE lineage and deployed Qt
notice inventory remain execution/release gates. The consumer launcher merge
failure has a separate reviewed logging diagnostic; it is not waived here.
Parent owns candidate approval, coordinated build slot, publication and merges.
