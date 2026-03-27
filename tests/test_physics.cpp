#include <gtest/gtest.h>
#include "physics/physics_layers.h"
#include "physics/physics_world.h"

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
