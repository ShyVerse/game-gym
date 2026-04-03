#pragma once

#include "build_version.h"

#include <string_view>

namespace gg::build_version {

inline constexpr std::string_view project_version() noexcept {
    return kProjectVersion;
}

inline constexpr std::string_view release_tag() noexcept {
    return kReleaseTag;
}

inline constexpr std::string_view release_version() noexcept {
    return kReleaseVersion;
}

inline constexpr std::string_view git_describe() noexcept {
    return kGitDescribe;
}

inline constexpr std::string_view display_version() noexcept {
    return kDisplayVersion;
}

inline constexpr bool is_exact_tag() noexcept {
    return kIsExactTag;
}

inline constexpr bool is_dirty() noexcept {
    return kIsDirty;
}

} // namespace gg::build_version
