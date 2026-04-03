#include "assets/asset_paths.h"

#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

TEST(AssetPathsTest, ResolvesPathInsideProjectRoot) {
    const auto root = fs::temp_directory_path() / "gg_asset_paths_root";
    auto result = gg::resolve_project_path(root, "assets/models/a.glb");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.path, (fs::absolute(root) / "assets/models/a.glb").lexically_normal());
}

TEST(AssetPathsTest, NormalizesRelativeSegments) {
    const auto root = fs::temp_directory_path() / "gg_asset_paths_root";
    auto result = gg::resolve_project_path(root, "./assets/scripts/../scripts/spin.ts");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.path, (fs::absolute(root) / "assets/scripts/spin.ts").lexically_normal());
}

TEST(AssetPathsTest, RejectsPathsOutsideProjectRoot) {
    const auto root = fs::temp_directory_path() / "gg_asset_paths_root";
    auto result = gg::resolve_project_path(root, "../escape.txt");
    EXPECT_FALSE(result.ok);
}
