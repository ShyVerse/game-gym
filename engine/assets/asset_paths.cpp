#include "assets/asset_paths.h"

#include <iterator>

namespace gg {
namespace {

bool is_within_root(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto root_it = root.begin();
    auto candidate_it = candidate.begin();

    for (; root_it != root.end() && candidate_it != candidate.end(); ++root_it, ++candidate_it) {
        if (*root_it != *candidate_it) {
            return false;
        }
    }

    return root_it == root.end();
}

} // namespace

PathResolveResult resolve_project_path(const std::filesystem::path& project_root,
                                       const std::filesystem::path& raw_path) {
    if (raw_path.empty()) {
        return {.ok = false, .error = "path must not be empty"};
    }

    const auto root = std::filesystem::absolute(project_root).lexically_normal();
    auto candidate = raw_path.is_absolute() ? raw_path : (root / raw_path);
    candidate = std::filesystem::absolute(candidate).lexically_normal();

    if (!is_within_root(root, candidate)) {
        return {.ok = false, .error = "path escapes project root: " + raw_path.string()};
    }

    return {.ok = true, .path = candidate};
}

} // namespace gg
