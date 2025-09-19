# wait for God container to stop, then clean up all Partition containers

#!/bin/bash
GOD_NAME="god"

# echo "[Watcher] Script started at $(date)"
# echo "[Watcher] Waiting for container '$GOD_NAME' to stop..."

# Start docker events in the background, capture its PID
docker events \
  --filter "container=$GOD_NAME" \
  --filter "event=die" \
  --format '{{.Actor.Attributes.name}}' > god_watcher.log &
EV_PID=$!

# Tail its output, look for first die event from god container
tail -f god_watcher.log | while read cname; do
    if [ "$cname" = "$GOD_NAME" ]; then

        # Kill partition containers
        # echo "[Watcher] God container stopped → cleaning up partitions..."
        docker rm -f $(docker ps -aq --filter "name=partition_") 2>/dev/null || true
        # echo "[Watcher] Cleanup complete → exiting."

        # Kill docker events process
        kill $EV_PID >/dev/null 2>&1

        # Remove log files (keep commented out, sometimes deleting these stops this script from working)
        # rm -f god_watcher.log
        # rm -f nohup.out

        break
    fi

done
