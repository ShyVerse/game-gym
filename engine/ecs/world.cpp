#include "ecs/world.h"

#include "ecs/systems.h"

namespace gg {

World::World(flecs::world world) : world_(std::move(world)) {}

std::unique_ptr<World> World::create() {
    flecs::world w;
    register_builtin_systems(w);
    return std::unique_ptr<World>(new World(std::move(w)));
}

flecs::entity World::create_entity(const std::string& name) {
    return world_.entity(name.c_str());
}

void World::destroy_entity(flecs::entity entity) {
    entity.destruct();
}

void World::progress(float dt) {
    world_.progress(dt);
}

flecs::world& World::raw() {
    return world_;
}

} // namespace gg
