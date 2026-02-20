export interface Vec2 {
  x: number;
  y: number;
}

export interface ShapeJS {
  id?: string;
  ownerId?: string;
  type: 'circle' | 'rectangle' | 'polygon' | 'line' | 'rectImage';
  position: Vec2;
  radius?: number;
  size?: Vec2;
  points?: Vec2[];
  color?: string;
}

export interface ConnectionTelemetry {
  shardId?: string | null;
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

export interface ShardTelemetry {
  shardId: string;
  downloadKbps: number;
  uploadKbps: number;
  connections: ConnectionTelemetry[];
}

export interface AuthorityEntityTelemetry {
  entityId: string;
  ownerId: string;
  world: number;
  x: number;
  y: number;
  z: number;
  isClient: boolean;
  clientId: string;
}

export interface DatabaseRecord {
  source: string;
  key: string;
  type: string;
  entryCount: number;
  ttlSeconds: number;
  payload: string;
}

export interface DatabaseSource {
  id: string;
  name: string;
  host: string;
  port: number;
  running: boolean;
  latencyMs: number | null;
}

export interface DatabaseSnapshotResponse {
  sources: DatabaseSource[];
  selectedSource: string | null;
  records: DatabaseRecord[];
}
