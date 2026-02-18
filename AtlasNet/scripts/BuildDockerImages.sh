#!/usr/bin/env bash
set -euo pipefail

# Default platforms
PLATFORMS="${PLATFORMS:-linux/amd64,linux/arm64}"

# Default bake file location
BAKE_FILE="${BAKE_FILE:-$(dirname "$0")/Dockerfiles/docker-bake.json}"

# Builder name
BUILDER_NAME="atlasnet-builder"
usage() {
    echo "Usage: $0 [-p platforms] [-f bake_file]"
    echo "  -p    Comma-separated platforms (default: $PLATFORMS)"
    echo "  -f    Path to docker-bake.json (default: $BAKE_FILE)"
    exit 1
}
while getopts "p:f:h" opt; do
    case "$opt" in
        p) PLATFORMS="$OPTARG" ;;
        f) BAKE_FILE="$OPTARG" ;;
        h) usage ;;
        *) usage ;;
    esac
done
# Determine progress style
if [ -t 1 ]; then
    PROGRESS="tty"
else
    PROGRESS="plain"
fi

echo "==> Using platforms: $PLATFORMS"
echo "==> Using bake file: $BAKE_FILE"

# Ensure buildx exists
if ! docker buildx version >/dev/null 2>&1; then
    echo "Docker buildx is required."
    exit 1
fi

# Create builder if it doesn't exist
if ! docker buildx inspect "$BUILDER_NAME" >/dev/null 2>&1; then
    echo "==> Creating buildx builder: $BUILDER_NAME"
    docker buildx create \
        --name "$BUILDER_NAME" \
        --driver docker-container \
        --use
else
    docker buildx use "$BUILDER_NAME"
fi

# Bootstrap builder (enables QEMU for cross-arch)
docker buildx inspect --bootstrap

echo "==> Building images in parallel with BuildKit..."

docker buildx bake -f ./scripts/Dockerfiles/docker-bake.json --set "*.platform=$PLATFORMS" --load


echo "==> Done."