#!/usr/bin/env python3
"""Pin a GitHub release asset in a Hub emulator manifest without downloading it.

The pin comes from the release's own JSON (`gh api repos/OWNER/REPO/releases/tags/TAG`): GitHub
publishes a `digest: sha256:...` and a `size` for every asset. This tool reads that metadata and
writes `sha256` and `bytes` into the manifest's `[install.<platform>]` block, the same keys the
install engine reads (docs/emulator-manifests.md section 2.2). It never fetches an asset, never
invents a hash, and never touches game, BIOS or key content.

  pin_artifact.py OWNER/REPO TAG ASSET                       print the pinned entry
  pin_artifact.py OWNER/REPO TAG ASSET --manifest M -p KEY   preview the change in M
  pin_artifact.py --manifest M [-p KEY]                      preview every github-release block in M
  add --write to save the change; --release-json FILE reads the metadata from a local file instead of gh
  pin_artifact.py --test                                     self-test on a local JSON fixture

When a release has no digest for the asset, or the tag is rolling ("latest"), nothing is pinned:
the manifest gets an `[unpinned.<platform>]` table with the reason instead.
"""
import argparse
import fnmatch
import json
import re
import subprocess
import sys
import tempfile
import tomllib
from datetime import date
from pathlib import Path
from urllib.parse import quote

ROOT = Path(__file__).resolve().parent.parent
FIXTURE = Path(__file__).resolve().parent / "fixtures" / "pin_artifact" / "release-sample.json"
HEX64 = re.compile(r"^[0-9a-f]{64}$")
REPO = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
# Anything that looks like game, BIOS, firmware or key content is refused outright.
CONTENT = re.compile(r"(\.(iso|chd|cue|gdi|cso|rom|nes|sfc|smc|gba|nds|z64|wad|wbfs|pem|key|p12|pfx|keys)$|bios|firmware|prod\.keys|title\.keys)", re.I)


class PinError(Exception):
    pass


def fetch_release(repo, tag):
    """Release metadata as JSON, read with `gh api`. This is metadata only, never an asset."""
    path = f"repos/{repo}/releases/tags/{quote(tag, safe='')}"
    try:
        done = subprocess.run(["gh", "api", path], capture_output=True, text=True, encoding="utf-8", timeout=90)
    except (OSError, subprocess.TimeoutExpired) as err:
        raise PinError(f"gh api failed for {repo} {tag}: {err}")
    if done.returncode != 0:
        raise PinError(f"gh api {path}: {done.stderr.strip() or 'exit ' + str(done.returncode)}")
    return json.loads(done.stdout)


def find_asset(release, wanted):
    """The one asset whose name is `wanted` (a glob is allowed when it matches exactly one)."""
    assets = release.get("assets") or []
    exact = [a for a in assets if a.get("name") == wanted]
    found = exact or [a for a in assets if fnmatch.fnmatchcase(a.get("name", ""), wanted)]
    if len(found) != 1:
        names = ", ".join(a.get("name", "?") for a in assets) or "none"
        raise PinError(f"asset {wanted!r} matched {len(found)} assets (release has: {names})")
    return found[0]


def pin_from_release(repo, tag, wanted, release):
    """A pin result for one asset. `status` is `pinned` or `unpinned` (with a `reason`)."""
    if not REPO.match(repo):
        raise PinError(f"not an owner/repo: {repo!r}")
    if CONTENT.search(wanted):
        raise PinError(f"refusing {wanted!r}: game, BIOS and key content is never pinned")
    if tag == "latest":
        return {"status": "unpinned", "asset": wanted,
                "reason": 'rolling tag "latest": the release changes under the same name, so no hash can stay true'}
    if release.get("tag_name") not in (None, tag):
        raise PinError(f"release tag {release.get('tag_name')!r} is not {tag!r}")
    asset = find_asset(release, wanted)
    name = asset["name"]
    if CONTENT.search(name):
        raise PinError(f"refusing {name!r}: game, BIOS and key content is never pinned")
    url = asset.get("browser_download_url") or ""
    expected = f"https://github.com/{repo}/releases/download/{quote(tag, safe='')}/{quote(name)}"
    if url and url != expected:
        raise PinError(f"asset URL {url!r} is not the release URL {expected!r}")
    digest = str(asset.get("digest") or "")
    size = asset.get("size")
    result = {"asset": name, "url": expected, "published_at": release.get("published_at", "")}
    if not digest.startswith("sha256:") or not HEX64.match(digest[7:].lower()):
        return {**result, "status": "unpinned",
                "reason": "the release metadata has no sha256 digest for this asset" if not digest
                else f"the release metadata digest is not sha256 ({digest.split(':')[0]})"}
    if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
        return {**result, "status": "unpinned", "reason": "the release metadata has no usable size for this asset"}
    return {**result, "status": "pinned", "sha256": digest[7:].lower(), "bytes": size}


