#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace gg {

inline std::string read_shader_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open shader: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

} // namespace gg
