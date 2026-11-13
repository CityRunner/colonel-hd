#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Regenerates tests/unit/vm/home_game_goldens.hpp (B-07): runs the sci-vm
# unit tests twice with COLONEL_HD_HOME_GAME_GOLDENS set, checks that both
# runs give the same goldens, and writes them into the header. Run in the
# toolchain container: docker/run.sh scripts/update-home-game-goldens.sh
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-${root}/build/debug}"
header="${root}/tests/unit/vm/home_game_goldens.hpp"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

cmake --build "${build}" --target sci-vm-tests >/dev/null
for run in 1 2; do
    COLONEL_HD_HOME_GAME_GOLDENS="${work}/run${run}.txt" "${build}/tests/unit/sci-vm-tests" >/dev/null
done
if ! cmp -s "${work}/run1.txt" "${work}/run2.txt"; then
    echo "The two runs gave different goldens:" >&2
    diff "${work}/run1.txt" "${work}/run2.txt" >&2 || true
    exit 1
fi
python3 - "${header}" "${work}/run1.txt" <<'PY'
import re
import sys

header, lines = sys.argv[1], open(sys.argv[2]).read()
text = open(header).read()
text = re.sub(r"(    return \{\n).*?(    \};\n)", lambda m: m.group(1) + "        // GOLDENS\n" + lines + m.group(2),
              text, flags=re.S)
open(header, "w").write(text)
PY
echo "$(grep -c '^        {"' "${header}") goldens written to ${header#"${root}/"}; both runs equal."
