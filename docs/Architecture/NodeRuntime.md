# Node runtime and hosts

Both host targets compile the same reusable node source. The concrete `AtlasNet::AtlasNetNode`
class owns identity, networking, lifecycle, and capability-driven behavior directly. Construct `AtlasNet::NodeConfig`,
inject an `IDatabaseBackend` when enabling `NodeCapability::Database`, then call
`Start()` and `Poll()` on the hosting thread. `Run(stop_token)` is an optional
blocking loop. The runtime reads no environment variables and installs no signal
handlers. The host determines the environment through construction; there is no
host-environment flag in node metadata.

`AtlasNetNode` owns CLI/environment configuration and process signals.
Default capabilities are `Shard,ClientIngress,ControllerEligible`. These flags
advertise eligibility; this refactor does not implement the existing unfinished
shard/controller/ingress systems. Ingress configuration requires `ClientIngress`.
Database capability enables the registry RPC service in the same node runtime.

| CLI option | Environment variable | Meaning |
| --- | --- | --- |
| `--capabilities` | `ATLASNET_CAPABILITIES` | Comma-separated capability names, or `None` |
| `--cluster-port` | `ATLASNET_CLUSTER_PORT` | UDP cluster port; default ephemeral |
| `--handshake-port` | `ATLASNET_HANDSHAKE_PORT` | UDP registry RPC port; default ephemeral |
| `--DB-host`, `--DB-port` | `ATLASNET_DB_HOST`, `ATLASNET_DB_PORT` | Upstream AtlasNet registry RPC endpoint |
| `--valkey-uri` | `ATLASNET_VALKEY_URI` | RESP connection URI for standalone database storage |
| `--ingress-sockets` | `ATLASNET_INGRESS_SOCKETS` | Existing ingress socket syntax |
| `--network-transport` | `ATLASNET_NETWORK_TRANSPORT` | Currently `UDP` only |

CLI options override environment variables. Ordinary standalone nodes require an
upstream registry (Debug builds retain the localhost:5000 development default).
A database-capable standalone node requires a Valkey URI and can omit an upstream
registry. For example:

```sh
AtlasNetNode --capabilities Database,Shard \
  --valkey-uri tcp://127.0.0.1:6379 --handshake-port 5000
AtlasNetNode --DB-host 127.0.0.1 --DB-port 5000
```

For local development, `AtlasNetNode-Local` is an executable CMake target with
every capability enabled. It can be launched directly under a debugger, so
breakpoints apply to the node process. The executable reuses Valkey on
`127.0.0.1:16379` when one is already responding there, or starts the bundled
Valkey server and stops it when the node exits:

```sh
cmake --build build --target AtlasNetNode-Local
./build/AtlasNetNode-Local
```

Set `ATLASNET_LOCAL_VALKEY_PORT` when configuring CMake to use another port.

`AtlasNetNode-ValkeyModule` is the Valkey module host, not another node class. It constructs the
same runtime with `Database` capability and `ValkeyModuleBackend`. The backend
uses `ValkeyModule_Call` on Valkey's event thread, including command replication.
Valkey's file-descriptor event API drives polling; unloading removes the event
registration before destroying the
runtime and its context. Module arguments accept an optional handshake port:

```sh
valkey-server --loadmodule /path/to/libAtlasNetNode-ValkeyModule.so 5000
```

The module never compiles or constructs `ValkeyRemoteBackend`. Standalone
database nodes inject that backend, which uses the existing redis++ RESP
client. Both backends write the same binary record to `AtlasNet:RegisteredNodes`.
The core still has its pre-existing redis++ dependency for other subsystems; the
runtime itself has no concrete backend dependency. Remote communication with
other instances remains possible without routing local module storage over TCP.

Capability registration uses `AtlasNet.DB.RegisterNode.v2`. The original ping
and registration RPCs remain available, with the original registration wire
layout. Legacy registrations receive the default capabilities. Deploy the new
registry host before new nodes, which use v2. Stored records now contain the v2
registration fields and capabilities, replacing the previous private timestamp
record; pre-existing registry entries should be refreshed by re-registering nodes.
Registry expiry/removal and cluster resolution remain outside this refactor.

Database-capability RPC contracts live in
`include/AtlasNet/Node/RPC/Database.hpp` under
`AtlasNet::RPC::Database`. Callers use `Ping`, `RegisterNode`, and
`RegisterNodeRequest` without depending on the destination node's host. The
original registration wire contract remains available as
`Legacy::RegisterNode`. `GetNodes` is declared for future registry discovery,
but no handler is currently registered.

Tests cover configuration, capability serialization, backend injection, legacy
and v2 RPC registration, both storage backends, standalone shutdown, and module
unload/reload. The lifecycle test disables Valkey TCP entirely while exercising
module storage and a standalone node's AtlasNet UDP handshake.
