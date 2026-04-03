#include "script/script_manager.h"

#include "script/file_watcher.h"
#include "script/script_engine.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <set>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace gg {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct ScriptManager::Impl {
    struct PendingCompile {
        std::string source_path;
        std::future<std::string> future;
        bool dirty = false;
    };

    ScriptEngine& engine;
    std::string script_dir;
    std::unique_ptr<FileWatcher> watcher;
    std::set<std::string> loaded_scripts;
    std::vector<PendingCompile> pending_compiles;

    Impl(ScriptEngine& e, std::string dir) : engine(e), script_dir(std::move(dir)) {}

    static std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return {};
        }
        std::ostringstream oss;
        oss << file.rdbuf();
        if (file.bad()) {
            return {};
        }
        return oss.str();
    }

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

    static std::string compile_typescript(const std::string& ts_path) {
        std::ostringstream cmd;
        cmd << "npx --yes -p typescript tsc --strict --noImplicitAny --pretty false --target ES2020"
            << " --module ES2020 " << shell_escape(ts_path) << " 2>&1";
        int rc = std::system(cmd.str().c_str());
        if (rc != 0) {
            return {};
        }
        fs::path js_path = ts_path;
        js_path.replace_extension(".js");
        if (fs::exists(js_path)) {
            return js_path.string();
        }
        return {};
    }

    static std::string resolve_load_path(const std::string& path) {
        fs::path file_path(path);
        const auto ext = file_path.extension().string();
        if (ext == ".ts") {
            return compile_typescript(path);
        }
        if (ext == ".js") {
            return path;
        }
        return {};
    }

    // Escape a string for safe use as a JS string literal inside double quotes.
    static std::string js_escape(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\\') {
                result += "\\\\";
            } else if (c == '"') {
                result += "\\\"";
            } else if (c == '\n') {
                result += "\\n";
            } else {
                result += c;
            }
        }
        return result;
    }

    // After executing a script, capture its lifecycle functions under a
    // per-path key in globalThis.__gg_scripts, then clear the globals
    // so the next script's definitions don't collide.
    void capture_lifecycle(const std::string& path) {
        const std::string key = js_escape(path);
        const std::string js = "globalThis.__gg_scripts = globalThis.__gg_scripts || {};\n"
                               "globalThis.__gg_scripts[\"" +
                               key +
                               "\"] = {\n"
                               "  onInit: typeof onInit === 'function' ? onInit : null,\n"
                               "  onUpdate: typeof onUpdate === 'function' ? onUpdate : null,\n"
                               "  onDestroy: typeof onDestroy === 'function' ? onDestroy : null,\n"
                               "};\n"
                               "if (typeof onInit === 'function') onInit = undefined;\n"
                               "if (typeof onUpdate === 'function') onUpdate = undefined;\n"
                               "if (typeof onDestroy === 'function') onDestroy = undefined;\n";
        engine.execute(js, "<capture:" + path + ">");
    }

    void call_script_lifecycle(const std::string& path, const std::string& fn) {
        const std::string key = js_escape(path);
        const std::string js = "(function() {\n"
                               "  var s = globalThis.__gg_scripts && globalThis.__gg_scripts[\"" +
                               key +
                               "\"];\n"
                               "  if (s && typeof s." +
                               fn + " === 'function') { s." + fn +
                               "(); }\n"
                               "})()";
        engine.execute(js, "<lifecycle:" + fn + ">");
    }

    void call_all_updates(float dt) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(dt));
        const std::string js =
            "(function() {\n"
            "  var scripts = globalThis.__gg_scripts;\n"
            "  if (!scripts) return;\n"
            "  var dt = " +
            std::string(buf) +
            ";\n"
            "  for (var key in scripts) {\n"
            "    if (scripts[key] && typeof scripts[key].onUpdate === 'function') {\n"
            "      scripts[key].onUpdate(dt);\n"
            "    }\n"
            "  }\n"
            "})()";
        engine.execute(js, "<onUpdate>");
    }

    void remove_script_entry(const std::string& path) {
        const std::string key = js_escape(path);
        engine.execute("if (globalThis.__gg_scripts) delete globalThis.__gg_scripts[\"" + key +
                           "\"];",
                       "<remove:" + path + ">");
    }

    void load_script_file(const std::string& source_path) {
        const std::string load_path = resolve_load_path(source_path);
        if (load_path.empty()) {
            return;
        }

        std::string source = read_file(load_path);
        if (source.empty()) {
            return;
        }

        auto result = engine.execute(source, load_path);
        if (!result.ok) {
            return;
        }

        loaded_scripts.insert(load_path);
        capture_lifecycle(load_path);
        call_script_lifecycle(load_path, "onInit");
    }

    void enqueue_compile(const std::string& source_path) {
        auto it = std::find_if(
            pending_compiles.begin(), pending_compiles.end(), [&](const PendingCompile& pending) {
                return pending.source_path == source_path;
            });
        if (it != pending_compiles.end()) {
            it->dirty = true;
            return;
        }

        pending_compiles.push_back(
            {.source_path = source_path,
             .future = std::async(std::launch::async,
                                  [source_path] { return compile_typescript(source_path); }),
             .dirty = false});
    }

    void drain_completed_compiles(ScriptManager& manager) {
        std::vector<std::string> requeue;
        auto it = pending_compiles.begin();
        while (it != pending_compiles.end()) {
            const auto status = it->future.wait_for(std::chrono::milliseconds(0));
            if (status != std::future_status::ready) {
                ++it;
                continue;
            }

            const std::string js_path = it->future.get();
            if (!js_path.empty()) {
                manager.reload(js_path);
            }
            if (it->dirty) {
                requeue.push_back(it->source_path);
            }
            it = pending_compiles.erase(it);
        }
        for (const auto& path : requeue) {
            enqueue_compile(path);
        }
    }
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ScriptManager::ScriptManager(ScriptEngine& engine, std::string script_dir)
    : impl_(std::make_unique<Impl>(engine, std::move(script_dir))) {}

