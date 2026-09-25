#!/usr/bin/env bash

set -euo pipefail

build_dir="${ATLASNET_BUILD_DIR:-build}"
compile_commands="${build_dir}/compile_commands.json"

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "clang-tidy is required to run the pre-commit linter." >&2
    exit 1
fi

if [[ ! -f "${compile_commands}" ]]; then
    echo "${compile_commands} was not found." >&2
    echo "Configure AtlasNet first (for example: cmake --preset linux-vcpkg-debug)." >&2
    exit 1
fi

exec clang-tidy --quiet -p="${build_dir}" "$@"
