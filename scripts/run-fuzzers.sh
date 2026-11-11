#!/usr/bin/env bash
# Runs every libFuzzer target of a fuzz build (preset "fuzz") for a fixed time
# each, starting from its seed corpus. New inputs go to a scratch corpus
# directory, never into the repository; with SCI_FUZZ_CORPUS=<dir> they are
# kept there (one folder per target), so later runs start from them (CI
# caches it).
#   scripts/run-fuzzers.sh build/fuzz 60
#
# Known failures (tests/fuzz/known-failures.txt, or SCI_FUZZ_KNOWN_FAILURES):
# a target listed there still runs; if it stops with the listed kind of
# failure (crash or timeout) and its report has a stack frame matching the
# listed pattern, the failure is reported as known (with its bug, B-xx) and
# the run goes on. Any other failure fails the run, as before. libFuzzer
# stops a target at its first failure, so a known one ends that target's
# time early. scripts/test-run-fuzzers.sh checks this logic.
set -euo pipefail
cd "$(dirname "$0")/.."

build_dir="${1:?usage: run-fuzzers.sh <fuzz build directory> [seconds per target]}"
seconds="${2:-60}"
if [[ -n "${SCI_FUZZ_CORPUS:-}" ]]; then
    scratch="${SCI_FUZZ_CORPUS}"
    mkdir -p "${scratch}"
else
    scratch="$(mktemp -d)"
    trap 'rm -rf "${scratch}"' EXIT
fi
artifacts="${PWD}/build/fuzz-artifacts"
logs="${PWD}/build/fuzz-logs"
known="${SCI_FUZZ_KNOWN_FAILURES:-${PWD}/tests/fuzz/known-failures.txt}"

# The failure in a libFuzzer log: crash, timeout or other.
failure_kind() {
    if grep -q 'SUMMARY: libFuzzer: timeout' "$1"; then
        echo timeout
    elif grep -qE 'SUMMARY: (libFuzzer: deadly signal|AddressSanitizer|UndefinedBehaviorSanitizer)' "$1"; then
        echo crash
    else
        echo other
    fi
}

# The bug of a known failure of target $1 with log $2, or nothing. Only the
# stack frames after the failure report count (not libFuzzer's NEW_FUNC
# lines).
known_bug() {
    local target=$1 log=$2 kind name bug listed pattern
    kind="$(failure_kind "${log}")"
    [[ -f "${known}" ]] || return 0
    while read -r name bug listed pattern; do
        [[ -z "${name}" || "${name}" == \#* ]] && continue
        if [[ "${name}" == "${target}" && "${listed}" == "${kind}" ]] &&
            sed -n '/^==[0-9]*== ERROR\|^ALARM:\|runtime error:/,$p' "${log}" | grep -E '^ +#[0-9]+ ' |
            grep -qE -- "${pattern}"; then
            echo "${bug}"
            return 0
        fi
    done < "${known}"
    return 0
}

while IFS='=' read -r target corpus; do
    [[ -z "${target}" ]] && continue
    binary="$(find "${build_dir}" -type f -name "${target}" -perm -u+x | head -n1)"
    echo "== ${target} (${seconds}s)"
    mkdir -p "${scratch}/${target}"
    # A single input taking more than 10 s is reported as a hang. Crash,
    # leak and out-of-memory inputs are kept in build/fuzz-artifacts.
    mkdir -p "${artifacts}"
    mkdir -p "${logs}"
    log="${logs}/${target}.log"
    if "${binary}" -max_total_time="${seconds}" -timeout=10 -print_final_stats=1 \
        -artifact_prefix="${artifacts}/${target}-" "${scratch}/${target}" "${corpus}" > "${log}" 2>&1; then
        tail -n 15 "${log}"
        continue
    fi
    bug="$(known_bug "${target}" "${log}")"
    if [[ -n "${bug}" ]]; then
        echo "known failure of ${target}: ${bug} ($(failure_kind "${log}"); tests/fuzz/known-failures.txt)"
        continue
    fi
    cat "${log}"
    echo "${target} failed (not a known failure)" >&2
    exit 1
done < "${build_dir}/fuzz-targets.txt"
