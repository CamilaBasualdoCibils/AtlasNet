#!/usr/bin/env bash
# get the directory this script is in
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# remove the package folder relative to the script
rm -rf "$SCRIPT_DIR/package"

# run cmake commands relative to the script directory
cmake -B "$SCRIPT_DIR/build" -S "$SCRIPT_DIR"
cmake --build "$SCRIPT_DIR/build" --parallel --target AtlasNet
cmake --install "$SCRIPT_DIR/build" --component AtlasNetBootstrap
