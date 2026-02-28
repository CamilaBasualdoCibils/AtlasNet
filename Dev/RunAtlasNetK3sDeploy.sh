#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

MANIFEST_TEMPLATE="${ROOT_DIR}/deploy/k8s/overlays/k3s/atlasnet-k3s.yaml"
NAMESPACE="${ATLASNET_K8S_NAMESPACE:-atlasnet}"
KUBE_CONTEXT="${ATLASNET_KUBECTL_CONTEXT:-}"
KUBECONFIG_FILE="${ATLASNET_KUBECONFIG:-}"
IMAGE_REPO="${ATLASNET_IMAGE_REPO:-${1:-}}"
IMAGE_TAG="${ATLASNET_IMAGE_TAG:-${2:-latest}}"
IMAGE_PULL_SECRET_NAME="${ATLASNET_IMAGE_PULL_SECRET_NAME:-}"
VALKEY_IMAGE="${ATLASNET_IMAGE_VALKEY:-valkey/valkey-bundle:latest}"
PROXY_PUBLIC_IP="${ATLASNET_PROXY_PUBLIC_IP:-}"
PROXY_PUBLIC_PORT="${ATLASNET_PROXY_PUBLIC_PORT:-2555}"

usage() {
    cat <<'EOF'
Usage:
  Dev/RunAtlasNetK3sDeploy.sh [image_repo] [image_tag]

Environment overrides:
  ATLASNET_K8S_NAMESPACE           Kubernetes namespace (default: atlasnet)
  ATLASNET_KUBECTL_CONTEXT         kubectl context to target (default: current)
  ATLASNET_KUBECONFIG              kubeconfig file path (optional)
  ATLASNET_IMAGE_REPO              Registry repo (example: ghcr.io/acme/atlasnet)
  ATLASNET_IMAGE_TAG               Image tag (default: latest)
  ATLASNET_IMAGE_WATCHDOG          Full watchdog image ref
  ATLASNET_IMAGE_PROXY             Full proxy image ref
  ATLASNET_IMAGE_SHARD             Full shard image ref
  ATLASNET_IMAGE_CARTOGRAPH        Full cartograph image ref
  ATLASNET_IMAGE_VALKEY            Full valkey image ref (default: valkey/valkey-bundle:latest)
  ATLASNET_IMAGE_PULL_SECRET_NAME  Existing docker-registry secret to attach to service accounts
  ATLASNET_PROXY_PUBLIC_IP         Optional public IP/DNS used by proxy for interlink registration
  ATLASNET_PROXY_PUBLIC_PORT       Optional public port (default: 2555)
EOF
}

require_cmd() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: '${cmd}' is required but not installed." >&2
        exit 1
    fi
}

resolve_image() {
    local component="$1"
    local override="$2"
    if [[ -n "${override}" ]]; then
        printf '%s' "${override}"
        return
    fi
    if [[ -z "${IMAGE_REPO}" ]]; then
        echo "Error: image repo missing. Set ATLASNET_IMAGE_REPO or pass positional arg [image_repo]." >&2
        exit 1
    fi
    printf '%s/%s:%s' "${IMAGE_REPO}" "${component}" "${IMAGE_TAG}"
}

escape_sed_replacement() {
    printf '%s' "$1" | sed -e 's/[\/&]/\\&/g'
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ ! -f "${MANIFEST_TEMPLATE}" ]]; then
    echo "Error: manifest template not found at '${MANIFEST_TEMPLATE}'." >&2
    exit 1
fi

require_cmd kubectl

WATCHDOG_IMAGE="$(resolve_image watchdog "${ATLASNET_IMAGE_WATCHDOG:-}")"
PROXY_IMAGE="$(resolve_image proxy "${ATLASNET_IMAGE_PROXY:-}")"
SHARD_IMAGE="$(resolve_image shard "${ATLASNET_IMAGE_SHARD:-}")"
CARTOGRAPH_IMAGE="$(resolve_image cartograph "${ATLASNET_IMAGE_CARTOGRAPH:-}")"
CLUSTER_ROLE_NAME="${NAMESPACE}-atlasnet-cartograph-cluster-reader"
CLUSTER_ROLE_BINDING_NAME="${NAMESPACE}-atlasnet-cartograph-cluster-reader"

KUBECTL=(kubectl)
if [[ -n "${KUBECONFIG_FILE}" ]]; then
    KUBECTL+=(--kubeconfig "${KUBECONFIG_FILE}")
fi
if [[ -n "${KUBE_CONTEXT}" ]]; then
    KUBECTL+=(--context "${KUBE_CONTEXT}")
