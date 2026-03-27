#include <gtest/gtest.h>
#include "physics/physics_layers.h"
#include "physics/physics_world.h"
#include "ecs/world.h"
#include "physics/physics_components.h"
#include "ecs/components.h"

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
    pw->remove_body(id);  // Should not crash
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
