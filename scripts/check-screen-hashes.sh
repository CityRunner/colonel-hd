#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Golden screens (review/BACKLOG_V2.md B-18, stage 5 step 0.3, scope of
# Q-02): every screen of the scenarios of tests/scenarios/scenarios.txt and
# of the playthrough, as the program composes it in original and in HD mode
# at N = 1, 2, 5 and 10. The screens are those sci-vm-trace --screens dumps
# (every 60 ticks; scripts/check-vm-hashes.sh hashes them at 1x); here
# sci-hd-frames --hashes composes each mode's whole frame at N and hashes
# it. One line per screen and N in tests/golden/screens.txt:
#
#   <run> <tick> original<N> <sha-256> hd<N> <sha-256>
#
#   scripts/check-screen-hashes.sh [--check] [--jobs <n>] [--file <hash file>]
#
# Without --check it writes the hash file; with --check it compares and
# names, per run and N, the first tick and mode that differ. Inside the
# toolchain container with the game at /game. SIERRA_HD_FRAMES names
# sci-hd-frames (default the release build); the analysis cache is
# build/screen-hashes-cache (made on the first run; the hashes do not
# depend on it).
set -uo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
frames="${SIERRA_HD_FRAMES:-${root}/build/release/tools/hd-frames/sci-hd-frames}"
file="${root}/tests/golden/screens.txt"
check=false jobs=4
while [[ $# -gt 0 ]]; do
    case "$1" in
    --check) check=true; shift ;;
    --jobs) jobs="$2"; shift 2 ;;
    --file) file="$2"; shift 2 ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done
work="$(mktemp -d "${root}/build/screen-hashes.XXXXXX")"
trap 'rm -rf "${work}"' EXIT
cache="${root}/build/screen-hashes-cache"
mkdir -p "${cache}"
export frames work cache root

# One run: <name> <replay> <seed> <last screen tick> <N>.
one() {
    local name=$1 replay=$2 seed=$3 last=$4 scale=$5
    "${frames}" /game "${work}" --replay "${replay}" --seed "${seed}" --scale "${scale}" --hints "${root}/hints" \
        --cache "${cache}" --at "$(seq -s, 0 60 "${last}")" --hashes "${work}/${name}-${scale}.raw" \
        > "${work}/${name}-${scale}.log" 2>&1
    local code=$?
    {
        echo "${name} exit${scale} ${code}"
        sed "s/^/${name} /" "${work}/${name}-${scale}.raw" 2>/dev/null
    } > "${work}/${name}-${scale}.hashes"
}
export -f one

# The job list, the longest runs first. The playthrough's last screen is
# at tick 337560 (the game quits at 337613).
{
    for scale in 10 5 2 1; do
        echo "playthrough ${root}/tests/playthrough/playthrough.txt 0 337560 ${scale}"
    done
    grep -vE '^[[:space:]]*(#|$)' "${root}/tests/scenarios/scenarios.txt" | while read -r replay stop seed; do
        for scale in 10 5 2 1; do
            echo "${replay%.txt}-${stop}-${seed} ${root}/tests/scenarios/${replay} ${seed} $((stop / 60 * 60)) ${scale}"
        done
    done
} > "${work}/jobs.txt"
xargs -P "${jobs}" -L 1 bash -c 'one "$@"' _ < "${work}/jobs.txt"

# In a fixed order: runs as listed, N ascending.
hashes="$(awk '{print $1}' "${work}/jobs.txt" | uniq | while read -r name; do
    for scale in 1 2 5 10; do cat "${work}/${name}-${scale}.hashes"; done
done)"
if [[ "${check}" == false ]]; then
    printf '%s\n' "${hashes}" > "${file}"
    echo "wrote $(wc -l < "${file}") lines to ${file}"
    grep -E ' exit[0-9]+ [^0]' "${file}" && exit 1
    exit 0
fi
# The first differing line per run and N.
diff <(printf '%s\n' "${hashes}") "${file}" | grep '^[<>]' | awk '
    { key = $2 " " ($3 ~ /^exit/ ? $3 : "N" substr($4, 9)) }
    !(key in seen) { seen[key] = 1; print "DIFFERS " $0; n++ }
    END { if (n) { print n " differences (first per run and N)"; exit 1 } }'
# awk's status: with pipefail, grep's "no line matched" would count as a failure.
code=${PIPESTATUS[2]}
[[ ${code} -eq 0 ]] && echo "all screen hashes match"
exit ${code}
