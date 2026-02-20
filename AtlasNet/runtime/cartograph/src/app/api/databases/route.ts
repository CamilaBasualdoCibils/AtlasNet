import { NextResponse } from 'next/server';
import { normalizeDatabaseSnapshot } from '../../lib/server/databaseSnapshot';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 1000;

export async function GET(req: Request) {
  const reqUrl = new URL(req.url);
  const source = (reqUrl.searchParams.get('source') ?? '').trim();
  const payload = await fetchNativeJson<unknown>({
    path: '/databases',
    timeoutMs: TIMEOUT_MS,
    query: source ? { source } : undefined,
  });
  return NextResponse.json(normalizeDatabaseSnapshot(payload), { status: 200 });
}
