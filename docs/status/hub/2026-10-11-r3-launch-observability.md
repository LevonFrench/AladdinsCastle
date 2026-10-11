# Launcher merge-CI failure — observability checkpoint

Consumer `32ec410` passed its Windows push launcher suite in 39.85s, while its
actual merge job failed that suite in 59.04s with no test output; 27/28 suites
passed. This is an ordinary CTest failure, not a reported outer timeout.
Launcher, install, Steam and fixture source bytes are unchanged since `ce2194e`.
Timing alone does not identify the case or establish runner variance.

Only the test registration changes: use the existing forced-text wrapper,
a dedicated launcher log and a 120s outer process bound. All assertions, inner
barrier/QTRY deadlines, synthetic inputs and owned-child cleanup are unchanged.
No production launcher or Steam behavior is changed.

One authorized CPU/offscreen diagnostic of the existing older binary passed
83/83 Qt phases in 45.933s. Binary hash/source-age and receipts remain private;
its exact build lineage is unverified, so it is diagnostic-only. Before/after
owned process counts were zero; no forced cleanup was needed. Slot released.
No second local attempt, rebuild, real Steam, native room or GPU work occurred.

The merge failure remains unresolved. Independent review precedes one hosted
observability run; another failure must identify and repair the actual case.
Consumer/calibration heads remain frozen. Portable source WIP is preserved on
its separate branch. Parent owns retry, integration and runtime decisions.
