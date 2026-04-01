#pragma once
#include <memory>
#include <string>
#include <vector>

namespace gg {
class GpuContext;
class Mesh;

class GltfLoader {
public:
    static std::vector<std::unique_ptr<Mesh>> load(const std::string& path,
                                                    GpuContext& ctx);
};
} // namespace gg
