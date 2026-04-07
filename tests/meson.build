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

  test_mcp = executable('test_mcp',
    'test_mcp.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('mcp', test_mcp)

  test_mcp_sse = executable('test_mcp_sse',
    'test_mcp_sse.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('mcp_sse', test_mcp_sse)

  test_mat4 = executable('test_mat4',
    'test_mat4.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('mat4', test_mat4)

  test_window_input = executable('test_window_input',
    'test_window_input.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('window_input', test_window_input)

  test_camera = executable('test_camera',
    'test_camera.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('camera', test_camera)

  test_mesh = executable('test_mesh',
    'test_mesh.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('mesh', test_mesh)

  test_gltf_loader = executable('test_gltf_loader',
    'test_gltf_loader.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('gltf_loader', test_gltf_loader)

  test_project_config = executable('test_project_config',
    'test_project_config.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('project_config', test_project_config)

  test_scene_serializer = executable('test_scene_serializer',
    'test_scene_serializer.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('scene_serializer', test_scene_serializer)

  test_asset_paths = executable('test_asset_paths',
    'test_asset_paths.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('asset_paths', test_asset_paths)

  test_scene_loader = executable('test_scene_loader',
    'test_scene_loader.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('scene_loader', test_scene_loader)

  test_version_info = executable('test_version_info',
    'test_version_info.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('version_info', test_version_info)

  test_vec3 = executable('test_vec3',
    'test_vec3.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('vec3', test_vec3)

  test_mat4_inverse = executable('test_mat4_inverse',
    'test_mat4_inverse.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('mat4_inverse', test_mat4_inverse)

  test_ray = executable('test_ray',
    'test_ray.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('ray', test_ray)

  test_gizmo_interaction = executable('test_gizmo_interaction',
    'test_gizmo_interaction.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('gizmo_interaction', test_gizmo_interaction)

  test_shader_utils = executable('test_shader_utils',
    'test_shader_utils.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('shader_utils', test_shader_utils)

  if enable_scripts
    test_script_engine = executable('test_script_engine',
      'test_script_engine.cpp',
      dependencies: [engine_dep, gtest_dep, gtest_main_dep],
    )
    test('script_engine', test_script_engine)

    test_script_types = executable('test_script_types',
      'test_script_types.cpp',
      dependencies: [engine_dep, gtest_dep, gtest_main_dep],
    )
    test('script_types', test_script_types)

    test_script_bindings = executable('test_script_bindings',
      'test_script_bindings.cpp',
      dependencies: [engine_dep, gtest_dep, gtest_main_dep],
    )
    test('script_bindings', test_script_bindings)

    test_script_manager = executable('test_script_manager',
      'test_script_manager.cpp',
      dependencies: [engine_dep, gtest_dep, gtest_main_dep],
    )
    test('script_manager', test_script_manager)

    test_script_integration = executable('test_script_integration',
      'test_script_integration.cpp',
      dependencies: [engine_dep, gtest_dep, gtest_main_dep],
    )
    test('script_integration', test_script_integration)
  endif
endif
