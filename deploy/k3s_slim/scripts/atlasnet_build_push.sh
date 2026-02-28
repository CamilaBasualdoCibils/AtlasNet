#!/usr/bin/env bash
set -euo pipefail

TEMPLATE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${TEMPLATE_DIR}/../.." && pwd)"
ENV_FILE="${TEMPLATE_DIR}/.env"
BUILD_PUSH_SCRIPT="${REPO_ROOT}/Dev/BuildAndPushAtlasNetImages.sh"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi

need_cmd bash

[[ -x "${BUILD_PUSH_SCRIPT}" ]] || die "Missing executable script: ${BUILD_PUSH_SCRIPT}"

: "${ATLASNET_IMAGE_REPO:?Set ATLASNET_IMAGE_REPO in deploy/k3s_slim/.env}"

ATLASNET_IMAGE_TAG="${ATLASNET_IMAGE_TAG:-}"
ATLASNET_IMAGE_PLATFORMS="${ATLASNET_IMAGE_PLATFORMS:-linux/amd64,linux/arm64}"
ATLASNET_BUILD_DIR="${ATLASNET_BUILD_DIR:-${REPO_ROOT}/build}"
ATLASNET_WITH_LATEST_TAG="${ATLASNET_WITH_LATEST_TAG:-false}"
ATLASNET_SKIP_STAGE_BUILD="${ATLASNET_SKIP_STAGE_BUILD:-false}"

args=(
  --repo "${ATLASNET_IMAGE_REPO}"
  --platforms "${ATLASNET_IMAGE_PLATFORMS}"
  --build-dir "${ATLASNET_BUILD_DIR}"
)

if [[ -n "${ATLASNET_IMAGE_TAG}" ]]; then
  args+=(--tag "${ATLASNET_IMAGE_TAG}")
fi
if [[ "${ATLASNET_WITH_LATEST_TAG}" == "true" ]]; then
  args+=(--with-latest)
fi
if [[ "${ATLASNET_SKIP_STAGE_BUILD}" == "true" ]]; then
  args+=(--skip-stage-build)
fi

echo "Publishing AtlasNet images via ${BUILD_PUSH_SCRIPT}"
echo "  repo: ${ATLASNET_IMAGE_REPO}"
if [[ -n "${ATLASNET_IMAGE_TAG}" ]]; then
  echo "  tag:  ${ATLASNET_IMAGE_TAG}"
else
  echo "  tag:  (auto)"
fi
echo "  platforms: ${ATLASNET_IMAGE_PLATFORMS}"

"${BUILD_PUSH_SCRIPT}" "${args[@]}"
