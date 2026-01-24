export interface ConnectionTelemetry {
  IdentityId: string;
  targetId: string;
  pingMs: number;
  inBytesPerSec: number;
  outBytesPerSec: number;
  inPacketsPerSec: number;
  pendingReliableBytes: number;
  pendingUnreliableBytes: number;
  sentUnackedReliableBytes: number;
  queueTimeUsec: number;
  qualityLocal: number;
  qualityRemote: number;
  state: string;
}

export type TelemetryConnectionRow = string[];

export interface ShardTelemetry {
  shardId: string;
  downloadKbps: number;
  uploadKbps: number;

  connections: TelemetryConnectionRow[];
}
