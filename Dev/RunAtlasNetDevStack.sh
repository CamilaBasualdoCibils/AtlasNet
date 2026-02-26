#!/usr/bin/env bash
set -euo pipefail

# Stack name (first argument)
STACK_NAME=${1:-atlasnet_dev}

# SHARD image name (second argument)
SHARD_IMAGE_NAME=${2:-"shard:latest"}

# Path to the stack file
STACK_FILE="./docker-stack-dev.yml"

if ! command -v docker >/dev/null 2>&1; then
    echo "Error: docker CLI is not available."
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "Error: docker daemon is not reachable (check /var/run/docker.sock mount/permissions)."
    exit 1
fi

SWARM_STATE="$(docker info --format '{{.Swarm.LocalNodeState}}' 2>/dev/null || echo inactive)"
if [ "$SWARM_STATE" != "active" ]; then
    echo "Docker Swarm is not active. Initializing single-node swarm..."
    if [ -n "${SWARM_ADVERTISE_ADDR:-}" ]; then
        docker swarm init --advertise-addr "${SWARM_ADVERTISE_ADDR}"
    else
        docker swarm init
    fi
fi

# Check if stack file exists
if [ ! -f "$STACK_FILE" ]; then
    echo "Error: $STACK_FILE not found."
    exit 1
fi

# Prepare a temporary stack file with SHARD_IMAGE_NAME replaced
TEMP_STACK_FILE=$(mktemp /tmp/docker-stack-XXXX.yml)

# Replace the macro SHARD_IMAGE_NAME in the YAML file
sed "s|SHARD_IMAGE_NAME|$SHARD_IMAGE_NAME|g" "$STACK_FILE" > "$TEMP_STACK_FILE"

# Check if the stack is already running
if docker stack ls --format '{{.Name}}' | grep -w "$STACK_NAME" > /dev/null; then
    echo "Forcefully removing existing stack '$STACK_NAME'..."

    # Kill all services in the stack
    for svc in $(docker stack services --format '{{.Name}}' "$STACK_NAME"); do
        echo "Removing service $svc..."
        docker service rm "$svc" 2>/dev/null
    done

    # Kill all containers in the stack (in case services linger)
    for ctr in $(docker ps -q --filter "label=com.docker.stack.namespace=$STACK_NAME"); do
        echo "Killing container $ctr..."
        docker kill "$ctr" 2>/dev/null
        docker rm -f "$ctr" 2>/dev/null
    done

    # Finally, remove the stack
    docker stack rm "$STACK_NAME" 2>/dev/null

    # Wait until the stack is gone
    echo "Waiting for stack to be fully removed..."
    while docker stack ls --format '{{.Name}}' | grep -w "$STACK_NAME" > /dev/null; do
        sleep 1
    done
fi
# Ensure overlay network exists
if ! docker network ls --format '{{.Name}}' | grep -w AtlasNet > /dev/null; then
    echo "Creating overlay network 'AtlasNet'..."
    docker network create --driver overlay AtlasNet
fi
# Deploy the stack using the temporary file
echo "Deploying Docker stack '$STACK_NAME' with shard image '$SHARD_IMAGE_NAME'..."
docker stack deploy -c "$TEMP_STACK_FILE" "$STACK_NAME" --detach=true

# Clean up
rm "$TEMP_STACK_FILE"

echo "Stack '$STACK_NAME' deployed successfully."
