#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"
CLUSTER_CONFIG_FILE="$ROOT_DIR/config/cluster.env"

FOLLOW_SCRIPT_PATH="/tmp/atlasnet_node_logs_follow.sh"
PID_FILE_PATH="/tmp/atlasnet_node_logs.pid"
LOG_OUTPUT_PATH="/tmp/atlasnet-node-logs.out"
LAUNCH_LOG_PATH="/tmp/atlasnet-node-logs-launch.log"

usage() {
  cat <<USAGE
Usage: $(basename "$0") <start|stop>
USAGE
}

die() {
  echo "ERROR: $*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."
}

parse_entry() {
  local entry="$1"
  local default_user="$2"

  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    printf '%s|%s\n' "${entry%@*}" "${entry#*@}"
  else
    printf '%s|%s\n' "$default_user" "$entry"
  fi
}

ACTION="${1:-}"
[[ "$ACTION" == "start" || "$ACTION" == "stop" ]] || {
  usage
  exit 1
}

if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi
if [[ -f "$CLUSTER_CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CLUSTER_CONFIG_FILE"
fi

: "${SERVER_IPS:?Set SERVER_IPS in .env}"
: "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
: "${WORKER_IPS:=}"
: "${WORKER_SSH_USER:=pi}"
: "${SSH_KEY:=$HOME/.ssh/id_ed25519}"

SSH_KEY="${SSH_KEY/#\~/$HOME}"
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"

need_cmd ssh

SSH_OPTS=(
  -i "$SSH_KEY"
  -o StrictHostKeyChecking=accept-new
  -o ConnectTimeout=8
)

declare -A TARGETS=()

add_targets() {
  local entries="$1"
  local default_user="$2"

  for entry in $entries; do
    [[ -n "$entry" ]] || continue

    local parsed user host key
    parsed="$(parse_entry "$entry" "$default_user")"
    user="${parsed%%|*}"
    host="${parsed##*|}"
    key="${user}@${host}"

    TARGETS["$key"]="$user|$host"
  done
}

add_targets "$SERVER_IPS" "$SERVER_SSH_USER"
add_targets "$WORKER_IPS" "$WORKER_SSH_USER"

run_on_target() {
  local user="$1"
  local host="$2"

  ssh "${SSH_OPTS[@]}" "${user}@${host}" bash -s -- "$ACTION" <<'REMOTE_SCRIPT'
set -euo pipefail

ACTION="$1"
FOLLOW_SCRIPT_PATH="/tmp/atlasnet_node_logs_follow.sh"
PID_FILE_PATH="/tmp/atlasnet_node_logs.pid"
LOG_OUTPUT_PATH="/tmp/atlasnet-node-logs.out"
LAUNCH_LOG_PATH="/tmp/atlasnet-node-logs-launch.log"

write_follow_script() {
  cat > "$FOLLOW_SCRIPT_PATH" <<'SCRIPT_EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  printf '[atlasnet-node-logs] %s\n' "$*"
}

scan_log_files() {
  mapfile -t log_files < <(sudo -n find /var/log/pods -type f -name '*.log' -print | sort)
}

calc_signature() {
  if [[ "${#log_files[@]}" -eq 0 ]]; then
    printf ''
    return 0
  fi

  printf '%s\n' "${log_files[@]}"
}

stop_tail() {
  if [[ -n "${tail_pid:-}" ]] && kill -0 "$tail_pid" >/dev/null 2>&1; then
    kill "$tail_pid" >/dev/null 2>&1 || true
    wait "$tail_pid" >/dev/null 2>&1 || true
  fi
  tail_pid=""
}

start_tail() {
  if [[ "${#log_files[@]}" -eq 0 ]]; then
    return 0
  fi

  log "tracking ${#log_files[@]} pod log files"
  sudo -n tail -n 0 -F "${log_files[@]}" &
  tail_pid="$!"
}

cleanup() {
  stop_tail
}

declare -a log_files=()
current_signature="__init__"
tail_pid=""

trap cleanup EXIT INT TERM

while true; do
  scan_log_files
  new_signature="$(calc_signature)"

  if [[ "$new_signature" != "$current_signature" ]]; then
    stop_tail
    current_signature="$new_signature"

    if [[ "${#log_files[@]}" -eq 0 ]]; then
      log "no pod log files found; waiting for pods..."
    else
      start_tail
    fi
  fi

  if [[ -n "${tail_pid:-}" ]] && ! kill -0 "$tail_pid" >/dev/null 2>&1; then
    log "tail process exited; restarting"
    start_tail
  fi

  sleep 2
done
SCRIPT_EOF
  chmod +x "$FOLLOW_SCRIPT_PATH"
}

is_running() {
  pgrep -f "$FOLLOW_SCRIPT_PATH" >/dev/null 2>&1
}

have_cmd() {
  command -v "$1" >/dev/null 2>&1
}

detect_node_profile() {
  NODE_PROFILE="headless"
  NODE_PRETTY_NAME=""

  if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    source /etc/os-release
    NODE_PRETTY_NAME="${PRETTY_NAME:-${NAME:-}}"
  fi

  local pretty_name_lc=""
  pretty_name_lc="$(printf '%s' "$NODE_PRETTY_NAME" | tr '[:upper:]' '[:lower:]')"

  if [[ "$pretty_name_lc" == *"ubuntu server"* ]]; then
    NODE_PROFILE="ubuntu-server"
    return 0
  fi

  if [[ "$pretty_name_lc" == *"kubuntu"* ]] || have_cmd konsole; then
    NODE_PROFILE="kubuntu"
    return 0
  fi

  if [[ "$pretty_name_lc" == *"ubuntu"* ]] && have_cmd x-terminal-emulator; then
    NODE_PROFILE="ubuntu-desktop"
    return 0
  fi

  if have_cmd x-terminal-emulator; then
    NODE_PROFILE="ubuntu-desktop"
  fi
}

prepare_desktop_launch_env() {
  DESKTOP_DISPLAY="${DISPLAY:-:0}"
  DESKTOP_WAYLAND_DISPLAY=""
  DESKTOP_XDG_RUNTIME_DIR="/run/user/$(id -u)"
  DESKTOP_DBUS_SESSION_BUS_ADDRESS=""
  DESKTOP_XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}"

  if [[ -S "$DESKTOP_XDG_RUNTIME_DIR/wayland-0" ]]; then
    DESKTOP_WAYLAND_DISPLAY="wayland-0"
  fi

  if [[ -S "$DESKTOP_XDG_RUNTIME_DIR/bus" ]]; then
    DESKTOP_DBUS_SESSION_BUS_ADDRESS="unix:path=$DESKTOP_XDG_RUNTIME_DIR/bus"
  fi
}

desktop_launch_available() {
  prepare_desktop_launch_env

  if [[ -S /tmp/.X11-unix/X0 ]]; then
    return 0
  fi

  if [[ -n "$DESKTOP_WAYLAND_DISPLAY" ]]; then
    return 0
  fi

  return 1
}

ensure_passwordless_sudo() {
  if sudo -n true >/dev/null 2>&1; then
    return 0
  fi

  echo "passwordless sudo is not configured for $(id -un) on $(hostname -s)"
  return 1
}

start_headless() {
  if is_running; then
    echo "already running"
    return 0
  fi

  nohup bash "$FOLLOW_SCRIPT_PATH" >"$LOG_OUTPUT_PATH" 2>&1 < /dev/null &
  echo "$!" > "$PID_FILE_PATH"
  sleep 1
  if is_running; then
    echo "started (headless)"
    return 0
  fi

  echo "failed to start (headless); recent output:"
  tail -n 20 "$LOG_OUTPUT_PATH" 2>/dev/null || true
  return 1
}

launch_desktop_terminal() {
  local terminal_name="$1"
  shift
  local env_args=()

  env_args+=("DISPLAY=${DESKTOP_DISPLAY:-:0}")
  env_args+=("XDG_RUNTIME_DIR=${DESKTOP_XDG_RUNTIME_DIR:-/run/user/$(id -u)}")
  env_args+=("XAUTHORITY=${DESKTOP_XAUTHORITY:-$HOME/.Xauthority}")

  if [[ -n "${DESKTOP_WAYLAND_DISPLAY:-}" ]]; then
    env_args+=("WAYLAND_DISPLAY=$DESKTOP_WAYLAND_DISPLAY")
  fi

  if [[ -n "${DESKTOP_DBUS_SESSION_BUS_ADDRESS:-}" ]]; then
    env_args+=("DBUS_SESSION_BUS_ADDRESS=$DESKTOP_DBUS_SESSION_BUS_ADDRESS")
  fi

  : > "$LAUNCH_LOG_PATH"
  nohup env \
    "${env_args[@]}" \
    "$@" >"$LAUNCH_LOG_PATH" 2>&1 < /dev/null &

  sleep 1
  if is_running; then
    echo "started (desktop terminal: ${terminal_name})"
    return 0
  fi

  return 1
}

start_desktop_terminal() {
  detect_node_profile
  echo "detected node profile: ${NODE_PROFILE}${NODE_PRETTY_NAME:+ ($NODE_PRETTY_NAME)}"

  case "$NODE_PROFILE" in
    kubuntu)
      if ! have_cmd konsole; then
        echo "kubuntu profile detected but 'konsole' is unavailable; falling back to headless"
        return 1
      fi
      if ! desktop_launch_available; then
        echo "kubuntu profile detected but no graphical session socket was found; falling back to headless"
        return 1
      fi
      if launch_desktop_terminal "konsole" konsole --noclose -e bash -lc "$FOLLOW_SCRIPT_PATH"; then
        return 0
      fi
      ;;
    ubuntu-desktop)
      if ! have_cmd x-terminal-emulator; then
        echo "ubuntu desktop profile detected but 'x-terminal-emulator' is unavailable; falling back to headless"
        return 1
      fi
      if ! desktop_launch_available; then
        echo "ubuntu desktop profile detected but no graphical session socket was found; falling back to headless"
        return 1
      fi
      if launch_desktop_terminal "x-terminal-emulator" x-terminal-emulator -e bash -lc "$FOLLOW_SCRIPT_PATH; exec bash"; then
        return 0
      fi
      ;;
    ubuntu-server|headless)
      echo "headless node detected; using background follower"
      return 1
      ;;
  esac

  echo "desktop launch attempted but follower not running; launch log:"
  tail -n 20 "$LAUNCH_LOG_PATH" 2>/dev/null || true
  return 1
}

stop_all() {
  if [[ -f "$PID_FILE_PATH" ]]; then
    kill "$(cat "$PID_FILE_PATH")" >/dev/null 2>&1 || true
    rm -f "$PID_FILE_PATH"
  fi

  pkill -f "$FOLLOW_SCRIPT_PATH" >/dev/null 2>&1 || true
  echo "stopped"
}

write_follow_script

case "$ACTION" in
  start)
    ensure_passwordless_sudo
    if start_desktop_terminal; then
      exit 0
    fi
    start_headless
    ;;
  stop)
    stop_all
    ;;
  *)
    echo "invalid action: $ACTION" >&2
    exit 1
    ;;
esac
REMOTE_SCRIPT
}

failed=0

for key in $(printf '%s\n' "${!TARGETS[@]}" | sort); do
  user_host="${TARGETS[$key]}"
  user="${user_host%%|*}"
  host="${user_host##*|}"

  echo "[$ACTION] ${user}@${host}"
  if ! run_on_target "$user" "$host"; then
    echo "[$ACTION] FAILED: ${user}@${host}" >&2
    failed=1
  fi
done

exit "$failed"
