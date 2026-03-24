import type Redis from 'ioredis';

function pollMs(): number {
  const n = Number(process.env.CARTOGRAPH_DEPS_READY_POLL_MS);
  return Number.isFinite(n) && n >= 50 ? Math.min(n, 60000) : 1000;
}

/** Bounded connect retries; aligns delay with native-server `cartographDeps` polling. */
export async function connectInternalRedisWithRetry(createClient: () => Redis): Promise<Redis> {
  const rounds = 8;
  const delayMs = pollMs();
  let lastErr: unknown;
  for (let i = 0; i < rounds; i += 1) {
    const client = createClient();
    try {
      await client.connect();
      return client;
    } catch (err) {
      lastErr = err;
      try {
        client.disconnect();
      } catch {
        /* ignore */
      }
      if (i + 1 < rounds && delayMs > 0) {
        await new Promise((r) => setTimeout(r, delayMs));
      }
    }
  }
  throw lastErr;
}
