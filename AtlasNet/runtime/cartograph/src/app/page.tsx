'use client';

import Link from 'next/link';
import { useEffect, useMemo, useState } from 'react';
import type { DatabaseSnapshotResponse } from './lib/cartographTypes';
import {
  computeMapBoundsCenter,
  computeNetworkEdgeCount,
  computeProjectedShardPositions,
  computeShardAnchorPositions,
  computeShardBoundsById,
  computeShardHoverBoundsById,
  isShardIdentity,
  normalizeShardId,
  type ShardHoverBounds,
} from './lib/mapData';
import {
  useAuthorityEntities,
  useHeuristicShapes,
  useNetworkTelemetry,
  useWorkersSnapshot,
} from './lib/hooks/useTelemetryFeeds';

const TELEMETRY_POLL_INTERVAL_MS = 1000;
const WORKERS_POLL_INTERVAL_MS = 5000;
const DATABASE_POLL_INTERVAL_MS = 5000;

interface ShardSummary {
  shardId: string;
  bounds: ShardHoverBounds | null;
  status: 'bounded' | 'unbounded' | 'bounded stale';
  hasClaimedBound: boolean;
  area: number;
  clientCount: number;
  entityCount: number;
  connectionCount: number;
  downloadKbps: number;
  uploadKbps: number;
}

interface DatabaseSummary {
  activeSources: number;
  totalKeys: number;
  heuristicType: string | null;
  lastUpdatedMs: number | null;
  errorText: string | null;
}

const EMPTY_DATABASE_SUMMARY: DatabaseSummary = {
  activeSources: 0,
  totalKeys: 0,
  heuristicType: null,
  lastUpdatedMs: null,
  errorText: null,
};

function formatRate(value: number): string {
  if (!Number.isFinite(value)) {
    return '0.0';
  }
  return value.toFixed(1);
}

function formatArea(value: number): string {
  if (!Number.isFinite(value) || value <= 0) {
    return '-';
  }
  if (value >= 1_000_000) {
    return `${(value / 1_000_000).toFixed(2)}M`;
  }
  if (value >= 1_000) {
    return `${(value / 1_000).toFixed(1)}k`;
  }
  return value.toFixed(0);
}

function parseHeuristicType(records: DatabaseSnapshotResponse['records']): string | null {
  if (!Array.isArray(records)) {
    return null;
  }

  const heuristicRecord = records.find(
    (record) => String(record.key ?? '').trim().toLowerCase() === 'heuristic_type'
  );
  if (!heuristicRecord) {
    return null;
  }

  const payload = String(heuristicRecord.payload ?? '').trim();
  if (!payload) {
    return null;
  }

  try {
    const parsed = JSON.parse(payload) as unknown;
    if (typeof parsed === 'string') {
      const text = parsed.trim();
      return text.length > 0 ? text : null;
    }
  } catch {}

  const firstLine = payload.split(/\r?\n/)[0]?.trim();
  return firstLine && firstLine.length > 0 ? firstLine : null;
}

function MetricCard({
  label,
  value,
  hint,
}: {
  label: string;
  value: string | number;
  hint: string;
}) {
  return (
    <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
      <div className="text-xs uppercase tracking-wide text-slate-400">{label}</div>
      <div className="font-mono text-2xl text-slate-100">{value}</div>
      <div className="text-xs text-slate-500">{hint}</div>
    </div>
  );
}

function computeGlobalBounds(boundsByShardId: Map<string, ShardHoverBounds>): ShardHoverBounds | null {
  let minX = Infinity;
  let maxX = -Infinity;
  let minY = Infinity;
  let maxY = -Infinity;
  let hasBounds = false;

  for (const bounds of boundsByShardId.values()) {
    hasBounds = true;
    if (bounds.minX < minX) minX = bounds.minX;
    if (bounds.maxX > maxX) maxX = bounds.maxX;
    if (bounds.minY < minY) minY = bounds.minY;
    if (bounds.maxY > maxY) maxY = bounds.maxY;
  }

  if (!hasBounds) {
    return null;
  }

  return {
    minX,
    maxX,
    minY,
    maxY,
    area: Math.max(1, (maxX - minX) * (maxY - minY)),
  };
}

