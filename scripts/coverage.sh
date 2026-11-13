#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Line coverage of the CI test suite (B-08; the method of review/MAP.md,
# section 6.4): a Debug build with clang's source-based coverage, without
# the tools and the program, all CTest tests, then llvm-cov per module and
# for src/sci-vm/src/machine.cpp. With --min-vm <percent>, exits with 1 when
# sci-vm's line coverage is below it.
#
#   docker/run.sh scripts/coverage.sh [--min-vm 70]
set -euo pipefail
cd "$(dirname "$0")/.."

min_vm=""
if [[ "${1:-}" == "--min-vm" ]]; then
    min_vm="${2:?--min-vm needs a percentage}"
fi
build=build/coverage
profiles="${PWD}/build/coverage-profiles"
rm -rf "${profiles}"
mkdir -p "${profiles}"
flags="-fprofile-instr-generate -fcoverage-mapping"
cmake -S . -B "${build}" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug -DSCI_BUILD_TOOLS=OFF -DSCI_BUILD_APP=OFF -DSCI_WARNINGS_AS_ERRORS=OFF \
    "-DCMAKE_CXX_FLAGS=${flags}" "-DCMAKE_C_FLAGS=${flags}" "-DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate" \
    > "${build}.configure.log"
cmake --build "${build}" > "${build}.build.log"
LLVM_PROFILE_FILE="${profiles}/%m-%p.profraw" ctest --test-dir "${build}" -j4 --output-on-failure > "${build}.test.log"
llvm-profdata merge -sparse "${profiles}"/*.profraw -o "${profiles}/all.profdata"

binaries=()
while IFS= read -r binary; do
    binaries+=("${binary}")
done < <(find "${build}/tests" -type f -perm -u+x -name 'sci-*' | sort)
objects=("${binaries[0]}")
for binary in "${binaries[@]:1}"; do
    objects+=(-object "${binary}")
done
report() {
    llvm-cov report "${objects[@]}" -instr-profile="${profiles}/all.profdata" \
        -ignore-filename-regex="(third_party|tests/|/usr/)" "$@" 2> /dev/null
}
report > "${build}.report.txt"

# Per module: the sum of its files' lines and missed lines.
python3 - "${build}.report.txt" "${min_vm}" <<'PY'
import re, sys
lines = {}
missed = {}
for row in open(sys.argv[1]):
    m = re.match(r"(\S+)\s+(?:\d+\s+\d+\s+\S+\s+){2}(\d+)\s+(\d+)\s+\S+", row)
    if not m or "src/" not in m.group(1):
        continue
    path = m.group(1)
    module = path.split("src/")[1].split("/")[0]
    lines[module] = lines.get(module, 0) + int(m.group(2))
    missed[module] = missed.get(module, 0) + int(m.group(3))
    if path.endswith("sci-vm/src/machine.cpp"):
        print(f"machine.cpp: {100 * (1 - int(m.group(3)) / int(m.group(2))):.1f} % of {m.group(2)} lines")
for module in sorted(lines):
    print(f"{module}: {100 * (1 - missed[module] / lines[module]):.1f} % of {lines[module]} lines")
if sys.argv[2]:
    vm = 100 * (1 - missed.get("sci-vm", 0) / max(lines.get("sci-vm", 1), 1))
    if vm < float(sys.argv[2]):
        print(f"sci-vm line coverage {vm:.1f} % is below {sys.argv[2]} %")
        sys.exit(1)
PY
