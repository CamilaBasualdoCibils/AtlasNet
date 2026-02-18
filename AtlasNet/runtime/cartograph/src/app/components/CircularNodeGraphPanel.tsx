'use client';

import { useEffect, useMemo, useState } from 'react';
import type { MouseEvent, WheelEvent } from 'react';
import type { ShardTelemetry } from '../lib/networkTelemetryTypes';
import { CircularNodeGraph } from './CircularNodeGraph';

type ConnectionLike =
  | {
      IdentityId: string;
      targetId: string;
      inBytesPerSec?: number;
      outBytesPerSec?: number;
    }
  | string[];

function getConnectionTarget(connection: ConnectionLike): string | null {
  if (Array.isArray(connection)) {
    return connection.length > 1 ? String(connection[1]) : null;
  }

  return connection.targetId ?? null;
}

function getConnectionRates(
  connection: ConnectionLike
): { inBytesPerSec: number; outBytesPerSec: number } | null {
  if (Array.isArray(connection)) {
    if (connection.length < 5) return null;
    const inBytesPerSec = Number(connection[3]);
    const outBytesPerSec = Number(connection[4]);
    if (!Number.isFinite(inBytesPerSec) || !Number.isFinite(outBytesPerSec)) {
      return null;
    }
    return { inBytesPerSec, outBytesPerSec };
  }

  if (
    typeof connection.inBytesPerSec === 'number' &&
    typeof connection.outBytesPerSec === 'number'
  ) {
    return {
      inBytesPerSec: connection.inBytesPerSec,
      outBytesPerSec: connection.outBytesPerSec,
    };
  }

  return null;
}

