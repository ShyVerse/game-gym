engine_sources = files(
  'core/window.cpp',
  'renderer/gpu_context.cpp',
  'renderer/renderer.cpp',
)

engine_inc = include_directories('.')

engine_lib = shared_library('game-gym-engine',
  engine_sources,
  include_directories: engine_inc,
  dependencies: [glfw_dep, wgpu_dep, glfw3webgpu_dep],
  install: true,
)

engine_dep = declare_dependency(
  include_directories: engine_inc,
  link_with: engine_lib,
  dependencies: [glfw_dep, wgpu_dep, glfw3webgpu_dep],
)
