export interface Vec2 { x: number; y: number }

export interface ShapeJS {
    id?: string;
    ownerId?: string;
    type: 'circle' | 'rectangle' | 'polygon' | 'line' | 'rectImage';
    position: Vec2;
    radius?: number;          // for circles
    size?: Vec2;              // for rectangles
    points?: Vec2[];          // for polygons
    color?: string;
}
