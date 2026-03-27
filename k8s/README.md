# k8s Directory Guide

This folder contains all Kubernetes-related tooling for AtlasNet.

## Layout
- `charts/atlasnet/`: shared AtlasNet runtime Helm chart (single source of truth for workloads).
- `k3s/`: remote/homelab k3s cluster automation (`make` targets + scripts).
- `k3d/`: local k3d dev-cluster scripts (used by CMake sandbox targets).
- `pi-native/`: Raspberry Pi build/push helper Makefile.

## Typical Workflows
### Local dev cluster (k3d)
From repo root:
```bash
cmake --build build --target sandbox_atlasnet_run_k3d
cmake --build build --target sandbox_atlasnet_run_k3d_fast
```

Supported transient node-loss test:
```bash
bash k8s/k3d/scripts/PauseK3dNode.sh atlasnet-dev agent-0
bash k8s/k3d/scripts/UnpauseK3dNode.sh atlasnet-dev agent-0
bash k8s/k3d/scripts/ReportAtlasNetK3dStatus.sh atlasnet-dev atlasnet-dev
```

`docker network disconnect/connect` is not a supported reconnect-validation path for `k3d`; it can leave the nested node without a recovered flannel overlay (`flannel.1`).

### Remote/homelab cluster (k3s)
From `k8s/k3s`:
```bash
make ssh-setup
make sudo-setup
make k3s-deploy
make atlasnet-push
make atlasnet-deploy
```

Cluster quarantine state can be inspected with:
```bash
bash k8s/k3s/scripts/report_atlasnet_cluster_status.sh atlasnet
```

Real reconnect validation target: physical `k3s` cable pull / replug. That is the source of truth for shard return after a node recovers.

## Notes
- AtlasNet runtime deploys now come from `k8s/charts/atlasnet` for both k3d and k3s paths.
- Platform add-ons (`metrics-server`, `MetalLB`, `ingress-nginx`, `cert-manager`) are installed via Helm in `k3s/scripts/platform.sh`.
- Local Docker-based latency automation is centralized in `Dev/LocalRuntimeLatency.sh` and uses the persistent helper container built from `Dev/LatencyHelper.dockerfile`.
  Example: `./Dev/LocalRuntimeLatency.sh auto apply-auto`
  Example: `ATLASNET_LOCAL_LATENCY_RUNTIME=k3d ./Dev/LocalRuntimeLatency.sh auto clear-all`
  Auto mode selects Swarm shard containers or k3d worker node containers; if both are running, set `ATLASNET_LOCAL_LATENCY_RUNTIME`.
  Deploy scripts also ensure the helper is running via `Dev/EnsureLatencyHelper.sh`.
