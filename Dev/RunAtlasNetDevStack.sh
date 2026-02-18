#!/bin/bash
set -e  # Exit on error

# Stack name (first argument)
STACK_NAME=${1:-atlasnet_dev}

# SHARD image name (second argument)
SHARD_IMAGE_NAME=${2:-"shard:latest"}

# Path to the stack file
STACK_FILE="./docker-stack-dev.yml"

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
    echo "Removing existing stack '$STACK_NAME'..."
    docker stack rm "$STACK_NAME"
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
