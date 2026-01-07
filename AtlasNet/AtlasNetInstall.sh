#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Optional install path argument
INSTALL_DIR="${1:-$SCRIPT_DIR/package}"

# Remove previous package folder if it exists
rm -rf "$INSTALL_DIR"


# Configure build with custom install prefix
cmake -B "$SCRIPT_DIR/build" -S "$SCRIPT_DIR" -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

# Build targets
cmake --build "$SCRIPT_DIR/build" --parallel --target AtlasNet AtlasNetServer_Static AtlasNetServer_Shared

# Install to the chosen prefix
cmake --install "$SCRIPT_DIR/build" --component AtlasNetBootstrap