function MiniShardMap({
  boundsByShardId,
  claimedBoundShardIds,
  focusShardId,
  highlightFocus,
}: {
  boundsByShardId: Map<string, ShardHoverBounds>;
  claimedBoundShardIds: Set<string>;
  focusShardId: string;
  highlightFocus: boolean;
}) {
  const visibleBoundsByShardId = useMemo(() => {
    const out = new Map<string, ShardHoverBounds>();
    for (const [shardId, bounds] of boundsByShardId) {
      if (claimedBoundShardIds.has(shardId)) {
        out.set(shardId, bounds);
      }
    }
    return out;
  }, [boundsByShardId, claimedBoundShardIds]);

  const globalBounds = useMemo(
    () => computeGlobalBounds(visibleBoundsByShardId),
    [visibleBoundsByShardId]
  );

  if (!globalBounds) {
    return (
      <div className="flex h-[118px] items-center justify-center rounded-lg border border-slate-800 bg-slate-950 text-xs text-slate-500">
        No bounds yet
      </div>
    );
  }

  const width = 200;
  const height = 118;
  const pad = 8;
  const spanX = Math.max(1, globalBounds.maxX - globalBounds.minX);
  const spanY = Math.max(1, globalBounds.maxY - globalBounds.minY);
  const scale = Math.min((width - pad * 2) / spanX, (height - pad * 2) / spanY);
  const contentWidth = spanX * scale;
  const contentHeight = spanY * scale;
  const offsetX = (width - contentWidth) / 2;
  const offsetY = (height - contentHeight) / 2;

  function toX(x: number): number {
    return offsetX + (x - globalBounds.minX) * scale;
  }

  function toY(y: number): number {
    return height - (offsetY + (y - globalBounds.minY) * scale);
  }

  return (
    <svg
      viewBox={`0 0 ${width} ${height}`}
      className="h-[118px] w-full rounded-lg border border-slate-800 bg-slate-950"
      role="img"
      aria-label={`Bounds minimap for ${focusShardId}`}
    >
      {Array.from(visibleBoundsByShardId.entries()).map(([shardId, bounds]) => {
        const x1 = toX(bounds.minX);
        const x2 = toX(bounds.maxX);
        const y1 = toY(bounds.minY);
        const y2 = toY(bounds.maxY);
        const left = Math.min(x1, x2);
        const top = Math.min(y1, y2);
        const rectWidth = Math.max(1, Math.abs(x2 - x1));
        const rectHeight = Math.max(1, Math.abs(y2 - y1));
        const isFocus = highlightFocus && shardId === focusShardId;

        return (
          <rect
            key={shardId}
            x={left}
            y={top}
            width={rectWidth}
            height={rectHeight}
            fill={isFocus ? 'rgba(56, 189, 248, 0.28)' : 'rgba(100, 116, 139, 0.08)'}
            stroke={isFocus ? 'rgba(56, 189, 248, 0.95)' : 'rgba(71, 85, 105, 0.55)'}
            strokeWidth={isFocus ? 1.8 : 1}
          />
        );
      })}
    </svg>
  );
}

function ShardSummaryCard({
  summary,
  boundsByShardId,
  claimedBoundShardIds,
}: {
  summary: ShardSummary;
  boundsByShardId: Map<string, ShardHoverBounds>;
  claimedBoundShardIds: Set<string>;
}) {
  return (
    <article className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
      <div className="mb-2 flex items-start justify-between gap-2">
        <div>
          <h3 className="font-mono text-sm text-slate-100">{summary.shardId}</h3>
          <p className="text-xs text-slate-500">bounds area {formatArea(summary.area)}</p>
        </div>
        <span className="rounded-full border border-slate-700 bg-slate-950 px-2 py-1 text-[10px] uppercase tracking-wide text-slate-300">
          {summary.status}
        </span>
      </div>

      <MiniShardMap
        boundsByShardId={boundsByShardId}
        claimedBoundShardIds={claimedBoundShardIds}
        focusShardId={summary.shardId}
        highlightFocus={summary.hasClaimedBound}
      />

      <div className="mt-3 grid grid-cols-2 gap-2 text-xs">
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          clients: <span className="font-mono">{summary.clientCount}</span>
        </div>
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          entities: <span className="font-mono">{summary.entityCount}</span>
        </div>
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          in bytes: <span className="font-mono">{formatRate(summary.downloadKbps)}</span>
        </div>
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          out bytes: <span className="font-mono">{formatRate(summary.uploadKbps)}</span>
        </div>
      </div>
    </article>
  );
}

