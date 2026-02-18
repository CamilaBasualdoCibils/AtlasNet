'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type { ShardTelemetry } from '../lib/networkTelemetryTypes';
import {
  parseAuthorityRows,
  type AuthorityEntityTelemetry,
} from '../lib/authorityTelemetryTypes';
import { createMapRenderer } from '../lib/mapRenderer';
import type { ShapeJS } from '../lib/types';

function normalizeShardId(value: string): string {
  return value.trim();
}

export default function MapPage() {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<ReturnType<typeof createMapRenderer> | null>(null);
  const [baseShapes, setBaseShapes] = useState<ShapeJS[]>([]);
  const [networkTelemetry, setNetworkTelemetry] = useState<ShardTelemetry[]>([]);
  const [authorityEntities, setAuthorityEntities] = useState<
    AuthorityEntityTelemetry[]
  >([]);
  const [showGnsConnections, setShowGnsConnections] = useState(false);
  const [showAuthorityEntities, setShowAuthorityEntities] = useState(true);

  // Fetch static heuristic shapes (base map)
  useEffect(() => {
    let alive = true;
    fetch('/api/heuristicfetch')
      .then((res) => res.json())
      .then((data: ShapeJS[]) => {
        if (!alive) return;
        setBaseShapes(Array.isArray(data) ? data : []);
      })
      .catch(console.error);
    return () => {
      alive = false;
    };
  }, []);

  // Poll network telemetry for GNS overlay
  useEffect(() => {
    let alive = true;
    async function poll() {
      try {
        const res = await fetch('/api/networktelemetry', { cache: 'no-store' });
        if (!res.ok) return;
        const data = (await res.json()) as ShardTelemetry[];
        if (!alive) return;
        setNetworkTelemetry(Array.isArray(data) ? data : []);
      } catch {
        if (!alive) return;
        setNetworkTelemetry([]);
      }
    }
    poll();
    const id = setInterval(poll, 500);
    return () => {
      alive = false;
      clearInterval(id);
    };
  }, []);

  // Poll authority telemetry rows for entity overlay
  useEffect(() => {
    let alive = true;
    async function poll() {
      try {
        const res = await fetch('/api/authoritytelemetry', { cache: 'no-store' });
        if (!res.ok) return;
        const raw = await res.json();
        if (!alive) return;
        setAuthorityEntities(parseAuthorityRows(raw));
      } catch {
        if (!alive) return;
        setAuthorityEntities([]);
      }
    }
    poll();
    const id = setInterval(poll, 500);
    return () => {
      alive = false;
      clearInterval(id);
    };
  }, []);

  const ownerPositions = useMemo(() => {
    const acc = new Map<string, { sumX: number; sumY: number; count: number }>();
    for (const entity of authorityEntities) {
      const current = acc.get(entity.ownerId) ?? { sumX: 0, sumY: 0, count: 0 };
      current.sumX += entity.x;
      current.sumY += entity.y;
      current.count += 1;
      acc.set(entity.ownerId, current);
    }
    const out = new Map<string, { x: number; y: number }>();
    for (const [ownerId, value] of acc) {
      out.set(ownerId, {
        x: value.sumX / Math.max(1, value.count),
        y: value.sumY / Math.max(1, value.count),
      });
    }
    return out;
  }, [authorityEntities]);

  const shardAnchorPositions = useMemo(() => {
    const out = new Map<string, { x: number; y: number }>();
    for (const shape of baseShapes) {
      if (!shape.id || shape.id.length === 0) continue;
      out.set(normalizeShardId(shape.id), {
        x: shape.position.x ?? 0,
        y: shape.position.y ?? 0,
      });
    }
    return out;
  }, [baseShapes]);

  const overlayShapes = useMemo<ShapeJS[]>(() => {
    const overlays: ShapeJS[] = [];

    if (showAuthorityEntities) {
      for (const entity of authorityEntities) {
        // Entity dot
        overlays.push({
          type: 'circle',
          position: { x: entity.x, y: entity.y },
          radius: 1.8,
          color: 'rgba(255, 240, 80, 1)',
        });

        // Owner link (entity -> owner centroid)
        const ownerPos =
          ownerPositions.get(normalizeShardId(entity.ownerId)) ??
          shardAnchorPositions.get(normalizeShardId(entity.ownerId));
        if (ownerPos) {
          overlays.push({
            type: 'line',
            position: { x: 0, y: 0 },
            points: [
              { x: entity.x, y: entity.y },
              { x: ownerPos.x, y: ownerPos.y },
            ],
            color: 'rgba(255, 200, 100, 0.55)',
          });
        }
      }
    }

    if (showGnsConnections) {
      const seen = new Set<string>();
      for (const shard of networkTelemetry) {
        const fromId = normalizeShardId(shard.shardId);
        const fromPos = ownerPositions.get(fromId) ?? shardAnchorPositions.get(fromId);
        if (!fromPos) continue;
        for (const connection of shard.connections) {
          const toId = normalizeShardId(connection.targetId);
          const toPos = ownerPositions.get(toId) ?? shardAnchorPositions.get(toId);
          if (!toPos) continue;

          // Undirected dedupe for display clarity
          const key =
            fromId < toId ? `${fromId}|${toId}` : `${toId}|${fromId}`;
          if (seen.has(key)) continue;
          seen.add(key);

          overlays.push({
            type: 'line',
            position: { x: 0, y: 0 },
            points: [
              { x: fromPos.x, y: fromPos.y },
              { x: toPos.x, y: toPos.y },
            ],
            color: 'rgba(80, 200, 255, 0.65)',
          });
        }
      }

      // Draw shard anchor dots so toggle visibly affects the map.
      for (const [shardId, anchor] of shardAnchorPositions) {
        const hasOwnerSample = ownerPositions.has(shardId);
        overlays.push({
          type: 'circle',
          position: anchor,
          radius: hasOwnerSample ? 2.6 : 2.1,
          color: hasOwnerSample
            ? 'rgba(80, 200, 255, 0.95)'
            : 'rgba(120, 170, 220, 0.75)',
        });
      }
    }

    return overlays;
  }, [
    authorityEntities,
    networkTelemetry,
    ownerPositions,
    shardAnchorPositions,
    showAuthorityEntities,
    showGnsConnections,
  ]);

  const combinedShapes = useMemo(
    () => [...baseShapes, ...overlayShapes],
    [baseShapes, overlayShapes]
  );

  // Init renderer once
  useEffect(() => {
    const container = containerRef.current;
    if (!container || rendererRef.current) {
      return;
    }
    rendererRef.current = createMapRenderer({ container, shapes: [] });
    return () => {
      rendererRef.current = null;
      container.innerHTML = '';
    };
  }, []);

  // Push shape updates to renderer
  useEffect(() => {
    rendererRef.current?.setShapes(combinedShapes);
  }, [combinedShapes]);

  return (
    <div style={{ width: '100%', height: '100%', position: 'relative' }}>
      <div
        style={{
          position: 'absolute',
          top: 12,
          left: 12,
          zIndex: 10,
          background: 'rgba(15, 23, 42, 0.82)',
          color: '#e2e8f0',
          border: '1px solid rgba(148, 163, 184, 0.45)',
          borderRadius: 10,
          padding: '8px 10px',
          display: 'flex',
          gap: 14,
          alignItems: 'center',
          fontSize: 13,
          backdropFilter: 'blur(4px)',
        }}
      >
        <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
          <input
            type="checkbox"
            checked={showGnsConnections}
            onChange={() => setShowGnsConnections(!showGnsConnections)}
          />
          GNS connections
        </label>
        <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
          <input
            type="checkbox"
            checked={showAuthorityEntities}
            onChange={() => setShowAuthorityEntities(!showAuthorityEntities)}
          />
          entities + owner links
        </label>
        <span style={{ opacity: 0.8 }}>
          entities: {authorityEntities.length} | shards: {shardAnchorPositions.size}
        </span>
      </div>

      <div
        ref={containerRef}
        style={{
          width: '100%',
          height: '100%',
          overflow: 'hidden',
          touchAction: 'none',
          border: '1px solid #ccc',
        }}
      />
    </div>
  );
}
