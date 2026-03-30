#pragma once

namespace gg {

class ScriptEngine;
class World;
class PhysicsWorld;

void register_script_bindings(ScriptEngine& engine, World& world, PhysicsWorld& physics);

} // namespace gg
