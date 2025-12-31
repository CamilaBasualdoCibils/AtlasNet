// app/api/redis/route.ts
import { NextResponse } from "next/server";
import Redis from "ioredis";

export async function GET(req: Request) {
  const redisHost = process.env.NEXT_PUBLIC__INTERNAL_REDIS_SERVICE_NAME;
  const redisPort = parseInt(process.env.NEXT_PUBLIC__INTERNAL_REDIS_PORT || "6379");

  const client = new Redis({
    host: redisHost,
    port: redisPort,
  });
client.on("connect", () => {
  console.log("Redis client connected!");
});

client.on("ready", () => {
  console.log("Redis client is ready to use.");
});

client.on("error", (err) => {
  console.error("Redis client error:", err);
});

client.on("end", () => {
  console.log("Redis connection closed.");
});
  try {
    const { searchParams } = new URL(req.url);
    const hashKey = searchParams.get("hash") || "Heuristic_Bounds_Pending";
    const values = await client.hgetall(hashKey);

    // Convert to raw text: one line per key=value
    let rawText = "";
    for (const [key, value] of Object.entries(values)) {
      rawText += `${key}=${value}\n`;
    }

    return new NextResponse(rawText, {
      headers: { "Content-Type": "text/plain" },
    });
  } catch (err) {
    console.error(err);
    return new NextResponse("Redis query failed: " + err, { status: 500 });
  } finally {
    client.disconnect();
  }
}
