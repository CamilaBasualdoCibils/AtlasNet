FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      docker.io \
      jq \
      util-linux \
      hotspot \
      novnc \
      websockify \
      x11vnc \
      xvfb \
      openbox; \
    if ! apt-get install -y --no-install-recommends linux-perf; then \
      apt-get install -y --no-install-recommends linux-tools-common linux-tools-generic \
        || apt-get install -y --no-install-recommends linux-tools-common; \
    fi; \
    command -v perf >/dev/null 2>&1; \
    rm -rf /var/lib/apt/lists/*

CMD ["bash"]
