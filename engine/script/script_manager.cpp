#include "script/script_manager.h"

namespace gg {

struct ScriptManager::Impl {};

ScriptManager::ScriptManager() : impl_(std::make_unique<Impl>()) {}
ScriptManager::~ScriptManager() = default;

std::unique_ptr<ScriptManager> ScriptManager::create(
        ScriptEngine& /*engine*/, const std::string& /*script_dir*/) {
    return std::unique_ptr<ScriptManager>(new ScriptManager());
}

void ScriptManager::load_all() {}
void ScriptManager::poll_changes() {}
void ScriptManager::reload(const std::string& /*path*/) {}
size_t ScriptManager::loaded_count() const { return 0; }

} // namespace gg
