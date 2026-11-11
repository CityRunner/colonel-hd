#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Checks the known-failure logic of scripts/run-fuzzers.sh (customer's
# decision 2a, 2026-11-06) with fake fuzz targets, without a fuzz build:
# a listed crash and a listed timeout pass; an unlisted crash in a listed
# target, a listed target's other kind of failure, and a crash of an
# unlisted target fail. Run from anywhere; exits with 1 on a wrong result.
set -uo pipefail
cd "$(dirname "$0")/.."

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
mkdir -p "${work}/build" "${work}/corpus"
cat > "${work}/known.txt" << 'LIST'
# test list
fuzz-a B-1 crash in sci::vm::Object::species\(\) const .*/object\.hpp:53
fuzz-b B-2 timeout in sci::vm::MidiParser::on_timer\(\) .*/midi_parser\.cpp
LIST

# A fake target printing a libFuzzer report: <name> <summary> <frame>.
fake() {
    cat > "${work}/build/$1" << FAKE
#!/usr/bin/env bash
# A matching frame before the report (as libFuzzer's coverage lines), which must not count.
echo "    #1 0x0 in sci::vm::Object::species() const /work/src/sci-vm/src/object.hpp:53"
echo "==1== ERROR: libFuzzer: something"
echo "    #9 0x1 in $3"
echo "SUMMARY: $2"
exit 1
FAKE
    chmod +x "${work}/build/$1"
}

failures=0
# expect <pass|fail> <target> <summary> <frame>
expect() {
    local want=$1
    fake "$2" "$3" "$4"
    echo "$2=${work}/corpus" > "${work}/build/fuzz-targets.txt"
    local got=fail
    if SCI_FUZZ_KNOWN_FAILURES="${work}/known.txt" scripts/run-fuzzers.sh "${work}/build" 1 > "${work}/out.txt" 2>&1; then
        got=pass
    fi
    if [[ "${got}" == "${want}" ]]; then
        echo "ok    $2: ${want} ($3)"
    else
        echo "WRONG $2: expected ${want}, got ${got} ($3)"
        cat "${work}/out.txt"
        failures=$((failures + 1))
    fi
}

expect pass fuzz-a "libFuzzer: deadly signal" "sci::vm::Object::species() const /work/src/sci-vm/src/object.hpp:53:48"
expect pass fuzz-b "libFuzzer: timeout" "sci::vm::MidiParser::on_timer() /work/src/sci-vm/src/midi_parser.cpp:352:14"
expect fail fuzz-a "libFuzzer: deadly signal" "sci::vm::Script::load() /work/src/sci-vm/src/script.cpp:10:1"
expect fail fuzz-a "libFuzzer: timeout" "sci::vm::Object::species() const /work/src/sci-vm/src/object.hpp:53:48"
expect fail fuzz-b "libFuzzer: deadly signal" "sci::vm::MidiParser::on_timer() /work/src/sci-vm/src/midi_parser.cpp:352:14"
expect fail fuzz-c "libFuzzer: deadly signal" "sci::vm::Object::species() const /work/src/sci-vm/src/object.hpp:53:48"
expect fail fuzz-a "libFuzzer: out-of-memory (malloc(1))" "sci::vm::Object::species() const /work/src/sci-vm/src/object.hpp:53:48"

[[ ${failures} -eq 0 ]] && echo "run-fuzzers known-failure logic: all checks passed"
exit $((failures > 0))
