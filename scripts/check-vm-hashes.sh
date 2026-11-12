#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Hashes of every VM output (review/BACKLOG_V2.md B-19, stage 5 step 0.4):
# runs sci-vm-trace in its deterministic mode over the scenarios of
# tests/scenarios/scenarios.txt (AdLib, and the MT-32 driver), the
# playthrough to its last checkpoint and the save round trips, and records
# one SHA-256 per output file in tests/golden/vm.txt:
#
#   <run> <stream> <sha-256>
#
# Streams: trace (kernel calls), screens (the sorted sha256sum list of the
# screens and 1x maps), actors, parser, layout, opl or midi, state (state
# hashes), globals, round-trips. Hashes are taken of the files sci-vm-trace
# writes, so the tool itself is unchanged.
#
#   scripts/check-vm-hashes.sh [--check] [--keep <folder>] [--jobs <n>] [--file <hash file>]
#
# Without --check it writes the hash file; with --check it compares and
# names, per run, the first stream that differs. --keep keeps the full
# outputs (traces, lists of screen hashes) there, e.g. build/vm-baseline,
# to diff a later mismatch; the screens themselves are always deleted once
# hashed (disk). Inside the toolchain container with the game at /game;
# COLONEL_HD_VM_TRACE names sci-vm-trace (default the release build),
# COLONEL_HD_MT32_ROMS the ROM folder for --music mt32 (default /roms/roland
# when it exists; empty: the messages are only logged), COLONEL_HD_PROGRAM
# the sierra-hd whose playthrough under Xvfb gives the end state (default
# the release build; tools/playthrough/program-run.sh, AdLib, N = 4).
set -uo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
vm_trace="${COLONEL_HD_VM_TRACE:-${root}/build/release/tools/vm-trace/sci-vm-trace}"
file="${root}/tests/golden/vm.txt"
check=false keep="" jobs=4
while [[ $# -gt 0 ]]; do
    case "$1" in
    --check) check=true; shift ;;
    --keep) keep="$2"; shift 2 ;;
    --jobs) jobs="$2"; shift 2 ;;
    --file) file="$2"; shift 2 ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done
work="$(mktemp -d "${root}/build/vm-hashes.XXXXXX")"
trap 'rm -rf "${work}"' EXIT
roms="${COLONEL_HD_MT32_ROMS-/roms/roland}"
if [[ -n "${roms}" && ! -d "${roms}" ]]; then roms=""; fi
export vm_trace work keep root

