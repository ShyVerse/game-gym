#include "renderer/gltf_loader.h"
#include "core/window.h"
#include "renderer/gpu_context.h"
#include "renderer/mesh.h"

#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>

class GltfLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        window_ = gg::Window::create(
            {.title = "gltf-test", .width = 320, .height = 240});
        ASSERT_NE(window_, nullptr);
        gpu_ = gg::GpuContext::create(*window_);
        ASSERT_NE(gpu_, nullptr);
    }
    std::unique_ptr<gg::Window> window_;
    std::unique_ptr<gg::GpuContext> gpu_;
};

TEST_F(GltfLoaderTest, LoadNonexistentFileReturnsEmpty) {
    auto meshes = gg::GltfLoader::load("nonexistent.glb", *gpu_);
    EXPECT_TRUE(meshes.empty());
}

TEST_F(GltfLoaderTest, LoadInvalidFileReturnsEmpty) {
    const char* path = "/tmp/gg_test_invalid.glb";
    {
        std::ofstream f(path);
        f << "not a glb file";
    }
    auto meshes = gg::GltfLoader::load(path, *gpu_);
    EXPECT_TRUE(meshes.empty());
    std::remove(path);
}
