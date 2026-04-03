#include "project/project_config.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;
namespace {

fs::path canonical_or_normal_for_test(const fs::path& path) {
    std::error_code ec;
    const auto canonical = fs::weakly_canonical(path, ec);
    if (!ec) {
        return canonical;
    }
    return fs::absolute(path).lexically_normal();
}

} // namespace

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
              canonical_or_normal_for_test(root / "scenes/start.scene.json"));
    EXPECT_EQ(result.config.assets_dir, canonical_or_normal_for_test(root / "assets"));
    EXPECT_EQ(result.config.scripts_dir, canonical_or_normal_for_test(root / "assets/scripts"));

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

TEST(ProjectConfigTest, InvalidStartupSceneTypeReturnsError) {
    const auto root = fs::temp_directory_path() / "gg_project_config_invalid_type";
    fs::remove_all(root);
    fs::create_directories(root);

    {
        std::ofstream file(root / "project.ggym");
        file << R"({
  "name": "boot-test",
  "startup_scene": 7
})";
    }

    auto result = gg::load_project_config((root / "project.ggym").string());
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());

    fs::remove_all(root);
}
