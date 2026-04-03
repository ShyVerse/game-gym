#include "version/version_info.h"

#include <gtest/gtest.h>
#include <regex>
#include <string>

TEST(VersionInfoTest, ProjectVersionIsSemverLike) {
    EXPECT_TRUE(std::regex_match(std::string(gg::build_version::project_version()),
                                 std::regex(R"(\d+\.\d+\.\d+)")));
}

TEST(VersionInfoTest, DisplayVersionIsNonEmpty) {
    EXPECT_FALSE(gg::build_version::display_version().empty());
    EXPECT_FALSE(gg::build_version::git_describe().empty());
}

TEST(VersionInfoTest, ExactTagImpliesReleaseVersionDisplay) {
    if (gg::build_version::is_exact_tag()) {
        EXPECT_EQ(gg::build_version::display_version(), gg::build_version::release_version());
    }
}
