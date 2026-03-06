#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  Dev/ProfileK3dHotspot.sh [cluster-name]

Profiles all k3d server/agent node containers for 30s by default, writes perf.data
files to /tmp, and opens each capture in Hotspot.

Environment overrides:
  ATLASNET_HOTSPOT_DURATION_S       Sample duration in seconds (default: 30)
  ATLASNET_HOTSPOT_FREQ_HZ          Perf sample frequency (default: 99)
  ATLASNET_HOTSPOT_OUT_DIR          Output base dir (default: /tmp/atlasnet-hotspot)
  ATLASNET_HOTSPOT_IMAGE            Profiler image name (default: atlasnet-hotspot-agent:latest)
  ATLASNET_HOTSPOT_CONTAINER        Profiler worker container name (default: atlasnet-hotspot-agent)
  ATLASNET_HOTSPOT_FORCE_REBUILD    Rebuild profiler image even if present (0/1, default: 0)
  ATLASNET_HOTSPOT_OPEN_MODE        container|host|novnc|none (default: container)
  ATLASNET_HOTSPOT_AUTO_OPEN        Auto-open captures after recording (0/1, default: 1)
  ATLASNET_HOTSPOT_KEEP_WORKER      Keep worker container running after capture (0/1, default: 0)
  ATLASNET_HOTSPOT_NOVNC_PORT_BASE  Base host port for noVNC viewers (default: 16080)
  ATLASNET_HOTSPOT_NOVNC_AUTO_OPEN_BROWSER  Open noVNC URLs in browser (0/1, default: 1)

Notes:
  - open mode "container" requires a working X11 setup:
      DISPLAY set, /tmp/.X11-unix mounted, and xhost allowing local docker.
  - open mode "host" requires hotspot installed on the host.
  - open mode "novnc" launches one noVNC URL per perf file.
EOF
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "Error: missing required command '$1'." >&2
        exit 1
    }
}

is_nonnegative_int() {
    [[ "$1" =~ ^[0-9]+$ ]]
}

escape_regex() {
    printf '%s' "$1" | sed -e 's/[][(){}.^$+*?|\\]/\\&/g'
}

CLUSTER_NAME="${1:-${ATLASNET_HOTSPOT_CLUSTER_NAME:-atlasnet-dev}}"
DURATION_S="${ATLASNET_HOTSPOT_DURATION_S:-30}"
FREQ_HZ="${ATLASNET_HOTSPOT_FREQ_HZ:-99}"
OUT_BASE_DIR="${ATLASNET_HOTSPOT_OUT_DIR:-/tmp/atlasnet-hotspot}"
PROFILER_IMAGE="${ATLASNET_HOTSPOT_IMAGE:-atlasnet-hotspot-agent:latest}"
WORKER_CONTAINER="${ATLASNET_HOTSPOT_CONTAINER:-atlasnet-hotspot-agent}"
FORCE_REBUILD="${ATLASNET_HOTSPOT_FORCE_REBUILD:-0}"
OPEN_MODE="${ATLASNET_HOTSPOT_OPEN_MODE:-container}"
AUTO_OPEN="${ATLASNET_HOTSPOT_AUTO_OPEN:-1}"
KEEP_WORKER="${ATLASNET_HOTSPOT_KEEP_WORKER:-0}"
NOVNC_PORT_BASE="${ATLASNET_HOTSPOT_NOVNC_PORT_BASE:-16080}"
NOVNC_AUTO_OPEN_BROWSER="${ATLASNET_HOTSPOT_NOVNC_AUTO_OPEN_BROWSER:-1}"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if ! is_nonnegative_int "$DURATION_S"; then
    echo "Error: ATLASNET_HOTSPOT_DURATION_S must be a nonnegative integer." >&2
    exit 1
fi
if ! is_nonnegative_int "$FREQ_HZ" || ((FREQ_HZ < 1)); then
    echo "Error: ATLASNET_HOTSPOT_FREQ_HZ must be an integer >= 1." >&2
    exit 1
fi
if [[ "$OPEN_MODE" != "container" && "$OPEN_MODE" != "host" && "$OPEN_MODE" != "novnc" && "$OPEN_MODE" != "none" ]]; then
    echo "Error: ATLASNET_HOTSPOT_OPEN_MODE must be one of: container, host, novnc, none." >&2
    exit 1
