#pragma once

#include <filesystem>
#include <string>

namespace gg {

struct PathResolveResult {
    bool ok = false;
    std::filesystem::path path;
    std::string error;
};

PathResolveResult resolve_project_path(const std::filesystem::path& project_root,
                                       const std::filesystem::path& raw_path);

} // namespace gg
