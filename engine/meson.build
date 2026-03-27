flecs_dep = dependency('flecs', fallback: ['flecs', 'flecs_dep'], required: true)

engine_sources = files(
  'core/window.cpp',
  'core/engine.cpp',
  'renderer/gpu_context.cpp',
  'renderer/renderer.cpp',
  'ecs/world.cpp',
  'ecs/systems.cpp',
)

engine_inc = include_directories('.')

engine_lib = shared_library('game-gym-engine',
  engine_sources,
  include_directories: engine_inc,
  dependencies: [glfw_dep, wgpu_dep, glfw3webgpu_dep, flecs_dep],
  install: true,
)

engine_dep = declare_dependency(
  include_directories: engine_inc,
  link_with: engine_lib,
  dependencies: [glfw_dep, wgpu_dep, glfw3webgpu_dep, flecs_dep],
)
