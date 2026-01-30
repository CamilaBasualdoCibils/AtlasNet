'use client';
import { useEffect, useRef, useState } from 'react';
import { createMapRenderer } from '../lib/mapRenderer';
import type { ShapeJS } from '../lib/types';

export default function MapPage() {
    const containerRef = useRef<HTMLDivElement>(null);
    const [shapes, setShapes] = useState<ShapeJS[]>([]); // start empty

    // Fetch shapes from the API route
    useEffect(() => {
        console.log("Fetching shapes");
        fetch('/api/heuristicfetch') // this should match your API route file
            .then(res => res.json())
            .then((data: ShapeJS[]) => {
                console.log("Fetched shapes in browser:", data);
                setShapes(data);
            })
            .catch(console.error);
    }, []);

    // Render the map whenever shapes update
    useEffect(() => {
        if (shapes.length === 0) return;
        const container = containerRef.current;
        if (!container) return;
        const renderer = createMapRenderer({ container, shapes });
        return () => {
            container.innerHTML = ''; // cleanup
        };
    }, [shapes]);

    return (
        <div
            ref={containerRef}
            style={{
                width: '100%',
                height: '100%',
                overflow: 'hidden',
                touchAction: 'none',
                border: '1px solid #ccc'
            }}
        ></div>
    );
}
