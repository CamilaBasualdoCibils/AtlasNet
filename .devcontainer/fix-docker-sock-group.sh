#!/usr/bin/env bash
set -euo pipefail

SOCKET_PATH="${1:-/var/run/docker.sock}"

if [[ ! -S "${SOCKET_PATH}" ]]; then
    exit 0
fi

SOCKET_GID="$(stat -c '%g' "${SOCKET_PATH}")"
GROUP_NAME="$(getent group "${SOCKET_GID}" | cut -d: -f1 || true)"

if [[ -z "${GROUP_NAME}" ]]; then
    GROUP_NAME="docker-host"
    sudo groupadd --gid "${SOCKET_GID}" "${GROUP_NAME}" 2>/dev/null || true
fi

if id -nG "${USER}" | tr ' ' '\n' | grep -Fxq "${GROUP_NAME}"; then
    exit 0
fi

sudo usermod -aG "${GROUP_NAME}" "${USER}"
echo "Added ${USER} to ${GROUP_NAME} (gid ${SOCKET_GID}). Reopen terminal if docker is still denied."
