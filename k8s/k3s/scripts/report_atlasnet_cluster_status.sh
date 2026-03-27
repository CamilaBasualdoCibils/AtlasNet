#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KUBECONFIG_PATH="${KUBECONFIG:-$ROOT_DIR/config/kubeconfig}"
NAMESPACE="${1:-${ATLASNET_K8S_NAMESPACE:-atlasnet}}"
TAINT_KEY="atlasnet.io/internaldb-unreachable"
STATUS_ANNOTATION="atlasnet.io/internaldb-netcheck-status"
REASON_ANNOTATION="atlasnet.io/internaldb-netcheck-reason"
UPDATED_AT_ANNOTATION="atlasnet.io/internaldb-netcheck-updated-at"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

need_cmd kubectl
[[ -f "$KUBECONFIG_PATH" ]] || die "Missing kubeconfig: $KUBECONFIG_PATH"

export KUBECONFIG="$KUBECONFIG_PATH"
kubectl get nodes >/dev/null 2>&1 || die "Could not reach the Kubernetes cluster with KUBECONFIG=$KUBECONFIG_PATH."

echo "AtlasNet node quarantine status"
echo " - kubeconfig: $KUBECONFIG_PATH"
echo " - namespace: $NAMESPACE"

while IFS= read -r node; do
  [[ -n "${node:-}" ]] || continue
  ready="$(kubectl get node "$node" -o go-template='{{range .status.conditions}}{{if eq .type "Ready"}}{{.status}}{{end}}{{end}}')"
  taint_effect="$(kubectl get node "$node" -o go-template='{{range .spec.taints}}{{if eq .key "'"$TAINT_KEY"'"}}{{.effect}}{{end}}{{end}}')"
  status="$(kubectl get node "$node" -o go-template='{{index .metadata.annotations "'"$STATUS_ANNOTATION"'"}}')"
  reason="$(kubectl get node "$node" -o go-template='{{index .metadata.annotations "'"$REASON_ANNOTATION"'"}}')"
  updated_at="$(kubectl get node "$node" -o go-template='{{index .metadata.annotations "'"$UPDATED_AT_ANNOTATION"'"}}')"

  [[ -n "$ready" ]] || ready="Unknown"
  [[ -n "$status" ]] || status="unknown"
  [[ -n "$reason" ]] || reason="unknown"
  [[ -n "$updated_at" ]] || updated_at="unknown"

  quarantined="no"
  if [[ -n "$taint_effect" ]]; then
    quarantined="yes"
  fi

  echo " - $node: Ready=$ready quarantined=$quarantined status=$status reason=$reason updated=$updated_at"
done < <(kubectl get nodes -o jsonpath='{range .items[*]}{.metadata.name}{"\n"}{end}')

echo
echo "Netcheck pods"
kubectl -n "$NAMESPACE" get pods \
  -l app=atlasnet-node-netcheck \
  --no-headers \
  -o 'custom-columns=NODE:.spec.nodeName,POD:.metadata.name,READY:.status.conditions[?(@.type=="Ready")].status' 2>/dev/null || true
