# AtlasNet

Scalable, universal distributed backend for Massively Multiplayer Onlines (MMO) games.

This project provides a distributed, containerized backend architecture designed for:

- High concurrency networking (TCP/UDP/WebSocket)
- Horizontal scaling
- Container orchestration
- Stateless microservices
- Production-grade deployment (Docker, Swarm, Kubernetes)

---

## ✨ Features

- Gateway Service (real-time networking)
- Authentication Service (JWT-based)
- Session / Match Service
- World / Game Logic Workers
- Redis cache support
- PostgreSQL persistence
- Message broker integration (NATS / Kafka optional)
- Prometheus metrics ready

---

## 🏗 Architecture Overview

---
## How to Use

### Server

A game server must implement our AtlasNetServer interface

### Client

A game client must implement our AtlasNetClient interface

---

## Running

### <img src="docs/assets/docker-mark-blue.svg" width="24" height="24"/> Docker Swarm

Running Atlasnet in a Docker swarm as a service is easy enough
```yml
version: "3.8"

services:
  WatchDog:
    image: watchdog
    networks: [AtlasNet]
 #   environment:
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock # REQUIRED
    deploy:
      labels:
        atlasnet.role: watchdog # REQUIRED
      placement:
          constraints:
            - 'node.role == manager'
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure

  Shard:
    image: shard:latest
    networks: [AtlasNet]
    deploy:
     labels:
        atlasnet.role: shard # REQUIRED
     resources:
        limits:
          cpus: "1.0"      # 1 core
          memory: 1G       # 1 GiB
     mode: replicated
     replicas: 0   # 0 replicas as requested (exists, but wont run)
     restart_policy:
       condition: on-failure

  InternalDB:
    image: valkey/valkey:latest
    command: ["valkey-server", "--appendonly", "yes", "--port", "6379"]
    networks: [AtlasNet]
    ports:
      - target: 6379
        published: 6379
        protocol: tcp
        mode: ingress
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
  Cartograph:
    image: cartograph
    networks: [AtlasNet]
    ports:
      - "3000:3000"   # Next.js default
      - "9229:9229"   # Node inspector
    deploy:
      placement:
          constraints:
            - 'node.role == manager'
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
  Proxy:
    image: ${REGISTRY_ADDR_OPT}proxy:latest
    networks: [AtlasNet]
    ports:
      - target: 25568
        published: 2555
        protocol: tcp
        mode: ingress
      - target: 25568
        published: 2555
        protocol: udp
        mode: ingress
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
#  BuiltInDB_Redis:
#    image: valkey/valkey:latest
#    command: ["valkey-server", "--appendonly", "yes", "--port", "2380"]
#    networks: [AtlasNet]
#    ports:
#      - target: 2380
#        published: 2380
#        protocol: tcp
#        mode: ingress
#      - target: 2380
#        published: 2380
#        protocol: udp
#        mode: ingress
#    deploy:
#      mode: replicated
#      replicas: 1
#      restart_policy:
#        condition: on-failure
#  BuiltInDB_PostGres:
#    image: postgres:16-alpine
#    command: ["postgres", "-c", "port=5432"]
#    environment:
#      POSTGRES_PASSWORD: postgres
#    volumes:
#      - pgdata:/var/lib/postgresql/data
#    networks: [AtlasNet]
volumes:
  pgdata:
    driver: local

networks:
  AtlasNet:
    external: true
    name: AtlasNet
```
### <img src="docs/assets/Kubernetes_logo.svg" width="24" height="24"/> Kubernetes
---