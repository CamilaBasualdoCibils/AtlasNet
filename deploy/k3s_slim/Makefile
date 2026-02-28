SHELL := /usr/bin/env bash

ENV_FILE ?= .env
ENV_EXPORTS := SERVER_IP SERVER_SSH_USER PI_WORKER_IP WORKER_SSH_USER SSH_KEY K3S_API_ENDPOINT K3SUP_USE_SUDO

-include $(ENV_FILE)
export $(ENV_EXPORTS)

.PHONY: bootstrap platform nodes linux-pi ssh-setup sudo-setup port-cleanup cleanup-cluster atlasnet-images atlasnet-deploy atlasnet-up atlasnet-status

bootstrap:
	./scripts/bootstrap.sh

platform:
	./scripts/platform.sh

nodes:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get nodes -o wide

ssh-setup:
	./scripts/ssh_setup.sh

sudo-setup:
	./scripts/sudo_setup.sh

port-cleanup:
	./scripts/port_cleanup.sh

cleanup-cluster:
	./scripts/cleanup_cluster.sh

atlasnet-images:
	./scripts/atlasnet_build_push.sh

atlasnet-deploy:
	./scripts/atlasnet_deploy.sh

atlasnet-up: atlasnet-images atlasnet-deploy

atlasnet-status:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl -n "$${ATLASNET_K8S_NAMESPACE:-atlasnet}" get deploy,svc,pods -o wide

linux-pi:
	@: "$${SERVER_IP:?Set SERVER_IP in .env or pass SERVER_IP=...}"
	@: "$${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env or pass SERVER_SSH_USER=...}"
	@: "$${PI_WORKER_IP:?Set PI_WORKER_IP in .env or pass PI_WORKER_IP=...}"
	./scripts/port_cleanup.sh
	./scripts/bootstrap.sh \
	  --server-ip "$$SERVER_IP" \
	  --server-user "$$SERVER_SSH_USER" \
	  --worker-ip "$$PI_WORKER_IP" \
	  --worker-user "$${WORKER_SSH_USER:-pi}" \
	  --ssh-key "$${SSH_KEY:-$$HOME/.ssh/id_ed25519}" \
	  --use-sudo "$${K3SUP_USE_SUDO:-true}" \
	  --api-endpoint "$${K3S_API_ENDPOINT:-$$SERVER_IP}"

# run in this order:
# ssh-setup
# sudo-setup
# linux-pi

#########################

#Run these from repo root:

#cd /home/danny/Desktop/AtlasNet/deploy/k3s_slim
#First-time (cluster + images + deploy):
#
#docker login ghcr.io
#make ssh-setup
#make sudo-setup
#make linux-pi
#make atlasnet-images
#make atlasnet-deploy
#make atlasnet-status
#If cluster is already up, just refresh images + rollout:
#
#cd /home/danny/Desktop/AtlasNet/deploy/k3s_slim
#docker login ghcr.io
#make atlasnet-up
#make atlasnet-status
#If your registry is private, set these in deploy/k3s_slim/.env before deploy:
#
#ATLASNET_CREATE_PULL_SECRET=true
#ATLASNET_REGISTRY_PASSWORD=<your token>
#(and username/server values already in there)

