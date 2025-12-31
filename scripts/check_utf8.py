#!/usr/bin/env python3
"""Validate tracked text files are valid UTF-8."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def list_tracked_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        check=True,
        stdout=subprocess.PIPE,
    )
    entries = result.stdout.split(b"\0")
    return [Path(entry.decode("utf-8")) for entry in entries if entry]


def is_binary(data: bytes) -> bool:
    return b"\0" in data


def validate_utf8(paths: list[Path]) -> list[Path]:
    failures: list[Path] = []
    for path in paths:
        try:
            data = path.read_bytes()
        except OSError:
            continue
        if is_binary(data):
            continue
        try:
            data.decode("utf-8")
        except UnicodeDecodeError:
            failures.append(path)
    return failures


def main() -> int:
    paths = list_tracked_files()
    failures = validate_utf8(paths)
    if failures:
        print("Non-UTF-8 text files detected:")
        for path in failures:
            print(f"- {path}")
        return 1
    print("All tracked text files are valid UTF-8.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
