#pragma once

#include <memory>
#include <string>

namespace gg {

class ScriptEngine;

class ScriptManager {
public:
    static std::unique_ptr<ScriptManager> create(ScriptEngine& engine,
                                                  const std::string& script_dir);
    ~ScriptManager();

    ScriptManager(const ScriptManager&) = delete;
    ScriptManager& operator=(const ScriptManager&) = delete;
    ScriptManager(ScriptManager&&) = delete;
    ScriptManager& operator=(ScriptManager&&) = delete;

    void load_all();
    void poll_changes();
    void reload(const std::string& path);
    void call_update(float dt);
    [[nodiscard]] size_t loaded_count() const;

private:
    ScriptManager(ScriptEngine& engine, std::string script_dir);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gg
