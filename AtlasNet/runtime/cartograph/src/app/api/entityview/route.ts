'use server';

import { NextResponse } from 'next/server';

const NATIVE_URL = 'http://127.0.0.1:4000/authoritytelemetry';
const TIMEOUT_MS = 500;

export async function GET() {
  const ac = new AbortController();
  const timeoutId = setTimeout(() => ac.abort(), TIMEOUT_MS);

  try {
    const response = await fetch(NATIVE_URL, {
      cache: 'no-store',
      signal: ac.signal,
    });

    if (!response.ok) {
      return NextResponse.json([], { status: 200 });
    }

    const data = await response.json();
    return NextResponse.json(data, { status: 200 });
  } catch {
    return NextResponse.json([], { status: 200 });
  } finally {
    clearTimeout(timeoutId);
  }
}
