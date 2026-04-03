#include "assets/asset_paths.h"

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

TEST(AssetPathsTest, ResolvesPathInsideProjectRoot) {
    const auto root = fs::temp_directory_path() / "gg_asset_paths_root";
    auto result = gg::resolve_project_path(root, "assets/models/a.glb");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.path, canonical_or_normal_for_test(root / "assets/models/a.glb"));
}

TEST(AssetPathsTest, NormalizesRelativeSegments) {
    const auto root = fs::temp_directory_path() / "gg_asset_paths_root";
    auto result = gg::resolve_project_path(root, "./assets/scripts/../scripts/spin.ts");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.path, canonical_or_normal_for_test(root / "assets/scripts/spin.ts"));
}

TEST(AssetPathsTest, RejectsPathsOutsideProjectRoot) {
    const auto root = fs::temp_directory_path() / "gg_asset_paths_root";
    auto result = gg::resolve_project_path(root, "../escape.txt");
    EXPECT_FALSE(result.ok);
}

TEST(AssetPathsTest, RejectsSymlinkEscapesOutsideProjectRoot) {
    const auto root = fs::temp_directory_path() / "gg_asset_paths_symlink";
    const auto outside = fs::temp_directory_path() / "gg_asset_paths_outside";
    fs::remove_all(root);
    fs::remove_all(outside);
    fs::create_directories(root / "assets");
    fs::create_directories(outside / "scripts");

    { std::ofstream(outside / "scripts/hack.ts") << "hack"; }

    fs::create_directory_symlink(outside / "scripts", root / "assets/link");

    auto result = gg::resolve_project_path(root, "assets/link/hack.ts");
    EXPECT_FALSE(result.ok);

    fs::remove_all(root);
    fs::remove_all(outside);
}
