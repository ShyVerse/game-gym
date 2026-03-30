#include "script/script_manager.h"
#include "script/file_watcher.h"
#include "script/script_engine.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace gg {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct ScriptManager::Impl {
    ScriptEngine& engine;
    std::string script_dir;
    std::unique_ptr<FileWatcher> watcher;
    std::set<std::string> loaded_scripts;

    Impl(ScriptEngine& e, std::string dir)
        : engine(e), script_dir(std::move(dir)) {}

    // Read an entire file into a string. Returns empty on failure.
    static std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return {};
        }
        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
    }

    // Escape a path for safe use inside a single-quoted shell argument.
    static std::string shell_escape(const std::string& s) {
        std::string result = "'";
        for (char c : s) {
            if (c == '\'') {
                result += "'\\''";
            } else {
                result += c;
            }
        }
        result += "'";
        return result;
    }

    // Compile a .ts file to .js using tsc.
    // Returns the path of the generated .js file, or empty on failure.
    static std::string compile_typescript(const std::string& ts_path) {
        std::ostringstream cmd;
        cmd << "npx tsc --strict --noImplicitAny --target ES2020"
            << " --module ES2020 " << shell_escape(ts_path) << " 2>&1";
        int rc = std::system(cmd.str().c_str());
        if (rc != 0) {
            return {};
        }
        // tsc writes .js next to the .ts file.
        fs::path js_path = ts_path;
        js_path.replace_extension(".js");
        if (fs::exists(js_path)) {
            return js_path.string();
        }
        return {};
    }
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ScriptManager::ScriptManager(ScriptEngine& engine, std::string script_dir)
    : impl_(std::make_unique<Impl>(engine, std::move(script_dir))) {}

ScriptManager::~ScriptManager() = default;

std::unique_ptr<ScriptManager> ScriptManager::create(
        ScriptEngine& engine, const std::string& script_dir) {
    auto mgr = std::unique_ptr<ScriptManager>(
        new ScriptManager(engine, script_dir));
    mgr->impl_->watcher = FileWatcher::create(script_dir);
    return mgr;
}

// ---------------------------------------------------------------------------
// load_all()
// ---------------------------------------------------------------------------

void ScriptManager::load_all() {
    if (!fs::is_directory(impl_->script_dir)) {
        return;
    }

    // Collect all .js files and sort alphabetically for deterministic order.
    std::vector<std::string> js_files;
    for (const auto& entry : fs::directory_iterator(impl_->script_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".js") {
            js_files.push_back(entry.path().string());
        }
    }
    std::sort(js_files.begin(), js_files.end());

    for (const auto& path : js_files) {
        std::string source = Impl::read_file(path);
        if (source.empty()) {
            continue;
        }

        auto result = impl_->engine.execute(source, path);
        if (!result.ok) {
            continue;
        }

        impl_->loaded_scripts.insert(path);

        // Call onInit() if the script defines it.
        impl_->engine.call_function("onInit");
    }
}

// ---------------------------------------------------------------------------
// poll_changes()
// ---------------------------------------------------------------------------

void ScriptManager::poll_changes() {
    if (!impl_->watcher) {
        return;
    }

    auto changed = impl_->watcher->poll_changes();

    for (const auto& path : changed) {
        fs::path file_path(path);
        auto ext = file_path.extension().string();

        if (ext == ".ts") {
            // Compile TypeScript, then reload the generated .js.
            std::string js_path = Impl::compile_typescript(path);
            if (!js_path.empty()) {
                reload(js_path);
            }
        } else if (ext == ".js") {
            reload(path);
        }
    }
}

// ---------------------------------------------------------------------------
// reload()
// ---------------------------------------------------------------------------

void ScriptManager::reload(const std::string& path) {
    // Call onDestroy() on the old module if it was previously loaded,
    // then remove from loaded_scripts immediately so a failed re-execute
    // does not leave a stale entry.
    if (impl_->loaded_scripts.count(path) > 0) {
        impl_->engine.call_function("onDestroy");
        impl_->loaded_scripts.erase(path);
    }

    // Read and execute the new source.
    std::string source = Impl::read_file(path);
    if (source.empty()) {
        return;
    }

    auto result = impl_->engine.execute(source, path);
    if (!result.ok) {
        return;
    }

    impl_->loaded_scripts.insert(path);

    // Call onInit() on the freshly loaded script.
    impl_->engine.call_function("onInit");
}

// ---------------------------------------------------------------------------
// loaded_count()
// ---------------------------------------------------------------------------

size_t ScriptManager::loaded_count() const {
    return impl_->loaded_scripts.size();
}

} // namespace gg
