#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Repository policy checks that are not covered by clang-tidy.

1. No game resources (SCI0 patch files such as SCRIPT.052 included), ROM
   files, saved games, program configuration or images derived from the
   game art are tracked (spec sections 2 and 3.4). Images are allowed only
   where listed in ALLOWED_IMAGE_DIRS (none so far).
2. No float or long double in project code (spec section 5.3: double only).
3. Every project source file and script (.cpp, .hpp, .py, .sh) starts with
   an SPDX licence identifier (in its first three lines, so after a shebang).
4. Every NOLINT in project code names its checks and states a reason after
   a colon (spec section 5.1; B-27): `// NOLINT(check): why`.

Run from anywhere; exits non-zero and lists every violation. With
--self-test it checks the name rules of point 1 against lists of names to
refuse and to accept, and reads no file.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

GAME_DATA = re.compile(
    r"(^|/)(resource\.(map|\d{3})|message\.(map|\d{3})|[^/]+\.(scr|hep|drv|p56|v56|sci|rom)"
    # SCI0 patch files, one resource each: SCRIPT.052, TEXT.052, PATCH.001, ...
    r"|(view|pic|script|text|sound|memory|vocab|font|cursor|patch)\.\d{3})$",
    re.IGNORECASE,
)
# Files the program writes next to itself: saved games and its configuration.
USER_DATA = re.compile(r"(^|/)([^/]+\.sav|colonel-hd\.ini)$", re.IGNORECASE)
# local/ holds the user's own files; only its README and placeholders are tracked.
LOCAL_ALLOWED = re.compile(r"^local/(README\.md|(game|rom|saves)/\.gitkeep)$")
IMAGE = re.compile(r"\.(png|ppm|pgm|bmp|gif|jpe?g|tga|webp)$", re.IGNORECASE)
ALLOWED_IMAGE_DIRS: tuple[str, ...] = ()
CPP_SOURCE = re.compile(r"\.(cpp|hpp)$")
SOURCE = re.compile(r"\.(cpp|hpp|py|sh)$")
FLOAT_TYPES = re.compile(r"\b(float|long\s+double)\b")
# A NOLINT marker with its optional check list; a reason follows a colon.
NOLINT = re.compile(r"NOLINT(?:NEXTLINE|BEGIN|END)?(\([^)]*\))?(:\s*\S)?")
SPDX = "SPDX-License-Identifier:"


def tracked_files() -> list[str]:
    """Tracked and new (not ignored) files that exist in the working tree."""
    out = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    ).stdout
    return sorted({p for p in out.decode().split("\0") if p and (ROOT / p).is_file()})


def strip_comments_and_strings(text: str) -> str:
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r'"(\\.|[^"\\])*"', '""', text)


def name_problem(path: str) -> str | None:
    """Why a file of this name is not committed, if it is not."""
    if GAME_DATA.search(path):
        return "looks like a game resource or ROM file"
    if USER_DATA.search(path):
        return "saved games and the program's configuration are not committed"
    if path.startswith("local/") and not LOCAL_ALLOWED.match(path):
        return "local/ holds your own files; they are not committed"
    return None


# Names the checks above must refuse and names they must accept.
SELF_TEST_REJECTED = (
    "SCRIPT.052",
    "text.052",
    "PATCH.001",
    "tests/data/View.123",
    "local/game/RESOURCE.MAP",
    "local/game/resource.001",
    "local/rom/MT32_CONTROL.ROM",
    "local/game/ADL.DRV",
    "local/saves/colonel-hd-001.sav",
    "tests/Act1.SAV",
    "colonel-hd.ini",
    "build-tree/Colonel-HD.ini",
    "local/game/RESOURCE.000",
    "local/game/README",
    "local/rom/notes.txt",
    "local/extra.txt",
)
SELF_TEST_ACCEPTED = (
    "hints/pic.052.json",
    "tests/scenarios/parser-words-kitchen.txt",
    "tests/golden/pictures.txt",
    "docs/formats/resources.md",
    "docs/formats/save-format.md",
    "src/sci-resource/src/resource_map.cpp",
    "src/sci-vm/src/script.cpp",
    "src/sci-text/src/font.cpp",
    "tests/fuzz/corpus/save/synthetic-file",
    "tools/scummvm-patches/0001-dump-resources.patch",
    "tests/unit/resource/patch_file_test.cpp",
    "script.0520",
    "view.12",
    "local/README.md",
    "local/game/.gitkeep",
    "local/rom/.gitkeep",
    "local/saves/.gitkeep",
)


def nolint_problem(line: str) -> str | None:
    """Why a line's NOLINT marker breaks rule 4, or None."""
    if "//" not in line:
        return None
    comment = line[line.index("//") :]
    for match in NOLINT.finditer(comment):
        if match.group(1) is None:
            return "NOLINT names no check (B-27): use NOLINT(check): reason"
        if match.group(2) is None and not match.group(0).startswith("NOLINTEND"):
            return "NOLINT states no reason (B-27): add ': reason' after the checks"
    return None


SELF_TEST_NOLINT_REJECTED = (
    "x(); // NOLINT",
    "x(); // NOLINT(*-reinterpret-cast)",
    "// NOLINTNEXTLINE(misc-no-recursion)",
    "// NOLINTNEXTLINE: a reason but no check",
)
SELF_TEST_NOLINT_ACCEPTED = (
    "x(); // NOLINT(*-reinterpret-cast): bytes as text.",
    "// NOLINTNEXTLINE(misc-no-recursion,readability-function-cognitive-complexity): kept close to ScummVM.",
    "// NOLINTEND(misc-no-recursion)",
    "x(); // no marker here",
)


def self_test() -> int:
    """Checks the name rules and the NOLINT rule against their lists."""
    failures = [f"{name}: accepted, must be refused" for name in SELF_TEST_REJECTED if not name_problem(name)]
    failures += [f"{name}: refused, must be accepted" for name in SELF_TEST_ACCEPTED if name_problem(name)]
    failures += [f"{line}: accepted, must be refused" for line in SELF_TEST_NOLINT_REJECTED if not nolint_problem(line)]
    failures += [f"{line}: refused, must be accepted" for line in SELF_TEST_NOLINT_ACCEPTED if nolint_problem(line)]
    for failure in failures:
        print(failure)
    print(f"self-test: {len(SELF_TEST_REJECTED)} names refused, {len(SELF_TEST_ACCEPTED)} accepted, {len(failures)} failures")
    return 1 if failures else 0


def main() -> int:
    if sys.argv[1:] == ["--self-test"]:
        return self_test()
    problems: list[str] = []
    for path in tracked_files():
        if path.startswith("third_party/"):
            continue
        problem = name_problem(path)
        if problem:
            problems.append(f"{path}: {problem}")
        if IMAGE.search(path) and not path.startswith(ALLOWED_IMAGE_DIRS or ("\0",)):
            problems.append(f"{path}: images are not committed (docs/reports/images/ is ignored)")
        if SOURCE.search(path):
            text = (ROOT / path).read_text(encoding="utf-8")
            if SPDX not in "\n".join(text.splitlines()[:3]):
                problems.append(f"{path}: missing SPDX licence identifier in the first lines")
            if not CPP_SOURCE.search(path):
                continue
            for number, line in enumerate(strip_comments_and_strings(text).splitlines(), 1):
                if FLOAT_TYPES.search(line):
                    problems.append(f"{path}:{number}: float and long double are not used (spec 5.3)")
            for number, line in enumerate(text.splitlines(), 1):
                problem = nolint_problem(line)
                if problem:
                    problems.append(f"{path}:{number}: {problem}")

    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
