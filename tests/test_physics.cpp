#include "ecs/components.h"
#include "ecs/world.h"
#include "physics/physics_components.h"
#include "physics/physics_layers.h"
#include "physics/physics_world.h"

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Task 2: Physics Layer / Collision Filter Tests
// ---------------------------------------------------------------------------

TEST(PhysicsLayersTest, StaticDoesNotCollideWithStatic) {
    gg::ObjectLayerPairFilterImpl filter;
    EXPECT_FALSE(filter.ShouldCollide(gg::Layers::STATIC, gg::Layers::STATIC));
}

TEST(PhysicsLayersTest, DynamicCollidesWithStatic) {
    gg::ObjectLayerPairFilterImpl filter;
    EXPECT_TRUE(filter.ShouldCollide(gg::Layers::DYNAMIC, gg::Layers::STATIC));
}

TEST(PhysicsLayersTest, DynamicCollidesWithDynamic) {
    gg::ObjectLayerPairFilterImpl filter;
    EXPECT_TRUE(filter.ShouldCollide(gg::Layers::DYNAMIC, gg::Layers::DYNAMIC));
}

TEST(PhysicsLayersTest, TriggerCollidesWithDynamic) {
    gg::ObjectLayerPairFilterImpl filter;
    EXPECT_TRUE(filter.ShouldCollide(gg::Layers::TRIGGER, gg::Layers::DYNAMIC));
}

TEST(PhysicsLayersTest, TriggerDoesNotCollideWithStatic) {
    gg::ObjectLayerPairFilterImpl filter;
    EXPECT_FALSE(filter.ShouldCollide(gg::Layers::TRIGGER, gg::Layers::STATIC));
}

TEST(PhysicsLayersTest, TriggerDoesNotCollideWithTrigger) {
    gg::ObjectLayerPairFilterImpl filter;
    EXPECT_FALSE(filter.ShouldCollide(gg::Layers::TRIGGER, gg::Layers::TRIGGER));
}

// ---------------------------------------------------------------------------
// Task 3: PhysicsWorld Core Tests
// ---------------------------------------------------------------------------

TEST(PhysicsWorldTest, CreatesSuccessfully) {
    auto world = gg::PhysicsWorld::create({});
    ASSERT_NE(world, nullptr);
}

TEST(PhysicsWorldTest, StepsWithoutCrash) {
    auto world = gg::PhysicsWorld::create({});
    for (int i = 0; i < 10; ++i) {
        world->step(1.0f / 60.0f);
    }
}

// ---------------------------------------------------------------------------
// Task 4: Body Management Tests
// ---------------------------------------------------------------------------

TEST(PhysicsBodyTest, AddBoxBody) {
    auto pw = gg::PhysicsWorld::create({});
    gg::BodyDef def;
    def.shape = gg::BoxShapeDesc{0.5f, 0.5f, 0.5f};
    def.motion_type = gg::MotionType::Dynamic;
    uint32_t id = pw->add_body({0, 10, 0}, {0, 0, 0, 1}, def);
    EXPECT_NE(id, UINT32_MAX);
}

TEST(PhysicsBodyTest, AddSphereBody) {
    auto pw = gg::PhysicsWorld::create({});
    gg::BodyDef def;
    def.shape = gg::SphereShapeDesc{0.5f};
    def.motion_type = gg::MotionType::Dynamic;
    uint32_t id = pw->add_body({0, 5, 0}, {0, 0, 0, 1}, def);
    EXPECT_NE(id, UINT32_MAX);
}

TEST(PhysicsBodyTest, AddCapsuleBody) {
    auto pw = gg::PhysicsWorld::create({});
    gg::BodyDef def;
    def.shape = gg::CapsuleShapeDesc{0.5f, 0.25f};
    def.motion_type = gg::MotionType::Dynamic;
    uint32_t id = pw->add_body({0, 5, 0}, {0, 0, 0, 1}, def);
    EXPECT_NE(id, UINT32_MAX);
}

TEST(PhysicsBodyTest, RemoveBody) {
    auto pw = gg::PhysicsWorld::create({});
    gg::BodyDef def;
    def.shape = gg::SphereShapeDesc{0.5f};
    def.motion_type = gg::MotionType::Dynamic;
    uint32_t id = pw->add_body({0, 10, 0}, {0, 0, 0, 1}, def);
    ASSERT_NE(id, UINT32_MAX);
    pw->remove_body(id); // Should not crash
}