def toml_string(value):
    return json.dumps(value, ensure_ascii=True)


def render_entry(platform, result):
    """The manifest lines a result adds, for printing."""
    if result["status"] == "pinned":
        return (f'[install.{platform}]\nasset = {toml_string(result["asset"])}\n'
                f'sha256 = "{result["sha256"]}"\nbytes = {result["bytes"]}\n')
    return (f'[unpinned.{platform}]\nreason = {toml_string(result["reason"])}\n'
            f'checked = "{date.today().isoformat()}"\n')


def section_bounds(lines, header):
    start = next((i for i, line in enumerate(lines) if line.strip() == header), None)
    if start is None:
        return None
    end = next((i for i in range(start + 1, len(lines)) if lines[i].lstrip().startswith("[")), len(lines))
    return start, end


def key_line(key):
    return re.compile(rf"^\s*{re.escape(key)}\s*=")


def apply_to_text(text, platform, result):
    """New manifest text with the result applied. Raises PinError on a conflict."""
    newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.replace("\r\n", "\n").split("\n")
    header = f"[install.{platform}]"
    bounds = section_bounds(lines, header)
    if bounds is None:
        raise PinError(f"no {header} block")
    start, end = bounds
    block = lines[start:end]
    old = next((re.search(r'"([^"]*)"', l).group(1) for l in block if key_line("sha256").match(l)), "")
    if result["status"] == "pinned":
        if HEX64.match(old.lower()) and old.lower() != result["sha256"]:
            raise PinError(f"{header} already pins {old}, the release says {result['sha256']}: not overwriting")
        if not any(key_line("asset").match(l) for l in block):
            raise PinError(f"{header} has no asset line")
        # asset stays where it was (now the exact name); the pin goes after archive, version or the last key.
        body = [f"asset = {toml_string(result['asset'])}" if key_line("asset").match(l) else l
                for l in block if not (key_line("sha256").match(l) or key_line("bytes").match(l))]
        pin = [f'sha256 = "{result["sha256"]}"', f'bytes = {result["bytes"]}']
        anchor = next((i for i, l in enumerate(body) if key_line("archive").match(l)), None)
        if anchor is None:
            anchor = next((i for i, l in enumerate(body) if key_line("version").match(l)), None)
        if anchor is None:
            anchor = max((i for i, l in enumerate(body) if "=" in l and not l.lstrip().startswith("#")), default=0)
        body[anchor + 1:anchor + 1] = pin
        lines[start:end] = body
        un = section_bounds(lines, f"[unpinned.{platform}]")
        if un:
            a, b = un
            del lines[a:b]
    else:
        if HEX64.match(old.lower()):
            raise PinError(f"{header} already has a pin; the release now gives none ({result['reason']}): not touching it")
        un = section_bounds(lines, f"[unpinned.{platform}]")
        entry = [f"[unpinned.{platform}]", f"reason = {toml_string(result['reason'])}",
                 f'checked = "{date.today().isoformat()}"', ""]
        if un:
            lines[un[0]:un[1]] = entry
        else:
            at = end
            while at > start + 1 and not lines[at - 1].strip():
                at -= 1
            lines[at:at] = [""] + entry[:-1]
            if at + len(entry) >= len(lines) or lines[at + len(entry)].strip():
                lines.insert(at + len(entry), "")
    out = newline.join(lines)
    parsed = tomllib.loads(out)
    got = parsed["install"][platform]
    if result["status"] == "pinned" and (got.get("sha256") != result["sha256"] or got.get("bytes") != result["bytes"]):
        raise PinError("internal error: the edited manifest does not hold the pin")
    return out


def blocks_to_pin(manifest_path, only_platform):
    data = tomllib.loads(Path(manifest_path).read_text(encoding="utf-8"))
    for platform, block in data.get("install", {}).items():
        if only_platform and platform != only_platform:
            continue
        if block.get("do") == "github-release":
            yield platform, block["repo"], block["tag"], block["asset"]


