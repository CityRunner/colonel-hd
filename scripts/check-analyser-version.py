#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that a change to the HD picture goldens bumps analyser_version (B-38).

analyser_version (src/sci-vector/include/sci/vector/build_scene.hpp) is
part of the analysis cache's key: it must change whenever the scenes
build_scene makes change, which shows in the HD lines of
tests/golden/pictures.txt and in the `hd`/`hdpri` picture lines of
tests/golden/resources.txt. This compares the working tree with a base
(argument, or $SCI_BASE_REF, or origin/main; their merge base with HEAD)
and fails when those lines differ but analyser_version does not.

Without the base (a shallow clone, no remote) it says so and passes.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = "src/sci-vector/include/sci/vector/build_scene.hpp"
VERSION = re.compile(r"analyser_version\s*=\s*(\d+)")
GOLDENS = ("tests/golden/pictures.txt", "tests/golden/resources.txt")


def hd_lines(path: str, text: str) -> list[str]:
    """The lines of a golden file that the analysis decides."""
    lines = text.splitlines()
    if path.endswith("pictures.txt"):
        return [line for line in lines if not line.startswith("#") and " original" not in line]
    return [line for line in lines if re.match(r"pic \d+ hd", line)]


def git(*args: str) -> str | None:
    result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=False)
    return result.stdout if result.returncode == 0 else None


def version_of(text: str) -> str | None:
    match = VERSION.search(text)
    return match.group(1) if match else None


def check(old: dict[str, str], new: dict[str, str]) -> str | None:
    """The problem, if the HD goldens changed but the version did not."""
    # A golden that the base does not have yet is new, not changed.
    changed = [path for path in GOLDENS if old[path] and hd_lines(path, old[path]) != hd_lines(path, new[path])]
    if changed and version_of(old[VERSION_FILE]) == version_of(new[VERSION_FILE]):
        return f"{', '.join(changed)} changed but analyser_version did not ({VERSION_FILE})"
    return None


def self_test() -> list[str]:
    base = {
        GOLDENS[0]: "pic 1 hd1 aa\npic 1 original1 bb\n",
        GOLDENS[1]: "pic 1 hd1 aa\npic 1 visual1 cc\n",
        VERSION_FILE: "analyser_version = 1;",
    }
    failures: list[str] = []
    if check(base, dict(base)):
        failures.append("self-test: no change was refused")
    if check(base, {**base, GOLDENS[0]: "pic 1 hd1 aa\npic 1 original1 dd\n"}):
        failures.append("self-test: an original-only change was refused")
    if not check(base, {**base, GOLDENS[1]: "pic 1 hd1 ee\npic 1 visual1 cc\n"}):
        failures.append("self-test: an HD change without a bump was accepted")
    if check(base, {**base, GOLDENS[0]: "pic 1 hd1 ff\n", VERSION_FILE: "analyser_version = 2;"}):
        failures.append("self-test: an HD change with a bump was refused")
    if check({**base, GOLDENS[1]: ""}, base):
        failures.append("self-test: a new golden file was refused")
    return failures


def main() -> int:
    failures = self_test()
    for failure in failures:
        print(failure)
    if failures:
        return 1
    base_ref = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("SCI_BASE_REF", "origin/main")
    base = git("merge-base", "HEAD", base_ref)
    if base is None:
        print(f"analyser_version: no base {base_ref} in this clone, not checked")
        return 0
    base = base.strip()
    old: dict[str, str] = {}
    new: dict[str, str] = {}
    for path in (*GOLDENS, VERSION_FILE):
        old[path] = git("show", f"{base}:{path}") or ""
        new[path] = (ROOT / path).read_text(encoding="utf-8")
    problem = check(old, new)
    if problem:
        print(f"PROBLEM: {problem}")
        return 1
    print(f"analyser_version: consistent with {base_ref} ({base[:7]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