fi
if ! is_nonnegative_int "$NOVNC_PORT_BASE" || ((NOVNC_PORT_BASE < 1024 || NOVNC_PORT_BASE > 65000)); then
    echo "Error: ATLASNET_HOTSPOT_NOVNC_PORT_BASE must be an integer in [1024, 65000]." >&2
    exit 1
fi

need_cmd docker
need_cmd grep
need_cmd sed
need_cmd awk

if ! docker info >/dev/null 2>&1; then
    echo "Error: docker daemon is not reachable." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DOCKERFILE_PATH="${SCRIPT_DIR}/HotspotProfiler.dockerfile"
[[ -f "$DOCKERFILE_PATH" ]] || {
    echo "Error: missing Dockerfile at $DOCKERFILE_PATH" >&2
    exit 1
}

if [[ "$FORCE_REBUILD" == "1" ]]; then
    echo "==> Removing existing profiler image '${PROFILER_IMAGE}' (forced rebuild)..."
    docker rmi -f "$PROFILER_IMAGE" >/dev/null 2>&1 || true
fi

if ! docker image inspect "$PROFILER_IMAGE" >/dev/null 2>&1; then
    echo "==> Building profiler image '${PROFILER_IMAGE}' (installs Hotspot + perf tools)..."
    docker build -f "$DOCKERFILE_PATH" -t "$PROFILER_IMAGE" "$REPO_ROOT"
else
    echo "==> Reusing existing profiler image '${PROFILER_IMAGE}'."
fi

cluster_regex="$(escape_regex "$CLUSTER_NAME")"
mapfile -t NODE_CONTAINERS < <(
    docker ps --format '{{.Names}}' \
      | grep -E "^k3d-${cluster_regex}-(server|agent)-[0-9]+$" \
      | sort
)