def run(args, getter=fetch_release):
    cache = {}

    def release(repo, tag):
        if args.release_json:
            return json.loads(Path(args.release_json).read_text(encoding="utf-8"))
        if tag == "latest":
            return {}
        if (repo, tag) not in cache:
            cache[(repo, tag)] = getter(repo, tag)
        return cache[(repo, tag)]

    if args.repo:
        if not (args.tag and args.asset):
            raise PinError("give OWNER/REPO TAG ASSET together")
        if args.manifest and not args.platform:
            raise PinError("give --platform with --manifest when pinning one asset")
        jobs = [(args.platform or "PLATFORM", args.repo, args.tag, args.asset)]
    elif args.manifest:
        jobs = list(blocks_to_pin(args.manifest, args.platform))
        if not jobs:
            raise PinError("no github-release blocks to pin")
    else:
        raise PinError("give OWNER/REPO TAG ASSET or --manifest")
    text = Path(args.manifest).read_text(encoding="utf-8") if args.manifest else None
    failures = 0
    for platform, repo, tag, asset in jobs:
        try:
            result = pin_from_release(repo, tag, asset, release(repo, tag))
            print(f"{repo} {tag} {platform}: {result['status']}"
                  + (f" ({result['reason']})" if result["status"] == "unpinned" else f" {result['sha256']} {result['bytes']} bytes"))
            if text is None:
                print(render_entry(platform, result))
            else:
                text = apply_to_text(text, platform, result)
        except PinError as err:
            failures += 1
            print(f"{repo} {tag} {platform}: ERROR {err}", file=sys.stderr)
    if text is not None and failures == 0:
        if args.write:
            Path(args.manifest).write_text(text, encoding="utf-8", newline="")
            print(f"wrote {Path(args.manifest).resolve()}")
        else:
            print("dry run: nothing written (add --write to save)")
    return 1 if failures else 0


