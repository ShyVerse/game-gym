#pragma once

#include <functional>
#include <memory>
#include <string>

namespace gg {

struct ScriptResult {
    bool ok = false;
    std::string value;
    std::string error;
};

class ScriptEngine {
public:
    static std::unique_ptr<ScriptEngine> create();
    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;
    ScriptEngine(ScriptEngine&&) = delete;
    ScriptEngine& operator=(ScriptEngine&&) = delete;

    ScriptResult execute(const std::string& source, const std::string& filename = "<eval>");
    ScriptResult execute_module(const std::string& path);
    ScriptResult call_function(const std::string& name, const std::string& args_json = "[]");

    using NativeCallback = std::function<std::string(const std::string& args_json)>;
    void register_function(const std::string& name, NativeCallback callback);

    void idle_gc(double deadline_seconds);
    void shutdown();
    [[nodiscard]] bool is_alive() const;

private:
    ScriptEngine();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gg
