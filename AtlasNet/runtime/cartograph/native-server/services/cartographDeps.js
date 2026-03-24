const Redis = require('ioredis');
const {
  getDatabaseTargets,
  createRedisConnectionOptions,
  PROBE_CONNECT_TIMEOUT_MS,
} = require('../config');

function internalRedisTarget() {
  const targets = getDatabaseTargets();
  const internal = targets.find((t) => t.id === 'internal');
  return internal || targets[0] || null;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function readyDeadlineMs() {
  const n = Number(process.env.CARTOGRAPH_DEPS_READY_MS);
  return Number.isFinite(n) && n > 0 ? Math.min(n, 600000) : 180000;
}

function readyPollMs() {
  const n = Number(process.env.CARTOGRAPH_DEPS_READY_POLL_MS);
  return Number.isFinite(n) && n >= 50 ? Math.min(n, 60000) : 1000;
}

async function tryPingInternalRedis() {
  const target = internalRedisTarget();
  if (!target) {
    return true;
  }
  const timeout = Math.min(8000, PROBE_CONNECT_TIMEOUT_MS + 2000);
  const client = new Redis(createRedisConnectionOptions(target, timeout));
  try {
    await client.connect();
    return (await client.ping()) === 'PONG';
  } catch {
    return false;
  } finally {
    try {
      client.disconnect();
    } catch {
      /* ignore */
    }
  }
}

/** Wait until InternalDB answers PING or deadline (no throw — logs once). */
async function awaitInternalRedisUp() {
  const maxMs = readyDeadlineMs();
  const deadline = Date.now() + maxMs;
  while (Date.now() < deadline) {
    if (await tryPingInternalRedis()) {
      return;
    }
    await sleep(readyPollMs());
  }
  console.warn(
    `[cartograph] InternalDB not reachable after ${maxMs}ms; continuing with per-request retries`
  );
}

/** In-cluster: wait for pod IP env before Interlink advertises (no throw). */
async function awaitK8sInterlinkEnv() {
  if (!process.env.KUBERNETES_SERVICE_HOST) {
    return;
  }
  const maxMs = Math.min(readyDeadlineMs(), 90000);
  const pollMs = Math.max(100, Math.min(readyPollMs(), 2000));
  const started = Date.now();
  while (Date.now() - started < maxMs) {
    if (
      String(process.env.POD_IP || '').trim() ||
      String(process.env.INTERLINK_ADVERTISE_IP || '').trim()
    ) {
      return;
    }
    await sleep(pollMs);
  }
  console.warn('[cartograph] POD_IP / INTERLINK_ADVERTISE_IP not set in time; Interlink may lag');
}

let prereqsFlight = null;

/** Single-flight: InternalDB up (best-effort), then K8s Interlink env (best-effort). */
function scheduleCartographPrereqs() {
  if (!prereqsFlight) {
    prereqsFlight = (async () => {
      await awaitInternalRedisUp();
      await awaitK8sInterlinkEnv();
    })();
  }
  return prereqsFlight;
}

module.exports = {
  scheduleCartographPrereqs,
  internalRedisTarget,
  sleep,
};
