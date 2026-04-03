#include "project/project_config.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

TEST(ProjectConfigTest, LoadsProjectConfigFromDisk) {
    const auto root = fs::temp_directory_path() / "gg_project_config";
    fs::remove_all(root);
    fs::create_directories(root / "assets/scripts");
    fs::create_directories(root / "scenes");

    {
        std::ofstream file(root / "project.ggym");
        file << R"({
  "name": "boot-test",
  "startup_scene": "scenes/start.scene.json",
  "assets_dir": "assets",
  "scripts_dir": "assets/scripts"
})";
    }

    auto result = gg::load_project_config((root / "project.ggym").string());
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.name, "boot-test");
    EXPECT_EQ(result.config.project_root, fs::absolute(root).lexically_normal());
    EXPECT_EQ(result.config.startup_scene,
              (fs::absolute(root) / "scenes/start.scene.json").lexically_normal());
    EXPECT_EQ(result.config.assets_dir, (fs::absolute(root) / "assets").lexically_normal());
    EXPECT_EQ(result.config.scripts_dir,
              (fs::absolute(root) / "assets/scripts").lexically_normal());

    fs::remove_all(root);
}

TEST(ProjectConfigTest, MissingFileReturnsError) {
    auto result = gg::load_project_config("/tmp/gg_missing_project.ggym");
    EXPECT_FALSE(result.ok);
}

TEST(ProjectConfigTest, MissingStartupSceneReturnsError) {
    const auto root = fs::temp_directory_path() / "gg_project_config_missing";
    fs::remove_all(root);
    fs::create_directories(root);

    {
        std::ofstream file(root / "project.ggym");
        file << R"({
  "name": "boot-test"
})";
    }

    auto result = gg::load_project_config((root / "project.ggym").string());
    EXPECT_FALSE(result.ok);

    fs::remove_all(root);
}
