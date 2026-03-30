#include "script/file_watcher.h"
#include "script/script_engine.h"
#include "script/script_manager.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class ScriptManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "gg_test_scripts";
        fs::remove_all(tmp_dir_);
        fs::create_directories(tmp_dir_);
        engine_ = gg::ScriptEngine::create();
    }

    void TearDown() override {
        manager_.reset();
        engine_.reset();
        fs::remove_all(tmp_dir_);
    }

    void write_js(const std::string& name, const std::string& content) {
        std::ofstream f(tmp_dir_ / name);
        f << content;
    }

    fs::path tmp_dir_;
    std::unique_ptr<gg::ScriptEngine> engine_;
    std::unique_ptr<gg::ScriptManager> manager_;
};

// ---------------------------------------------------------------------------
// FileWatcher tests
// ---------------------------------------------------------------------------

TEST(FileWatcherTest, CreatesSuccessfully) {
    auto tmp = fs::temp_directory_path() / "gg_fw_test";
    fs::create_directories(tmp);
    auto watcher = gg::FileWatcher::create(tmp.string());
    ASSERT_NE(watcher, nullptr);
    EXPECT_EQ(watcher->directory(), tmp.string());
    fs::remove_all(tmp);
}

// ---------------------------------------------------------------------------
// ScriptManager tests
// ---------------------------------------------------------------------------

TEST_F(ScriptManagerTest, CreatesSuccessfully) {
    manager_ = gg::ScriptManager::create(*engine_, tmp_dir_.string());
    ASSERT_NE(manager_, nullptr);
    EXPECT_EQ(manager_->loaded_count(), 0u);
}

TEST_F(ScriptManagerTest, LoadAllFindsJsFiles) {
    write_js("hello.js",
             "function onInit() { globalThis.__hello = 'world'; }\n");

    manager_ = gg::ScriptManager::create(*engine_, tmp_dir_.string());
    manager_->load_all();

    EXPECT_EQ(manager_->loaded_count(), 1u);

    // Verify the function was defined in the engine.
    auto result = engine_->call_function("onInit");
    EXPECT_TRUE(result.ok);
}

TEST_F(ScriptManagerTest, LoadAllCallsOnInit) {
    write_js("init_test.js",
             "function onInit() { globalThis.__initCalled = true; }\n");

    manager_ = gg::ScriptManager::create(*engine_, tmp_dir_.string());
    manager_->load_all();

    auto result = engine_->execute("globalThis.__initCalled");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.value, "true");
}

TEST_F(ScriptManagerTest, ReloadCallsOnDestroyThenOnInit) {
    // Initial script: sets __version to 1 on init, sets __destroyed on destroy.
    write_js("versioned.js",
             "function onInit() { globalThis.__version = 1; }\n"
             "function onDestroy() { globalThis.__destroyed = true; }\n");

    manager_ = gg::ScriptManager::create(*engine_, tmp_dir_.string());
    manager_->load_all();

    // Verify initial state.
    {
        auto r = engine_->execute("globalThis.__version");
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.value, "1");
    }

    // Overwrite the file with a new version.
    write_js("versioned.js",
             "function onInit() { globalThis.__version = 2; }\n"
             "function onDestroy() { globalThis.__destroyed = true; }\n");

    // Trigger reload.
    auto script_path = (tmp_dir_ / "versioned.js").string();
    manager_->reload(script_path);

    // onDestroy should have been called (sets __destroyed).
    {
        auto r = engine_->execute("globalThis.__destroyed");
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.value, "true");
    }

    // onInit of the new version should have been called (sets __version to 2).
    {
        auto r = engine_->execute("globalThis.__version");
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.value, "2");
    }
}
