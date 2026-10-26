#!/usr/bin/env python3
"""Lint the emulator manifests in data/emulators/ before they can reach the Hub's downloader.

Every downloadable artifact (`do = "github-release"` or `"download"`) must have:
  - an https URL on a host the Hub's downloader allows (read from ArtifactStore::globalHosts),
  - a sha256 and a size (`bytes`), or an `[unpinned.<platform>]` table that says why not,
  - a licence and a licence note on the manifest,
  - no game, BIOS, firmware or key content (by file name).
A manifest the engine will not install automatically (not `download-from-upstream-only`, or gated
other than `consent-install`) gets warnings instead of errors for the pin, host and licence rules.
Schema: docs/emulator-manifests.md.

  validate_emulator_manifests.py            lint data/emulators (exit 1 on any error)
  validate_emulator_manifests.py --test     self-test on in-memory manifests
"""
import re
import sys
import tomllib
from pathlib import Path
from urllib.parse import quote, urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pin_artifact import CONTENT, HEX64, REPO  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DOWNLOADS = ("github-release", "download")


def allowed_hosts(root=ROOT):
    """The downloader's own allow-list, read from the C++ so there is one list, not two."""
    source = (root / "hub/src/core/install/ArtifactStore.cpp").read_text(encoding="utf-8")
    body = re.search(r"ArtifactStore::globalHosts\(\)\s*\{(.*?)\};", source, re.S)
    hosts = set(re.findall(r'"([^"]+)"', body.group(1))) if body else set()
    if "github.com" not in hosts:
        raise SystemExit("could not read ArtifactStore::globalHosts() from ArtifactStore.cpp")
    return hosts


def installable(manifest):
    """True when the engine will download this tool on its own (see Engine::planTool)."""
    return (manifest.get("redistribution") == "download-from-upstream-only"
            and manifest.get("gate", "consent-install") == "consent-install")


def artifact_url(block):
    if block.get("do") == "github-release":
        return f"https://github.com/{block.get('repo', '')}/releases/download/{quote(str(block.get('tag', '')), safe='')}/{quote(str(block.get('asset', '')))}"
    return str(block.get("url", ""))


def lint_manifest(name, manifest, hosts):
    """(errors, warnings, pinned, unpinned) for one parsed manifest."""
    errors, warnings = [], []
    pinned = unpinned = 0
    strict = installable(manifest)

    def soft(message, hard=True):
        (errors if strict and hard else warnings).append(message if message.startswith(name + " ") else f"{name}: {message}")

    if manifest.get("id") != name:
        errors.append(f"{name}: id {manifest.get('id')!r} must equal the file name")
    install = manifest.get("install", {})
    notes = manifest.get("unpinned", {})
    downloads = {p: b for p, b in install.items() if isinstance(b, dict) and b.get("do") in DOWNLOADS}
    for platform in notes:
        if platform not in install:
            errors.append(f"{name}: [unpinned.{platform}] has no [install.{platform}] block")
    if downloads:
        if str(manifest.get("license", "?")).strip() in ("", "?"):
            soft("licence is not recorded (license = \"?\")")
        if not str(manifest.get("license_note", "")).strip():
            soft("no license_note (where the licence was read, and any caveat)")
    for platform, block in downloads.items():
        label = f"{name} [install.{platform}]"
        url = artifact_url(block)
        parts = urlsplit(url)
        names = [Path(parts.path).name, str(block.get("checksum_asset", ""))]
        for item in names:
            if item and CONTENT.search(item):
                errors.append(f"{label}: {item!r} looks like game, BIOS, firmware or key content")
        if "media" in block:
            errors.append(f"{label}: an artifact cannot request media")
        sha = str(block.get("sha256", "?"))
        reason = str(notes.get(platform, {}).get("reason", "")).strip()
        is_pinned = bool(HEX64.match(sha))
        if sha not in ("?", "") and not is_pinned:
            errors.append(f"{label}: sha256 must be 64 lower-case hex digits or \"?\" (got {sha!r})")
        if is_pinned and platform in notes:
            errors.append(f"{label}: has a sha256 and an [unpinned.{platform}] table; remove one")
        if parts.scheme != "https" or parts.username or parts.password or parts.fragment or not parts.hostname:
            errors.append(f"{label}: needs a plain https URL (got {url!r})")
        else:
            host_ok = parts.hostname.lower() in hosts
            if block["do"] == "github-release":
                if not REPO.match(str(block.get("repo", ""))) or not str(block.get("tag", "")).strip() or not str(block.get("asset", "")).strip():
                    errors.append(f"{label}: github-release needs repo (owner/name), tag and asset")
            if not host_ok:
                soft(f"{label}: host {parts.hostname} is not on the downloader allow-list", hard=is_pinned)
        rolling = block.get("do") == "github-release" and (block.get("tag") == "latest" or re.search(r"[*?\[]", str(block.get("asset", ""))))
        if rolling and is_pinned:
            errors.append(f"{label}: a rolling tag or a glob asset cannot carry a pin")
        if is_pinned:
            size = block.get("bytes")
            if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
                errors.append(f"{label}: a pinned artifact needs bytes (its size)")
            pinned += 1
        else:
            unpinned += 1
            if not reason:
                soft(f"{label}: no sha256 and no [unpinned.{platform}] reason")
    return errors, warnings, pinned, unpinned