# One run: <name> then sci-vm-trace's arguments after the trace file. Writes
# <work>/<name>.hashes.
one() {
    local name=$1
    shift
    local out="${work}/${name}"
    mkdir -p "${out}/screens" "${out}/saves"
    "${vm_trace}" /game "${out}/trace.txt" --screens "${out}/screens" --saves "${out}/saves" "$@" \
        > "${out}/log.txt" 2>&1
    local code=$?
    (cd "${out}/screens" && find . -type f -printf '%P\n' | LC_ALL=C sort | xargs -r sha256sum) > "${out}/screens.txt"
    rm -rf "${out}/screens" "${out}/saves"
    {
        echo "${name} exit ${code}"
        local f
        for f in trace screens actors parser layout opl midi state globals round-trips; do
            if [[ -f "${out}/${f}.txt" ]]; then
                echo "${name} ${f} $(sha256sum < "${out}/${f}.txt" | cut -d' ' -f1)"
            fi
        done
    } > "${work}/${name}.hashes"
    if [[ -n "${keep}" ]]; then
        mkdir -p "${keep}/${name}"
        cp "${out}"/*.txt "${keep}/${name}/"
    fi
    rm -rf "${out}"
}
export -f one

# The job list: one line per run, arguments separated by spaces.
{
    grep -vE '^[[:space:]]*(#|$)' "${root}/tests/scenarios/scenarios.txt" | while read -r replay stop seed; do
        base="${replay%.txt}-${stop}-${seed}"
        ticks="$((stop / 6)),$((stop * 2 / 6)),$((stop * 3 / 6)),$((stop * 4 / 6)),$((stop * 5 / 6)),$((stop - 1))"
        common="--seed ${seed} --stop-tick ${stop} --replay ${root}/tests/scenarios/${replay}"
        echo "${base}-adlib ${common} --actors @/actors.txt --parser @/parser.txt --layout @/layout.txt --opl @/opl.txt --state-hash ${ticks} @/state.txt"
        echo "${base}-mt32 ${common} --music mt32 --midi @/midi.txt${roms:+ --roms ${roms}}"
        trips="$((stop / 6)),$((stop * 2 / 6)),$((stop * 3 / 6)),$((stop * 4 / 6)),$((stop * 5 / 6))"
        echo "${base}-round-trips ${common} --opl @/opl.txt --state-hash ${ticks} @/state.txt --round-trip ${trips} @/round-trips.txt"
    done
    checkpoints="$(grep -v '^#' "${root}/tests/playthrough/checkpoints.txt" | awk 'NF >= 4 {print $2}' | sort -n | paste -sd, -)"
    echo "playthrough-adlib --seed 0 --stop-tick ${checkpoints##*,} --replay ${root}/tests/playthrough/playthrough.txt --actors @/actors.txt --parser @/parser.txt --layout @/layout.txt --opl @/opl.txt --state-hash ${checkpoints} @/state.txt --globals ${checkpoints} @/globals.txt"
} > "${work}/jobs.txt"

# "@/" stands for the run's own output folder.
run_line() {
    local name=$1
    shift
    one "${name}" "${@//@\//${work}/${name}/}"
}
export -f run_line
xargs -P "${jobs}" -L 1 bash -c 'run_line "$@"' _ < "${work}/jobs.txt"

# The program's own playthrough (Xvfb, --fast): its end state, which
# program-run.sh also checks against sci-vm-trace's.
COLONEL_HD_VM_TRACE="${vm_trace}" "${root}/tools/playthrough/program-run.sh" --out "${work}/program" \
    > "${work}/program.log" 2>&1
program_code=$?
{
    echo "program-hd4-adlib exit ${program_code}"
    if [[ -f "${work}/program/end-state.txt" ]]; then
        echo "program-hd4-adlib end-state $(sha256sum < "${work}/program/end-state.txt" | cut -d' ' -f1)"
    fi
} > "${work}/program.hashes"
if [[ -n "${keep}" ]]; then
    mkdir -p "${keep}/program-hd4-adlib"
    cp "${work}/program.log" "${work}/program/"*.txt "${keep}/program-hd4-adlib/" 2>/dev/null
fi

hashes="$( (cut -d' ' -f1 "${work}/jobs.txt" | while read -r name; do cat "${work}/${name}.hashes"; done; cat "${work}/program.hashes"))"
if [[ "${check}" == false ]]; then
    printf '%s\n' "${hashes}" > "${file}"
    echo "wrote $(wc -l < "${file}") lines to ${file}"
    exit 0
fi
differences=0
while read -r name stream hash; do
    want="$(grep -E "^${name} ${stream} " "${file}" | cut -d' ' -f3)"
    if [[ "${want}" != "${hash}" ]]; then
        if ! grep -q "^${name} " <<< "${reported:-}"; then
            echo "DIFFERS ${name}: first differing stream ${stream} (expected ${want:-none}, got ${hash})"
            reported+="${name} "$'\n'
            differences=$((differences + 1))
        fi
    fi
done <<< "${hashes}"
missing="$(comm -23 <(cut -d' ' -f1,2 "${file}" | sort) <(cut -d' ' -f1,2 <<< "${hashes}" | sort))"
if [[ -n "${missing}" ]]; then
    echo "missing streams:"; echo "${missing}"
    differences=$((differences + 1))
fi
echo "$([[ ${differences} -eq 0 ]] && echo "all VM hashes match" || echo "${differences} runs differ")"
[[ ${differences} -eq 0 ]]
