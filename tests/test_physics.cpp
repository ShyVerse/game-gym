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