def self_test():
    release = json.loads(FIXTURE.read_text(encoding="utf-8"))
    repo, tag = "example/emulator", "v1.0"
    ok = True

    def check(name, cond):
        nonlocal ok
        print(("ok   " if cond else "FAIL ") + name)
        ok = ok and bool(cond)

    def raises(fn):
        try:
            fn()
        except PinError:
            return True
        return False

    zip_digest = "a" * 63 + "1"
    r = pin_from_release(repo, tag, "emu-win64-1.0.zip", release)
    check("pins the digest, lower-cased, and the size", r["status"] == "pinned" and r["sha256"] == zip_digest and r["bytes"] == 1234567)
    check("pins from a glob that matches one asset", pin_from_release(repo, tag, "emu-*.AppImage", release)["asset"] == "emu-x86_64-1.0.AppImage")
    check("no digest field: unpinned with a reason, no hash", (lambda x: x["status"] == "unpinned" and "no sha256" in x["reason"] and "sha256" not in x)(pin_from_release(repo, tag, "emu-old.zip", release)))
    check("a non-sha256 digest is unpinned", pin_from_release(repo, tag, "emu-md5.zip", release)["status"] == "unpinned")
    check("a malformed sha256 is unpinned, not trusted", pin_from_release(repo, tag, "emu-short.zip", release)["status"] == "unpinned")
    check("no size: unpinned", pin_from_release(repo, tag, "emu-nosize.zip", release)["status"] == "unpinned")
    check("rolling tag latest is unpinned", pin_from_release(repo, "latest", "emu-win64-1.0.zip", {})["status"] == "unpinned")
    check("a glob matching two assets is refused", raises(lambda: pin_from_release(repo, tag, "emu-win*.zip", release)))
    check("a missing asset is refused", raises(lambda: pin_from_release(repo, tag, "nope.zip", release)))
    check("a BIOS-looking asset is refused", raises(lambda: pin_from_release(repo, tag, "ps2-bios.zip", release)))
    check("a disc image is refused", raises(lambda: pin_from_release(repo, tag, "game.iso", release)))
    check("a bad repo is refused", raises(lambda: pin_from_release("not a repo", tag, "x", release)))
    check("a wrong release tag is refused", raises(lambda: pin_from_release(repo, "v9", "emu-win64-1.0.zip", release)))
    other = json.loads(json.dumps(release))
    other["assets"][0]["browser_download_url"] = "https://evil.example/emu-win64-1.0.zip"
    check("an asset URL off the release is refused", raises(lambda: pin_from_release(repo, tag, "emu-win64-1.0.zip", other)))

    manifest = ('id = "emu"\n\n[install.windows-x64]\ndo = "github-release"\nrepo = "example/emulator"\ntag = "v1.0"\n'
                'asset = "emu-win64-1.0.zip"\nversion = "v1.0"\narchive = "zip"\nnote = "Pinned test entry."\n\n'
                '[install.linux-x64]\ndo = "github-release"\nrepo = "example/emulator"\ntag = "v1.0"\nasset = "emu-*.AppImage"\n'
                'version = "v1.0"\narchive = "none"\n\n[portable]\nmarker = "none"\n')
    out = apply_to_text(manifest, "windows-x64", r)
    parsed = tomllib.loads(out)["install"]["windows-x64"]
    check("writes sha256 and bytes after archive, keeps note", parsed["sha256"] == zip_digest and parsed["bytes"] == 1234567 and parsed["note"] == "Pinned test entry.")
    check("the pin sits right after the archive line", "archive = \"zip\"\nsha256 =" in out)
    check("running it again changes nothing", apply_to_text(out, "windows-x64", r) == out)
    glob = apply_to_text(manifest, "linux-x64", pin_from_release(repo, tag, "emu-*.AppImage", release))
    check("a glob in the manifest becomes the exact asset name", tomllib.loads(glob)["install"]["linux-x64"]["asset"] == "emu-x86_64-1.0.AppImage")
    bad = out.replace(zip_digest, "b" * 64)
    check("a different existing pin is never overwritten", raises(lambda: apply_to_text(bad, "windows-x64", r)))
    un = pin_from_release(repo, tag, "emu-old.zip", release)
    marked = apply_to_text(manifest.replace("emu-win64-1.0.zip", "emu-old.zip"), "windows-x64", un)
    table = tomllib.loads(marked)
    check("unpinned writes an [unpinned.<platform>] reason and no hash", "no sha256" in table["unpinned"]["windows-x64"]["reason"] and "sha256" not in table["install"]["windows-x64"])
    check("unpinned twice keeps one table", apply_to_text(marked, "windows-x64", un).count("[unpinned.windows-x64]") == 1)
    repinned = apply_to_text(marked.replace("emu-old.zip", "emu-win64-1.0.zip"), "windows-x64", r)
    check("pinning removes a stale [unpinned] table", "unpinned" not in tomllib.loads(repinned))
    check("an existing pin is kept when the release stops giving one", raises(lambda: apply_to_text(out, "windows-x64", un)))
    check("CRLF manifests keep CRLF", "\r\n" in apply_to_text(manifest.replace("\n", "\r\n"), "windows-x64", r))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "emu.toml"
        path.write_text(manifest, encoding="utf-8", newline="")
        args = argparse.Namespace(repo=None, tag=None, asset=None, manifest=str(path), platform=None, write=True, release_json=str(FIXTURE))
        sys.stdout = open(Path(tmp) / "out.txt", "w", encoding="utf-8")
        try:
            code = run(args)
        finally:
            sys.stdout.close()
            sys.stdout = sys.__stdout__
        done = tomllib.loads(path.read_text(encoding="utf-8"))["install"]
        check("whole-manifest run pins both blocks from a local JSON file, no network", code == 0 and done["windows-x64"]["sha256"] == zip_digest and done["linux-x64"]["bytes"] == 7654321)
    check("the fixture-only run never called gh", not subprocess_used)
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


subprocess_used = False
_real_run = subprocess.run


def _guard(*a, **k):
    global subprocess_used
    subprocess_used = True
    return _real_run(*a, **k)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("repo", nargs="?", help="GitHub owner/repo")
    ap.add_argument("tag", nargs="?", help="release tag")
    ap.add_argument("asset", nargs="?", help="asset name (a glob that matches one asset is allowed)")
    ap.add_argument("--manifest", help="manifest to update (data/emulators/<id>.toml)")
    ap.add_argument("-p", "--platform", help="platform key, e.g. windows-x64")
    ap.add_argument("--write", action="store_true", help="save the change (default is a dry run)")
    ap.add_argument("--release-json", help="read the release metadata from this local JSON file instead of gh")
    ap.add_argument("--test", action="store_true", help="self-test on a local JSON fixture")
    args = ap.parse_args(argv)
    if args.test:
        subprocess.run = _guard
        return self_test()
    try:
        return run(args)
    except PinError as err:
        print(f"ERROR {err}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
