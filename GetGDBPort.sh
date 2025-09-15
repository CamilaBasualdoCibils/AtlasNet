#!/bin/bash

# Usage: ./get_container_port.sh <image-name>
IMAGE_NAME="$1"

if [ -z "$IMAGE_NAME" ]; then
  echo "Usage: $0 <image-name>"
  exit 1
fi

# Get containers matching the image name

CONTAINERS=$(docker ps --filter "ancestor=$IMAGE_NAME" --format "{{.ID}} {{.Ports}}")
if [ -z "$CONTAINERS" ]; then
  echo "No running containers found for image: $IMAGE_NAME"
  exit 1
fi
echo "--------------------------------------[========]--------------------------------------"

IFS=$'\n' read -rd '' -a lines <<< "$CONTAINERS"

ports_list=()

for i in "${!lines[@]}"; do
    line="${lines[$i]}"
    
  CID=$(echo "$line" | awk '{print $1}')
  PORTS=$(docker inspect --format='{{range $p, $conf := .NetworkSettings.Ports}}{{(index $conf 0).HostPort}} {{end}}' "$CID")
  PORTS="${PORTS//[[:space:]]/}"
  NAME=$(docker inspect --format='{{.Name}}' "$CID" | sed 's/^\/\(.*\)/\1/')
  UPTIME=$(docker inspect --format='{{.State.Running}} {{.State.StartedAt}}' "$CID")
  if [[ $(echo $UPTIME | awk '{print $1}') == "true" ]]; then
    STARTED_AT=$(echo $UPTIME | awk '{print $2}')
    # convert to seconds since epoch
    START_TS=$(date --date="$STARTED_AT" +%s)
    NOW_TS=$(date +%s)
    DIFF=$((NOW_TS - START_TS))
    
    # convert to human-readable
    DAYS=$((DIFF/86400))
    HOURS=$(( (DIFF%86400)/3600 ))
    MINUTES=$(( (DIFF%3600)/60 ))
    SECONDS=$((DIFF%60))
    
    UPTIME_HR=""
    [[ $DAYS -ne 0 ]] && UPTIME_HR+="${DAYS}d "
    [[ $HOURS -ne 0 ]] && UPTIME_HR+="${HOURS}h "
    [[ $MINUTES -ne 0 ]] && UPTIME_HR+="${MINUTES}m "
    [[ $SECONDS -ne 0 ]] && UPTIME_HR+="${SECONDS}s"
else
    UPTIME_HR="Not running"
fi
    echo "[$i] $NAME | CID: $CID | Port:$PORTS | Uptime: $UPTIME_HR"
    ports_list+=("$PORTS")
done

if [[ ${#ports_list[@]} -eq 1 ]]; then
    echo "${ports_list[0]}"
    exit 0
fi

# Wait for user input
read -p "Select a number to get its port: " choice
if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 0 && choice < ${#ports_list[@]} )); then
    echo "Selected port: ${ports_list[$choice]}"
else
    echo "Invalid choice."
fi

