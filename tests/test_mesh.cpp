#include "core/window.h"
#include "renderer/gpu_context.h"
#include "renderer/mesh.h"

#include <gtest/gtest.h>

class MeshTest : public ::testing::Test {
protected:
    void SetUp() override {
        window_ = gg::Window::create({.title = "mesh-test", .width = 320, .height = 240});
        ASSERT_NE(window_, nullptr);
        gpu_ = gg::GpuContext::create(*window_);
        ASSERT_NE(gpu_, nullptr);
    }
    std::unique_ptr<gg::Window> window_;
    std::unique_ptr<gg::GpuContext> gpu_;
};

TEST_F(MeshTest, CreateFromVerticesAndIndices) {
    std::vector<gg::Vertex> verts = {
        {{0, 0, 0}, {0, 1, 0}, {0, 0}},
        {{1, 0, 0}, {0, 1, 0}, {1, 0}},
        {{0, 0, 1}, {0, 1, 0}, {0, 1}},
    };
    std::vector<uint32_t> indices = {0, 1, 2};
    auto mesh = gg::Mesh::create(*gpu_, verts, indices);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->index_count(), 3u);
    EXPECT_EQ(mesh->vertex_count(), 3u);
    EXPECT_NE(mesh->vertex_buffer(), nullptr);
    EXPECT_NE(mesh->index_buffer(), nullptr);
}

TEST_F(MeshTest, EmptyMeshIsNull) {
    std::vector<gg::Vertex> verts;
    std::vector<uint32_t> indices;
    auto mesh = gg::Mesh::create(*gpu_, verts, indices);
    EXPECT_EQ(mesh, nullptr);
}
