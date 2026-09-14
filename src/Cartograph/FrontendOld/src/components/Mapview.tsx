import { useEffect, useRef, useState } from "react";
import * as THREE from "three";

const STRESS_POINT_COUNT = 100_000;

export default function Mapview() {
  const mountRef = useRef<HTMLDivElement>(null);
  const entityStreamWS = useRef<WebSocket | null>(null);

  // ---------------- UI STATE ----------------
  const [mode, setMode] = useState<"2d" | "3d">("3d");
  const [handedness, setHandedness] = useState<"lhs" | "rhs">("rhs");
  const [upAxis, setUpAxis] = useState<"y" | "z">("y");
  const [zoom, setZoom] = useState(1);

  // ---------------- THREE REFS ----------------
  const cameraRef = useRef<THREE.OrthographicCamera | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);

  const pointsRef = useRef<THREE.Points | null>(null);
  const positionsRef = useRef<Float32Array | null>(null);

  const entityIndexMapRef = useRef<Map<string, number>>(new Map());
  const nextIndexRef = useRef(0);

  useEffect(() => {
    const mount = mountRef.current;
    if (!mount) return;

    // ---------------- RENDERER ----------------
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(window.devicePixelRatio);

    renderer.setSize(mount.clientWidth, mount.clientHeight);
    renderer.setClearColor(0x0f172a);

    renderer.domElement.style.display = "block";
    renderer.domElement.style.pointerEvents = "auto";
    renderer.domElement.style.touchAction = "none";

    mount.appendChild(renderer.domElement);
    rendererRef.current = renderer;

    // ---------------- SCENE ----------------
    const scene = new THREE.Scene();
    sceneRef.current = scene;

    // ---------------- CAMERA ----------------
    const frustum = 500;
    const aspect = mount.clientWidth / mount.clientHeight;

    const camera = new THREE.OrthographicCamera(
      -frustum * aspect,
      frustum * aspect,
      frustum,
      -frustum,
      0.1,
      5000
    );

    camera.position.set(600, 500, 600);
    camera.lookAt(0, 0, 0);
    cameraRef.current = camera;

    // ---------------- GRID ----------------
    scene.add(new THREE.GridHelper(2000, 40, 0x444444, 0x222222));

    // ---------------- POINT CLOUD ----------------
    const positions = new Float32Array(STRESS_POINT_COUNT * 3);

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute(
      "position",
      new THREE.BufferAttribute(positions, 3)
    );

    // ---------------- TRIANGLE SPRITE MATERIAL (ONLY CHANGE) ----------------
    const canvas = document.createElement("canvas");
    canvas.width = 64;
    canvas.height = 64;

    const ctx = canvas.getContext("2d")!;
    ctx.fillStyle = "#38bdf8";
    ctx.beginPath();
    ctx.moveTo(32, 8);
    ctx.lineTo(56, 56);
    ctx.lineTo(8, 56);
    ctx.closePath();
    ctx.fill();

    const texture = new THREE.CanvasTexture(canvas);

    const material = new THREE.PointsMaterial({
      size: 30,
      map: texture,
      transparent: true,
      alphaTest: 0.5,
      depthWrite: false,
    });

    const points = new THREE.Points(geometry, material);
    scene.add(points);

    pointsRef.current = points;
    positionsRef.current = positions;

    // ---------------- WEBSOCKET ----------------
    entityStreamWS.current = new WebSocket(
      `ws://${window.location.host}/api/entity-stream`
    );

    entityStreamWS.current.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);

        const posArray = positionsRef.current;
        const geo = pointsRef.current?.geometry as THREE.BufferGeometry;
        if (!posArray || !geo) return;

        let maxIndex = 0;

        for (const [entityId, payload] of Object.entries<any>(data)) {
          let index = entityIndexMapRef.current.get(entityId);

          if (index === undefined) {
            index = nextIndexRef.current++;
            entityIndexMapRef.current.set(entityId, index);
          }

          const pos =
            payload?.baseInfo?.location?.position?.position;

          const i = index * 3;

          posArray[i] = pos?.x ?? 0;
          posArray[i + 1] = pos?.y ?? 0;
          posArray[i + 2] = pos?.z ?? 0;

          maxIndex = Math.max(maxIndex, index);
        }

        geo.setDrawRange(0, maxIndex + 1);
        geo.attributes.position.needsUpdate = true;
      } catch (e) {
        console.error("WS update failed:", e);
      }
    };

    // ---------------- INPUT ----------------
    let dragging = false;
    let lastX = 0;
    let lastY = 0;

    const canvasEl = renderer.domElement;

    function onMouseDown(e: MouseEvent) {
      dragging = true;
      lastX = e.clientX;
      lastY = e.clientY;
    }

    function onMouseUp() {
      dragging = false;
    }

    function onMouseMove(e: MouseEvent) {
      if (!dragging || !cameraRef.current) return;

      const camera = cameraRef.current;

      const dx = e.clientX - lastX;
      const dy = e.clientY - lastY;

      lastX = e.clientX;
      lastY = e.clientY;

      const scale = (camera.right - camera.left) / mount.clientWidth;

      const right = new THREE.Vector3();
      const up = new THREE.Vector3();

      camera.matrixWorld.extractBasis(right, up, new THREE.Vector3());

      right.multiplyScalar(-dx * scale);
      up.multiplyScalar(dy * scale);

      camera.position.add(right);
      camera.position.add(up);
    }

    function onWheel(e: WheelEvent) {
      if (!cameraRef.current) return;
      e.preventDefault();

      cameraRef.current.zoom *= e.deltaY > 0 ? 0.9 : 1.1;
      cameraRef.current.zoom = Math.max(
        0.2,
        Math.min(cameraRef.current.zoom, 10)
      );
      cameraRef.current.updateProjectionMatrix();
    }

    canvasEl.addEventListener("mousedown", onMouseDown);
    window.addEventListener("mouseup", onMouseUp);
    window.addEventListener("mousemove", onMouseMove);
    canvasEl.addEventListener("wheel", onWheel, { passive: false });

    // ---------------- RESIZE ----------------
    function onResize() {
      if (!cameraRef.current || !rendererRef.current || !mount) return;

      const aspect = mount.clientWidth / mount.clientHeight;

      cameraRef.current.left = -frustum * aspect;
      cameraRef.current.right = frustum * aspect;
      cameraRef.current.top = frustum;
      cameraRef.current.bottom = -frustum;

      cameraRef.current.updateProjectionMatrix();
      rendererRef.current.setSize(
        mount.clientWidth,
        mount.clientHeight
      );
    }

    window.addEventListener("resize", onResize);

    // ---------------- LOOP ----------------
    let frame = 0;

    const animate = () => {
      frame = requestAnimationFrame(animate);
      renderer.render(scene, camera);
    };

    animate();

    // ---------------- CLEANUP ----------------
    return () => {
      cancelAnimationFrame(frame);

      window.removeEventListener("resize", onResize);
      window.removeEventListener("mouseup", onMouseUp);
      window.removeEventListener("mousemove", onMouseMove);

      canvasEl.removeEventListener("mousedown", onMouseDown);
      canvasEl.removeEventListener("wheel", onWheel);

      entityStreamWS.current?.close();

      renderer.dispose();
      mount.removeChild(renderer.domElement);
    };
  }, []);

  // ---------------- UI EFFECTS ----------------
  useEffect(() => {
    const camera = cameraRef.current;
    if (!camera) return;

    if (mode === "2d") {
      camera.position.set(0, 800, 0);
      camera.lookAt(0, 0, 0);
      camera.zoom = zoom;
    } else {
      camera.position.set(600, 500, 600);
      camera.lookAt(0, 0, 0);
    }

    camera.updateProjectionMatrix();
  }, [mode, zoom]);

  useEffect(() => {
    const scene = sceneRef.current;
    if (!scene) return;
    scene.scale.x = handedness === "lhs" ? -1 : 1;
  }, [handedness]);

  useEffect(() => {
    const scene = sceneRef.current;
    if (!scene) return;
    scene.rotation.x = upAxis === "z" ? -Math.PI / 2 : 0;
  }, [upAxis]);

  return (
    <div
      style={{
        height: "100%",
        width: "100%",
        display: "flex",
        flexDirection: "column",
      }}
    >
      {/* BUTTONS */}
      <div style={{ display: "flex", gap: 8, padding: 8, flexWrap: "wrap" }}>
        <div className="btn-group">
          <button
            className={`btn btn-sm ${mode === "2d" ? "btn-primary" : "btn-outline-primary"
              }`}
            onClick={() => setMode("2d")}
          >
            2D
          </button>
          <button
            className={`btn btn-sm ${mode === "3d" ? "btn-primary" : "btn-outline-primary"
              }`}
            onClick={() => setMode("3d")}
          >
            3D
          </button>
        </div>

        <div className="btn-group">
          <button
            className={`btn btn-sm ${handedness === "lhs"
                ? "btn-primary"
                : "btn-outline-primary"
              }`}
            onClick={() => setHandedness("lhs")}
          >
            LHS
          </button>
          <button
            className={`btn btn-sm ${handedness === "rhs"
                ? "btn-primary"
                : "btn-outline-primary"
              }`}
            onClick={() => setHandedness("rhs")}
          >
            RHS
          </button>
        </div>

        <div className="btn-group">
          <button
            className={`btn btn-sm ${upAxis === "y" ? "btn-primary" : "btn-outline-primary"
              }`}
            onClick={() => setUpAxis("y")}
          >
            Y-Up
          </button>
          <button
            className={`btn btn-sm ${upAxis === "z" ? "btn-primary" : "btn-outline-primary"
              }`}
            onClick={() => setUpAxis("z")}
          >
            Z-Up
          </button>
        </div>
      </div>

      {/* MAP */}
      <div
        ref={mountRef}
        style={{
          flex: 1,
          width: "100%",
          minHeight: 0,
          cursor: "grab",
        }}
      />
    </div>
  );
}