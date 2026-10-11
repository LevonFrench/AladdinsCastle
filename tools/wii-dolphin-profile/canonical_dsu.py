# SPDX-License-Identifier: GPL-3.0-only
"""Frozen shell-stick/1 right-primary DSU contract, never caller-supplied policy.

The published producer emits these one-remote bytes for both split and docked,
and both flat/scripted right-primary roles. A producer contract change requires
a reviewed template update; adjacent manifests cannot extend this policy.
"""
import configparser
import hashlib
from pathlib import Path
import re

TEMPLATE_SHA256 = "4fce5331e9ddda67365f5908a02f31fece9275c60279598446cd1af1153bc139"
LEGACY_SHA256 = "6e3ab876ee5f38d2560e169b5f725bcec67e64759928622a31d7ea7fec5efbde"


def render(server, legacy=False):
    # Same server-name grammar as the shared shell-stick/1 producer.
    if not isinstance(server, str) or re.fullmatch(r"[A-Za-z0-9_-]+", server) is None:
        raise ValueError("Invalid canonical DSU producer server label")
    name = "canonical_dsu_legacy.ini" if legacy else "canonical_dsu.ini"
    template = Path(__file__).with_name(name).read_bytes()
    if hashlib.sha256(template).hexdigest() != (LEGACY_SHA256 if legacy else TEMPLATE_SHA256):
        raise ValueError("Trusted DSU template bytes changed; reviewed update required")
    return template.replace(b"AladdinsCastle", server.encode("ascii"))


def validate(payload, server, allow_legacy=False):
    parsed = configparser.ConfigParser(interpolation=None)
    parsed.optionxform = str
    try:
        parsed.read_string(payload.decode("utf-8"))
    except (UnicodeError, configparser.Error) as error:
        raise ValueError("Invalid generated DSU profile syntax") from error
    sections = {"Wiimote1", "Wiimote2", "Wiimote3", "Wiimote4", "BalanceBoard"}
    if parsed.defaults() or set(parsed.sections()) != sections:
        raise ValueError("Canonical DSU requires exactly all five slots and no defaults")
    for section in ("Wiimote2", "Wiimote3", "Wiimote4", "BalanceBoard"):
        if dict(parsed[section]) != {"Source": "0"}:
            raise ValueError("Non-selected Wii/BalanceBoard slots must be disabled")
    if parsed["Wiimote1"].get("Source") != "1":
        raise ValueError("Canonical profile must use emulated DSU Source 1")
    if parsed["Wiimote1"].get("Device") != f"DSUClient/0/{server}":
        raise ValueError("Canonical DSU profile label differs from product metadata; regenerate it")
    expected = [render(server)]
    if allow_legacy:
        expected.append(render(server, legacy=True))
    if payload not in expected:
        raise ValueError("DSU controls differ from trusted one-remote generator bytes")
