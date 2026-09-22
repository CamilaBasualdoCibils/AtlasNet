# AtlasNet Local Stack

A VS Code bottom panel that launches real local AtlasNet processes in separate C++/GDB debug sessions. No Docker is used. The initial stack contains three Database nodes, two Shard nodes, and two ControllerEligible + Database + ClientIngress nodes, backed by one temporary local Valkey process (also under GDB).

## Install and run

1. Install VS Code's Microsoft C/C++ extension (`ms-vscode.cpptools`) and GDB.
2. Build a Debug configuration of AtlasNet: `cmake --build build --target AtlasNetNode Valkey-Server-Compile`. Configure CMake first if necessary.
3. Install the packaged VSIX using **Extensions: Install from VSIX…**, then reload VS Code. To produce the package yourself, run `npm run package` in this directory (requires npm registry access for `@vscode/vsce`).
4. Open the AtlasNet repository as a single-folder workspace. Run **AtlasNet: Open Local Stack** from the Command Palette.
5. Edit counts, capabilities, paths, ports and extra arguments; press **Run stack**. Select processes in **Run and Debug → Call Stack** to switch debugger context. Normal source breakpoints apply to all processes.

The panel saves configuration to `.vscode/atlasnet-stack.json` when saving or running. Do not store credentials in a committed configuration. It does not build automatically. Use **Stop stack** to stop only sessions launched by this panel; individual session stop buttons are also available. Stop the stack before reloading the extension host.

## Arguments and networking

Arguments are JSON arrays, passed directly to the debugger without shell interpretation. For example, `["--ingress-sockets", "TCP:port=0"]` requests an ephemeral ingress port on a ClientIngress node. `${instance}` expands to the 1-based replica number within a group; `${index}` is the 0-based global node index; `${workspaceFolder}` expands to the repository path. Split a group into count-one entries for individual arguments. Fixed custom ingress ports must be distinct between local processes.

Cluster and registry UDP ports increment from their configured bases in group order. Generated networking/capability flags cannot be overridden in extra arguments. Database nodes share the configured Valkey store; shards use the first database node's registry endpoint. Database-capable nodes launch first. AtlasNet's own registry handshake retries handle registry startup. Pausing a registry at a breakpoint during startup can exhaust those retries.

Managed Valkey binds only to loopback, defaults to TCP port 46379, disables persistence and starts without the AtlasNet Valkey module (standalone nodes provide the registry). Its URI must match the managed port. Disable managed Valkey to use an already running server. All database nodes share that server, so this simulates processes and capabilities, not independent database replicas or network isolation. ClientIngress eligibility alone does not configure ingress sockets, and ControllerEligible does not force a controller role.

The extension checks executables and managed Valkey availability before launch, waits up to 15 seconds for Valkey TCP readiness, and stops the stack if a debugger launch fails. A later process exit is shown by its session disappearing; other nodes remain available for diagnosis. Port allocation avoids collisions inside the generated stack; other applications can still occupy these UDP ports. Debug Console contains process startup errors.

## Development

No JavaScript dependencies or compilation are needed at runtime. Run `npm test` for launch-plan tests. To try the source directly, run `code --extensionDevelopmentPath=/absolute/path/to/tools/vscode-local-stack /absolute/path/to/AtlasNet`. The panel uses VS Code's [webview API](https://code.visualstudio.com/api/extension-guides/webview) and [debug API](https://code.visualstudio.com/api/references/vscode-api#debug).
