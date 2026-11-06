#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The local gate with the game files (tests/README.md, "The local gate";
# review/BACKLOG_V2.md B-08, B-18, B-19): every golden output and VM hash,
# checked against the committed files. CI cannot run it (no game files).
#
#   S4  tests/golden/resources.txt   every picture layer (N = 1 ... 16), cel, glyph, cursor
#       tests/golden/pictures.txt    every picture and picture stack (HD and original)
#       tests/golden/sounds.txt      the OPL output of every sound
#       tests/golden/screens.txt     every scenario and playthrough screen, original and HD, N = 1, 2, 5, 10
#   S5  tests/golden/vm.txt          every output of sci-vm-trace over the scenarios, the
#                                    playthrough and the round trips, and the program's end state
#
#   scripts/check-local.sh [<build folder>]     (default build/release)
#
# Inside the toolchain container with the game at /game and the MT-32 ROMs
# at /roms (folder roland/ inside):
#
#   SIERRA_HD_MOUNTS=/path/to/game:/game,/path/to/roms:/roms docker/run.sh scripts/check-local.sh build/release
#
# Runs every check, prints one line per check with its time, and exits with
# 1 if any failed. About 25-45 minutes on 4 cores (review/REFACTOR_REPORT.md).
set -uo pipefail
cd "$(dirname "$0")/.."

build="${1:-build/release}"
log="build/check-local"
mkdir -p "${log}"
failed=0

check() {
    local name=$1
    shift
    local start
    start=$(date +%s)
    "$@" > "${log}/${name}.log" 2>&1
    local code=$?
    local result=ok
    if [[ ${code} -ne 0 ]]; then
        result=FAILED
        failed=1
    fi
    printf '%-10s %-6s %5d s  (%s)\n' "${name}" "${result}" $(($(date +%s) - start)) "${log}/${name}.log"
}

check resources "${build}/tools/golden-resources/sci-golden-resources" /game tests/golden/resources.txt --hints hints --check
check pictures "${build}/tools/golden-hashes/sci-golden-hashes" /game tests/golden/pictures.txt --check --hints hints
check sounds "${build}/tools/sound-render/sci-sound-render" /game --golden tests/golden/sounds.txt --check
check screens env SIERRA_HD_FRAMES="${PWD}/${build}/tools/hd-frames/sci-hd-frames" scripts/check-screen-hashes.sh --check
check vm env SIERRA_HD_VM_TRACE="${PWD}/${build}/tools/vm-trace/sci-vm-trace" \
    SIERRA_HD_PROGRAM="${PWD}/${build}/src/app/sierra-hd" scripts/check-vm-hashes.sh --check

if [[ ${failed} -ne 0 ]]; then
    echo "The local gate FAILED: see the logs above."
    exit 1
fi
echo "The local gate passed."
