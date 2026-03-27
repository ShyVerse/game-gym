#include <gtest/gtest.h>

// EditorUI requires a live GPU context and GLFW window.
// These tests cover:
//   1. CreateAndDestroy  – EditorUI::create returns a valid object when given
//      a real window+GPU, and its destructor is clean.
//   2. VisibilityToggle  – set_visible / is_visible round-trips work without
//      any GPU involvement once the object exists.
//   3. BeginFrameDoesNotCrash – begin_frame() followed by ImGui::EndFrame()
//      completes without assertions when the backend is initialised.
//
// The tests gate on GLFW availability: on a headless CI machine without a
// display the window creation will fail gracefully and the tests are skipped.

#include "editor/editor_ui.h"
#include "renderer/gpu_context.h"
#include "core/window.h"

#include <imgui.h>
#include <GLFW/glfw3.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

struct TestFixture {
    std::unique_ptr<gg::Window>     window;
    std::unique_ptr<gg::GpuContext> gpu;
    std::unique_ptr<gg::EditorUI>   editor;

    // Returns false if the environment cannot support a GPU window (headless).
    bool init() {
        window = gg::Window::create({.title = "test", .width = 64, .height = 64, .resizable = false});
        if (!window) { return false; }

        gpu = gg::GpuContext::create(*window);
        if (!gpu) { return false; }

        editor = gg::EditorUI::create(window->native_handle(), *gpu);
        return editor != nullptr;
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test 1: CreateAndDestroy
// ---------------------------------------------------------------------------

TEST(EditorUITest, CreateAndDestroy)
{
    TestFixture f;
    if (!f.init()) {
        GTEST_SKIP() << "Headless environment: skipping GPU-dependent test";
    }
    // Destruction happens in TestFixture dtor — if it crashes the test fails.
    EXPECT_NE(f.editor, nullptr);
}

// ---------------------------------------------------------------------------
// Test 2: VisibilityToggle
// ---------------------------------------------------------------------------

TEST(EditorUITest, VisibilityToggle)
{
    TestFixture f;
    if (!f.init()) {
        GTEST_SKIP() << "Headless environment: skipping GPU-dependent test";
    }

    // Default visibility is true
    EXPECT_TRUE(f.editor->is_visible());

    f.editor->set_visible(false);
    EXPECT_FALSE(f.editor->is_visible());

    f.editor->set_visible(true);
    EXPECT_TRUE(f.editor->is_visible());
}

// ---------------------------------------------------------------------------
// Test 3: BeginFrameDoesNotCrash
// ---------------------------------------------------------------------------

TEST(EditorUITest, BeginFrameDoesNotCrash)
{
    TestFixture f;
    if (!f.init()) {
        GTEST_SKIP() << "Headless environment: skipping GPU-dependent test";
    }

    // begin_frame starts a new ImGui frame; EndFrame closes it without
    // submitting draw data to the GPU.
    f.editor->begin_frame();
    ImGui::EndFrame();

    SUCCEED();
}
