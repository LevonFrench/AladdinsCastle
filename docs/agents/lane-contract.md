# Lane contract

Fill this in before a lane starts and keep it with the lane's output. Copy the block
below and replace every `<...>`. A lane without a stated budget gets the defaults in the
usage rules, and only the owner can raise them, in chat. A follow-up prompt that an agent
suggested carries the defaults too.

```
LANE CONTRACT                                     contract id: <date>-<lane>
Lane: <name>        Runner: claude | codex      Model/effort: <cheapest capable>
Deliverable (one): <what exists when done>
Acceptance check: <the assembled-result test; a command or a picture, not a component check>
Branch/worktree: <branch> / <worktree path>      Draft PR only: yes (the owner merges)
Owns (may edit): <paths>
Must not touch: <other lanes' paths, the live Hub folder, the private play folder,
                 appconfig.json, codex/libacvr-contract, main>
Reads: <inputs>                          Writes private: <lane output folder under .local/>

BUDGET (usage-rule defaults unless the owner raised them in chat)
Rounds: <=3 review-fix        Workers: <=2 besides the coordinator, cheapest model, low/medium effort
Wall time: <=2 h without an owner message, or stop at <time>
Allowance: stop at +25 points for this run or 75% weekly total
Hard stops: 90% weekly = write the handoff only; 100% or credits falling = stop immediately
No-progress rule: the same check fails 2 rounds with no new hypothesis = stop, report 2-3 options

RESOURCES
Heavy work only through: python tools/heavy_lease.py run --lane <name> --purpose <why> --minutes <=45 -- <command>
Every run: hidden, muted, below-normal priority, time-limited, no dialogs, no XR runtime started
HEADSET-ON present = code and docs only. Gap after each measurement hold: <15 min>

AUTHORITY
May: commit to the lane branch; push the lane branch after check_public_privacy.py passes; open a draft PR
Must ask the owner: merge, push to main, applying a change to a live file, downloads,
                    moving anything private into the public tree
Evidence levels on every claim: E1, E2, E3 (agents cannot produce E3)

STATE AND RECOVERY
Step 0 on start: read this lane's own previous run-status files (below). If one says "running"
  and the lease is no longer held by that process, mark it "killed", move its partial output to
  a partial/ folder, and say so in the first line of the report.
Run status: each run writes <lane output>/RUN-<id>/status.json with state "running" before it starts
  and "done", "failed" or "killed" when it ends, plus: branch, head commit, command, start, end,
  exit code, lease id. Never append across runs; one folder per run.
Checkpoint: commit work in progress to the lane branch every <20 min>.
Live files: change them only through a script that records the before-hash, backup path and
  after-hash and can undo; never by hand. Keep the backup until the owner confirms.
Proposals for AGENCY.md or DECISIONS.md go in <lane output>/proposals.md; the lane never edits
  those root files.

STOP AND REPORT
Stop when: the acceptance check passes, the budget is reached, a blocker appears, HEADSET-ON stays
  for over <30 min> with no code work left, or the owner writes.
REPORT sections: Done (with an evidence level per claim) | Not done | Cost | Left open |
  What the owner needs to decide (at most 5, ranked) | Headset checks (at most 8, each under 2 min)
Backlog (found on the way, out of scope): <list for the owner>
```

## Cost section of the report

Every report states what the run cost, so the owner can see whether a stop condition fired:

| Item | Value |
|---|---|
| Rounds used / budget | `<n> / <n>` |
| Workers used, model and effort | `<n>, <model>, <effort>` |
| Wall time | `<hh:mm>` |
| Heavy holds (from the lease ledger) | `<count>, <total minutes>, <killed ones>` |
| Allowance at start / end | `<%> / <%>` (Claude: from the usage tool; Codex: `rate_limits.primary.used_percent`) |
| Credits balance at start / end | `<n> / <n>` (any fall is a hard stop) |

The lease ledger, `ledger.jsonl` in the lease folder, lists every hold with the lane,
purpose, start, end, exit status and whether it was killed.

## Recovery in one paragraph

If a lane dies mid-run (reboot, lock-up, killed wrapper), its lease file is left behind
with a holder process that no longer exists. The next `heavy_lease.py run` recognises that
as stale, kills any recorded orphan child, notes the recovery in the ledger and takes the
lease. The lane's own step 0 then marks the half-finished run `killed` and moves its output
aside. Nothing is deleted.