fi

kctl() {
    "${KUBECTL[@]}" "$@"
}

TEMP_MANIFEST="$(mktemp /tmp/atlasnet-k3s-XXXX.yaml)"
trap 'rm -f "${TEMP_MANIFEST}"' EXIT

sed \
    -e "s|__NAMESPACE__|$(escape_sed_replacement "${NAMESPACE}")|g" \
    -e "s|__CLUSTER_ROLE_NAME__|$(escape_sed_replacement "${CLUSTER_ROLE_NAME}")|g" \
    -e "s|__CLUSTER_ROLE_BINDING_NAME__|$(escape_sed_replacement "${CLUSTER_ROLE_BINDING_NAME}")|g" \
    -e "s|__IMAGE_WATCHDOG__|$(escape_sed_replacement "${WATCHDOG_IMAGE}")|g" \
    -e "s|__IMAGE_PROXY__|$(escape_sed_replacement "${PROXY_IMAGE}")|g" \
    -e "s|__IMAGE_SHARD__|$(escape_sed_replacement "${SHARD_IMAGE}")|g" \
    -e "s|__IMAGE_CARTOGRAPH__|$(escape_sed_replacement "${CARTOGRAPH_IMAGE}")|g" \
    -e "s|__IMAGE_VALKEY__|$(escape_sed_replacement "${VALKEY_IMAGE}")|g" \
    -e "s|__PROXY_PUBLIC_IP__|$(escape_sed_replacement "${PROXY_PUBLIC_IP}")|g" \
    -e "s|__PROXY_PUBLIC_PORT__|$(escape_sed_replacement "${PROXY_PUBLIC_PORT}")|g" \
    "${MANIFEST_TEMPLATE}" >"${TEMP_MANIFEST}"

echo "==> Verifying Kubernetes API reachability..."
kctl --request-timeout=5s get --raw=/readyz >/dev/null

echo "==> Applying AtlasNet manifest to namespace '${NAMESPACE}'..."
kctl apply -f "${TEMP_MANIFEST}" >/dev/null

if [[ -n "${IMAGE_PULL_SECRET_NAME}" ]]; then
    echo "==> Attaching image pull secret '${IMAGE_PULL_SECRET_NAME}' to service accounts..."
    for sa in default atlasnet-watchdog atlasnet-cartograph; do
        kctl -n "${NAMESPACE}" patch serviceaccount "${sa}" \
            --type merge \
            -p "{\"imagePullSecrets\":[{\"name\":\"${IMAGE_PULL_SECRET_NAME}\"}]}" >/dev/null
    done
fi

echo "==> Waiting for nodes to be Ready..."
kctl wait --for=condition=Ready nodes --all --timeout=180s >/dev/null

echo "==> Waiting for AtlasNet deployments..."
kctl -n "${NAMESPACE}" rollout status deployment/atlasnet-internaldb --timeout=240s
kctl -n "${NAMESPACE}" rollout status deployment/atlasnet-watchdog --timeout=240s
kctl -n "${NAMESPACE}" rollout status deployment/atlasnet-proxy --timeout=240s
kctl -n "${NAMESPACE}" rollout status deployment/atlasnet-cartograph --timeout=240s

echo "==> Waiting for watchdog-driven shard scale-up..."
shard_ready=false
for _attempt in {1..120}; do
    desired="$(kctl -n "${NAMESPACE}" get deployment atlasnet-shard -o jsonpath='{.spec.replicas}' 2>/dev/null || echo 0)"
    ready="$(kctl -n "${NAMESPACE}" get deployment atlasnet-shard -o jsonpath='{.status.readyReplicas}' 2>/dev/null || echo 0)"
    desired="${desired:-0}"
    ready="${ready:-0}"
    if [[ "${desired}" =~ ^[0-9]+$ && "${ready}" =~ ^[0-9]+$ && "${desired}" -gt 0 && "${ready}" -ge 1 ]]; then
        echo "   - shard deployment desired=${desired}, ready=${ready}"
        shard_ready=true
        break
    fi
    sleep 2
done

if [[ "${shard_ready}" != true ]]; then
    echo "Warning: shard deployment did not report a ready replica in the expected window." >&2
fi

echo
echo "AtlasNet services (${NAMESPACE}):"
kctl -n "${NAMESPACE}" get svc atlasnet-cartograph atlasnet-proxy internaldb -o wide
echo
echo "Deployment complete."
echo "If EXTERNAL-IP is pending on LoadBalancer services, install/configure a bare-metal LB (MetalLB) or enable k3s ServiceLB."
