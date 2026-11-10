#!/usr/bin/env bash
set -euo pipefail

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="/mingw64/bin:$DEVKITPRO/tools/bin:$DEVKITA64/bin:$DEVKITPRO/portlibs/switch/bin:$PATH"

cd "$(dirname "$0")/.."

detect_build_jobs() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    elif command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu
    else
        printf '4\n'
    fi
}

nxnow_build_jobs="${NXNOW_BUILD_JOBS:-$(detect_build_jobs)}"
nxnow_version="${NXNOW_VERSION:-$(bash scripts/read-version.sh)}"

cmake -B build/switch -G Ninja -DNXNOW_VERSION="$nxnow_version"
cmake --build build/switch --target cjson mbedtls srtp2 usrsctp -j"$nxnow_build_jobs"
cmake --build build/switch --target NxNow.nro -j"$nxnow_build_jobs"

printf '\nBuilt: %s\n' "$(pwd)/build/switch/NxNow.nro"
