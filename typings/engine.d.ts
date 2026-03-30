// Type definitions for the game-gym scripting API (V8 runtime).
// These declarations describe the global objects and lifecycle hooks
// available to user scripts running inside the engine.

// ---- Geometry primitives ----------------------------------------------------

interface Vec3 {
    x: number;
    y: number;
    z: number;
}

interface Quat {
    x: number;
    y: number;
    z: number;
    w: number;
}

interface TransformData {
    position?: Vec3;
    rotation?: Quat;
    scale?: Vec3;
}

// ---- Physics types ----------------------------------------------------------

type ShapeType = "box" | "sphere" | "capsule";
type MotionType = "static" | "dynamic" | "kinematic";

interface BodyDef {
    shape: ShapeType;
    motionType?: MotionType;
    halfExtents?: Vec3;
    radius?: number;
    halfHeight?: number;
    isSensor?: boolean;
    friction?: number;
    restitution?: number;
}

interface RayHit {
    bodyId: number;
    fraction: number;
    point: Vec3;
    normal: Vec3;
}

interface ContactEvent {
    bodyIdA: number;
    bodyIdB: number;
    type: "begin" | "persist" | "end";
    point: Vec3;
    normal: Vec3;
}

// ---- ECS types --------------------------------------------------------------

interface EntityInfo {
    name: string;
    transform?: {
        position: Vec3;
        rotation: Quat;
        scale: Vec3;
    };
    velocity?: {
        linear: Vec3;
        angular: Vec3;
    };
}

type ComponentType =
    | "Transform"
    | "Velocity"
    | "RigidBody"
    | "Renderable";

// ---- Global objects ---------------------------------------------------------

declare const world: {
    createEntity(name: string): { created: string } | { error: string };
    destroyEntity(name: string): { destroyed: string } | { error: string };
    getEntity(name: string): EntityInfo | null;
    listEntities(): string[];
    setTransform(
        name: string,
        transform: TransformData,
    ): { updated: string } | { error: string };
    hasComponent(name: string, component: ComponentType): boolean;
};

declare const physics: {
    addBody(position: Vec3, rotation: Quat, bodyDef: BodyDef): number;
    removeBody(bodyId: number): boolean;
    getPosition(bodyId: number): Vec3 | null;
    setPosition(bodyId: number, position: Vec3): boolean;
    raycast(
        origin: Vec3,
        direction: Vec3,
        maxDistance: number,
    ): RayHit | null;
    contactEvents(): ContactEvent[];
};

// ---- Lifecycle hooks --------------------------------------------------------
// User scripts may define these top-level functions.  The engine calls them at
// the appropriate point in the frame loop.

declare function onInit(): void;
declare function onUpdate(dt: number): void;
declare function onDestroy(): void;
