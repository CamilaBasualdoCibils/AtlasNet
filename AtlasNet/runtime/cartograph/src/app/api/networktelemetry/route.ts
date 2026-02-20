import { NextResponse } from 'next/server';
import type { ShardTelemetry } from '../../lib/cartographTypes';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 500;

export async function GET() {
  const data = await fetchNativeJson<ShardTelemetry[]>({
    path: '/networktelemetry',
    timeoutMs: TIMEOUT_MS,
  });
  return NextResponse.json(Array.isArray(data) ? data : [], { status: 200 });
}
