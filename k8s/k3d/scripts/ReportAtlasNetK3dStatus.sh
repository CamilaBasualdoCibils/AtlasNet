#!/usr/bin/env bash
set -euo pipefail

CLUSTER_NAME="${1:-atlasnet-dev}"
NAMESPACE="${2:-${ATLASNET_K8S_NAMESPACE:-atlasnet-dev}}"
CONTEXT="k3d-${CLUSTER_NAME}"
TAINT_KEY="atlasnet.io/internaldb-unreachable"
STATUS_ANNOTATION="atlasnet.io/internaldb-netcheck-status"
REASON_ANNOTATION="atlasnet.io/internaldb-netcheck-reason"
UPDATED_AT_ANNOTATION="atlasnet.io/internaldb-netcheck-updated-at"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

need_cmd kubectl

kubectl --context "$CONTEXT" get nodes >/dev/null 2>&1 || die "Could not reach kubectl context '$CONTEXT'."

echo "AtlasNet node quarantine status"
echo " - context: $CONTEXT"
echo " - namespace: $NAMESPACE"

while IFS= read -r node; do
  [[ -n "${node:-}" ]] || continue
  ready="$(kubectl --context "$CONTEXT" get node "$node" -o go-template='{{range .status.conditions}}{{if eq .type "Ready"}}{{.status}}{{end}}{{end}}')"
  taint_effect="$(kubectl --context "$CONTEXT" get node "$node" -o go-template='{{range .spec.taints}}{{if eq .key "'"$TAINT_KEY"'"}}{{.effect}}{{end}}{{end}}')"
  status="$(kubectl --context "$CONTEXT" get node "$node" -o go-template='{{index .metadata.annotations "'"$STATUS_ANNOTATION"'"}}')"
  reason="$(kubectl --context "$CONTEXT" get node "$node" -o go-template='{{index .metadata.annotations "'"$REASON_ANNOTATION"'"}}')"
  updated_at="$(kubectl --context "$CONTEXT" get node "$node" -o go-template='{{index .metadata.annotations "'"$UPDATED_AT_ANNOTATION"'"}}')"

  [[ -n "$ready" ]] || ready="Unknown"
  [[ -n "$status" ]] || status="unknown"
  [[ -n "$reason" ]] || reason="unknown"
  [[ -n "$updated_at" ]] || updated_at="unknown"

  quarantined="no"
  if [[ -n "$taint_effect" ]]; then
    quarantined="yes"
  fi

  echo " - $node: Ready=$ready quarantined=$quarantined status=$status reason=$reason updated=$updated_at"
done < <(kubectl --context "$CONTEXT" get nodes -o jsonpath='{range .items[*]}{.metadata.name}{"\n"}{end}')

echo
echo "Netcheck pods"
kubectl --context "$CONTEXT" -n "$NAMESPACE" get pods \
  -l app=atlasnet-node-netcheck \
  --no-headers \
  -o 'custom-columns=NODE:.spec.nodeName,POD:.metadata.name,READY:.status.conditions[?(@.type=="Ready")].status' 2>/dev/null || true