export default function OverviewPage() {
  const heuristicShapes = useHeuristicShapes({
    intervalMs: TELEMETRY_POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const authorityEntities = useAuthorityEntities({
    intervalMs: TELEMETRY_POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const networkTelemetry = useNetworkTelemetry({
    intervalMs: TELEMETRY_POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const workersSnapshot = useWorkersSnapshot({
    intervalMs: WORKERS_POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });

  const [databaseSummary, setDatabaseSummary] =
    useState<DatabaseSummary>(EMPTY_DATABASE_SUMMARY);

  useEffect(() => {
    let alive = true;

    async function pollDatabaseSummary() {
      try {
        const response = await fetch('/api/databases?decodeSerialized=0', {
          cache: 'no-store',
        });
        if (!response.ok) {
          if (!alive) {
            return;
          }
          setDatabaseSummary((prev) => ({
            ...prev,
            errorText: `Database endpoint returned ${response.status}.`,
          }));
          return;
        }

        const payload = (await response.json()) as DatabaseSnapshotResponse;
        if (!alive) {
          return;
        }

        const sources = Array.isArray(payload.sources)
          ? payload.sources.filter((source) => source.running)
          : [];
        const records = Array.isArray(payload.records) ? payload.records : [];
        setDatabaseSummary({
          activeSources: sources.length,
          totalKeys: records.length,
          heuristicType: parseHeuristicType(records),
          lastUpdatedMs: Date.now(),
          errorText: null,
        });
      } catch {
        if (!alive) {
          return;
        }
        setDatabaseSummary((prev) => ({
          ...prev,
          errorText: 'Database summary fetch failed.',
        }));
      }
    }

    void pollDatabaseSummary();
    const intervalId = setInterval(() => {
      void pollDatabaseSummary();
    }, DATABASE_POLL_INTERVAL_MS);

    return () => {
      alive = false;
      clearInterval(intervalId);
    };
  }, []);

  const shardBoundsById = useMemo(
    () => computeShardBoundsById(heuristicShapes),
    [heuristicShapes]
  );

  const shardAnchorPositions = useMemo(
    () => computeShardAnchorPositions(heuristicShapes),
    [heuristicShapes]
  );

  const mapBoundsCenter = useMemo(
    () => computeMapBoundsCenter(heuristicShapes),
    [heuristicShapes]
  );

  const shardTelemetryById = useMemo(() => {
    const out = new Map<string, (typeof networkTelemetry)[number]>();
    for (const shard of networkTelemetry) {
      const shardId = normalizeShardId(shard.shardId);
      if (!shardId) {
        continue;
      }
      out.set(shardId, shard);
    }
    return out;
  }, [networkTelemetry]);

  const entitiesByShardId = useMemo(() => {
    const out = new Map<string, number>();
    for (const entity of authorityEntities) {
      const shardId = normalizeShardId(entity.ownerId);
      if (!isShardIdentity(shardId)) {
        continue;
      }
      out.set(shardId, (out.get(shardId) ?? 0) + 1);
    }
    return out;
  }, [authorityEntities]);

  const clientsByShardId = useMemo(() => {
    const out = new Map<string, number>();
    for (const entity of authorityEntities) {
      if (!entity.isClient) {
        continue;
      }
      const shardId = normalizeShardId(entity.ownerId);
      if (!isShardIdentity(shardId)) {
        continue;
      }
      out.set(shardId, (out.get(shardId) ?? 0) + 1);
    }
    return out;
  }, [authorityEntities]);

  const uniqueClientCount = useMemo(() => {
    const keys = new Set<string>();
    for (const entity of authorityEntities) {
      if (!entity.isClient) {
        continue;
      }
      const key = entity.clientId.trim() || `entity:${entity.entityId}`;
      keys.add(key);
    }
    return keys.size;
  }, [authorityEntities]);

  const contextCount = workersSnapshot.contexts.length;
  const reachableContextCount = workersSnapshot.contexts.filter(
    (context) => context.status === 'ok'
  ).length;

  const networkNodeIds = useMemo(() => {
    const out: string[] = [];
    const seen = new Set<string>();
    for (const shard of networkTelemetry) {
      const shardId = normalizeShardId(shard.shardId);
      if (!shardId || seen.has(shardId)) {
        continue;
      }
      seen.add(shardId);
      out.push(shardId);
    }
    return out;
  }, [networkTelemetry]);
  const networkNodeIdSet = useMemo(
    () => new Set(networkNodeIds),
    [networkNodeIds]
  );

  const projectedShardPositions = useMemo(
    () =>
      computeProjectedShardPositions({
        mapBoundsCenter,
        networkNodeIds,
        shardAnchorPositions,
      }),
    [mapBoundsCenter, networkNodeIds, shardAnchorPositions]
  );

  const shardBoundsByIdWithNetworkFallback = useMemo(
    () =>
      computeShardHoverBoundsById({
        networkNodeIds,
        projectedShardPositions,
        shardBoundsById,
      }),
    [networkNodeIds, projectedShardPositions, shardBoundsById]
  );

  const claimedBoundShardIds = useMemo(
    () => new Set(shardBoundsById.keys()),
    [shardBoundsById]
  );

  const networkEdgeCount = useMemo(
    () => computeNetworkEdgeCount({ networkNodeIdSet, networkTelemetry }),
    [networkNodeIdSet, networkTelemetry]
  );

  const shardSummaries = useMemo(() => {
    const ids = new Set<string>();
    for (const shardId of shardBoundsByIdWithNetworkFallback.keys()) ids.add(shardId);
    for (const shardId of shardTelemetryById.keys()) ids.add(shardId);
    for (const shardId of entitiesByShardId.keys()) ids.add(shardId);

    const out: ShardSummary[] = [];
    for (const shardId of ids) {
      const telemetry = shardTelemetryById.get(shardId);
      const claimedBound = shardBoundsById.get(shardId) ?? null;
      const bounds = shardBoundsByIdWithNetworkFallback.get(shardId) ?? null;
      const hasClaimedBound = claimedBound != null;
      const hasNetworkTelemetry = telemetry != null;
      const status: ShardSummary['status'] = hasClaimedBound
        ? hasNetworkTelemetry
          ? 'bounded'
          : 'bounded stale'
        : 'unbounded';
      out.push({
        shardId,
        bounds,
        status,
        hasClaimedBound,
        area: claimedBound?.area ?? 0,
        clientCount: clientsByShardId.get(shardId) ?? 0,
        entityCount: entitiesByShardId.get(shardId) ?? 0,
        connectionCount: telemetry?.connections.length ?? 0,
        downloadKbps: telemetry?.downloadKbps ?? 0,
        uploadKbps: telemetry?.uploadKbps ?? 0,
      });
    }

    out.sort((left, right) => left.shardId.localeCompare(right.shardId));
    return out;
  }, [
    clientsByShardId,
    entitiesByShardId,
    shardBoundsById,
    shardBoundsByIdWithNetworkFallback,
    shardTelemetryById,
  ]);

  return (
    <div className="space-y-6">
      <div className="flex flex-wrap items-end justify-between gap-3">
        <div>
          <h1 className="text-2xl font-semibold text-slate-100">Overview</h1>
          <p className="mt-1 text-sm text-slate-400">
            High-level status across map, clients, workers, network, and database.
          </p>
        </div>
        <div className="flex flex-wrap gap-2 text-xs">
          <Link
            href="/map"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Map
          </Link>
          <Link
            href="/clients"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Clients
          </Link>
          <Link
            href="/workers"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Workers
          </Link>
          <Link
            href="/database"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Database
          </Link>
          <Link
            href="/network"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Network
          </Link>
        </div>
      </div>

      <section className="grid grid-cols-2 gap-3 md:grid-cols-3 xl:grid-cols-5">
        <MetricCard
          label="Clients"
          value={uniqueClientCount}
          hint={`${authorityEntities.length} tracked entities`}
        />
        <MetricCard
          label="Worker Contexts"
          value={contextCount}
          hint={`${reachableContextCount} reachable`}
        />
        <MetricCard
          label="Shards"
          value={shardSummaries.length}
          hint={`${networkNodeIdSet.size} in telemetry`}
        />
        <MetricCard
          label="Network Links"
          value={networkEdgeCount}
          hint="deduplicated GNS edges"
        />
        <MetricCard
          label="Curr Bounds Heuristic"
          value={databaseSummary.heuristicType ?? 'unknown'}
          hint="from Heuristic_Type"
        />
      </section>

      <section className="grid grid-cols-1 gap-4 xl:grid-cols-2">
        <article className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4">
          <h2 className="text-sm font-semibold uppercase tracking-wide text-slate-400">Workers</h2>
          <p className="mt-2 text-sm text-slate-300">
            {contextCount} contexts discovered, {reachableContextCount} currently reachable.
          </p>
          <p className="mt-1 text-xs text-slate-500">
            {workersSnapshot.error ?? 'Worker snapshot stream healthy.'}
          </p>
        </article>

        <article className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4">
          <h2 className="text-sm font-semibold uppercase tracking-wide text-slate-400">Database</h2>
          <p className="mt-2 text-sm text-slate-300">
            {databaseSummary.activeSources} active sources, {databaseSummary.totalKeys} keys loaded.
          </p>
          <p className="mt-1 text-xs text-slate-500">
            {databaseSummary.errorText ??
              (databaseSummary.lastUpdatedMs
                ? `Updated ${new Date(databaseSummary.lastUpdatedMs).toLocaleTimeString()}`
                : 'Waiting for first DB snapshot...')}
          </p>
        </article>
      </section>

      <section className="space-y-3">
        <div className="flex items-center justify-between">
          <h2 className="text-lg font-semibold text-slate-100">Server Bounds Minimap</h2>
          <p className="text-xs text-slate-500">{shardSummaries.length} servers</p>
        </div>

        {shardSummaries.length === 0 ? (
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4 text-sm text-slate-500">
            Waiting for shard telemetry and map bounds...
          </div>
        ) : (
          <div className="grid grid-cols-1 gap-3 md:grid-cols-2 2xl:grid-cols-3">
            {shardSummaries.map((summary) => (
              <ShardSummaryCard
                key={summary.shardId}
                summary={summary}
                boundsByShardId={shardBoundsByIdWithNetworkFallback}
                claimedBoundShardIds={claimedBoundShardIds}
              />
            ))}
          </div>
        )}
      </section>
    </div>
  );
}
