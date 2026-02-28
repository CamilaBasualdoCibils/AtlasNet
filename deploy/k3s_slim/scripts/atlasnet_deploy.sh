#!/usr/bin/env bash
set -euo pipefail

TEMPLATE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${TEMPLATE_DIR}/../.." && pwd)"
ENV_FILE="${TEMPLATE_DIR}/.env"
KUBECONFIG_PATH="${TEMPLATE_DIR}/config/kubeconfig"
DEPLOY_SCRIPT="${REPO_ROOT}/Dev/RunAtlasNetK3sDeploy.sh"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi

need_cmd kubectl
need_cmd bash

[[ -f "${KUBECONFIG_PATH}" ]] || die "Missing kubeconfig: ${KUBECONFIG_PATH} (run 'make linux-pi' first)"
[[ -x "${DEPLOY_SCRIPT}" ]] || die "Missing executable script: ${DEPLOY_SCRIPT}"

: "${ATLASNET_IMAGE_REPO:?Set ATLASNET_IMAGE_REPO in deploy/k3s_slim/.env}"

ATLASNET_IMAGE_TAG="${ATLASNET_IMAGE_TAG:-latest}"
ATLASNET_K8S_NAMESPACE="${ATLASNET_K8S_NAMESPACE:-atlasnet}"
ATLASNET_IMAGE_PULL_SECRET_NAME="${ATLASNET_IMAGE_PULL_SECRET_NAME:-}"
ATLASNET_CREATE_PULL_SECRET="${ATLASNET_CREATE_PULL_SECRET:-false}"

export ATLASNET_KUBECONFIG="${KUBECONFIG_PATH}"
export ATLASNET_K8S_NAMESPACE
export ATLASNET_IMAGE_REPO
export ATLASNET_IMAGE_TAG
export ATLASNET_IMAGE_PULL_SECRET_NAME

if [[ -n "${ATLASNET_PROXY_PUBLIC_IP:-}" ]]; then
  export ATLASNET_PROXY_PUBLIC_IP
fi
if [[ -n "${ATLASNET_PROXY_PUBLIC_PORT:-}" ]]; then
  export ATLASNET_PROXY_PUBLIC_PORT
fi
if [[ -n "${ATLASNET_IMAGE_VALKEY:-}" ]]; then
  export ATLASNET_IMAGE_VALKEY
fi
if [[ -n "${ATLASNET_KUBECTL_CONTEXT:-}" ]]; then
  export ATLASNET_KUBECTL_CONTEXT
fi

if [[ "${ATLASNET_CREATE_PULL_SECRET}" == "true" ]]; then
  [[ -n "${ATLASNET_IMAGE_PULL_SECRET_NAME}" ]] || \
    die "ATLASNET_CREATE_PULL_SECRET=true requires ATLASNET_IMAGE_PULL_SECRET_NAME"
  : "${ATLASNET_REGISTRY_SERVER:?Set ATLASNET_REGISTRY_SERVER in .env}"
  : "${ATLASNET_REGISTRY_USERNAME:?Set ATLASNET_REGISTRY_USERNAME in .env}"
  : "${ATLASNET_REGISTRY_PASSWORD:?Set ATLASNET_REGISTRY_PASSWORD in .env}"
  ATLASNET_REGISTRY_EMAIL="${ATLASNET_REGISTRY_EMAIL:-atlasnet@example.local}"

  echo "Creating/updating image pull secret '${ATLASNET_IMAGE_PULL_SECRET_NAME}' in namespace '${ATLASNET_K8S_NAMESPACE}'..."
  kubectl --kubeconfig "${KUBECONFIG_PATH}" create namespace "${ATLASNET_K8S_NAMESPACE}" --dry-run=client -o yaml | \
    kubectl --kubeconfig "${KUBECONFIG_PATH}" apply -f - >/dev/null

    kubectl --kubeconfig "${KUBECONFIG_PATH}" -n "${ATLASNET_K8S_NAMESPACE}" create secret docker-registry "${ATLASNET_IMAGE_PULL_SECRET_NAME}" \
    --docker-server="${ATLASNET_REGISTRY_SERVER}" \
    --docker-username="${ATLASNET_REGISTRY_USERNAME}" \
    --docker-password="${ATLASNET_REGISTRY_PASSWORD}" \
    --docker-email="${ATLASNET_REGISTRY_EMAIL}" \
    --dry-run=client -o yaml | \
    kubectl --kubeconfig "${KUBECONFIG_PATH}" apply -f - >/dev/null
elif [[ -n "${ATLASNET_IMAGE_PULL_SECRET_NAME}" ]]; then
  if ! kubectl --kubeconfig "${KUBECONFIG_PATH}" -n "${ATLASNET_K8S_NAMESPACE}" get secret "${ATLASNET_IMAGE_PULL_SECRET_NAME}" >/dev/null 2>&1; then
    echo "Warning: pull secret '${ATLASNET_IMAGE_PULL_SECRET_NAME}' was not found in namespace '${ATLASNET_K8S_NAMESPACE}'." >&2
    echo "         Set ATLASNET_CREATE_PULL_SECRET=true or create the secret manually before deploy." >&2
  fi
fi

echo "Deploying AtlasNet to k3s..."
echo "  kubeconfig: ${ATLASNET_KUBECONFIG}"
echo "  namespace:  ${ATLASNET_K8S_NAMESPACE}"
echo "  image repo: ${ATLASNET_IMAGE_REPO}"
echo "  image tag:  ${ATLASNET_IMAGE_TAG}"

"${DEPLOY_SCRIPT}" "${ATLASNET_IMAGE_REPO}" "${ATLASNET_IMAGE_TAG}"
