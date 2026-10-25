# Security and content notices

AladdinsCastle is a working development repository with no 0.01 release. It is a Hub (a Qt
program) that launches VR setups for emulators and games the user supplies. It contains no
game content and ships no emulator builds.

## Reporting a vulnerability

Please do not put exploit details in a public issue.

- If the repository's **Security** tab offers **Report a vulnerability**, use it. That is a
  private report to the maintainer.
- If it does not, open a public issue titled "Security contact request" that says only that you
  have a security report and nothing about the problem. The maintainer will reply with a private
  way to send it.

Useful in a report: the version or commit, what you did, what happened, and what you expected.
There is no bounty. This is a one-person project, so replies may take several days.

In scope: the Hub and its install and launch code (for example path handling, archive
extraction, command construction, the SteamVR overlay), the `libacvr` library, the tools in
`tools/`, and the website in `website/`. Vulnerabilities in the emulators and engines that the
setups patch belong with those upstream projects; the patches here only change how they draw and
take input.

## Reporting content that should not be here

If you believe something in this repository infringes a copyright, trademark or licence, or is
game data that should not be public (ROMs, BIOS files, disc images, keys, saves, or text and
images copied from a game), tell the maintainer the same way as above: private report if the
Security tab offers one, otherwise an issue titled "Content notice" that names the file and
commit but does not repeat the material. Include:

- the file path and commit or pull request;
- what the material is and who owns it;
- why you think it should not be here.

The maintainer will remove the file from the tree promptly while the notice is looked at, and
will deal with the Git history if the material turns out to be in it. The project's rule is that
no game content, BIOS, keys or saves are ever committed ([CONTRIBUTING.md](CONTRIBUTING.md), rule 1),
and `tools/check_public_privacy.py` plus the ignore list in `.gitignore` are there to catch
mistakes. Licences and notices for third-party code are in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## What this repository keeps out of Git

Game files and disc images, saves and save states, memory cards, keys, credentials, private
paths, and the owner's library or scan results. If you find any of these in a tracked file, that
is a bug: report it as above.
