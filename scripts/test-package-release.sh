#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks the release archive's layout (B-03): the program, hints/, the
# README, licenses/ and the empty data folders game/, rom/ and saves/, each
# with a README.txt and nothing else; no game data anywhere.
#
#   scripts/test-package-release.sh <build directory>     (Linux build)
set -euo pipefail

build="${1:?usage: test-package-release.sh <build directory>}"
root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

archive_dir=$("$root/scripts/package-release.sh" "$build" linux-test "$work/out")
archive="$archive_dir.tar.gz"
name=$(basename "$archive_dir")
tar -tzf "$archive" | sort > "$work/list.txt"

failed=0
expect() {
    if ! grep -qx "$1" "$work/list.txt"; then
        echo "missing from the archive: $1"
        failed=1
    fi
}
expect "$name/colonel-hd"
expect "$name/README.md"
expect "$name/hints/README.md"
for folder in game rom saves; do
    expect "$name/$folder/"
    expect "$name/$folder/README.txt"
    extra=$(grep "^$name/$folder/." "$work/list.txt" | grep -vx "$name/$folder/README.txt" || true)
    if [[ -n "$extra" ]]; then
        echo "unexpected in $folder/: $extra"
        failed=1
    fi
done
if grep -iE '\.(map|[0-9]{3})$|(^|/)(resource|patch|view|pic|script|text|sound|vocab|font|cursor)\.' "$work/list.txt" |
    grep -v "^$name/hints/" | grep -q .; then
    echo "game data in the archive:"
    grep -iE '\.(map|[0-9]{3})$' "$work/list.txt" | grep -v "^$name/hints/"
    failed=1
fi
[[ "$failed" == 0 ]] && echo "release archive layout: ok ($(wc -l < "$work/list.txt") entries)"
exit "$failed"
