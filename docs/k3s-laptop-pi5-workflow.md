# AtlasNet K3s Workflow (Linux Laptop + Raspberry Pi 5)

This guide sets up a mixed-architecture cluster:

- Laptop (x86_64): `k3s` server/control-plane
- Pi 5 (arm64): `k3s` agent/worker

AtlasNet images are built as multi-arch and pushed to a registry, then pulled by `k3s`.

## 1. Prerequisites

On the laptop:

- Docker with `buildx`
- CMake build already configured for this repo (`cmake -S . -B build`)
- `kubectl`

On laptop and Pi:

- Static/reliable LAN IPs recommended
- Port `6443/tcp` reachable from Pi to laptop (k3s API)

Registry access:

- Log in before publishing images (example):
  - `docker login ghcr.io`

## 2. Install k3s server on laptop

```bash
curl -sfL https://get.k3s.io | sh -
```

Export kubeconfig for your user:

```bash
mkdir -p ~/.kube
sudo cp /etc/rancher/k3s/k3s.yaml ~/.kube/config
sudo chown "$USER":"$USER" ~/.kube/config
```

If needed, replace `127.0.0.1` in `~/.kube/config` with laptop LAN IP so other machines/tools can use it.

## 3. Join Pi 5 as k3s agent

On laptop, get the join token:

```bash
sudo cat /var/lib/rancher/k3s/server/node-token
```

On Pi 5, join using laptop LAN IP:

```bash
curl -sfL https://get.k3s.io | \
  K3S_URL=https://<LAPTOP_LAN_IP>:6443 \
  K3S_TOKEN=<TOKEN_FROM_LAPTOP> \
  sh -
```

Verify from laptop:

```bash
kubectl get nodes -o wide
```

You should see both an `amd64` and an `arm64` node.

## 4. Build and push AtlasNet multi-arch images

From repo root on laptop:

```bash
./Dev/BuildAndPushAtlasNetImages.sh \
  --repo ghcr.io/<your-org-or-user>/atlasnet \
  --tag v0.1.0
```

Notes:

- Default platforms are `linux/amd64,linux/arm64`.
- Add `--with-latest` if you also want `:latest` tags.

## 5. (Optional) Create image pull secret

If your registry is private:

```bash
kubectl create namespace atlasnet --dry-run=client -o yaml | kubectl apply -f -
kubectl -n atlasnet create secret docker-registry atlasnet-regcred \
  --docker-server=ghcr.io \
  --docker-username=<username> \
  --docker-password=<token-or-password> \
  --docker-email=<email>
```

## 6. Deploy AtlasNet to k3s

Public registry example:

```bash
ATLASNET_IMAGE_REPO=ghcr.io/<your-org-or-user>/atlasnet \
ATLASNET_IMAGE_TAG=v0.1.0 \
./Dev/RunAtlasNetK3sDeploy.sh
```

Private registry example:

```bash
ATLASNET_IMAGE_REPO=ghcr.io/<your-org-or-user>/atlasnet \
ATLASNET_IMAGE_TAG=v0.1.0 \
ATLASNET_IMAGE_PULL_SECRET_NAME=atlasnet-regcred \
./Dev/RunAtlasNetK3sDeploy.sh
```

Optional proxy public endpoint registration for Interlink:

```bash
ATLASNET_PROXY_PUBLIC_IP=<public-ip-or-dns> \
ATLASNET_PROXY_PUBLIC_PORT=2555 \
ATLASNET_IMAGE_REPO=ghcr.io/<your-org-or-user>/atlasnet \
ATLASNET_IMAGE_TAG=v0.1.0 \
./Dev/RunAtlasNetK3sDeploy.sh
```

## 7. Check service exposure

```bash
kubectl -n atlasnet get svc atlasnet-cartograph atlasnet-proxy -o wide
```

- `atlasnet-cartograph`: TCP `3000` (`9229` inspector)
- `atlasnet-proxy`: TCP+UDP `2555`

If `EXTERNAL-IP` is pending:

- ensure k3s ServiceLB is enabled, or
- install MetalLB for bare-metal `LoadBalancer` support.

## 8. Rolling updates

For a new version:

1. Build/push with a new tag.
2. Redeploy with `ATLASNET_IMAGE_TAG=<new-tag>`.

`RunAtlasNetK3sDeploy.sh` reapplies manifests and waits for rollout status.
