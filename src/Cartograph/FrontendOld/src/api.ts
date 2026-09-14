const API_BASE =
  import.meta.env.VITE_API_BASE_URL ?? ""; 
// "" = same origin (reverse gateway works)

export async function apiFetch(path: string, options?: RequestInit) {
  const url = `${API_BASE}${path}`;

  const res = await fetch(url, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options?.headers || {}),
    },
  });

  if (!res.ok) {
    throw new Error(`API error ${res.status}`);
  }

  return res.json();
}