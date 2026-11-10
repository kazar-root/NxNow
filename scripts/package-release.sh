#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    printf 'Usage: %s <version> [build-directory]\n' "$0" >&2
    exit 2
fi
version="$1"
build_directory="${2:-switch}"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
    printf 'Invalid release version: %s\n' "$version" >&2
    exit 2
fi
repo="$(cd "$(dirname "$0")/.." && pwd)"
nro="$repo/build/$build_directory/NxNow.nro"
if [[ ! -f "$nro" ]]; then
    printf 'Missing build artifact: %s\n' "$nro" >&2
    exit 1
fi
python3 "$repo/scripts/verify-nro-assets.py" "$nro" --resources "$repo/resources"
release_directory="$repo/../NxNow$version"
mkdir -p "$release_directory"
cp "$nro" "$release_directory/NxNow.nro"
printf 'Packaged standalone NRO: %s/NxNow.nro\n' "$release_directory"
