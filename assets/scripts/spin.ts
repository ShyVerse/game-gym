/// <reference path="../../typings/engine.d.ts" />

let angle = 0;

function onInit(): void {
    const existing: EntityInfo | null = world.getEntity("test_cube");
    if (existing !== null) {
        world.setTransform("test_cube", {
            position: { x: 0, y: 0, z: 0 },
        });
    }
}

function onUpdate(dt: number): void {
    angle += dt * 1.0;
    const s = Math.sin(angle * 0.5);
    const c = Math.cos(angle * 0.5);

    world.setTransform("test_cube", {
        rotation: { x: 0, y: s, z: 0, w: c },
    });
}

function onDestroy(): void {}
