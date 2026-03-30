#include "ecs/components.h"
#include "ecs/world.h"
#include "physics/physics_components.h"
#include "physics/physics_world.h"
#include "script/script_bindings.h"
#include "script/script_engine.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Test fixture: creates engine, world, physics, and registers bindings.
// ---------------------------------------------------------------------------

class ScriptBindingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = gg::ScriptEngine::create();
        world_ = gg::World::create();
        physics_ = gg::PhysicsWorld::create({});
        gg::register_script_bindings(*engine_, *world_, *physics_);
    }

    std::unique_ptr<gg::ScriptEngine> engine_;
    std::unique_ptr<gg::World> world_;
    std::unique_ptr<gg::PhysicsWorld> physics_;
};

// ---------------------------------------------------------------------------
// 1. CreateEntityFromJS
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, CreateEntityFromJS) {
    auto result = engine_->execute("JSON.stringify(__ecs_createEntity('hero'))");
    ASSERT_TRUE(result.ok) << result.error;

    // Verify the entity actually exists in the C++ world with Name component
    bool found = false;
    world_->raw().each([&](flecs::entity, const gg::Name& n) {
        if (n.value == "hero") {
            found = true;
        }
    });
    EXPECT_TRUE(found) << "Entity 'hero' should exist in the ECS world";
}

// ---------------------------------------------------------------------------
// 2. ListEntitiesFromJS
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, ListEntitiesFromJS) {
    // Create entities from C++ side
    auto e1 = world_->create_entity("alpha");
    e1.set<gg::Name>({"alpha"});
    e1.set<gg::Transform>({});

    auto e2 = world_->create_entity("beta");
    e2.set<gg::Name>({"beta"});
    e2.set<gg::Transform>({});

    auto result = engine_->execute("JSON.stringify(__ecs_listEntities())");
    ASSERT_TRUE(result.ok) << result.error;

    auto arr = json::parse(result.value);
    ASSERT_TRUE(arr.is_array());

    // Collect names from the result
    std::vector<std::string> names;
    for (const auto& elem : arr) {
        names.push_back(elem.get<std::string>());
    }

    EXPECT_NE(std::find(names.begin(), names.end(), "alpha"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "beta"), names.end());
}

// ---------------------------------------------------------------------------
// 3. GetEntityReturnsNullForMissing
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, GetEntityReturnsNullForMissing) {
    auto result = engine_->execute(
        "JSON.stringify(__ecs_getEntity('nonexistent'))");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value, "null");
}

// ---------------------------------------------------------------------------
// 4. DestroyEntityFromJS
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, DestroyEntityFromJS) {
    // Create entity from C++ side
    auto entity = world_->create_entity("doomed");
    entity.set<gg::Name>({"doomed"});
    entity.set<gg::Transform>({});

    // Destroy from JS
    auto result = engine_->execute("JSON.stringify(__ecs_destroyEntity('doomed'))");
    ASSERT_TRUE(result.ok) << result.error;

    // Verify entity is gone
    bool found = false;
    world_->raw().each([&](flecs::entity, const gg::Name& n) {
        if (n.value == "doomed") {
            found = true;
        }
    });
    EXPECT_FALSE(found) << "Entity 'doomed' should no longer exist";
}

// ---------------------------------------------------------------------------
// 5. SetTransformFromJS (uses the high-level `world` wrapper)
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, SetTransformFromJS) {
    // Create entity from C++ side
    auto entity = world_->create_entity("mover");
    entity.set<gg::Name>({"mover"});
    entity.set<gg::Transform>({});

    auto result = engine_->execute(
        "JSON.stringify(world.setTransform('mover', "
        "{position:{x:10,y:20,z:30}, rotation:{x:0,y:0,z:0,w:1}, "
        "scale:{x:2,y:2,z:2}}))");
    ASSERT_TRUE(result.ok) << result.error;

    const auto* t = entity.get<gg::Transform>();
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 10.0f);
    EXPECT_FLOAT_EQ(t->position.y, 20.0f);
    EXPECT_FLOAT_EQ(t->position.z, 30.0f);
    EXPECT_FLOAT_EQ(t->scale.x, 2.0f);
    EXPECT_FLOAT_EQ(t->scale.y, 2.0f);
    EXPECT_FLOAT_EQ(t->scale.z, 2.0f);
}

// ---------------------------------------------------------------------------
// 6. HasComponentFromJS
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, HasComponentFromJS) {
    auto entity = world_->create_entity("checker");
    entity.set<gg::Name>({"checker"});
    entity.set<gg::Transform>({});
    // Intentionally no Velocity component

    auto result_transform = engine_->execute(
        "JSON.stringify(__ecs_hasComponent('checker', 'Transform'))");
    ASSERT_TRUE(result_transform.ok) << result_transform.error;
    EXPECT_EQ(result_transform.value, "true");

    auto result_velocity = engine_->execute(
        "JSON.stringify(__ecs_hasComponent('checker', 'Velocity'))");
    ASSERT_TRUE(result_velocity.ok) << result_velocity.error;
    EXPECT_EQ(result_velocity.value, "false");
}

// ---------------------------------------------------------------------------
// 7. AddAndQueryPhysicsBody
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, AddAndQueryPhysicsBody) {
    auto result = engine_->execute(
        "JSON.stringify(__physics_addBody("
        "{x:0,y:5,z:0}, "
        "{x:0,y:0,z:0,w:1}, "
        "{shape:'box', motionType:'dynamic'}))");
    ASSERT_TRUE(result.ok) << result.error;

    auto body_id = json::parse(result.value);
    ASSERT_TRUE(body_id.is_number_unsigned());
    EXPECT_NE(body_id.get<uint32_t>(), UINT32_MAX);
}

// ---------------------------------------------------------------------------
// 8. RaycastFromJS
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, RaycastFromJS) {
    // Create a static body at origin (large box)
    gg::BodyDef def;
    def.shape = gg::BoxShapeDesc{5, 5, 5};
    def.motion_type = gg::MotionType::Static;
    def.layer = gg::PhysicsLayer::Static;
    physics_->add_body({0, 0, 0}, {0, 0, 0, 1}, def);

    // Step to let broadphase settle
    physics_->step(0.0f);

    auto result = engine_->execute(
        "JSON.stringify(__physics_raycast("
        "{x:0,y:0,z:-20}, {x:0,y:0,z:1}, 100))");
    ASSERT_TRUE(result.ok) << result.error;

    auto hit_json = json::parse(result.value);
    ASSERT_TRUE(hit_json.is_object()) << "Expected a hit object, got: " << result.value;
    EXPECT_TRUE(hit_json.contains("bodyId"));
    EXPECT_TRUE(hit_json.contains("fraction"));
    EXPECT_GT(hit_json.at("fraction").get<float>(), 0.0f);
    EXPECT_LT(hit_json.at("fraction").get<float>(), 1.0f);
}

// ---------------------------------------------------------------------------
// 9. CreateEntityWithEmptyNameReturnsError
// ---------------------------------------------------------------------------

TEST_F(ScriptBindingsTest, CreateEntityWithEmptyNameReturnsError) {
    auto result = engine_->execute("JSON.stringify(__ecs_createEntity(''))");
    ASSERT_TRUE(result.ok) << result.error;

    auto parsed = json::parse(result.value);
    ASSERT_TRUE(parsed.is_object());
    EXPECT_TRUE(parsed.contains("error"));
}
