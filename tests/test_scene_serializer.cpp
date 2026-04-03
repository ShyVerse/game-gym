#include "scene/scene_serializer.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

TEST(SceneSerializerTest, RoundTripsSceneDocument) {
    gg::SceneDocument scene;
    scene.entities.push_back({
        .name = "player",
        .transform =
            {
                .position = {1.0f, 2.0f, 3.0f},
                .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
                .scale = {1.0f, 1.0f, 1.0f},
            },
        .mesh_asset_path = "assets/models/player.glb",
        .script_asset_path = "assets/scripts/player.ts",
    });

    const auto root = fs::temp_directory_path() / "gg_scene_serializer";
    fs::remove_all(root);
    fs::create_directories(root);

    std::string error;
    ASSERT_TRUE(gg::save_scene_document(scene, (root / "start.scene.json").string(), &error))
        << error;

    auto loaded = gg::load_scene_document((root / "start.scene.json").string());
    ASSERT_TRUE(loaded.ok) << loaded.error;
    ASSERT_EQ(loaded.document.entities.size(), 1u);
    EXPECT_EQ(loaded.document.entities[0].name, "player");
    EXPECT_EQ(loaded.document.entities[0].mesh_asset_path, "assets/models/player.glb");
    EXPECT_EQ(loaded.document.entities[0].script_asset_path, "assets/scripts/player.ts");
    EXPECT_FLOAT_EQ(loaded.document.entities[0].transform.position.y, 2.0f);

    fs::remove_all(root);
}

TEST(SceneSerializerTest, EmptySceneSerializesEntitiesArray) {
    gg::SceneDocument scene;
    const auto serialized = gg::serialize_scene_document(scene);
    EXPECT_NE(serialized.find("\"entities\": []"), std::string::npos);
}

TEST(SceneSerializerTest, InvalidJsonFailsWithError) {
    const auto root = fs::temp_directory_path() / "gg_scene_serializer_invalid";
    fs::remove_all(root);
    fs::create_directories(root);

    {
        std::ofstream file(root / "broken.scene.json");
        file << "{ not valid json";
    }

    auto result = gg::load_scene_document((root / "broken.scene.json").string());
    EXPECT_FALSE(result.ok);

    fs::remove_all(root);
}
