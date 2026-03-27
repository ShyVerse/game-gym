#pragma once
#include <flecs.h>

namespace gg {

/// Register all built-in ECS systems into the given world.
void register_builtin_systems(flecs::world& world);

} // namespace gg
