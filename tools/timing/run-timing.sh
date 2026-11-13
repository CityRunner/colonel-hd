#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The timing run (B-10, docs/timing.md) on Linux: the playthrough at N = 1,
# 2, 5 and 10, one CSV per N, the first with an empty analysis cache.
#
#   tools/timing/run-timing.sh <colonel-hd> <game folder> <output folder>
#
# In the toolchain container it needs a display: there, use
# tools/playthrough/program-run.sh --timing (Xvfb) instead.
set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "usage: $0 <colonel-hd> <game folder> <output folder>" >&2
    exit 2
fi
program="$1" game="$2" out="$3"
replay="$(cd "$(dirname "$0")/../.." && pwd)/tests/playthrough/playthrough.txt"
rm -rf "${out}/data"
mkdir -p "${out}/data"
for n in 1 2 5 10; do
    echo "Timing at N = ${n} ..."
    "${program}" --data "${out}/data" --replay "${replay}" --seed 0 --timing "${out}/timing-${n}.csv" \
        --scale "${n}" "${game}"
done
echo "Done: ${out}/timing-{1,2,5,10}.csv"
