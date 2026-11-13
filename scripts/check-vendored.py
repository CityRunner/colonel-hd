#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks the vendored files against the SHA-256 values recorded for them.

Every component in third_party/ has a VENDORED.md. The SHA-256 of its files
are in a table of VENDORED.md (rows that start with a file name in backticks
and have a SHA-256 in backticks in a later cell) or, for a large component,
in a SHA256SUMS file next to it (`sha256sum` output, B-48). Each named file
must exist and have that hash, and every other tracked file of the
component must be VENDORED.md or SHA256SUMS itself.

A component listed in NOT_COVERED that records no hash is reported as not
covered, without failing; none is listed now. A mismatch
that is logged in the backlog goes into KNOWN_MISMATCHES with its item and
is then reported without failing. This script reports; it never changes a
vendored file.

Run from anywhere; exits non-zero and lists every problem that is not known.
"""

import hashlib
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
THIRD_PARTY = ROOT / "third_party"

ROW = re.compile(r"^\|\s*`([^`]+)`\s*\|(?:.*\|)?\s*`([0-9a-f]{64})`\s*\|")

# "component/path" -> backlog item, for mismatches that are logged and kept.
KNOWN_MISMATCHES: dict[str, str] = {}

# Components without per-file hashes -> backlog item.
NOT_COVERED: dict[str, str] = {}


SUM = re.compile(r"^([0-9a-f]{64})  (.+)$")

# Files of a component that hold its records, not vendored code.
RECORD_FILES = {"VENDORED.md", "SHA256SUMS"}


def recorded_hashes(vendored: Path) -> dict[str, str]:
    """The file names and hashes in the tables of a VENDORED.md and in the
    component's SHA256SUMS, if it has one."""
    records: dict[str, str] = {}
    for line in vendored.read_text(encoding="utf-8").splitlines():
        match = ROW.match(line)
        if match:
            records[match.group(1)] = match.group(2)
    sums = vendored.parent / "SHA256SUMS"
    if sums.is_file():
        for line in sums.read_text(encoding="utf-8").splitlines():
            match = SUM.match(line)
            if match:
                records[match.group(2)] = match.group(1)
    return records


def tracked_files(component: Path) -> list[str]:
    """Tracked files of a component, relative to it."""
    out = subprocess.run(
        ["git", "ls-files", "-z", "--", "."],
        cwd=component,
        check=True,
        capture_output=True,
    ).stdout.decode("utf-8")
    return sorted(name for name in out.split("\0") if name)


def check_component(component: Path) -> tuple[list[str], list[str]]:
    """The problems and the notes of one component."""
    name = component.name
    vendored = component / "VENDORED.md"
    if not vendored.is_file():
        return [f"{name}: no VENDORED.md"], []
    records = recorded_hashes(vendored)
    if not records:
        if name in NOT_COVERED:
            count = len(tracked_files(component)) - 1
            return [], [f"{name}: not covered, no SHA-256 recorded for its {count} files ({NOT_COVERED[name]})"]
        return [f"{name}: VENDORED.md records no SHA-256"], []

    problems: list[str] = []
    notes: list[str] = []
    for path, expected in sorted(records.items()):
        file = component / path
        key = f"{name}/{path}"
        if not file.is_file():
            problems.append(f"{key}: recorded in VENDORED.md but missing")
            continue
        actual = hashlib.sha256(file.read_bytes()).hexdigest()
        if actual == expected:
            continue
        message = f"{key}: SHA-256 is {actual}, VENDORED.md records {expected}"
        if key in KNOWN_MISMATCHES:
            notes.append(f"{message} (known, {KNOWN_MISMATCHES[key]})")
        else:
            problems.append(message)
    for path in tracked_files(component):
        if path not in RECORD_FILES and path not in records:
            problems.append(f"{name}/{path}: tracked but not recorded in VENDORED.md")
    notes.append(f"{name}: {len(records)} files checked")
    return problems, notes


def main() -> int:
    problems: list[str] = []
    notes: list[str] = []
    for component in sorted(path for path in THIRD_PARTY.iterdir() if path.is_dir()):
        component_problems, component_notes = check_component(component)
        problems += component_problems
        notes += component_notes
    for note in notes:
        print(note)
    for problem in problems:
        print(f"PROBLEM: {problem}")
    print(f"{len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
