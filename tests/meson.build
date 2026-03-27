if get_option('enable_tests')
  gtest_dep = dependency('gtest', fallback: ['gtest', 'gtest_dep'],
    required: true)
  gtest_main_dep = dependency('gtest_main', fallback: ['gtest', 'gtest_main_dep'],
    required: true)

  test_window = executable('test_window',
    'test_window.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('window', test_window)

  test_gpu = executable('test_gpu_context',
    'test_gpu_context.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('gpu_context', test_gpu)

  test_ecs = executable('test_ecs',
    'test_ecs.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('ecs', test_ecs)

  test_physics = executable('test_physics',
    'test_physics.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('physics', test_physics)

  test_compute = executable('test_compute',
    'test_compute.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('compute', test_compute,
    workdir: meson.source_root(),
  )

  test_editor = executable('test_editor',
    'test_editor.cpp',
    dependencies: [engine_dep, imgui_dep, gtest_dep, gtest_main_dep],
  )
  test('editor', test_editor,
    workdir: meson.source_root(),
  )
endif
