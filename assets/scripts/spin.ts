/// <reference path="../../typings/engine.d.ts" />

function onInit(): void {
    const existing: EntityInfo | null = world.getEntity("script_boot_marker");
    if (existing !== null) {
        return;
    }

    world.createEntity("script_boot_marker");
    world.setTransform("script_boot_marker", {
        position: { x: 0, y: 1.5, z: 0 },
    });
}

function onUpdate(_dt: number): void {}

function onDestroy(): void {
    world.destroyEntity("script_boot_marker");
}
