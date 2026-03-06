#!/usr/bin/env bash
set -euo pipefail

SOURCE_ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
HOTSPOT_BUILD_DIR="${2:-${SOURCE_ROOT}/build-hotspot}"

BUILD_TYPE="${ATLASNET_HOTSPOT_BUILD_TYPE:-RelWithDebInfo}"
RUN_TARGET="${ATLASNET_HOTSPOT_RUN_TARGET:-sandbox_atlasnet_run}"
GENERATOR="${ATLASNET_HOTSPOT_CMAKE_GENERATOR:-Ninja}"
MAKE_PROGRAM="${ATLASNET_HOTSPOT_CMAKE_MAKE_PROGRAM:-}"
TOOLCHAIN_FILE="${ATLASNET_HOTSPOT_CMAKE_TOOLCHAIN_FILE:-}"
VCPKG_TARGET_TRIPLET="${ATLASNET_HOTSPOT_VCPKG_TARGET_TRIPLET:-}"
VCPKG_HOST_TRIPLET="${ATLASNET_HOTSPOT_VCPKG_HOST_TRIPLET:-}"
VCPKG_INSTALLED_DIR="${ATLASNET_HOTSPOT_VCPKG_INSTALLED_DIR:-}"

die() {
    echo "Error: $*" >&2
    exit 1
}

[[ -f "${SOURCE_ROOT}/CMakeLists.txt" ]] || die "Missing CMakeLists.txt at source root: ${SOURCE_ROOT}"
command -v cmake >/dev/null 2>&1 || die "cmake is required."

if [[ "$GENERATOR" == "Ninja" ]] && ! command -v ninja >/dev/null 2>&1; then
    echo "Warning: Ninja generator requested but ninja is not installed; using CMake default generator."
    GENERATOR=""
fi

HOTSPOT_STACK_FLAGS="-fno-omit-frame-pointer -fno-optimize-sibling-calls"
C_DEBUG_FLAGS="${ATLASNET_HOTSPOT_C_FLAGS_DEBUG:--O0 -g3 ${HOTSPOT_STACK_FLAGS}}"
CXX_DEBUG_FLAGS="${ATLASNET_HOTSPOT_CXX_FLAGS_DEBUG:--O0 -g3 ${HOTSPOT_STACK_FLAGS}}"
C_RELWITHDEBINFO_FLAGS="${ATLASNET_HOTSPOT_C_FLAGS_RELWITHDEBINFO:--O2 -g ${HOTSPOT_STACK_FLAGS}}"
CXX_RELWITHDEBINFO_FLAGS="${ATLASNET_HOTSPOT_CXX_FLAGS_RELWITHDEBINFO:--O2 -g ${HOTSPOT_STACK_FLAGS}}"

echo "==> Configuring hotspot-friendly build directory: ${HOTSPOT_BUILD_DIR}"
echo "==> Build type: ${BUILD_TYPE}"
echo "==> Run target: ${RUN_TARGET}"

CONFIGURE_CMD=(
    cmake
    -S "${SOURCE_ROOT}"
    -B "${HOTSPOT_BUILD_DIR}"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    "-DCMAKE_C_FLAGS_DEBUG=${C_DEBUG_FLAGS}"
    "-DCMAKE_CXX_FLAGS_DEBUG=${CXX_DEBUG_FLAGS}"
    "-DCMAKE_C_FLAGS_RELWITHDEBINFO=${C_RELWITHDEBINFO_FLAGS}"
    "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=${CXX_RELWITHDEBINFO_FLAGS}"
)

if [[ -n "$GENERATOR" ]]; then
    CONFIGURE_CMD+=(-G "$GENERATOR")
fi

if [[ -n "$MAKE_PROGRAM" ]]; then
    CONFIGURE_CMD+=("-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
fi

if [[ -n "$TOOLCHAIN_FILE" ]]; then
    CONFIGURE_CMD+=("-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
fi

if [[ -n "$VCPKG_TARGET_TRIPLET" ]]; then
    CONFIGURE_CMD+=("-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
fi

if [[ -n "$VCPKG_HOST_TRIPLET" ]]; then
    CONFIGURE_CMD+=("-DVCPKG_HOST_TRIPLET=${VCPKG_HOST_TRIPLET}")
fi

if [[ -n "$VCPKG_INSTALLED_DIR" ]]; then
    CONFIGURE_CMD+=("-DVCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}")
fi

"${CONFIGURE_CMD[@]}"

echo "==> Building and running ${RUN_TARGET} ..."
cmake --build "${HOTSPOT_BUILD_DIR}" --target "${RUN_TARGET}"

echo
echo "Hotspot debug run finished."
echo "Hotspot-friendly build dir: ${HOTSPOT_BUILD_DIR}"