ScriptManager::~ScriptManager() = default;

std::unique_ptr<ScriptManager> ScriptManager::create(ScriptEngine& engine,
                                                     const std::string& script_dir) {
    auto mgr = std::unique_ptr<ScriptManager>(new ScriptManager(engine, script_dir));
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

    std::vector<std::string> script_files;
    for (const auto& entry : fs::directory_iterator(impl_->script_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto ext = entry.path().extension().string();
        if (ext == ".js" || ext == ".ts") {
            script_files.push_back(entry.path().string());
        }
    }
    std::sort(script_files.begin(), script_files.end());

    for (const auto& path : script_files) {
        impl_->load_script_file(path);
    }
}

void ScriptManager::load_paths(const std::vector<std::string>& paths) {
    std::vector<std::string> unique_paths = paths;
    std::sort(unique_paths.begin(), unique_paths.end());
    unique_paths.erase(std::unique(unique_paths.begin(), unique_paths.end()), unique_paths.end());

    for (const auto& path : unique_paths) {
        impl_->load_script_file(path);
    }
}

// ---------------------------------------------------------------------------
// poll_changes()
// ---------------------------------------------------------------------------

void ScriptManager::poll_changes() {
    if (!impl_->watcher) {
        return;
    }

    impl_->drain_completed_compiles(*this);
    auto changed = impl_->watcher->poll_changes();

    for (const auto& path : changed) {
        fs::path file_path(path);
        auto ext = file_path.extension().string();

        if (ext == ".ts") {
            impl_->enqueue_compile(path);
        } else if (ext == ".js") {
            reload(path);
        }
    }
}

// ---------------------------------------------------------------------------
// reload()
// ---------------------------------------------------------------------------

void ScriptManager::reload(const std::string& path) {
    const std::string load_path = Impl::resolve_load_path(path);
    if (load_path.empty()) {
        return;
    }

    if (impl_->loaded_scripts.count(load_path) > 0) {
        impl_->call_script_lifecycle(load_path, "onDestroy");
        impl_->remove_script_entry(load_path);
        impl_->loaded_scripts.erase(load_path);
    }

    std::string source = Impl::read_file(load_path);
    if (source.empty()) {
        return;
    }

    auto result = impl_->engine.execute(source, load_path);
    if (!result.ok) {
        return;
    }

    impl_->loaded_scripts.insert(load_path);
    impl_->capture_lifecycle(load_path);
    impl_->call_script_lifecycle(load_path, "onInit");
}

// ---------------------------------------------------------------------------
// call_update()
// ---------------------------------------------------------------------------

void ScriptManager::call_update(float dt) {
    impl_->call_all_updates(dt);
}

// ---------------------------------------------------------------------------
// loaded_count()
// ---------------------------------------------------------------------------

size_t ScriptManager::loaded_count() const {
    return impl_->loaded_scripts.size();
}

} // namespace gg
