#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# For a golden mismatch (scripts/check-screen-hashes.sh, check-local.sh):
# renders one screen, original | HD side by side at N, with a reference
# build (the original code, e.g. build/baseline-release) and the current
# build, and writes the pixel difference (scripts/diff-frames.py). Picture
# stacks work the same with --stack in place of the replay.
#
#   scripts/diff-screen.sh <reference build> <current build> <replay> <seed> <tick> <N>
#   scripts/diff-screen.sh <reference build> <current build> --stack <n>[+<n>...] <N>
#
# Inside the toolchain container with the game at /game. Output, derived
# from the game art and never committed: build/screen-diff/.
set -euo pipefail
cd "$(dirname "$0")/.."

usage="usage: diff-screen.sh <reference build> <current build> (<replay> <seed> <tick> | --stack <list>) <N>"
reference="${1:?${usage}}"
current="${2:?${usage}}"
out="build/screen-diff"
mkdir -p "${out}/reference" "${out}/current"
if [[ "${3:-}" == --stack ]]; then
    what=(--stack "${4:?${usage}}")
    scale="${5:?${usage}}"
    name="stack-$(tr '+' '-' <<< "$4")-${scale}"
else
    what=(--replay "${3:?${usage}}" --seed "${4:?${usage}}" --at "${5:?${usage}}")
    scale="${6:?${usage}}"
    name="$(basename "$3" .txt)-$4-$5-${scale}"
fi
for side in reference current; do
    build="${reference}"
    [[ "${side}" == current ]] && build="${current}"
    rm -rf "${out}/${side}/${name}"
    mkdir -p "${out}/${side}/${name}"
    "${build}/tools/hd-frames/sci-hd-frames" /game "${out}/${side}/${name}" "${what[@]}" --scale "${scale}" \
        --hints hints > "${out}/${side}/${name}.log" 2>&1
done
first="$(find "${out}/reference/${name}" -name '*.png' | head -1)"
second="$(find "${out}/current/${name}" -name '*.png' | head -1)"
python3 scripts/diff-frames.py "${first}" "${second}" "${out}/${name}-difference.png"
