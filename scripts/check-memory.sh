#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# B-11: the program's peak resident memory over the playthrough at N = 1
# and N = 10 (the timing mode, docs/timing.md) against the baseline in
# tests/golden/memory.txt; fails when a run exceeds its baseline by more
# than 10 %. With --update, measures each N three times and writes the
# medians as the new baseline (after a deliberate change, saying why in the
# commit; the numbers are recorded per release in the release notes).
#
#   COLONEL_HD_MOUNTS=/path/to/game:/game docker/run.sh scripts/check-memory.sh [--update]
#
# Needs build/release (colonel-hd and sci-vm-trace) and the game at /game.
set -euo pipefail
cd "$(dirname "$0")/.."

update=0
if [[ "${1:-}" == "--update" ]]; then
    update=1
fi
golden=tests/golden/memory.txt
out=build/memory
mkdir -p "${out}"

# The peak memory of one timing run at N, in KiB.
measure() {
    local n="$1" run="$2"
    tools/playthrough/program-run.sh --scale "${n}" --timing "${PWD}/${out}/timing-${n}-${run}.csv" \
        --out "${out}/run-${n}-${run}" > "${out}/run-${n}-${run}.log" 2>&1 || {
        echo "the run at N = ${n} failed: ${out}/run-${n}-${run}.log" >&2
        exit 1
    }
    grep '^peak_memory_kib,' "${out}/timing-${n}-${run}.csv" | cut -d, -f2
}

status=0
lines=()
for n in 1 10; do
    if [[ "${update}" == 1 ]]; then
        values=("$(measure "${n}" 1)" "$(measure "${n}" 2)" "$(measure "${n}" 3)")
        median="$(printf '%s\n' "${values[@]}" | sort -n | sed -n 2p)"
        echo "N = ${n}: ${values[*]} KiB, median ${median}"
        lines+=("${n} ${median}")
        continue
    fi
    baseline="$(awk -v n="${n}" '$1 == n { print $2 }' "${golden}")"
    peak="$(measure "${n}" 1)"
    limit=$(( baseline + baseline / 10 ))
    if (( peak > limit )); then
        echo "memory N = ${n}: ${peak} KiB, over the baseline ${baseline} KiB + 10 % (${limit})"
        status=1
    else
        echo "memory N = ${n}: ${peak} KiB, baseline ${baseline} KiB (limit ${limit}) ok"
    fi
done
if [[ "${update}" == 1 ]]; then
    {
        echo "# B-11: peak resident memory (KiB) of the playthrough in colonel-hd's timing"
        echo "# mode at N = 1 and 10, median of 3 runs in the toolchain container (Xvfb)."
        echo "# scripts/check-memory.sh fails above these + 10 %; --update rewrites them."
        printf '%s\n' "${lines[@]}"
    } > "${golden}"
    echo "wrote ${golden}"
fi
exit "${status}"
