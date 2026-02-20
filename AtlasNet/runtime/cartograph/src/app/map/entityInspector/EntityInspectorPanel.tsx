'use client';

import { useEffect, useRef, useState } from 'react';
import type { MouseEvent as ReactMouseEvent } from 'react';
import type { RefObject } from 'react';
import type { AuthorityEntityTelemetry } from '../../lib/cartographTypes';
import { DatabaseExplorerInspector } from '../../database/components/DatabaseExplorerInspector';
import type { MapProjectionMode, MapViewMode } from '../../lib/mapRenderer';
import { useEntityDatabaseDetails } from './useEntityDatabaseDetails';

const ENTITY_INSPECTOR_DESKTOP_GRID_COLUMNS_CLASS =
  'lg:grid-cols-[minmax(0,1fr)_minmax(0,2fr)]';
const PANEL_MIN_WIDTH_PX = 520;
const FOLLOW_SELECTED_ENTITIES_STORAGE_KEY =
  'cartograph.map.entityInspector.followSelectedEntities';
const FOLLOW_PADDING = 1.8;
const FOLLOW_ZOOM_DAMPEN = 1;
const FOLLOW_MIN_WORLD_EXTENT = 8;
const FOLLOW_MIN_VISIBLE_WIDTH_PX = 180;
const FOLLOW_MIN_VISIBLE_HEIGHT_PX = 120;
const FOLLOW_BBOX_PADDING = 1.25;
const FOLLOW_MIN_SCREEN_MARGIN_PX = 180;
const PANEL_WIDTH_FALLBACK_RATIO = 0.5;
const FOLLOW_LERP_ALPHA = 0.05;
const FOLLOW_BOUNDS_LERP_ALPHA = 0.1;
const RESTORE_LERP_ALPHA = 0.12;
const CAMERA_EPSILON = 0.001;
const FOLLOW_MIN_SCALE_2D = 0.05;
const FOLLOW_MAX_SCALE_2D = 20;
const FOLLOW_MIN_DISTANCE_3D = 5;
const FOLLOW_MAX_DISTANCE_3D = 20000;
const FOLLOW_MIN_ORTHO_HEIGHT_3D = 1;
const FOLLOW_MAX_ORTHO_HEIGHT_3D = 20000;
const FOLLOW_MIN_PROJECTED_EXTENT_PX = 2;

interface CameraTransform2D {
  scale: number;
  offsetX: number;
  offsetY: number;
}

interface CameraTransform3D {
  targetX: number;
  targetY: number;
  targetZ: number;
  distance: number;
  yaw: number;
  pitch: number;
  roll: number;
  orthoHeight: number;
  projectionMode: MapProjectionMode;
}

interface FollowBoundsState {
  centerX: number;
  centerY: number;
  width: number;
  height: number;
}

interface MapRendererFollowController {
  setScale: (value: number) => void;
  setOffset: (x: number, y: number) => void;
  getViewState2D: () => CameraTransform2D;
  getViewState3D: () => CameraTransform3D;
  setViewState3D: (nextViewState: Partial<CameraTransform3D>) => void;
  projectMapPoint: (point: { x: number; y: number; z?: number }) => {
    x: number;
    y: number;
  } | null;
}

