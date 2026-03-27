project('game-gym', 'cpp', 'c',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'c_std=c11',
    'default_library=shared',
    'warning_level=2',
  ],
)

host_os = host_machine.system()
cpp = meson.get_compiler('cpp')

glfw_dep = dependency('glfw3', fallback: ['glfw', 'glfw_dep'])

wgpu_inc = include_directories('wgpu-native/include')
if host_os == 'darwin'
  wgpu_lib = cpp.find_library('wgpu_native',
    dirs: [meson.project_source_root() / 'wgpu-native' / 'lib'],
    required: true)
  platform_deps = [
    dependency('appleframeworks', modules: ['Metal', 'QuartzCore']),
  ]
elif host_os == 'linux'
  wgpu_lib = cpp.find_library('wgpu_native',
    dirs: [meson.project_source_root() / 'wgpu-native' / 'lib'],
    required: true)
  platform_deps = []
elif host_os == 'windows'
  wgpu_lib = cpp.find_library('wgpu_native',
    dirs: [meson.project_source_root() / 'wgpu-native' / 'lib'],
    required: true)
  platform_deps = []
endif

wgpu_dep = declare_dependency(
  include_directories: wgpu_inc,
  dependencies: [wgpu_lib] + platform_deps,
)

# On macOS, glfw3webgpu.c uses Foundation/QuartzCore (Objective-C APIs).
# We compile it as Objective-C by adding the 'objc' language and using
# override_options to treat the .c file as objc.
if host_os == 'darwin'
  add_languages('objc', native: false)
  glfw3webgpu_lib = static_library('glfw3webgpu',
    'third_party/glfw3webgpu/glfw3webgpu.c',
    include_directories: [
      include_directories('third_party/glfw3webgpu'),
      wgpu_inc,
    ],
    dependencies: [glfw_dep],
    override_options: ['c_std=none'],
    objc_args: ['-x', 'objective-c'],
  )
else
  glfw3webgpu_lib = static_library('glfw3webgpu',
    'third_party/glfw3webgpu/glfw3webgpu.c',
    include_directories: [
      include_directories('third_party/glfw3webgpu'),
      wgpu_inc,
    ],
    dependencies: [glfw_dep],
  )
endif

glfw3webgpu_dep = declare_dependency(
  include_directories: include_directories('third_party/glfw3webgpu'),
  link_with: glfw3webgpu_lib,
)

subdir('engine')
subdir('app')
subdir('tests')
