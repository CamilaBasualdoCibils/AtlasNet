#!/usr/bin/env bash
set -euo pipefail

CLUSTER_NAME="${1:-atlasnet-dev}"
NODE_REF="${2:-agent-0}"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

need_cmd docker

if [[ "$NODE_REF" == k3d-* ]]; then
  NODE_CONTAINER="$NODE_REF"
else
  NODE_CONTAINER="k3d-${CLUSTER_NAME}-${NODE_REF}"
fi

docker inspect "$NODE_CONTAINER" >/dev/null 2>&1 || die "Node container '$NODE_CONTAINER' does not exist."
docker pause "$NODE_CONTAINER" >/dev/null

echo "Paused $NODE_CONTAINER"
echo "Supported k3d transient node-loss simulation is pause/unpause."
echo "Avoid docker network disconnect/connect for reconnect validation; it can leave flannel unrecovered."
