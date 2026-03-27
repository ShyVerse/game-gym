#pragma once
#include <flecs.h>
#include <memory>
#include <string>

namespace gg {

/// Thin RAII wrapper around flecs::world.
class World {
public:
    /// Factory: creates and initialises a World with all built-in systems.
    static std::unique_ptr<World> create();

    ~World() = default;

    // Non-copyable, movable.
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = default;
    World& operator=(World&&) = default;

    /// Create a named entity and return its id.
    flecs::entity create_entity(const std::string& name);

    /// Destroy an entity.
    void destroy_entity(flecs::entity entity);

    /// Advance the world by dt seconds.
    void progress(float dt);

    /// Direct access to the underlying flecs world.
    flecs::world& raw();

private:
    explicit World(flecs::world world);

    flecs::world world_;
};

} // namespace gg
