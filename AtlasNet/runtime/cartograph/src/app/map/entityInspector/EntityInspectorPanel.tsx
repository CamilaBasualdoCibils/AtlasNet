'use client';

import { useEffect, useRef, useState } from 'react';
import type { AuthorityEntityTelemetry } from '../../lib/cartographTypes';
import { DatabaseExplorerInspector } from '../../database/components/DatabaseExplorerInspector';
import { useEntityDatabaseDetails } from './useEntityDatabaseDetails';

interface EntityInspectorPanelProps {
  selectedEntities: AuthorityEntityTelemetry[];
  activeEntityId: string | null;
  hoveredEntityId: string | null;
  pollIntervalMs: number;
  minPollIntervalMs: number;
  maxPollIntervalMs: number;
  pollDisabledAtMs: number;
  onSetPollIntervalMs: (value: number) => void;
  onSelectEntity: (entityId: string) => void;
  onHoverEntity: (entityId: string | null) => void;
  onClearSelection: () => void;
}

function formatNumber(value: number, digits = 2): string {
  if (!Number.isFinite(value)) {
    return '0';
  }
  return value.toFixed(digits);
}

export function EntityInspectorPanel({
  activeEntityId,
  hoveredEntityId,
  maxPollIntervalMs,
  minPollIntervalMs,
  onSetPollIntervalMs,
  pollIntervalMs,
  pollDisabledAtMs,
  onClearSelection,
  onHoverEntity,
  onSelectEntity,
  selectedEntities,
}: EntityInspectorPanelProps) {
  const panelRef = useRef<HTMLDivElement | null>(null);
  const [maxPanelWidthPx, setMaxPanelWidthPx] = useState(0);
  const [panelWidthPx, setPanelWidthPx] = useState<number | null>(null);

  useEffect(() => {
    const panel = panelRef.current;
    const parent = panel?.parentElement;
    if (!parent) {
      return;
    }

    const updateMaxWidth = () => {
      setMaxPanelWidthPx(Math.max(520, parent.clientWidth - 24));
    };

    updateMaxWidth();

    const observer = new ResizeObserver(updateMaxWidth);
    observer.observe(parent);
    return () => observer.disconnect();
  }, []);

  useEffect(() => {
    if (panelWidthPx == null || maxPanelWidthPx <= 0) {
      return;
    }
    if (panelWidthPx > maxPanelWidthPx) {
      setPanelWidthPx(maxPanelWidthPx);
    }
  }, [maxPanelWidthPx, panelWidthPx]);

  useEffect(() => {
    if (!panelRef.current) {
      return;
    }

    const panel = panelRef.current;
    let dragStartX = 0;
    let dragStartWidth = 0;
    let activePointerId: number | null = null;

    const handlePointerMove = (event: PointerEvent) => {
      if (activePointerId == null || event.pointerId !== activePointerId) {
        return;
      }

      const dragDelta = dragStartX - event.clientX;
      const nextWidth = dragStartWidth + dragDelta;
      const clampedWidth = Math.min(maxPanelWidthPx, Math.max(520, nextWidth));
      setPanelWidthPx(clampedWidth);
    };

    const finishResize = (event: PointerEvent) => {
      if (activePointerId == null || event.pointerId !== activePointerId) {
        return;
      }
      activePointerId = null;
      try {
        panel.releasePointerCapture(event.pointerId);
      } catch {}
    };

    const handlePointerDown = (event: PointerEvent) => {
      const target = event.target as HTMLElement | null;
      if (!target?.dataset?.resizeHandle) {
        return;
      }
      event.preventDefault();
      event.stopPropagation();

      activePointerId = event.pointerId;
      dragStartX = event.clientX;
      dragStartWidth = panel.getBoundingClientRect().width;
      try {
        panel.setPointerCapture(event.pointerId);
      } catch {}
    };

    panel.addEventListener('pointerdown', handlePointerDown);
    panel.addEventListener('pointermove', handlePointerMove);
    panel.addEventListener('pointerup', finishResize);
    panel.addEventListener('pointercancel', finishResize);

    return () => {
      panel.removeEventListener('pointerdown', handlePointerDown);
      panel.removeEventListener('pointermove', handlePointerMove);
      panel.removeEventListener('pointerup', finishResize);
      panel.removeEventListener('pointercancel', finishResize);
    };
  }, [maxPanelWidthPx]);

  const activeEntity =
    selectedEntities.find((entity) => entity.entityId === activeEntityId) ?? null;
  const visibleEntities = activeEntity ? [activeEntity] : selectedEntities;
  const details = useEntityDatabaseDetails(activeEntity, pollIntervalMs);

  if (selectedEntities.length === 0) {
    return null;
  }

  return (
    <div
      ref={panelRef}
      style={{
        position: 'absolute',
        top: 12,
        right: 12,
        bottom: 12,
        width: panelWidthPx == null ? '50%' : `${panelWidthPx}px`,
        minWidth: 520,
        maxWidth: 'calc(100% - 24px)',
        display: 'flex',
        flexDirection: 'column',
        borderRadius: 10,
        border: '1px solid rgba(148, 163, 184, 0.35)',
        background: 'rgba(2, 6, 23, 0.9)',
        color: '#e2e8f0',
        backdropFilter: 'blur(4px)',
        pointerEvents: 'auto',
        overflow: 'hidden',
      }}
    >
      <div
        data-resize-handle="true"
        title="Drag to resize panel. Double-click to reset width."
        onDoubleClick={() => setPanelWidthPx(null)}
        style={{
          position: 'absolute',
          left: 0,
          top: 0,
          bottom: 0,
          width: 8,
          cursor: 'ew-resize',
          zIndex: 10,
          background: 'transparent',
        }}
      />

      <div
        style={{
          padding: '10px 12px',
          borderBottom: '1px solid rgba(148, 163, 184, 0.25)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: 12,
        }}
      >
        <div>
          <div style={{ fontSize: 12, opacity: 0.75 }}>Entity Inspector</div>
          <div style={{ fontSize: 14, fontWeight: 600 }}>
            {selectedEntities.length} selected
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
          <label
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: 8,
              fontSize: 11,
              color: '#cbd5e1',
              minWidth: 210,
            }}
          >
            detail poll:
            <input
              type="range"
              min={minPollIntervalMs}
              max={maxPollIntervalMs}
              step={250}
              value={pollIntervalMs}
              onChange={(event) =>
                onSetPollIntervalMs(
                  Math.max(
                    minPollIntervalMs,
                    Math.min(maxPollIntervalMs, Number(event.target.value))
                  )
                )
              }
            />
            {pollIntervalMs >= pollDisabledAtMs ? 'off' : `${pollIntervalMs}ms`}
          </label>

          <button
            type="button"
            onClick={onClearSelection}
            style={{
              border: '1px solid rgba(148, 163, 184, 0.4)',
              borderRadius: 6,
              background: 'rgba(15, 23, 42, 0.8)',
              color: '#e2e8f0',
              padding: '4px 8px',
              fontSize: 12,
              cursor: 'pointer',
            }}
          >
            Clear
          </button>
        </div>
      </div>

      <div
        style={{
          borderBottom: activeEntity
            ? '1px solid rgba(148, 163, 184, 0.2)'
            : 'none',
          flex: activeEntity ? '0 0 auto' : '1 1 auto',
          maxHeight: activeEntity ? 180 : undefined,
          minHeight: 0,
          overflow: 'auto',
          padding: '8px 10px',
          display: 'flex',
          flexDirection: 'column',
          gap: 6,
        }}
      >
        {visibleEntities.map((entity) => {
          const isActive = entity.entityId === activeEntityId;
          const isHovered = entity.entityId === hoveredEntityId;

          return (
            <button
              key={entity.entityId}
              type="button"
              onMouseEnter={() => onHoverEntity(entity.entityId)}
              onMouseLeave={() => onHoverEntity(null)}
              onClick={() => onSelectEntity(entity.entityId)}
              style={{
                textAlign: 'left',
                border: `1px solid ${
                  isActive
                    ? 'rgba(59, 130, 246, 0.95)'
                    : isHovered
                      ? 'rgba(96, 165, 250, 0.7)'
                      : 'rgba(148, 163, 184, 0.28)'
                }`,
                borderRadius: 8,
                background: isHovered
                  ? 'rgba(30, 64, 175, 0.4)'
                  : isActive
                    ? 'rgba(30, 58, 138, 0.55)'
                    : 'rgba(15, 23, 42, 0.7)',
                color: '#e2e8f0',
                padding: '6px 8px',
                cursor: 'pointer',
              }}
            >
              <div
                style={{
                  fontFamily:
                    'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace',
                  fontSize: 12,
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                }}
                title={entity.entityId}
              >
                {entity.entityId}
              </div>
              <div style={{ fontSize: 11, opacity: 0.82 }}>
                owner: {entity.ownerId} | client: {entity.clientId || '-'} | world:{' '}
                {entity.world}
              </div>
              <div style={{ fontSize: 11, opacity: 0.74 }}>
                pos: ({formatNumber(entity.x)}, {formatNumber(entity.y)},{' '}
                {formatNumber(entity.z)})
              </div>
            </button>
          );
        })}
      </div>

      <div
        style={{
          flex: activeEntity ? '1 1 auto' : '0 0 auto',
          minHeight: 0,
          maxHeight: activeEntity ? '1000px' : '0px',
          opacity: activeEntity ? 1 : 0,
          transform: activeEntity ? 'translateY(0px)' : 'translateY(18px)',
          transition:
            'max-height 220ms ease, opacity 180ms ease, transform 220ms ease',
          overflow: 'hidden',
        }}
      >
        <div
          style={{
            height: '100%',
            minHeight: 0,
            overflow: 'auto',
            padding: activeEntity ? 14 : 0,
            display: 'flex',
            flexDirection: 'column',
            gap: 10,
            pointerEvents: activeEntity ? 'auto' : 'none',
          }}
        >
          <div style={{ fontSize: 12, opacity: 0.82 }}>
            Click the highlighted entity row again to collapse details.
          </div>

          {activeEntity && details.loading && !details.data && (
            <div style={{ fontSize: 12, opacity: 0.85 }}>
              Loading database details...
            </div>
          )}

          {activeEntity && details.loading && details.data && (
            <div style={{ fontSize: 12, opacity: 0.75 }}>Refreshing details...</div>
          )}

          {activeEntity && details.error && !details.data && (
            <div
              style={{
                fontSize: 12,
                color: '#fecaca',
                border: '1px solid rgba(248, 113, 113, 0.35)',
                borderRadius: 8,
                background: 'rgba(127, 29, 29, 0.25)',
                padding: '6px 8px',
              }}
            >
              {details.error}
            </div>
          )}

          {activeEntity && details.data && (
            <>
              <div
                style={{
                  fontSize: 12,
                  opacity: 0.85,
                  display: 'flex',
                  flexWrap: 'wrap',
                  gap: 10,
                }}
              >
                <span>matches: {details.data.totalMatches}</span>
                <span>sources: {details.data.sourceSummaries.length}</span>
                {details.data.truncated && <span>truncated</span>}
              </div>

              <div style={{ fontSize: 11, opacity: 0.74 }}>
                search terms: {details.data.terms.join(', ')}
              </div>

              <DatabaseExplorerInspector
                records={details.data.records}
                loading={false}
                selectionScopeKey={activeEntity.entityId}
                containerClassName="pb-2"
                controlsClassName="!mb-2"
                explorerViewportClassName="max-h-[32vh] overflow-auto p-2"
                inspectorViewportClassName="max-h-[32vh] overflow-auto p-3"
              />
            </>
          )}

          {activeEntity && details.error && details.data && (
            <div
              style={{
                fontSize: 12,
                color: '#fecaca',
                border: '1px solid rgba(248, 113, 113, 0.35)',
                borderRadius: 8,
                background: 'rgba(127, 29, 29, 0.25)',
                padding: '6px 8px',
              }}
            >
              {details.error}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
