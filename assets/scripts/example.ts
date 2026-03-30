/// <reference path="../../typings/engine.d.ts" />

// Example patrol-bot script.
// The entity walks back and forth along the X axis between -BOUND and +BOUND.

let direction: number = 1.0;
const SPEED: number = 2.0;
const BOUND: number = 10.0;

function onInit(): void {
    world.createEntity("patrol_bot");
    world.setTransform("patrol_bot", {
        position: { x: 0, y: 0.5, z: 0 },
    });
}

function onUpdate(dt: number): void {
    const info: EntityInfo | null = world.getEntity("patrol_bot");
    if (info === null || info.transform === undefined) {
        return;
    }

    let x: number = info.transform.position.x + direction * SPEED * dt;

    if (x > BOUND) {
        x = BOUND;
        direction = -1.0;
    } else if (x < -BOUND) {
        x = -BOUND;
        direction = 1.0;
    }

    world.setTransform("patrol_bot", {
        position: {
            x,
            y: info.transform.position.y,
            z: info.transform.position.z,
        },
    });
}

function onDestroy(): void {
    world.destroyEntity("patrol_bot");
}