def lint_tree(root=ROOT):
    hosts = allowed_hosts(root)
    errors, warnings, pinned, unpinned = [], [], 0, 0
    for path in sorted((root / "data/emulators").glob("*.toml")):
        try:
            manifest = tomllib.loads(path.read_text(encoding="utf-8"))
        except tomllib.TOMLDecodeError as err:
            errors.append(f"{path.name}: not valid TOML: {err}")
            continue
        e, w, p, u = lint_manifest(path.stem, manifest, hosts)
        errors += e
        warnings += w
        pinned += p
        unpinned += u
    return errors, warnings, pinned, unpinned


def self_test():
    hosts = allowed_hosts()
    ok = True

    def check(name, cond):
        nonlocal ok
        print(("ok   " if cond else "FAIL ") + name)
        ok = ok and bool(cond)

    def lint(text, name="emu"):
        return lint_manifest(name, tomllib.loads(text), hosts)

    head = 'id = "emu"\nlicense = "GPL-3.0"\nlicense_note = "COPYING read."\nredistribution = "download-from-upstream-only"\n'
    pin = "a" * 64
    good = head + f'[install.windows-x64]\ndo = "github-release"\nrepo = "example/emu"\ntag = "v1"\nasset = "emu-win.zip"\nsha256 = "{pin}"\nbytes = 100\n'
    e, w, p, u = lint(good)
    check("a complete pinned github-release passes", not e and not w and (p, u) == (1, 0))
    e, *_ = lint(good.replace("bytes = 100\n", ""))
    check("a pin without a size is an error", any("bytes" in x for x in e))
    e, *_ = lint(good.replace('sha256 = "' + pin + '"', 'sha256 = "' + pin.upper() + '"'))
    check("an upper-case or malformed sha256 is an error", any("sha256 must be" in x for x in e))
    e, *_ = lint(good.replace('license_note = "COPYING read."\n', ""))
    check("a missing licence note is an error", any("license_note" in x for x in e))
    e, *_ = lint(good.replace('license = "GPL-3.0"', 'license = "?"'))
    check("an unresolved licence is an error", any("licence" in x for x in e))
    e, *_ = lint(good.replace('asset = "emu-win.zip"', 'asset = "emu-bios.zip"'))
    check("BIOS content is rejected", any("BIOS" in x for x in e))
    e, *_ = lint(good.replace('asset = "emu-win.zip"', 'asset = "game.chd"'))
    check("a disc image is rejected", any("game" in x for x in e))
    e, *_ = lint(good.replace('asset = "emu-win.zip"', 'asset = "emu-win.zip"\nmedia = "x"'))
    check("an artifact asking for media is rejected", any("media" in x for x in e))
    e, *_ = lint(good.replace('tag = "v1"', 'tag = "latest"'))
    check("a pin on a rolling tag is an error", any("rolling" in x for x in e))
    e, *_ = lint(good.replace('asset = "emu-win.zip"', 'asset = "emu-*.zip"'))
    check("a pin on a glob asset is an error", any("rolling" in x for x in e))
    e, *_ = lint(head + f'[install.windows-x64]\ndo = "download"\nurl = "http://github.com/x/emu.zip"\nsha256 = "{pin}"\nbytes = 100\n')
    check("http is rejected", any("https" in x for x in e))
    e, *_ = lint(head + f'[install.windows-x64]\ndo = "download"\nurl = "https://user:pw@github.com/x/emu.zip"\nsha256 = "{pin}"\nbytes = 100\n')
    check("credentials in a URL are rejected", any("https" in x for x in e))
    e, *_ = lint(head + f'[install.windows-x64]\ndo = "download"\nurl = "https://example.org/emu.zip"\nsha256 = "{pin}"\nbytes = 100\n')
    check("an unlisted host with a pin is an error", any("allow-list" in x for x in e))
    e, w, p, u = lint(head + '[install.windows-x64]\ndo = "download"\nurl = "https://example.org/emu.zip"\nsha256 = "?"\n[unpinned.windows-x64]\nreason = "no checksum published"\n')
    check("an unlisted host that is unpinned with a reason is only a warning", not e and w and (p, u) == (0, 1))
    e, *_ = lint(head + '[install.windows-x64]\ndo = "github-release"\nrepo = "example/emu"\ntag = "v1"\nasset = "emu.zip"\n')
    check("no pin and no reason is an error", any("no sha256" in x for x in e))
    e, w, p, u = lint(head + '[install.windows-x64]\ndo = "github-release"\nrepo = "example/emu"\ntag = "v1"\nasset = "emu.zip"\n[unpinned.windows-x64]\nreason = "release has no digest"\n')
    check("an unpinned entry with a reason passes", not e and (p, u) == (0, 1))
    e, *_ = lint(good + '[unpinned.windows-x64]\nreason = "x"\n')
    check("a pin and an unpinned table together are an error", any("remove one" in x for x in e))
    e, *_ = lint(good + '[unpinned.linux-x64]\nreason = "x"\n')
    check("an unpinned table with no install block is an error", any("no [install.linux-x64]" in x for x in e))
    e, *_ = lint(good, name="other")
    check("the id must match the file name", any("file name" in x for x in e))
    e, w, *_ = lint(head.replace('"GPL-3.0"', '"?"') + 'gate = "locate-only"\n[install.windows-x64]\ndo = "download"\nurl = "https://example.org/emu.zip"\nsha256 = "?"\n')
    check("a locate-only tool gets warnings, not errors, for pin, host and licence", not e and len(w) >= 3)
    e, *_ = lint(head.replace('"download-from-upstream-only"', '"manual-only"') + '[install.windows-x64]\ndo = "download"\nurl = "https://example.org/emu-bios.zip"\n')
    check("content is rejected even when the tool is not installable", any("BIOS" in x for x in e))
    check("manual installs are not artifacts", lint(head + '[install.windows-x64]\ndo = "manual"\nurl = "https://example.org/"\n')[:2] == ([], []))
    check("the downloader's host list is read from the C++", {"github.com", "api.github.com"} <= hosts)
    errors, warnings, pinned, unpinned = lint_tree()
    check("the real manifests pass", not errors)
    check("the real manifests pin something", pinned > 0)
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


def main():
    if "--test" in sys.argv[1:]:
        return self_test()
    errors, warnings, pinned, unpinned = lint_tree()
    for line in warnings:
        print("warning:", line)
    for line in errors:
        print("ERROR:", line)
    print(f"{pinned} pinned and {unpinned} unpinned artifacts; {len(errors)} errors, {len(warnings)} warnings")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
