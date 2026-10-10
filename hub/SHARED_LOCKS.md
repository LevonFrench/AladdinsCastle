# Cooperative resource locks

Hub copies running as the same OS user coordinate changes to the same resource,
even when their portable folders and temporary-directory environments differ.
The lock identity is the canonical absolute resource path, case-folded on Windows.
Existing linked ancestors are rejected, and containment and identity are checked
again after acquiring the guard. Missing resources use their canonical existing
ancestor plus the remaining path, preserving identity across creation/removal.

Windows uses the OS LocalAppData known folder, with a user-owned directory and a
protected user/SYSTEM ACL. Linux uses a fixed system-temporary namespace qualified
by the effective UID, with user ownership and no group/other access. Environment
variables do not choose the shared namespace. This coordinates cooperating copies
on one host in the same OS/filesystem namespace. Different OS users, different
hosts, or isolated namespaces sharing a network payload are outside this contract.

Resources are acquired in canonical sorted order with immediate refusal rather
than waiting. A mutation guard excludes other mutations and reader registration.
Each launch registers its own usage lease while holding that guard briefly, so
several games can use one tool concurrently. Install, rollback, recovery and
removal hold the mutation guard before changing owned payloads. Steam replacement
uses the actual account's shortcuts target and rechecks the approved preview and
current hash under that guard. Other tools and Steam accounts remain independent.

Flat launch plans retain the original tool package root before profile preparation
rewrites an executable path (including PCSX2's profile clone). Launch holds usage
through preparation and the tracked child lifetime. A crashed holder's usage lease
is reclaimed only when its recorded child has exited or the PID refers to a new
creation. Windows process creation time and Linux process start time identify that
creation; failed queries and unreadable/invalid evidence refuse mutation. A dead
holder without a child record can be reclaimed by QLockFile's stale-holder check
only if it never entered the launch reservation phase.

Before attempting an OS child start, the live holder durably writes a pending
marker for every payload. Any failed marker write prevents spawn. The started
callback replaces these markers with PID/creation records. Proven failed starts
and normal completion clear the live holder's evidence. An orphaned pending
marker is retained even if the launch might never have spawned: its unknown
child cannot be proved absent. Stop the associated game and have an operator
review that lease; production does not automatically delete it or infer safety
from a timeout. Partial reservations are cleaned only by the live holder that
knows it did not attempt spawn.

This tracks the immediate launched child. Detached descendants and arbitrary
external edits do not participate in the cooperative contract. The locks do not
provide an operating-system sandbox or make external writers cooperate.