TEST(PhysicsBodyTest, DynamicBodyFallsUnderGravity) {
    gg::PhysicsConfig cfg;
    cfg.gravity = {0, -9.81f, 0};
    auto pw = gg::PhysicsWorld::create(cfg);

    gg::BodyDef def;
    def.shape = gg::SphereShapeDesc{0.5f};
    def.motion_type = gg::MotionType::Dynamic;
    uint32_t id = pw->add_body({0, 10, 0}, {0, 0, 0, 1}, def);

    for (int i = 0; i < 60; ++i) {
        pw->step(1.0f / 60.0f);
    }

    gg::Vec3 pos = pw->get_position(id);
    EXPECT_LT(pos.y, 10.0f);
    EXPECT_GT(pos.y, 0.0f);
}

TEST(PhysicsBodyTest, StaticBodyDoesNotMove) {
    gg::PhysicsConfig cfg;
    cfg.gravity = {0, -9.81f, 0};
    auto pw = gg::PhysicsWorld::create(cfg);

    gg::BodyDef def;
    def.shape = gg::BoxShapeDesc{50, 1, 50};
    def.motion_type = gg::MotionType::Static;
    def.layer = gg::PhysicsLayer::Static;
    uint32_t id = pw->add_body({0, 0, 0}, {0, 0, 0, 1}, def);

    for (int i = 0; i < 60; ++i) {
        pw->step(1.0f / 60.0f);
    }

    gg::Vec3 pos = pw->get_position(id);
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

// ---------------------------------------------------------------------------
// Task 5: ECS ↔ Physics Sync Tests
// ---------------------------------------------------------------------------

TEST(PhysicsSyncTest, GravityUpdatesECSTransform) {
    gg::PhysicsConfig cfg;
    cfg.gravity = {0, -9.81f, 0};
    auto pw = gg::PhysicsWorld::create(cfg);
    auto ecs = gg::World::create();

    auto entity = ecs->create_entity("ball");
    gg::Transform t0;
    t0.position = {0, 10, 0};
    entity.set<gg::Transform>(t0);

    gg::BodyDef def;
    def.shape = gg::SphereShapeDesc{0.5f};
    def.motion_type = gg::MotionType::Dynamic;
    uint32_t body_id = pw->add_body({0, 10, 0}, {0, 0, 0, 1}, def);

    gg::RigidBody rb;
    rb.body_id = body_id;
    rb.sync_to_physics = false;
    entity.set<gg::RigidBody>(rb);

    for (int i = 0; i < 30; ++i) {
        pw->step_with_ecs(1.0f / 60.0f, ecs->raw());
    }

    const auto* t = entity.get<gg::Transform>();
    ASSERT_NE(t, nullptr);
    EXPECT_LT(t->position.y, 10.0f);
}

TEST(PhysicsSyncTest, ManualTransformSyncsToPhysics) {
    gg::PhysicsConfig cfg;
    cfg.gravity = {0, 0, 0};
    auto pw = gg::PhysicsWorld::create(cfg);
    auto ecs = gg::World::create();

    auto entity = ecs->create_entity("teleporter");
    gg::Transform t0;
    t0.position = {0, 0, 0};
    entity.set<gg::Transform>(t0);

    gg::BodyDef def;
    def.shape = gg::BoxShapeDesc{1, 1, 1};
    def.motion_type = gg::MotionType::Dynamic;
    uint32_t body_id = pw->add_body({0, 0, 0}, {0, 0, 0, 1}, def);

    gg::RigidBody rb;
    rb.body_id = body_id;
    rb.sync_to_physics = false;
    entity.set<gg::RigidBody>(rb);

    // Manually teleport via ECS
    gg::Transform t1;
    t1.position = {100, 200, 300};
    entity.set<gg::Transform>(t1);

    gg::RigidBody rb2;
    rb2.body_id = body_id;
    rb2.sync_to_physics = true;
    entity.set<gg::RigidBody>(rb2);

    pw->step_with_ecs(1.0f / 60.0f, ecs->raw());

    gg::Vec3 pos = pw->get_position(body_id);
    EXPECT_NEAR(pos.x, 100.0f, 0.1f);
    EXPECT_NEAR(pos.y, 200.0f, 0.1f);
    EXPECT_NEAR(pos.z, 300.0f, 0.1f);
}

TEST(PhysicsSyncTest, VelocitySystemSkipsPhysicsEntities) {
    auto ecs = gg::World::create();

    // Entity WITH RigidBody — VelocitySystem should skip it
    auto physics_entity = ecs->create_entity("physics_obj");
    gg::Transform t0;
    t0.position = {0, 0, 0};
    physics_entity.set<gg::Transform>(t0);
    gg::Velocity v0;
    v0.linear = {10, 0, 0};
    physics_entity.set<gg::Velocity>(v0);
    gg::RigidBody rb;
    rb.body_id = 42;
    rb.sync_to_physics = false;
    physics_entity.set<gg::RigidBody>(rb);

    // Entity WITHOUT RigidBody — VelocitySystem should move it
    auto simple_entity = ecs->create_entity("simple_obj");
    gg::Transform t1;
    t1.position = {0, 0, 0};
    simple_entity.set<gg::Transform>(t1);
    gg::Velocity v1;
    v1.linear = {10, 0, 0};
    simple_entity.set<gg::Velocity>(v1);

    ecs->progress(1.0f);

    const auto* t_physics = physics_entity.get<gg::Transform>();
    const auto* t_simple = simple_entity.get<gg::Transform>();

    EXPECT_FLOAT_EQ(t_physics->position.x, 0.0f);
    EXPECT_NEAR(t_simple->position.x, 10.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Task 6: Raycast Tests
// ---------------------------------------------------------------------------

TEST(PhysicsRaycastTest, HitsStaticBody) {
    gg::PhysicsConfig cfg;
    cfg.gravity = {0, 0, 0};
    auto pw = gg::PhysicsWorld::create(cfg);

    gg::BodyDef def;
    def.shape = gg::BoxShapeDesc{5, 5, 5};
    def.motion_type = gg::MotionType::Static;
    def.layer = gg::PhysicsLayer::Static;
    pw->add_body({0, 0, 0}, {0, 0, 0, 1}, def);

    pw->step(0.0f); // Let broadphase settle

    gg::RayHit hit;
    bool did_hit = pw->raycast({0, 0, -20}, {0, 0, 1}, 100.0f, hit);
    EXPECT_TRUE(did_hit);
    EXPECT_NE(hit.body_id, UINT32_MAX);
    EXPECT_GT(hit.fraction, 0.0f);
    EXPECT_LT(hit.fraction, 1.0f);
}

TEST(PhysicsRaycastTest, MissesWhenNoBody) {
    gg::PhysicsConfig cfg;
    cfg.gravity = {0, 0, 0};
    auto pw = gg::PhysicsWorld::create(cfg);

    gg::RayHit hit;
    bool did_hit = pw->raycast({0, 0, 0}, {0, 1, 0}, 100.0f, hit);
    EXPECT_FALSE(did_hit);
}

// ---------------------------------------------------------------------------
// Task 6: Contact Event Tests
// ---------------------------------------------------------------------------

TEST(PhysicsContactTest, DetectsCollision) {
    gg::PhysicsConfig cfg;
    cfg.gravity = {0, -9.81f, 0};
    auto pw = gg::PhysicsWorld::create(cfg);

    // Static floor
    gg::BodyDef floor_def;
    floor_def.shape = gg::BoxShapeDesc{50, 1, 50};
    floor_def.motion_type = gg::MotionType::Static;
    floor_def.layer = gg::PhysicsLayer::Static;
    pw->add_body({0, -1, 0}, {0, 0, 0, 1}, floor_def);

    // Dynamic sphere above
    gg::BodyDef sphere_def;
    sphere_def.shape = gg::SphereShapeDesc{0.5f};
    sphere_def.motion_type = gg::MotionType::Dynamic;
    pw->add_body({0, 2, 0}, {0, 0, 0, 1}, sphere_def);

    bool found_contact = false;
    for (int i = 0; i < 120; ++i) {
        pw->step(1.0f / 60.0f);
        for (const auto& ev : pw->contact_events()) {
            if (ev.type == gg::ContactType::Begin) {
                found_contact = true;
            }
        }
        if (found_contact)
            break;
    }
    EXPECT_TRUE(found_contact);
}

TEST(PhysicsContactTest, TriggerDetectsDynamic) {
    gg::PhysicsConfig cfg;
    cfg.gravity = {0, -9.81f, 0};
    auto pw = gg::PhysicsWorld::create(cfg);

    // Trigger zone
    gg::BodyDef trigger_def;
    trigger_def.shape = gg::BoxShapeDesc{5, 5, 5};
    trigger_def.motion_type = gg::MotionType::Static;
    trigger_def.layer = gg::PhysicsLayer::Trigger;
    trigger_def.is_sensor = true;
    pw->add_body({0, 0, 0}, {0, 0, 0, 1}, trigger_def);

    // Dynamic sphere falling through
    gg::BodyDef sphere_def;
    sphere_def.shape = gg::SphereShapeDesc{0.5f};
    sphere_def.motion_type = gg::MotionType::Dynamic;
    pw->add_body({0, 10, 0}, {0, 0, 0, 1}, sphere_def);

    bool found_trigger = false;
    for (int i = 0; i < 120; ++i) {
        pw->step(1.0f / 60.0f);
        for (const auto& ev : pw->contact_events()) {
            if (ev.type == gg::ContactType::Begin) {
                found_trigger = true;
            }
        }
        if (found_trigger)
            break;
    }
    EXPECT_TRUE(found_trigger);
}
