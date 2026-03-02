#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT_DIR/../.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"
LEGACY_CONFIG_FILE="$ROOT_DIR/config/cluster.env"
ATLASNET_DOCKERFILE="$REPO_ROOT/AtlasNet/docker/dockerfiles/BuildDockerfile"
ATLASNET_CONTEXT="$REPO_ROOT/AtlasNet"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

[[ -f "$ENV_FILE" ]] || die "Missing .env at $ENV_FILE"
# shellcheck disable=SC1090
source "$ENV_FILE"
if [[ -f "$LEGACY_CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$LEGACY_CONFIG_FILE"
fi

need_cmd docker
docker info >/dev/null 2>&1 || die "Docker daemon is not reachable."
docker buildx version >/dev/null 2>&1 || die "Docker buildx is required."

: "${DOCKERHUB_NAMESPACE:=}"
: "${ATLASNET_IMAGE_TAG:=latest}"
: "${ATLASNET_MULTIARCH_PLATFORMS:=linux/amd64,linux/arm64}"
: "${ATLASNET_MULTIARCH_BUILDER_NAME:=atlasnet-multiarch}"
: "${DOCKERHUB_USERNAME:=}"
: "${DOCKERHUB_TOKEN:=}"

[[ -n "$DOCKERHUB_NAMESPACE" ]] || die "DOCKERHUB_NAMESPACE is required in .env"
[[ -f "$ATLASNET_DOCKERFILE" ]] || die "Missing Dockerfile: $ATLASNET_DOCKERFILE"

: "${ATLASNET_WATCHDOG_IMAGE:=${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_PROXY_IMAGE:=${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_SHARD_IMAGE:=${DOCKERHUB_NAMESPACE}/shard:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_CARTOGRAPH_IMAGE:=${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG}}"

ensure_buildx_builder() {
  if ! docker buildx inspect "$ATLASNET_MULTIARCH_BUILDER_NAME" >/dev/null 2>&1; then
    echo "Creating buildx builder '$ATLASNET_MULTIARCH_BUILDER_NAME' ..."
    docker buildx create --name "$ATLASNET_MULTIARCH_BUILDER_NAME" --driver docker-container --use >/dev/null
  else
    docker buildx use "$ATLASNET_MULTIARCH_BUILDER_NAME" >/dev/null
  fi

  docker buildx inspect "$ATLASNET_MULTIARCH_BUILDER_NAME" --bootstrap >/dev/null
}

build_push_target() {
  local target="$1"
  local image_ref="$2"

  echo "Building + pushing $image_ref (target=$target, platforms=$ATLASNET_MULTIARCH_PLATFORMS)"
  docker buildx build \
    --platform "$ATLASNET_MULTIARCH_PLATFORMS" \
    -f "$ATLASNET_DOCKERFILE" \
    --target "$target" \
    -t "$image_ref" \
    --push \
    "$ATLASNET_CONTEXT"
}

if [[ -n "$DOCKERHUB_USERNAME" && -n "$DOCKERHUB_TOKEN" ]]; then
  echo "Logging into Docker Hub as $DOCKERHUB_USERNAME ..."
  printf '%s' "$DOCKERHUB_TOKEN" | docker login --username "$DOCKERHUB_USERNAME" --password-stdin
fi

ensure_buildx_builder

echo "Publishing AtlasNet multi-arch images to Docker Hub namespace '$DOCKERHUB_NAMESPACE' ..."
build_push_target "watchdog" "$ATLASNET_WATCHDOG_IMAGE"
build_push_target "proxy" "$ATLASNET_PROXY_IMAGE"
build_push_target "shard" "$ATLASNET_SHARD_IMAGE"
build_push_target "cartograph" "$ATLASNET_CARTOGRAPH_IMAGE"

echo
echo "Done. Published images:"
echo " - $ATLASNET_WATCHDOG_IMAGE"
echo " - $ATLASNET_PROXY_IMAGE"
echo " - $ATLASNET_SHARD_IMAGE"
echo " - $ATLASNET_CARTOGRAPH_IMAGE"
echo
echo "Next:"
echo " - deploy/k3s_slim: make atlasnet-deploy"