export function CircularNodeGraphPanel({
  telemetry,
}: {
  telemetry: ShardTelemetry[];
}) {
  const [isExpanded, setIsExpanded] = useState(false);
  const [hoveredNodeId, setHoveredNodeId] = useState<string | null>(null);
  const [selectedNodeId, setSelectedNodeId] = useState<string | null>(null);
  const [resetViewToken, setResetViewToken] = useState(0);
  const shardCount = telemetry.length;
  const connectionCount = telemetry.reduce(
    (sum, shard) => sum + shard.connections.length,
    0
  );

  const { nodeIds, edges } = useMemo(() => {
    const nodeIdsLocal: string[] = telemetry.map((shard) => shard.shardId);
    const nodePositions = new Set(nodeIdsLocal);
    const edgeMap = new Map<string, {
      from: string;
      to: string;
      inBytesPerSec: number;
      outBytesPerSec: number;
    }>();

    for (const shard of telemetry) {
      for (const rawConnection of shard.connections as ConnectionLike[]) {
        const targetId = getConnectionTarget(rawConnection);
        if (!targetId) continue;
        if (!nodePositions.has(targetId)) continue;

        const sourceId = shard.shardId;
        if (sourceId === targetId) continue;

        const rates = getConnectionRates(rawConnection);
        const edgeKey = `${sourceId}->${targetId}`;
        const existing = edgeMap.get(edgeKey);
        const inRate = rates?.inBytesPerSec ?? 0;
        const outRate = rates?.outBytesPerSec ?? 0;
        if (existing) {
          // Merge duplicate directed edges from telemetry rows.
          existing.inBytesPerSec += inRate;
          existing.outBytesPerSec += outRate;
        } else {
          edgeMap.set(edgeKey, {
            from: sourceId,
            to: targetId,
            inBytesPerSec: inRate,
            outBytesPerSec: outRate,
          });
        }
      }
    }

    return { nodeIds: nodeIdsLocal, edges: Array.from(edgeMap.values()) };
  }, [telemetry]);

  const focusedNodeId = selectedNodeId ?? hoveredNodeId;
  const nodeStats = useMemo(() => {
    const stats = new Map<
      string,
      { downloadKbps: number; uploadKbps: number; connections: number }
    >();
    for (const shard of telemetry) {
      stats.set(shard.shardId, {
        downloadKbps: shard.downloadKbps,
        uploadKbps: shard.uploadKbps,
        connections: shard.connections.length,
      });
    }
    return stats;
  }, [telemetry]);

  function buildNodes(width: number, height: number) {
    const centerX = width / 2;
    const centerY = height / 2;
    const nodeCount = nodeIds.length;
    const baseRadius = Math.min(width, height) * 0.35;
    const radius = baseRadius + Math.max(0, nodeCount - 8) * 8;

    return nodeIds.map((id: string, index: number) => {
      const angle = (Math.PI * 2 * index) / Math.max(1, nodeCount);
      const x = centerX + Math.cos(angle) * radius;
      const y = centerY + Math.sin(angle) * radius;
      return { id, x, y };
    });
  }

  useEffect(() => {
    if (!isExpanded) return;

    function onKeyDown(event: KeyboardEvent) {
      if (event.key === 'Escape') {
        setIsExpanded(false);
      }
    }

    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [isExpanded]);

  return (
    <section className="rounded-2xl bg-slate-900/60 border border-slate-800 p-4 space-y-2">
      <div className="flex items-center justify-between">
        <div>
          <h3 className="text-sm font-medium text-slate-200">
            Circular Node Graph
          </h3>
          <p className="text-xs text-slate-400">
            Stub panel. Rendering will be wired in later.
          </p>
        </div>
        <div className="text-xs text-slate-400 font-mono">
          {shardCount} shards • {connectionCount} connections
        </div>
      </div>

      {/* Preview: clickable to expand */}
      <button
        type="button"
        onClick={() => setIsExpanded(true)}
        className="w-full rounded-xl border border-slate-800 bg-slate-950/40 p-2"
        onWheel={(event: WheelEvent) => {
          event.preventDefault();
        }}
      >
        <div className="flex items-center justify-center">
          {/* Keep preview centered but allow full width */}
          <div className="h-64 w-full">
            <CircularNodeGraph
              width={520}
              height={320}
              nodes={buildNodes(520, 320)}
              edges={edges}
              focusedNodeId={focusedNodeId}
              selectedNodeId={selectedNodeId}
              onNodeHover={setHoveredNodeId}
              onNodeSelect={(nodeId) =>
                setSelectedNodeId((prev) => (prev === nodeId ? null : nodeId))
              }
              nodeStats={nodeStats}
              resetViewToken={resetViewToken}
            />
          </div>
        </div>
        {nodeIds.length === 0 && (
          <div className="py-8 text-center text-xs text-slate-500">
            Waiting for telemetry to display nodes.
          </div>
        )}
      </button>

      {isExpanded && (
        <div
          className="fixed inset-0 z-50 bg-black/40"
          onClick={() => setIsExpanded(false)}
        >
          <aside
            className="absolute right-0 top-0 h-full w-[85%] bg-slate-950 border-l border-slate-800 overflow-y-auto"
            onClick={(event: MouseEvent) => event.stopPropagation()}
            onWheel={(event: WheelEvent) => {
              event.preventDefault();
            }}
          >
            {/* Header stays padded, graph fills remaining space */}
            <div className="flex h-full flex-col gap-4 p-6">
              <div className="flex items-center justify-between">
                <div>
                  <h2 className="text-xl font-semibold text-slate-100">
                    Circular Node Graph
                  </h2>
                  <p className="text-sm text-slate-400">
                    Inter-shard connections at a glance
                  </p>
                </div>
                <div className="flex items-center gap-2">
                  <button
                    type="button"
                    onClick={() => {
                      setHoveredNodeId(null);
                      setSelectedNodeId(null);
                      setResetViewToken((token: number) => token + 1);
                    }}
                    className="rounded-xl border border-slate-700 px-3 py-1 text-sm text-slate-300 hover:bg-slate-800"
                  >
                    Reset view
                  </button>
                  <button
                    type="button"
                    onClick={() => setIsExpanded(false)}
                    className="rounded-xl border border-slate-700 px-3 py-1 text-sm text-slate-300 hover:bg-slate-800"
                  >
                    Close
                  </button>
                </div>
              </div>

              {/* Graph area: fill all remaining space */}
              <div className="flex-1 rounded-2xl border border-slate-800 bg-slate-900/60 p-2">
                <div className="h-full w-full">
                  <CircularNodeGraph
                    width={1200}
                    height={720}
                    nodes={buildNodes(1200, 720)}
                    edges={edges}
                    focusedNodeId={focusedNodeId}
                    selectedNodeId={selectedNodeId}
                    onNodeHover={setHoveredNodeId}
                    onNodeSelect={(nodeId) =>
                      setSelectedNodeId((prev) =>
                        prev === nodeId ? null : nodeId
                      )
                    }
                    nodeStats={nodeStats}
                    resetViewToken={resetViewToken}
                  />
                </div>
              </div>
            </div>
          </aside>
        </div>
      )}
    </section>
  );
}
