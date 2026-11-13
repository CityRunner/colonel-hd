#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that every fuzz target is in the list that is fuzzed (B-44).

tests/fuzz/CMakeLists.txt writes fuzz-targets.txt, the list that
scripts/run-fuzzers.sh and the CI fuzz job run, from the targets added
before that point. A target added after it is built but never fuzzed.
This checks, without a build, that the list is written after the last
sci_add_fuzz_target, and that every tests/fuzz/fuzz_*.cpp is the source of
a target.

Run from anywhere; exits non-zero and lists every problem.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FUZZ = ROOT / "tests" / "fuzz"

TARGET = re.compile(r"^sci_add_fuzz_target\(NAME\s+(\S+)", re.MULTILINE)
SOURCE = re.compile(r"^\s*SOURCE\s+([^\s)]+)", re.MULTILINE)
LIST = re.compile(r"fuzz-targets\.txt")


def problems_in(text: str, sources: list[str]) -> tuple[list[str], list[str]]:
    """The problems and the target names of a fuzz CMakeLists.txt text."""
    problems: list[str] = []
    targets = [(m.group(1), m.start()) for m in TARGET.finditer(text)]
    list_match = LIST.search(text)
    if list_match is None:
        problems.append("tests/fuzz/CMakeLists.txt does not write fuzz-targets.txt")
    else:
        for name, start in targets:
            if start > list_match.start():
                problems.append(f"{name} is added after fuzz-targets.txt is written, so it is never fuzzed")
    used = set(SOURCE.findall(text))
    for source in sources:
        if source not in used:
            problems.append(f"tests/fuzz/{source} is not the source of any fuzz target")
    return problems, [name for name, _ in targets]


def self_test() -> list[str]:
    """Problems of the check on known-good and known-bad texts."""
    good = "sci_add_fuzz_target(NAME a\n    SOURCE a.cpp)\n# fuzz-targets.txt\n"
    bad = "# fuzz-targets.txt\nsci_add_fuzz_target(NAME a\n    SOURCE a.cpp)\n"
    failures: list[str] = []
    if problems_in(good, ["a.cpp"])[0]:
        failures.append("self-test: a correct file was refused")
    if not problems_in(bad, ["a.cpp"])[0]:
        failures.append("self-test: a target after the list was accepted")
    if not problems_in(good, ["a.cpp", "b.cpp"])[0]:
        failures.append("self-test: an unused fuzz source was accepted")
    return failures


def main() -> int:
    problems = self_test()
    text = (FUZZ / "CMakeLists.txt").read_text(encoding="utf-8")
    sources = sorted(p.name for p in FUZZ.glob("fuzz_*.cpp"))
    found, targets = problems_in(text, sources)
    problems += found
    print(f"{len(targets)} fuzz targets, all in the fuzzed list" if not found else f"{len(targets)} fuzz targets")
    for problem in problems:
        print(f"PROBLEM: {problem}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
