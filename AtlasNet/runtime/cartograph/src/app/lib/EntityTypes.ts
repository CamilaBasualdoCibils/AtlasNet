export interface AtlasEntity {
  entityId: string;
  ownerId: string;
  world: number;
    position: {
    x: number;
    y: number;
    z: number;
  };
  isClient: boolean;
  clientId: string;
}

export function parseEntityView(
  raw: Record<string, any[]>
): Map<number, AtlasEntity[]> {
  const result = new Map<number, AtlasEntity[]>();

  for (const [boundIdStr, entries] of Object.entries(raw)) {
    const boundId = Number(boundIdStr);
    if (!Array.isArray(entries)) continue;

    const parsed: AtlasEntity[] = entries.map((entry) => ({
      entityId: entry.EntityID,
      ownerId: entry.ClientID,
      world: entry.WorldID,
      position: {
        x: entry.position?.x ?? 0,
        y: entry.position?.y ?? 0,
        z: entry.position?.z ?? 0,
      },
      isClient: entry.ISClient,
      clientId: entry.ClientID,
    }));

    result.set(boundId, parsed);
  }

  console.log(
    "parseEntityView result:",
    JSON.stringify(Array.from(result.entries()), null, 2)
  );
  return result;
}