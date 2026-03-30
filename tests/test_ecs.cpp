#include "ecs/components.h"
#include "ecs/world.h"

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Component default value tests
// ---------------------------------------------------------------------------

TEST(ComponentsTest, TransformDefaultScale) {
    gg::Transform t{};
    EXPECT_FLOAT_EQ(t.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.y, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.z, 1.0f);
}

TEST(ComponentsTest, TransformDefaultPosition) {
    gg::Transform t{};
    EXPECT_FLOAT_EQ(t.position.x, 0.0f);
    EXPECT_FLOAT_EQ(t.position.y, 0.0f);
    EXPECT_FLOAT_EQ(t.position.z, 0.0f);
}

TEST(ComponentsTest, QuatDefaultIdentity) {
    gg::Quat q{};
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
}

// ---------------------------------------------------------------------------
// World / entity tests
// ---------------------------------------------------------------------------

TEST(WorldTest, CreatesSuccessfully) {
    auto world = gg::World::create();
    ASSERT_NE(world, nullptr);
}

TEST(WorldTest, CreateEntityWithTransform) {
    auto world = gg::World::create();
    auto entity = world->create_entity("player");
    entity.set<gg::Transform>({});
    EXPECT_TRUE(entity.has<gg::Transform>());
}

TEST(WorldTest, CreateEntityWithMultipleComponents) {
    auto world = gg::World::create();
    auto entity = world->create_entity("actor");
    entity.set<gg::Transform>({});
    entity.set<gg::Velocity>({});
    entity.set<gg::Name>({"actor"});
    EXPECT_TRUE(entity.has<gg::Transform>());
    EXPECT_TRUE(entity.has<gg::Velocity>());
    EXPECT_TRUE(entity.has<gg::Name>());
}

TEST(WorldTest, QueryFindsMatchingEntities) {
    auto world = gg::World::create();

    auto e1 = world->create_entity("e1");
    e1.set<gg::Transform>({});
    e1.add<gg::Renderable>();

    auto e2 = world->create_entity("e2");
    e2.set<gg::Transform>({});
    // no Renderable

    int count = 0;
    // Use query builder: match Transform + Renderable tag.
    world->raw().query_builder<gg::Transform>().with<gg::Renderable>().build().each(
        [&count](gg::Transform&) { ++count; });
    EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// VelocitySystem integration test
// ---------------------------------------------------------------------------

TEST(WorldTest, VelocitySystemUpdatesPosition) {
    auto world = gg::World::create();

    auto entity = world->create_entity("mover");
    entity.set<gg::Transform>({});
    entity.set<gg::Velocity>({.linear = {1.0f, 0.0f, 0.0f}});

    constexpr float dt = 1.0f;
    world->progress(dt);

    const auto* transform = entity.get<gg::Transform>();
    ASSERT_NE(transform, nullptr);
    EXPECT_NEAR(transform->position.x, 1.0f, 0.001f);
    EXPECT_NEAR(transform->position.y, 0.0f, 0.001f);
    EXPECT_NEAR(transform->position.z, 0.0f, 0.001f);
}
