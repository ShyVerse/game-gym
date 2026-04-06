#include "renderer/shader_utils.h"

#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>

TEST(ShaderUtilsTest, ReadExistingShaderFile) {
    // Write a temp shader file
    const char* path = "/tmp/test_shader.wgsl";
    {
        std::ofstream f(path);
        f << "@vertex fn vs_main() -> @builtin(position) vec4f { return vec4f(0); }";
    }
    auto content = gg::read_shader_file(path);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("vs_main"), std::string::npos);
    std::remove(path);
}

TEST(ShaderUtilsTest, ThrowsOnMissingFile) {
    EXPECT_THROW(gg::read_shader_file("/nonexistent/shader.wgsl"), std::runtime_error);
}
