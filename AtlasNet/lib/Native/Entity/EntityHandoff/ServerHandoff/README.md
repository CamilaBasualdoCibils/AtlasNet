## ServerHandoff (Second Version)

## Simple Protocol

- sender picks one agreed switch time in Unix microseconds:
  `transferTimeUs = nowUnixUs + handoffDelayUs`
- sender sends entity snapshot to target shard
- target stores it as pending
- target adopts when local `nowUnixUs >= transferTimeUs`
- sender drops local authority when local `nowUnixUs >= transferTimeUs`

This version still uses timed handoff

## Tick Flow (Runtime)

1. Read incoming handoff packets from mailbox.
2. Adopt due incoming handoffs.
3. Simulate local entities.
4. Plan/send outgoing handoffs for boundary crossings.
5. Commit due outgoing handoffs.
6. Push telemetry snapshots on interval.

## Current Limits

- Still not perfect in all heavy-load failure cases.
- Reliability is better handled by extra recovery work around this core flow.
- Watchdog manages shard health and bound requeue, but handoff execution is in shard servers.

## Main Tuning Knob

- `SH_BorderHandoffPlanner::Options::handoffDelay`
  - Bigger value: more safety margin for network jitter.
  - Smaller value: faster switch, less buffer.
- Build-time override:
  - `ATLASNET_ENTITY_HANDOFF_TRANSFER_DELAY_US` (default `60000` = `60ms`)

## File Responsibilities

- `SH_ServerAuthorityManager.hpp/.cpp`
  - Public entrypoint used by `AtlasNetServer`.
  - Forwards lifecycle and incoming handoff calls to runtime.

- `SH_ServerAuthorityRuntime.hpp/.cpp`
  - Runs the handoff tick order.
  - Wires election, planner, mailbox, and telemetry.

- `SH_OwnershipElection.hpp/.cpp`
  - Picks which shard owns the test stream.
  - Re-evaluates owner from shared DB key and live servers.

- `SH_BorderHandoffPlanner.hpp/.cpp`
  - Detects border crossing.
  - Finds target shard from claimed bounds.
  - Sends handoff packet and returns pending outgoing records.

- `SH_TransferMailbox.hpp/.cpp`
  - Stores pending incoming/outgoing handoffs per entity.
  - Adopts incoming when due.
  - Commits outgoing when due.

- `SH_TelemetryPublisher.hpp/.cpp`
  - Converts tracker data into `AuthorityManifest` rows.

- `SH_HandoffTypes.hpp`
  - Shared structs for pending incoming/outgoing handoff state.

- `SH_EntityAuthorityTracker.hpp/.cpp`
  - Stores local authority state (`authoritative` / `passing`) and telemetry rows.

- `SH_HandoffPacketManager.hpp/.cpp`
  - Sends and receives handoff packets for ServerHandoff.

- `SH_HandoffConnectionManager.hpp/.cpp`
  - Tracks active handoff peer links and reaps inactive ones.

- `SH_HandoffConnectionLeaseCoordinator.hpp/.cpp`
  - Optional Redis lease helper to reduce duplicate shard links.
