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

# wgpu-native release zips place headers under include/webgpu/.
# Expose both the parent (for #include <webgpu/webgpu.h>) and the
# subdirectory (for #include <webgpu.h> used by glfw3webgpu).
wgpu_inc = include_directories('wgpu-native/include', 'wgpu-native/include/webgpu')
if host_os == 'darwin'
  wgpu_lib = cpp.find_library('wgpu_native',
    dirs: [meson.project_source_root() / 'wgpu-native' / 'lib'],
    required: true)
  platform_deps = [
    dependency('appleframeworks', modules: [
      'Metal', 'QuartzCore', 'Cocoa', 'CoreGraphics',
      'IOKit', 'CoreFoundation',
    ]),
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

# On macOS, glfw3webgpu uses Foundation/QuartzCore (Objective-C).
# Use .m file on macOS, .c on other platforms.
if host_os == 'darwin'
  add_languages('objc', native: false)
  glfw3webgpu_lib = static_library('glfw3webgpu',
    'third_party/glfw3webgpu/glfw3webgpu_macos.m',
    include_directories: [
      include_directories('third_party/glfw3webgpu'),
      wgpu_inc,
    ],
    dependencies: [glfw_dep, dependency('appleframeworks', modules: ['Foundation', 'QuartzCore'])],
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

# --- Dear ImGui -----------------------------------------------------------
imgui_inc = [
  include_directories('third_party/imgui'),
  include_directories('third_party/imgui/backends'),
  wgpu_inc,
]

imgui_common_sources = files(
  'third_party/imgui/imgui.cpp',
  'third_party/imgui/imgui_draw.cpp',
  'third_party/imgui/imgui_tables.cpp',
  'third_party/imgui/imgui_widgets.cpp',
  'third_party/imgui/imgui_demo.cpp',
  'third_party/imgui/backends/imgui_impl_glfw.cpp',
)

# imgui_impl_wgpu.cpp contains Objective-C++ code on macOS (CAMetalLayer etc.).
# Pass -x objective-c++ so the C++ compiler treats the .cpp as ObjC++ on macOS.
if host_os == 'darwin'
  imgui_wgpu_lib = static_library('imgui_wgpu_backend',
    'third_party/imgui/backends/imgui_impl_wgpu.cpp',
    include_directories: imgui_inc,
    dependencies: [glfw_dep, dependency('appleframeworks', modules: ['Cocoa', 'QuartzCore'])],
    cpp_args: ['-DIMGUI_IMPL_WEBGPU_BACKEND_WGPU', '-x', 'objective-c++'],
  )
else
  imgui_wgpu_lib = static_library('imgui_wgpu_backend',
    'third_party/imgui/backends/imgui_impl_wgpu.cpp',
    include_directories: imgui_inc,
    dependencies: [glfw_dep],
    cpp_args: ['-DIMGUI_IMPL_WEBGPU_BACKEND_WGPU'],
  )
endif

imgui_lib = static_library('imgui',
  imgui_common_sources,
  include_directories: imgui_inc,
  dependencies: [glfw_dep],
  cpp_args: ['-DIMGUI_IMPL_WEBGPU_BACKEND_WGPU'],
)

imgui_dep = declare_dependency(
  include_directories: [
    include_directories('third_party/imgui'),
    include_directories('third_party/imgui/backends'),
  ],
  link_with: [imgui_lib, imgui_wgpu_lib],
  compile_args: ['-DIMGUI_IMPL_WEBGPU_BACKEND_WGPU'],
)

# --- V8 JavaScript engine (optional) ----------------------------------------
enable_scripts = get_option('enable_scripts')
if enable_scripts
  v8_inc = include_directories('v8/include')
  v8_lib = cpp.find_library('v8_monolith',
    dirs: [meson.project_source_root() / 'v8' / 'lib'],
    required: true)

  if host_os == 'darwin'
    v8_platform_deps = [
      dependency('appleframeworks', modules: ['CoreFoundation', 'CoreServices']),
    ]
  elif host_os == 'linux'
    v8_platform_deps = [
      dependency('threads'),
      cpp.find_library('dl', required: true),
    ]
  else
    v8_platform_deps = []
  endif

  v8_dep = declare_dependency(
    include_directories: v8_inc,
    dependencies: [v8_lib] + v8_platform_deps,
  )
else
  v8_dep = dependency('', required: false)
endif

generated_inc = include_directories('generated')

subdir('engine')
subdir('app')
subdir('tests')
