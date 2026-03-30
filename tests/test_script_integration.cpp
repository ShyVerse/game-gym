#include "ecs/components.h"
#include "ecs/world.h"
#include "physics/physics_world.h"
#include "script/script_bindings.h"
#include "script/script_engine.h"
#include "script/script_manager.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Integration test fixture
// Combines ScriptEngine + ScriptBindings + ScriptManager with a real
// ECS World and PhysicsWorld.
// ---------------------------------------------------------------------------

class ScriptIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "gg_integration_test";
        fs::remove_all(tmp_dir_);
        fs::create_directories(tmp_dir_);

        engine_ = gg::ScriptEngine::create();
        ASSERT_NE(engine_, nullptr);

        world_ = gg::World::create();
        ASSERT_NE(world_, nullptr);

        physics_ = gg::PhysicsWorld::create({});
        ASSERT_NE(physics_, nullptr);

        gg::register_script_bindings(*engine_, *world_, *physics_);

        manager_ = gg::ScriptManager::create(*engine_, tmp_dir_.string());
        ASSERT_NE(manager_, nullptr);
    }

    void TearDown() override {
        manager_.reset();
        engine_.reset();
        world_.reset();
        physics_.reset();
        fs::remove_all(tmp_dir_);
    }

    /// Helper: write a .js file into the temp script directory.
    void write_js(const std::string& name, const std::string& content) {
        std::ofstream f(tmp_dir_ / name);
        f << content;
    }

    fs::path tmp_dir_;
    std::unique_ptr<gg::ScriptEngine> engine_;
    std::unique_ptr<gg::World> world_;
    std::unique_ptr<gg::PhysicsWorld> physics_;
    std::unique_ptr<gg::ScriptManager> manager_;
};

// ---------------------------------------------------------------------------
// 1. ScriptCreatesEntityOnInit
//    A script defines onInit which calls world.createEntity().
//    After load_all(), the entity must exist in the C++ ECS world.
// ---------------------------------------------------------------------------

TEST_F(ScriptIntegrationTest, ScriptCreatesEntityOnInit) {
    write_js("spawn.js",
             "function onInit() {\n"
             "    world.createEntity('scripted_npc');\n"
             "}\n");

    manager_->load_all();

    bool found = false;
    world_->raw().each([&](flecs::entity, const gg::Name& n) {
        if (n.value == "scripted_npc") {
            found = true;
        }
    });
    EXPECT_TRUE(found) << "Entity 'scripted_npc' should exist after onInit";
}

// ---------------------------------------------------------------------------
// 2. OnUpdateReceivesDeltaTime
//    A script stores the dt argument it receives from onUpdate into
//    globalThis.__dt.  We call onUpdate with 0.016 and verify the value.
// ---------------------------------------------------------------------------

TEST_F(ScriptIntegrationTest, OnUpdateReceivesDeltaTime) {
    write_js("dt_check.js",
             "function onUpdate(dt) {\n"
             "    globalThis.__dt = dt;\n"
             "}\n");

    manager_->load_all();

    manager_->call_update(0.016f);

    auto read_result = engine_->execute("globalThis.__dt");
    ASSERT_TRUE(read_result.ok) << read_result.error;

    const double dt = std::stod(read_result.value);
    EXPECT_NEAR(dt, 0.016, 1e-6);
}

// ---------------------------------------------------------------------------
// 3. ScriptErrorDoesNotCrashEngine
//    A script that throws inside onUpdate must not crash the host process.
//    The ScriptEngine should remain alive and usable after the exception.
// ---------------------------------------------------------------------------

TEST_F(ScriptIntegrationTest, ScriptErrorDoesNotCrashEngine) {
    write_js("bad.js",
             "function onUpdate(dt) {\n"
             "    throw new Error('intentional kaboom');\n"
             "}\n");

    manager_->load_all();

    // call_update internally catches errors via TryCatch -- should not crash.
    manager_->call_update(0.016f);

    // Engine should still be alive and functional.
    EXPECT_TRUE(engine_->is_alive());

    auto ok_result = engine_->execute("1 + 1");
    ASSERT_TRUE(ok_result.ok) << ok_result.error;
    EXPECT_EQ(ok_result.value, "2");
}

// ---------------------------------------------------------------------------
// 4. MultipleScriptsLoadInOrder
//    Two scripts (alphabetically ordered filenames) each append a letter
//    to globalThis.__order.  After load_all(), we verify "AB".
// ---------------------------------------------------------------------------

TEST_F(ScriptIntegrationTest, MultipleScriptsLoadInOrder) {
    write_js("a_first.js",
             "if (!globalThis.__order) { globalThis.__order = ''; }\n"
             "globalThis.__order += 'A';\n");

    write_js("b_second.js",
             "if (!globalThis.__order) { globalThis.__order = ''; }\n"
             "globalThis.__order += 'B';\n");

    manager_->load_all();

    auto result = engine_->execute("globalThis.__order");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value, "AB");
}
