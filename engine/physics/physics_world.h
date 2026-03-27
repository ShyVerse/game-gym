#pragma once

#include "physics/physics_components.h"
#include <memory>
#include <vector>

namespace flecs { struct world; }

namespace gg {

class PhysicsWorld {
public:
    static std::unique_ptr<PhysicsWorld> create(const PhysicsConfig& config);
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&) = delete;
    PhysicsWorld& operator=(PhysicsWorld&&) = delete;

    void step(float dt);
    void step_with_ecs(float dt, flecs::world& ecs);
    uint32_t add_body(const Vec3& position, const Quat& rotation, const BodyDef& def);
    void remove_body(uint32_t body_id);
    void set_position(uint32_t body_id, const Vec3& pos);
    Vec3 get_position(uint32_t body_id) const;
    Quat get_rotation(uint32_t body_id) const;
    bool raycast(const Vec3& origin, const Vec3& direction, float max_distance, RayHit& out_hit) const;
    const std::vector<ContactEvent>& contact_events() const;

private:
    PhysicsWorld();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gg
