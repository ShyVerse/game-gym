flecs_dep = dependency('flecs', fallback: ['flecs', 'flecs_dep'], required: true)
jolt_dep = dependency('Jolt', fallback: ['jolt', 'Jolt_dep'], required: true)
nlohmann_json_dep = dependency('nlohmann_json', fallback: ['nlohmann_json', 'nlohmann_json_dep'], required: true)

engine_sources = files(
  'core/window.cpp',
  'core/engine.cpp',
  'renderer/gpu_context.cpp',
  'renderer/renderer.cpp',
  'ecs/world.cpp',
  'ecs/systems.cpp',
  'physics/physics_layers.cpp',
  'physics/physics_world.cpp',
  'compute/gpu_buffer.cpp',
  'compute/compute_pipeline.cpp',
  'renderer/camera.cpp',
  'renderer/mesh.cpp',
  'editor/editor_ui.cpp',
  'mcp/mcp_server.cpp',
  'mcp/mcp_tools.cpp',
  'mcp/mcp_stdio_transport.cpp',
)

engine_inc = include_directories('.')

# Jolt is compiled with these defines in all non-release configs; we must match
# them exactly or RegisterTypes() will abort on version-mismatch detection.
jolt_client_defines = [
  '-DJPH_DEBUG_RENDERER',
  '-DJPH_PROFILE_ENABLED',
]

# Gather all base dependencies
engine_base_deps = [glfw_dep, wgpu_dep, glfw3webgpu_dep, flecs_dep, jolt_dep, imgui_dep, nlohmann_json_dep]

# Script sources (only compiled when V8 is enabled)
if enable_scripts
  engine_sources += files(
    'script/script_engine.cpp',
    'script/script_bindings.cpp',
    'script/script_manager.cpp',
  )
  if host_os == 'darwin'
    engine_sources += files('script/file_watcher_macos.cpp')
  elif host_os == 'linux'
    engine_sources += files('script/file_watcher_linux.cpp')
  endif
  engine_base_deps += v8_dep
  engine_script_args = ['-DGG_ENABLE_SCRIPTS=1']
else
  engine_script_args = []
endif

engine_lib = shared_library('game-gym-engine',
  engine_sources,
  include_directories: [engine_inc, generated_inc],
  cpp_args: jolt_client_defines + engine_script_args,
  dependencies: engine_base_deps,
  install: true,
)

engine_dep = declare_dependency(
  include_directories: [engine_inc, generated_inc],
  link_with: engine_lib,
  dependencies: engine_base_deps,
  compile_args: engine_script_args,
)
