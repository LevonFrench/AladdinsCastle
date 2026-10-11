# SPDX-License-Identifier: GPL-3.0-only
"""Read canonical product labels; never change stored user data or identifiers."""
import json
from pathlib import Path
import re


def load(path):
    return validate(json.loads(Path(path).read_text(encoding="utf-8")))


def validate(data):
    if not isinstance(data, dict) or type(data.get("schema")) is not int or data["schema"] != 1:
        raise ValueError("Expected product schema 1")
    for key in ("display_name", "short_id", "dsu_server_name"):
        text = data.get(key)
        if not isinstance(text, str) or not text or len(text) > 120:
            raise ValueError("Invalid product label: " + key)
        if any(ord(c) < 32 or ord(c) == 127 for c in text):
            raise ValueError("Control characters in product label")
        try:
            text.encode("utf-8")
        except UnicodeError as error:
            raise ValueError("Invalid Unicode in product label") from error
    if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", data["short_id"]):
        raise ValueError("Product short_id must be a safe profile filename prefix")
    if any(c in data["dsu_server_name"] for c in ':;`/\\'):
        raise ValueError("DSU name contains a protocol/binding delimiter")
    if data["dsu_server_name"] != data["dsu_server_name"].strip():
        raise ValueError("DSU name cannot have outer whitespace")
    return data


def snapshot(data):
    validate(data)
    return {key: data[key] for key in ("schema", "display_name", "short_id", "dsu_server_name")}


def viewer_header(path):
    # Hex-escape every UTF-8 byte: quotes cannot inject C++, emoji do not become
    # invalid surrogate UCNs, and MSVC's current source codepage is irrelevant.
    raw = (load(path)["display_name"] + " 3D theatre").encode("utf-8")
    title = '"' + ''.join('\\x' + format(byte, '02x') for byte in raw) + '"'
    return '#pragma once\nnamespace theatre { inline constexpr const char* productTitle = ' + title + '; }\n'


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--product", required=True, type=Path)
    parser.add_argument("--header", required=True, type=Path)
    args = parser.parse_args()
    args.header.write_text(viewer_header(args.product), encoding="utf-8")
