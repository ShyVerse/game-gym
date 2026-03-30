#include "ecs/systems.h"

#include "ecs/components.h"
#include "physics/physics_components.h"

namespace gg {

void register_builtin_systems(flecs::world& world) {
    // VelocitySystem: integrate linear velocity into Transform position.
    // Skip entities managed by the physics engine (those with a RigidBody).
    world.system<Transform, const Velocity>("VelocitySystem")
        .without<RigidBody>()
        .each([](flecs::iter& it, size_t, Transform& transform, const Velocity& vel) {
            const float dt = it.delta_time();
            transform.position.x += vel.linear.x * dt;
            transform.position.y += vel.linear.y * dt;
            transform.position.z += vel.linear.z * dt;
        });
}

} // namespace gg
