"use client"
import { useEffect, useState } from "react";

export default function ShowRedis() {
  const [rawText, setRawText] = useState<string>("");

  useEffect(() => {
    fetch("/api/redis?hash=Heuristic_Bounds_Pending")
      .then((res) => res.text()) // <- read as raw text
      .then((text) => setRawText(text))
      .catch(console.error);
  }, []);

  return (
    <div style={{ padding: "2rem", fontFamily: "sans-serif" }}>
      <h1>Redis Hash Table (Raw Text)</h1>
      <pre>{rawText}</pre>
    </div>
  );
}
