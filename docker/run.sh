#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs a command inside the toolchain image with the repository mounted at
# /work. Extra read-only mounts (game folder, ScummVM source) are passed with
# COLONEL_HD_MOUNTS="host:container[,host:container...]". With
# COLONEL_HD_CCACHE=1 (CI), compiler output is cached in .ccache in the
# repository; otherwise ccache stays off.
#
#   docker/run.sh cmake --workflow --preset release
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "${here}/.." && pwd)"
tag="$("${here}/image-tag.sh")"

args=(--rm --init -v "${repo}:/work" -w /work -e HOME=/tmp)
if [[ "${COLONEL_HD_CCACHE:-}" == 1 ]]; then
    args+=(-e CCACHE_DIR=/work/.ccache -e CCACHE_BASEDIR=/work -e CCACHE_MAXSIZE=400M
           -e CMAKE_C_COMPILER_LAUNCHER=ccache -e CMAKE_CXX_COMPILER_LAUNCHER=ccache)
else
    args+=(-e CCACHE_DISABLE=1)
fi
if [[ -t 0 ]]; then args+=(-it); fi
if [[ -n "${COLONEL_HD_MOUNTS:-}" ]]; then
    IFS=',' read -ra mounts <<< "${COLONEL_HD_MOUNTS}"
    for m in "${mounts[@]}"; do args+=(-v "${m}:ro"); done
fi

docker run "${args[@]}" "${tag}" "$@"