interface EntityInspectorPanelProps {
  containerRef: RefObject<HTMLDivElement | null>;
  rendererRef: RefObject<MapRendererFollowController | null>;
  viewMode: MapViewMode;
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

function lerp(from: number, to: number, alpha: number): number {
  return from + (to - from) * alpha;
}

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

function shortestAngleDelta(from: number, to: number): number {
  let delta = to - from;
  while (delta > Math.PI) {
    delta -= Math.PI * 2;
  }
  while (delta < -Math.PI) {
    delta += Math.PI * 2;
  }
  return delta;
}

function lerpAngle(from: number, to: number, alpha: number): number {
  return from + shortestAngleDelta(from, to) * alpha;
}

function getBoundsForEntities(
  selectedEntities: AuthorityEntityTelemetry[]
): {
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
} | null {
  let minX = Infinity;
  let maxX = -Infinity;
  let minY = Infinity;
  let maxY = -Infinity;

  for (const entity of selectedEntities) {
    if (!Number.isFinite(entity.x) || !Number.isFinite(entity.y)) {
      continue;
    }
    minX = Math.min(minX, entity.x);
    maxX = Math.max(maxX, entity.x);
    minY = Math.min(minY, entity.y);
    maxY = Math.max(maxY, entity.y);
  }

  if (
    !Number.isFinite(minX) ||
    !Number.isFinite(maxX) ||
    !Number.isFinite(minY) ||
    !Number.isFinite(maxY)
  ) {
    return null;
  }

  return { minX, maxX, minY, maxY };
}

export function EntityInspectorPanel({
  containerRef,
  rendererRef,
  viewMode,
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
  const resizeSessionRef = useRef<{
    startX: number;
    startWidth: number;
  } | null>(null);
  const preFollowCamera2DRef = useRef<CameraTransform2D | null>(null);
  const preFollowCamera3DRef = useRef<CameraTransform3D | null>(null);
  const followLockedRotation3DRef = useRef<{
    yaw: number;
    pitch: number;
    roll: number;
  } | null>(null);
  const smoothedFollowBoundsRef = useRef<FollowBoundsState | null>(null);
  const wasFollowActiveRef = useRef(false);
  const [panelWidthPx, setPanelWidthPx] = useState<number | null>(null);
  const [followSelectedEntities, setFollowSelectedEntities] = useState(false);

  function getMaxPanelWidthPx(): number {
    const parent = panelRef.current?.parentElement;
    if (!parent) {
      return Number.POSITIVE_INFINITY;
    }
    return Math.max(PANEL_MIN_WIDTH_PX, parent.clientWidth - 24);
  }

  useEffect(() => {
    try {
      const persistedValue = window.localStorage.getItem(
        FOLLOW_SELECTED_ENTITIES_STORAGE_KEY
      );
      if (persistedValue === '1' || persistedValue === 'true') {
        setFollowSelectedEntities(true);
      }
    } catch {}
  }, []);

  useEffect(() => {
    try {
      window.localStorage.setItem(
        FOLLOW_SELECTED_ENTITIES_STORAGE_KEY,
        followSelectedEntities ? '1' : '0'
      );
    } catch {}
  }, [followSelectedEntities]);

  useEffect(() => {
    const renderer = rendererRef.current;
    const container = containerRef.current;
    if (!renderer || !container) {
      return;
    }

    const followActive = followSelectedEntities && selectedEntities.length > 0;
    if (followActive && !wasFollowActiveRef.current) {
      preFollowCamera2DRef.current = null;
      preFollowCamera3DRef.current = null;
      followLockedRotation3DRef.current = null;
      smoothedFollowBoundsRef.current = null;
    }
    wasFollowActiveRef.current = followActive;

    if (followActive) {
      if (viewMode === '2d' && !preFollowCamera2DRef.current) {
        preFollowCamera2DRef.current = renderer.getViewState2D();
        smoothedFollowBoundsRef.current = null;
      }
      if (viewMode === '3d' && !preFollowCamera3DRef.current) {
        const current3D = renderer.getViewState3D();
        preFollowCamera3DRef.current = current3D;
        followLockedRotation3DRef.current = {
          yaw: current3D.yaw,
          pitch: current3D.pitch,
          roll: current3D.roll,
        };
        smoothedFollowBoundsRef.current = null;
      }
    }

    if (!followActive) {
      if (viewMode === '2d' && preFollowCamera2DRef.current != null) {
        const target = preFollowCamera2DRef.current;
        let rafId = 0;
        const tick = () => {
          const current = rendererRef.current;
          if (!current || viewMode !== '2d') {
            preFollowCamera2DRef.current = null;
            smoothedFollowBoundsRef.current = null;
            return;
          }

          const currentTransform = current.getViewState2D();
          const nextScale = lerp(currentTransform.scale, target.scale, RESTORE_LERP_ALPHA);
          const nextOffsetX = lerp(
            currentTransform.offsetX,
            target.offsetX,
            RESTORE_LERP_ALPHA
          );
          const nextOffsetY = lerp(
            currentTransform.offsetY,
            target.offsetY,
            RESTORE_LERP_ALPHA
          );

          current.setScale(nextScale);
          current.setOffset(nextOffsetX, nextOffsetY);

          const done =
            Math.abs(nextScale - target.scale) < CAMERA_EPSILON &&
            Math.abs(nextOffsetX - target.offsetX) < CAMERA_EPSILON &&
            Math.abs(nextOffsetY - target.offsetY) < CAMERA_EPSILON;

          if (done) {
            current.setScale(target.scale);
            current.setOffset(target.offsetX, target.offsetY);
            preFollowCamera2DRef.current = null;
            smoothedFollowBoundsRef.current = null;
            return;
          }

          rafId = window.requestAnimationFrame(tick);
        };

        rafId = window.requestAnimationFrame(tick);
        return () => {
          window.cancelAnimationFrame(rafId);
        };
      }

      if (viewMode === '3d' && preFollowCamera3DRef.current != null) {
        const target = preFollowCamera3DRef.current;
        let rafId = 0;
        const tick = () => {
          const current = rendererRef.current;
          if (!current || viewMode !== '3d') {
            preFollowCamera3DRef.current = null;
            followLockedRotation3DRef.current = null;
            smoothedFollowBoundsRef.current = null;
            return;
          }

          const currentTransform = current.getViewState3D();
          const nextTargetX = lerp(
            currentTransform.targetX,
            target.targetX,
            RESTORE_LERP_ALPHA
          );
          const nextTargetY = lerp(
            currentTransform.targetY,
            target.targetY,
            RESTORE_LERP_ALPHA
          );
          const nextTargetZ = lerp(
            currentTransform.targetZ,
            target.targetZ,
            RESTORE_LERP_ALPHA
          );
          const nextDistance = lerp(
            currentTransform.distance,
            target.distance,
            RESTORE_LERP_ALPHA
          );
          const nextOrthoHeight = lerp(
            currentTransform.orthoHeight,
            target.orthoHeight,
            RESTORE_LERP_ALPHA
          );
          const nextYaw = lerpAngle(
            currentTransform.yaw,
            target.yaw,
            RESTORE_LERP_ALPHA
          );
          const nextPitch = lerp(
            currentTransform.pitch,
            target.pitch,
            RESTORE_LERP_ALPHA
          );
          const nextRoll = lerpAngle(
            currentTransform.roll,
            target.roll,
            RESTORE_LERP_ALPHA
          );

          current.setViewState3D({
            projectionMode: target.projectionMode,
            targetX: nextTargetX,
            targetY: nextTargetY,
            targetZ: nextTargetZ,
            distance: nextDistance,
            orthoHeight: nextOrthoHeight,
            yaw: nextYaw,
            pitch: nextPitch,
            roll: nextRoll,
          });

          const done =
            Math.abs(nextTargetX - target.targetX) < CAMERA_EPSILON &&
            Math.abs(nextTargetY - target.targetY) < CAMERA_EPSILON &&
            Math.abs(nextTargetZ - target.targetZ) < CAMERA_EPSILON &&
            Math.abs(nextDistance - target.distance) < CAMERA_EPSILON &&
            Math.abs(nextOrthoHeight - target.orthoHeight) < CAMERA_EPSILON &&
            Math.abs(shortestAngleDelta(nextYaw, target.yaw)) < CAMERA_EPSILON &&
            Math.abs(nextPitch - target.pitch) < CAMERA_EPSILON &&
            Math.abs(shortestAngleDelta(nextRoll, target.roll)) < CAMERA_EPSILON;

          if (done) {
            current.setViewState3D({
              projectionMode: target.projectionMode,
              targetX: target.targetX,
              targetY: target.targetY,
              targetZ: target.targetZ,
              distance: target.distance,
              orthoHeight: target.orthoHeight,
              yaw: target.yaw,
              pitch: target.pitch,
              roll: target.roll,
            });
            preFollowCamera3DRef.current = null;
            followLockedRotation3DRef.current = null;
            smoothedFollowBoundsRef.current = null;
            return;
          }

          rafId = window.requestAnimationFrame(tick);
        };

        rafId = window.requestAnimationFrame(tick);
        return () => {
          window.cancelAnimationFrame(rafId);
        };
      }

      return;
    }

    if (!getBoundsForEntities(selectedEntities)) {
      return;
    }
    let rafId = 0;
    const tick = () => {
      const currentRenderer = rendererRef.current;
      const currentContainer = containerRef.current;
      if (
        !currentRenderer ||
        !currentContainer ||
        !followSelectedEntities ||
        selectedEntities.length === 0
      ) {
        return;
      }

      const latestBounds = getBoundsForEntities(selectedEntities);
      if (!latestBounds) {
        return;
      }

      const containerWidth = Math.max(1, currentContainer.clientWidth);
      const containerHeight = Math.max(1, currentContainer.clientHeight);
      const measuredPanelWidth = panelRef.current?.getBoundingClientRect().width;
      const panelWidth =
        typeof measuredPanelWidth === 'number' && Number.isFinite(measuredPanelWidth)
          ? measuredPanelWidth
          : panelWidthPx ?? containerWidth * PANEL_WIDTH_FALLBACK_RATIO;

      const worldWidth = Math.max(
        FOLLOW_MIN_WORLD_EXTENT,
        latestBounds.maxX - latestBounds.minX
      );
      const worldHeight = Math.max(
        FOLLOW_MIN_WORLD_EXTENT,
        latestBounds.maxY - latestBounds.minY
      );
      const rawCenterX = (latestBounds.minX + latestBounds.maxX) / 2;
      const rawCenterY = (latestBounds.minY + latestBounds.maxY) / 2;
      const previousSmoothedBounds = smoothedFollowBoundsRef.current;
      const smoothedBounds: FollowBoundsState = previousSmoothedBounds
        ? {
            centerX: lerp(
              previousSmoothedBounds.centerX,
              rawCenterX,
              FOLLOW_BOUNDS_LERP_ALPHA
            ),
            centerY: lerp(
              previousSmoothedBounds.centerY,
              rawCenterY,
              FOLLOW_BOUNDS_LERP_ALPHA
            ),
            width: lerp(
              previousSmoothedBounds.width,
              worldWidth,
              FOLLOW_BOUNDS_LERP_ALPHA
            ),
            height: lerp(
              previousSmoothedBounds.height,
              worldHeight,
              FOLLOW_BOUNDS_LERP_ALPHA
            ),
          }
        : {
            centerX: rawCenterX,
            centerY: rawCenterY,
            width: worldWidth,
            height: worldHeight,
          };
      smoothedFollowBoundsRef.current = smoothedBounds;

      const obstructionWidth = clamp(
        panelWidth,
        0,
        Math.max(0, containerWidth - FOLLOW_MIN_VISIBLE_WIDTH_PX)
      );
      const followScreenMarginPx =
        activeEntityId != null ? 0 : FOLLOW_MIN_SCREEN_MARGIN_PX;
      const safeLeft = followScreenMarginPx;
      const safeRight = Math.max(
        safeLeft + FOLLOW_MIN_VISIBLE_WIDTH_PX,
        containerWidth - obstructionWidth - followScreenMarginPx
      );
      const safeTop = followScreenMarginPx;
      const safeBottom = Math.max(
        safeTop + FOLLOW_MIN_VISIBLE_HEIGHT_PX,
        containerHeight - followScreenMarginPx
      );
      const usableWidth = Math.max(
        FOLLOW_MIN_VISIBLE_WIDTH_PX,
        safeRight - safeLeft
      );
      const usableHeight = Math.max(
        FOLLOW_MIN_VISIBLE_HEIGHT_PX,
        safeBottom - safeTop
      );

      const paddedWorldWidth =
        smoothedBounds.width * FOLLOW_PADDING * FOLLOW_BBOX_PADDING;
      const paddedWorldHeight =
        smoothedBounds.height * FOLLOW_PADDING * FOLLOW_BBOX_PADDING;
      const scaleX = usableWidth / paddedWorldWidth;
      const scaleY = usableHeight / paddedWorldHeight;
      const targetScale = clamp(
        Math.min(scaleX, scaleY) * FOLLOW_ZOOM_DAMPEN,
        FOLLOW_MIN_SCALE_2D,
        FOLLOW_MAX_SCALE_2D
      );

      const centerX = smoothedBounds.centerX;
      const centerY = smoothedBounds.centerY;
      const desiredScreenX = safeLeft + usableWidth / 2;
      const desiredScreenY = safeTop + usableHeight / 2;

      if (viewMode === '2d') {
        const targetOffsetX = desiredScreenX - centerX * targetScale;
        const targetOffsetY = desiredScreenY - centerY * targetScale;

        const current = currentRenderer.getViewState2D();
        const nextScale = clamp(
          lerp(current.scale, targetScale, FOLLOW_LERP_ALPHA),
          FOLLOW_MIN_SCALE_2D,
          FOLLOW_MAX_SCALE_2D
        );
        const nextOffsetX = lerp(
          current.offsetX,
          targetOffsetX,
          FOLLOW_LERP_ALPHA
        );
        const nextOffsetY = lerp(
          current.offsetY,
          targetOffsetY,
          FOLLOW_LERP_ALPHA
        );

        currentRenderer.setScale(nextScale);
        currentRenderer.setOffset(nextOffsetX, nextOffsetY);
      } else if (viewMode === '3d') {
        const current3D = currentRenderer.getViewState3D();
        const projectedEntities = selectedEntities
          .map((entity) =>
            currentRenderer.projectMapPoint({
              x: entity.x,
              y: entity.y,
              z: entity.z,
            })
          )
          .filter(
            (point): point is { x: number; y: number } => point != null
          );
        if (projectedEntities.length === 0) {
          rafId = window.requestAnimationFrame(tick);
          return;
        }

        let minScreenX = Infinity;
        let maxScreenX = -Infinity;
        let minScreenY = Infinity;
        let maxScreenY = -Infinity;
        for (const point of projectedEntities) {
          minScreenX = Math.min(minScreenX, point.x);
          maxScreenX = Math.max(maxScreenX, point.x);
          minScreenY = Math.min(minScreenY, point.y);
          maxScreenY = Math.max(maxScreenY, point.y);
        }
        const projectedWidth = Math.max(
          FOLLOW_MIN_PROJECTED_EXTENT_PX,
          maxScreenX - minScreenX
        );
        const projectedHeight = Math.max(
          FOLLOW_MIN_PROJECTED_EXTENT_PX,
          maxScreenY - minScreenY
        );
        const projectedScreenCenterX = (minScreenX + maxScreenX) / 2;
        const projectedScreenCenterY = (minScreenY + maxScreenY) / 2;

        const targetProjectedWidth = Math.max(
          FOLLOW_MIN_PROJECTED_EXTENT_PX,
          usableWidth / (FOLLOW_PADDING * FOLLOW_BBOX_PADDING)
        );
        const targetProjectedHeight = Math.max(
          FOLLOW_MIN_PROJECTED_EXTENT_PX,
          usableHeight / (FOLLOW_PADDING * FOLLOW_BBOX_PADDING)
        );

        let zoomScaleFactor = Math.max(
          projectedWidth / targetProjectedWidth,
          projectedHeight / targetProjectedHeight
        );
        if (!Number.isFinite(zoomScaleFactor) || zoomScaleFactor <= 0) {
          zoomScaleFactor = 1;
        }

        const targetDistance = clamp(
          current3D.distance * zoomScaleFactor * FOLLOW_ZOOM_DAMPEN,
          FOLLOW_MIN_DISTANCE_3D,
          FOLLOW_MAX_DISTANCE_3D
        );
        const targetOrthoHeight = clamp(
          current3D.orthoHeight * zoomScaleFactor * FOLLOW_ZOOM_DAMPEN,
          FOLLOW_MIN_ORTHO_HEIGHT_3D,
          FOLLOW_MAX_ORTHO_HEIGHT_3D
        );

        const projectedCenter = currentRenderer.projectMapPoint({
          x: centerX,
          y: centerY,
        });
        if (!projectedCenter) {
          rafId = window.requestAnimationFrame(tick);
          return;
        }

        const projectedXUnit = currentRenderer.projectMapPoint({
          x: centerX + 1,
          y: centerY,
        });
        const projectedYUnit = currentRenderer.projectMapPoint({
          x: centerX,
          y: centerY + 1,
        });

        let targetCenterX = centerX;
        let targetCenterY = centerY;
        if (projectedXUnit && projectedYUnit) {
          const basisXX = projectedXUnit.x - projectedCenter.x;
          const basisXY = projectedXUnit.y - projectedCenter.y;
          const basisYX = projectedYUnit.x - projectedCenter.x;
          const basisYY = projectedYUnit.y - projectedCenter.y;
          const deltaScreenX = desiredScreenX - projectedScreenCenterX;
          const deltaScreenY = desiredScreenY - projectedScreenCenterY;
          const determinant = basisXX * basisYY - basisXY * basisYX;

          if (Math.abs(determinant) > 1e-5) {
            const worldOffsetX =
              (deltaScreenX * basisYY - deltaScreenY * basisYX) /
              determinant;
            const worldOffsetY =
              (basisXX * deltaScreenY - basisXY * deltaScreenX) /
              determinant;

            if (Number.isFinite(worldOffsetX) && Number.isFinite(worldOffsetY)) {
              targetCenterX += worldOffsetX;
              targetCenterY += worldOffsetY;
            }
          }
        }

        currentRenderer.setViewState3D({
          targetX: lerp(current3D.targetX, targetCenterX, FOLLOW_LERP_ALPHA),
          targetZ: lerp(current3D.targetZ, targetCenterY, FOLLOW_LERP_ALPHA),
          distance:
            current3D.projectionMode === 'perspective'
              ? lerp(current3D.distance, targetDistance, FOLLOW_LERP_ALPHA)
              : undefined,
          orthoHeight:
            current3D.projectionMode === 'orthographic'
              ? lerp(current3D.orthoHeight, targetOrthoHeight, FOLLOW_LERP_ALPHA)
              : undefined,
          yaw: followLockedRotation3DRef.current?.yaw ?? current3D.yaw,
          pitch: followLockedRotation3DRef.current?.pitch ?? current3D.pitch,
          roll: followLockedRotation3DRef.current?.roll ?? current3D.roll,
        });
      }

      rafId = window.requestAnimationFrame(tick);
    };

    rafId = window.requestAnimationFrame(tick);
    return () => {
      window.cancelAnimationFrame(rafId);
    };
  }, [
    activeEntityId,
    containerRef,
    followSelectedEntities,
    panelWidthPx,
    rendererRef,
    selectedEntities,
    viewMode,
  ]);

  useEffect(() => {
    const handleMouseMove = (event: MouseEvent) => {
      const session = resizeSessionRef.current;
      if (!session) {
        return;
      }

      const dragDelta = session.startX - event.clientX;
      const nextWidth = session.startWidth + dragDelta;
      const clampedWidth = Math.min(
        getMaxPanelWidthPx(),
        Math.max(PANEL_MIN_WIDTH_PX, nextWidth)
      );
      setPanelWidthPx(clampedWidth);
    };

    const handleMouseUp = () => {
      if (!resizeSessionRef.current) {
        return;
      }
      resizeSessionRef.current = null;
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    };

    const clampWidthToContainer = () => {
      setPanelWidthPx((previous) => {
        if (previous == null) {
          return previous;
        }
        return Math.min(
          getMaxPanelWidthPx(),
          Math.max(PANEL_MIN_WIDTH_PX, previous)
        );
      });
    };

    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
    window.addEventListener('resize', clampWidthToContainer);
    clampWidthToContainer();

    let observer: ResizeObserver | null = null;
    const parent = panelRef.current?.parentElement;
    if (parent && typeof ResizeObserver !== 'undefined') {
      observer = new ResizeObserver(clampWidthToContainer);
      observer.observe(parent);
    }

    return () => {
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
      window.removeEventListener('resize', clampWidthToContainer);
      observer?.disconnect();
    };
  }, []);

  function startResizeDrag(
    event: ReactMouseEvent<HTMLDivElement>
  ): void {
    if (event.button !== 0 || !panelRef.current) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    resizeSessionRef.current = {
      startX: event.clientX,
      startWidth: panelRef.current.getBoundingClientRect().width,
    };
    document.body.style.cursor = 'ew-resize';
    document.body.style.userSelect = 'none';
  }

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
        minWidth: PANEL_MIN_WIDTH_PX,
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
        zIndex: 30,
      }}
    >
      <div
        title="Drag to resize panel. Double-click to reset width."
        onMouseDown={startResizeDrag}
        onDoubleClick={() => setPanelWidthPx(null)}
        style={{
          position: 'absolute',
          left: 0,
          top: 0,
          bottom: 0,
          width: 10,
          cursor: 'ew-resize',
          zIndex: 10,
          background: 'rgba(148, 163, 184, 0.12)',
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

          <label
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: 6,
              fontSize: 11,
              color: '#cbd5e1',
              whiteSpace: 'nowrap',
            }}
            title="Continuously frame selected entities and keep them centered in visible map area."
          >
            <input
              type="checkbox"
              checked={followSelectedEntities}
              onChange={(event) => setFollowSelectedEntities(event.target.checked)}
            />
            camera follow
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
            Close
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
            overflow: 'hidden',
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
                fillHeight
                containerClassName="pb-2 flex-1 min-h-0 flex flex-col"
                controlsClassName="!mb-2"
                explorerViewportClassName="h-full min-h-0 overflow-auto p-2"
                inspectorViewportClassName="h-full min-h-0 overflow-auto p-3"
                desktopGridColumnsClassName={
                  ENTITY_INSPECTOR_DESKTOP_GRID_COLUMNS_CLASS
                }
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
