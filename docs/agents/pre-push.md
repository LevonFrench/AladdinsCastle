# Pre-push guard

`tools/pre_push.py` is a push check, and `tools/hooks/pre-push` is the thin Git hook that
calls it. The repository does not install it: Git hooks are local state, and turning one
on for every checkout is the owner's decision.

It refuses:

- any push whose remote branch is `main` or `master`, including deleting it;
- any push that names `libacvr-contract` (local or remote ref);

and otherwise runs `python tools/check_public_privacy.py` in the checkout being pushed
from. A failing privacy check refuses the push. There is no environment-variable override.
`git push --no-verify` skips every hook, so agents must never use it.

## How the owner can install it

Pick one. Both cover every worktree of this repository, because they live in the shared Git
configuration and not in one checkout.

1. **Point Git at the tracked hook folder** (follows updates to the hook):

   ```
   git config core.hooksPath <path to a checkout of main>/tools/hooks
   ```

   Use an absolute path to a stable checkout that has the file (after this change is
   merged, the main checkout). A relative `tools/hooks` also works, but silently does
   nothing in a worktree on a branch that lacks the folder.

2. **Copy it into the shared hooks folder** (nothing to keep in sync, but it needs a new
   copy whenever the hook changes):

   ```
   copy tools\hooks\pre-push <git-common-dir>\hooks\pre-push
   ```

   `<git-common-dir>` is the output of `git rev-parse --git-common-dir` in the main checkout.

Check it with `git push --dry-run origin HEAD:refs/heads/main`. It should print
`pre-push: push to main is refused`. A dry run still runs the hook and sends nothing.

The hook runs `python`, so `python` must be on the PATH that Git's `sh` sees (the Microsoft
Store `python3` alias does not work). To remove it: `git config --unset core.hooksPath`, or
delete the copied hook.

## What it does not do

- It does not protect the remote. A branch-protection rule on `main` that requires a pull
  request and the CI checks would; that is the owner's setting on GitHub.
- It runs the privacy check on the working tree, not on the commits being pushed.
- Unit tests: `python tools/pre_push.py --test`.
