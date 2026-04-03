#include "ecs/components.h"
#include "ecs/world.h"
#include "scene/scene_loader.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

TEST(SceneLoaderTest, LoadsEntitiesIntoWorld) {
    const auto root = fs::temp_directory_path() / "gg_scene_loader";
    fs::remove_all(root);
    fs::create_directories(root / "scenes");

    {
        std::ofstream file(root / "scenes/start.scene.json");
        file << R"({
  "entities": [
    {
      "name": "crate",
      "transform": {
        "position": { "x": 1, "y": 2, "z": 3 },
        "rotation": { "x": 0, "y": 0, "z": 0, "w": 1 },
        "scale": { "x": 1, "y": 1, "z": 1 }
      },
      "mesh_asset": "assets/models/crate.glb",
      "script_asset": "assets/scripts/crate.ts"
    }
  ]
})";
    }

    auto world = gg::World::create();
    auto result =
        gg::load_scene_into_world((root / "scenes/start.scene.json").string(), root, *world);
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.entity_count, 1u);
    ASSERT_EQ(result.mesh_assets.size(), 1u);
    ASSERT_EQ(result.script_assets.size(), 1u);

    bool found = false;
    world->raw().each([&](flecs::entity entity, const gg::Name& name) {
        if (name.value != "crate") {
            return;
        }

        found = true;
        const auto* transform = entity.get<gg::Transform>();
        ASSERT_NE(transform, nullptr);
        EXPECT_FLOAT_EQ(transform->position.x, 1.0f);

        const auto* renderable = entity.get<gg::Renderable>();
        ASSERT_NE(renderable, nullptr);
        EXPECT_EQ(renderable->mesh_asset_path,
                  (fs::absolute(root) / "assets/models/crate.glb").lexically_normal().string());

        const auto* script = entity.get<gg::ScriptComponent>();
        ASSERT_NE(script, nullptr);
        EXPECT_EQ(script->script_asset_path,
                  (fs::absolute(root) / "assets/scripts/crate.ts").lexically_normal().string());
    });

    EXPECT_TRUE(found);
    fs::remove_all(root);
}

TEST(SceneLoaderTest, MissingMeshFileStillCreatesEntity) {
    const auto root = fs::temp_directory_path() / "gg_scene_loader_missing_mesh";
    fs::remove_all(root);
    fs::create_directories(root / "scenes");

    {
        std::ofstream file(root / "scenes/start.scene.json");
        file << R"({
  "entities": [
    {
      "name": "crate",
      "mesh_asset": "assets/models/missing.glb"
    }
  ]
})";
    }

    auto world = gg::World::create();
    auto result =
        gg::load_scene_into_world((root / "scenes/start.scene.json").string(), root, *world);
    ASSERT_TRUE(result.ok) << result.error;

    bool found = false;
    world->raw().each([&](flecs::entity entity, const gg::Name& name) {
        if (name.value == "crate") {
            found = true;
            EXPECT_TRUE(entity.has<gg::Renderable>());
        }
    });

    EXPECT_TRUE(found);
    fs::remove_all(root);
}