if ((${#NODE_CONTAINERS[@]} == 0)); then
    echo "Error: no k3d server/agent node containers found for cluster '$CLUSTER_NAME'." >&2
    echo "Hint: verify cluster name with: k3d cluster list" >&2
    exit 1
fi

timestamp="$(date +%Y%m%d-%H%M%S)"
RUN_DIR="${OUT_BASE_DIR}/${CLUSTER_NAME}-${timestamp}"
if ! mkdir -p "$RUN_DIR" 2>/dev/null; then
    fallback_base="/tmp/atlasnet-hotspot-${USER:-$(id -u)}"
    echo "Warning: cannot write to '${OUT_BASE_DIR}'. Falling back to '${fallback_base}'."
    OUT_BASE_DIR="$fallback_base"
    RUN_DIR="${OUT_BASE_DIR}/${CLUSTER_NAME}-${timestamp}"
    mkdir -p "$RUN_DIR"
fi

echo "==> Profiling cluster '$CLUSTER_NAME' for ${DURATION_S}s at ${FREQ_HZ}Hz."
echo "==> Nodes:"
for node in "${NODE_CONTAINERS[@]}"; do
    echo "   - $node"
done
echo "==> Output directory: $RUN_DIR"

cleanup_worker() {
    if [[ "$KEEP_WORKER" == "1" ]]; then
        return
    fi
    docker rm -f "$WORKER_CONTAINER" >/dev/null 2>&1 || true
}
trap cleanup_worker EXIT

docker rm -f "$WORKER_CONTAINER" >/dev/null 2>&1 || true

declare -a RUN_ARGS=(
    docker run -d --name "$WORKER_CONTAINER"
    --privileged
    --pid=host
    --net=host
    -v /var/run/docker.sock:/var/run/docker.sock
    -v /sys/fs/cgroup:/sys/fs/cgroup:ro
    -v "$RUN_DIR":/profiles
)

if [[ -n "${DISPLAY:-}" && -d /tmp/.X11-unix ]]; then
    RUN_ARGS+=(-e "DISPLAY=${DISPLAY}" -v /tmp/.X11-unix:/tmp/.X11-unix)
fi

RUN_ARGS+=("$PROFILER_IMAGE" sleep infinity)
"${RUN_ARGS[@]}" >/dev/null

PERF_EXEC_MODE="container"
PERF_BIN="perf"

HOST_PERF_BIN="$(
    docker exec "$WORKER_CONTAINER" bash -lc "nsenter -t 1 -m -- sh -lc 'command -v perf 2>/dev/null || true'" \
        | tr -d '\r' | head -n1
)"

if [[ -n "$HOST_PERF_BIN" ]] && \
   docker exec "$WORKER_CONTAINER" bash -lc "nsenter -t 1 -m -- \"$HOST_PERF_BIN\" --version >/dev/null 2>&1"; then
    PERF_EXEC_MODE="host"
    PERF_BIN="$HOST_PERF_BIN"
else
    if ! docker exec "$WORKER_CONTAINER" bash -lc "perf --version >/dev/null 2>&1"; then
        echo "==> container 'perf' unavailable; rebuilding profiler image..."
        docker rm -f "$WORKER_CONTAINER" >/dev/null 2>&1 || true
        docker build --no-cache -f "$DOCKERFILE_PATH" -t "$PROFILER_IMAGE" "$REPO_ROOT"
        "${RUN_ARGS[@]}" >/dev/null
    fi

    if ! docker exec "$WORKER_CONTAINER" bash -lc "perf --version >/dev/null 2>&1"; then
        echo "Error: no usable perf binary found (host perf unavailable and container perf unusable)." >&2
        echo "Hint: install perf on host, then rerun with ATLASNET_HOTSPOT_FORCE_REBUILD=1." >&2
        exit 1
    fi
fi

echo "==> Perf mode: ${PERF_EXEC_MODE} (${PERF_BIN})"

echo "==> Recording perf data on all k3d server/agent nodes in parallel..."
declare -a REC_PIDS=()
for node in "${NODE_CONTAINERS[@]}"; do
    safe_node="$(printf '%s' "$node" | tr -c 'A-Za-z0-9._-' '_')"
    if [[ "$PERF_EXEC_MODE" == "host" ]]; then
        output_file="${RUN_DIR}/${safe_node}.perf.data"
    else
        output_file="/profiles/${safe_node}.perf.data"
    fi

    docker exec \
        -e PERF_EXEC_MODE="$PERF_EXEC_MODE" \
        -e PERF_BIN="$PERF_BIN" \
        -e PERF_OUT="$output_file" \
        "$WORKER_CONTAINER" bash -lc "
        set -euo pipefail
        node_pid=\$(docker inspect -f '{{.State.Pid}}' '$node')
        pids=\$(docker top '$node' -eo pid 2>/dev/null \
          | awk 'NR>1 && \$1 ~ /^[0-9]+$/ {print \$1}' \
          | sort -nu \
          | paste -sd, -)

        if [[ -z \"\$pids\" ]]; then
          pids=\$(ps -eo pid=,ppid= | awk -v root=\"\$node_pid\" '
            {
              pid=\$1
              ppid=\$2
              if (pid ~ /^[0-9]+$/ && ppid ~ /^[0-9]+$/) {
                children[ppid]=children[ppid] \" \" pid
              }
            }
            END {
              queue[1]=root
              head=1
              tail=1
              seen[root]=1
              while (head <= tail) {
                cur=queue[head++]
                split(children[cur], arr, \" \")
                for (i in arr) {
                  child=arr[i]
                  if (child == \"\" || seen[child]) {
                    continue
                  }
                  seen[child]=1
                  queue[++tail]=child
                }
              }
              out=\"\"
              for (pid in seen) {
                if (pid ~ /^[0-9]+$/) {
                  out = out (out ? \",\" : \"\") pid
                }
              }
              print out
            }
          ')
        fi

        [[ -n \"\$pids\" ]] || { echo 'No PIDs found for $node (docker top + descendants fallback)' >&2; exit 1; }

        if [[ \"\$PERF_EXEC_MODE\" == \"host\" ]]; then
          nsenter -t 1 -m -- \"\$PERF_BIN\" record -F '$FREQ_HZ' -g --call-graph fp -p \"\$pids\" \
            -o \"\$PERF_OUT\" -- sleep '$DURATION_S'
        else
          \"\$PERF_BIN\" record -F '$FREQ_HZ' -g --call-graph fp -p \"\$pids\" \
            -o \"\$PERF_OUT\" -- sleep '$DURATION_S'
        fi
    " &
    REC_PIDS+=("$!")
done

record_failed=0
for pid in "${REC_PIDS[@]}"; do
    if ! wait "$pid"; then
        record_failed=1
    fi
done

mapfile -t PERF_FILES < <(find "$RUN_DIR" -maxdepth 1 -type f -name '*.perf.data' | sort)

if ((${#PERF_FILES[@]} == 0)); then
    mapfile -t PERF_FILES < <(
        docker exec "$WORKER_CONTAINER" bash -lc "find /profiles -maxdepth 1 -type f -name '*.perf.data' | sort" \
            | sed "s#^/profiles/#${RUN_DIR}/#"
    )
fi

if ((record_failed == 1)); then
    echo "Error: one or more perf recording jobs failed." >&2
    exit 1
fi

if ((${#PERF_FILES[@]} == 0)); then
    echo "Error: no perf.data files were produced." >&2
    exit 1
fi

echo "==> Captured perf files:"
for file in "${PERF_FILES[@]}"; do
    echo "   - $file"
done

open_with_host_hotspot() {
    if ! command -v hotspot >/dev/null 2>&1; then
        echo "Warning: host 'hotspot' command not found; cannot auto-open in host mode."
        return 1
    fi
    for file in "${PERF_FILES[@]}"; do
        hotspot "$file" >/dev/null 2>&1 &
    done
    return 0
}

open_with_container_hotspot() {
    if [[ -z "${DISPLAY:-}" || ! -d /tmp/.X11-unix ]]; then
        echo "Warning: DISPLAY or /tmp/.X11-unix missing; cannot auto-open in container mode."
        return 1
    fi

    for file in "${PERF_FILES[@]}"; do
        base="$(basename "$file")"
        viewer_name="atlasnet-hotspot-viewer-$(printf '%s' "$base" | tr -c 'A-Za-z0-9' '-' | tr 'A-Z' 'a-z')"
        viewer_name="${viewer_name:0:55}"
        docker rm -f "$viewer_name" >/dev/null 2>&1 || true
        docker run -d --rm \
            --name "$viewer_name" \
            --net=host \
            -e "DISPLAY=${DISPLAY}" \
            -v /tmp/.X11-unix:/tmp/.X11-unix \
            -v "$RUN_DIR":/profiles \
            "$PROFILER_IMAGE" \
            hotspot "/profiles/$base" >/dev/null
    done
    return 0
}

open_with_novnc_hotspot() {
    local idx=0
    local base web_port viewer_name url

    for file in "${PERF_FILES[@]}"; do
        base="$(basename "$file")"
        web_port="$((NOVNC_PORT_BASE + idx))"
        viewer_name="atlasnet-hotspot-novnc-$(printf '%s' "$base" | tr -c 'A-Za-z0-9' '-' | tr 'A-Z' 'a-z')"
        viewer_name="${viewer_name:0:55}"
        docker rm -f "$viewer_name" >/dev/null 2>&1 || true

        docker run -d --rm \
            --name "$viewer_name" \
            -p "${web_port}:6080" \
            -v "$RUN_DIR":/profiles \
            "$PROFILER_IMAGE" \
            bash -lc "
                set -euo pipefail
                export DISPLAY=:99
                Xvfb :99 -screen 0 1920x1080x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
                openbox >/tmp/openbox.log 2>&1 &
                x11vnc -display :99 -forever -shared -nopw -rfbport 5900 >/tmp/x11vnc.log 2>&1 &
                websockify --web=/usr/share/novnc/ 6080 localhost:5900 >/tmp/websockify.log 2>&1 &
                sleep 1
                hotspot \"/profiles/${base}\"
            " >/dev/null

        url="http://127.0.0.1:${web_port}/vnc.html?autoconnect=true&resize=scale&quality=8"
        echo "   - $base => $url"

        if [[ "$NOVNC_AUTO_OPEN_BROWSER" == "1" ]]; then
            if command -v xdg-open >/dev/null 2>&1; then
                xdg-open "$url" >/dev/null 2>&1 || true
            elif command -v open >/dev/null 2>&1; then
                open "$url" >/dev/null 2>&1 || true
            fi
        fi

        idx=$((idx + 1))
    done
    return 0
}

if [[ "$AUTO_OPEN" == "1" && "$OPEN_MODE" != "none" ]]; then
    echo "==> Opening captures with Hotspot (mode: $OPEN_MODE)..."
    case "$OPEN_MODE" in
        host)
            open_with_host_hotspot || true
            ;;
        container)
            open_with_container_hotspot || true
            ;;
        novnc)
            echo "==> noVNC viewer URLs:"
            open_with_novnc_hotspot || true
            ;;
    esac
fi

echo "==> Done."
echo "Perf captures: $RUN_DIR"
