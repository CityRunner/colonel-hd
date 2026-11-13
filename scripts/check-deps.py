#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that each library's declared dependencies are the ones it uses (B-22).

Every library under src/ is declared with sci_add_library(NAME sci-x ...
DEPENDS ...) in src/sci-x/CMakeLists.txt. Its sources and headers include
other libraries' headers as "sci/<name>/...", which belong to sci-<name>.
This fails when a library includes a header of a library it does not
declare (even one it gets through another dependency), or declares a
dependency whose headers it never includes. Without a build.

Run from anywhere; exits non-zero and lists every problem.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

DECLARATION = re.compile(r"sci_add_library\((.*?)\)", re.DOTALL)
NAME = re.compile(r"\bNAME\s+(sci-[a-z0-9-]+)")
DEPENDS = re.compile(r"\bDEPENDS\s+((?:sci-[a-z0-9-]+\s*)+)")
INCLUDE = re.compile(r'^\s*#\s*include\s+"sci/([a-z0-9_]+)/', re.MULTILINE)
SOURCE = re.compile(r"\.(cpp|hpp|h)$")


def declared(cmake_text: str) -> tuple[str, set[str]] | None:
    """The library's name and declared dependencies, or None."""
    match = DECLARATION.search(cmake_text)
    if not match:
        return None
    name = NAME.search(match.group(1))
    if not name:
        return None
    depends = DEPENDS.search(match.group(1))
    return name.group(1), set(depends.group(1).split()) if depends else set()


def used(library_dir: Path, name: str) -> set[str]:
    """The libraries whose headers a library's files include, itself left out."""
    libraries: set[str] = set()
    for path in library_dir.rglob("*"):
        if path.is_file() and SOURCE.search(path.name):
            for short in INCLUDE.findall(path.read_text(encoding="utf-8")):
                libraries.add(f"sci-{short}")
    libraries.discard(name)
    return libraries


def problems_of(name: str, declared_deps: set[str], used_deps: set[str]) -> list[str]:
    """What is wrong with one library's declaration."""
    problems = [f"{name} includes headers of {dep} but does not declare it" for dep in sorted(used_deps - declared_deps)]
    problems += [f"{name} declares {dep} but includes none of its headers" for dep in sorted(declared_deps - used_deps)]
    return problems


def self_test() -> list[str]:
    """Problems of the check on known cases."""
    failures: list[str] = []
    if declared("sci_add_library(NAME sci-a\n    SOURCES a.cpp\n    DEPENDS sci-b sci-c)") != ("sci-a", {"sci-b", "sci-c"}):
        failures.append("self-test: DEPENDS not read")
    if problems_of("sci-a", {"sci-b"}, {"sci-b"}):
        failures.append("self-test: a correct library was refused")
    if not problems_of("sci-a", {"sci-b"}, {"sci-b", "sci-c"}):
        failures.append("self-test: an undeclared include was accepted")
    if not problems_of("sci-a", {"sci-b", "sci-c"}, {"sci-b"}):
        failures.append("self-test: an unused dependency was accepted")
    return failures


def main() -> int:
    problems = self_test()
    count = 0
    for cmake in sorted(SRC.glob("sci-*/CMakeLists.txt")):
        found = declared(cmake.read_text(encoding="utf-8"))
        if found is None:
            problems.append(f"{cmake.relative_to(ROOT)}: no sci_add_library(NAME ...)")
            continue
        name, deps = found
        count += 1
        problems += problems_of(name, deps, used(cmake.parent, name))
    print(f"{count} libraries checked")
    for problem in problems:
        print(f"PROBLEM: {problem}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
