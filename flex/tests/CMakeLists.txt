# ============================================================================
# Test Suites - Link only required targets for faster compilation
# ============================================================================

enable_testing()

# ----------------------------------------------------------------------------
# Foundation Tests (no scene, DSL, window, or GPU dependency)
# ----------------------------------------------------------------------------
add_executable(test_animation test_animation.cpp)
target_link_libraries(test_animation PRIVATE flex_animation Salts::TinyTest)
set_target_properties(test_animation PROPERTIES FOLDER "flex/tests")
add_test(NAME test_animation COMMAND test_animation)

add_executable(test_render test_render.cpp)
target_link_libraries(test_render PRIVATE flex_render Salts::TinyTest)
set_target_properties(test_render PROPERTIES FOLDER "flex/tests")
add_test(NAME test_render COMMAND test_render)

if(TARGET flex_render_engine_gcanvas)
  add_executable(test_gcanvas_renderer test_gcanvas_renderer.cpp)
  target_link_libraries(
    test_gcanvas_renderer
    PRIVATE flex_render_engine_gcanvas Salts::TinyTest)
  set_target_properties(
    test_gcanvas_renderer PROPERTIES FOLDER "flex/tests")
  add_test(NAME test_gcanvas_renderer COMMAND test_gcanvas_renderer)
endif()

if(TARGET flex_backend_opengl AND TARGET flex_backend_vulkan AND
   TARGET flex_backend_tui AND TARGET flex_backend_d2d)
  add_executable(test_backend_modules test_backend_modules.cpp)
  target_link_libraries(
    test_backend_modules
    PRIVATE flex_backend_opengl flex_backend_vulkan flex_backend_tui
            flex_backend_d2d Salts::TinyTest)
  set_target_properties(test_backend_modules PROPERTIES FOLDER "flex/tests")
  add_test(NAME test_backend_modules COMMAND test_backend_modules)
endif()

# ----------------------------------------------------------------------------
# Compiler Tests (lexer/parser only)
# ----------------------------------------------------------------------------
add_executable(test_lexer_parser test_lexer_parser.cpp)
target_link_libraries(test_lexer_parser PRIVATE flex_compiler Salts::TinyTest)
target_include_directories(test_lexer_parser PRIVATE ../include)
set_target_properties(test_lexer_parser PROPERTIES FOLDER "flex/tests")
add_test(NAME test_lexer_parser COMMAND test_lexer_parser)

# ----------------------------------------------------------------------------
# Runtime Tests (runtime system only) - Clean version
# ----------------------------------------------------------------------------
add_executable(test_runtime test_runtime.cpp)
target_link_libraries(test_runtime PRIVATE flex_runtime Salts::TinyTest)
target_include_directories(test_runtime PRIVATE ../include)
set_target_properties(test_runtime PROPERTIES FOLDER "flex/tests")
add_test(NAME test_runtime COMMAND test_runtime)

add_executable(test_property_cmeta test_property_cmeta.cpp)
target_link_libraries(test_property_cmeta PRIVATE flex_runtime Salts::TinyTest)
target_include_directories(test_property_cmeta PRIVATE ../include)
set_target_properties(test_property_cmeta PROPERTIES FOLDER "flex/tests")
add_test(NAME test_property_cmeta COMMAND test_property_cmeta)

add_executable(test_cubic_bezier test_cubic_bezier.cpp)
target_link_libraries(test_cubic_bezier PRIVATE flex_runtime Salts::TinyTest)
target_include_directories(test_cubic_bezier PRIVATE ../include)
set_target_properties(test_cubic_bezier PROPERTIES FOLDER "flex/tests")
add_test(NAME test_cubic_bezier COMMAND test_cubic_bezier)

add_executable(benchmark_mir_expression benchmark_mir_expression.cpp)
target_link_libraries(
  benchmark_mir_expression PRIVATE flex_runtime Salts::TinyTest)
target_include_directories(benchmark_mir_expression PRIVATE ../include)
set_target_properties(
  benchmark_mir_expression PROPERTIES FOLDER "flex/benchmarks")

add_executable(benchmark_motion_path benchmark_motion_path.cpp)
target_link_libraries(
  benchmark_motion_path PRIVATE flex_runtime Salts::TinyTest)
target_include_directories(benchmark_motion_path PRIVATE ../include)
set_target_properties(
  benchmark_motion_path PROPERTIES FOLDER "flex/benchmarks")

# ----------------------------------------------------------------------------
# Behavior Tests (render/layout/animation contract)
# ----------------------------------------------------------------------------
add_executable(test_behavior test_behavior.cpp)
target_link_libraries(test_behavior PRIVATE flex_runtime Salts::TinyTest)
target_include_directories(test_behavior PRIVATE ../include)
set_target_properties(test_behavior PROPERTIES FOLDER "flex/tests")
add_test(NAME test_behavior COMMAND test_behavior)

# ----------------------------------------------------------------------------
# Standard Trace Tests (canonical backend-neutral scene contract)
# ----------------------------------------------------------------------------
add_executable(test_standard test_standard.cpp)
target_link_libraries(test_standard PRIVATE flex Salts::TinyTest)
target_compile_definitions(
  test_standard PRIVATE FLEX_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples")
if(TARGET flex_backend_tui)
  target_link_libraries(test_standard PRIVATE flex_backend_tui)
endif()
if(TARGET flex_backend_opengl)
  target_link_libraries(test_standard PRIVATE flex_backend_opengl)
  target_compile_definitions(test_standard PRIVATE FLEX_HAS_OPENGL=1)
  if(WIN32)
    target_link_libraries(test_standard PRIVATE user32 gdi32)
  endif()
endif()
if(TARGET flex_backend_d2d)
  target_link_libraries(test_standard PRIVATE flex_backend_d2d)
endif()
target_include_directories(test_standard PRIVATE ../include)
set_target_properties(test_standard PROPERTIES FOLDER "flex/tests")
add_test(NAME test_standard COMMAND test_standard)

option(FLEX_BUILD_GCANVAS_GPU_TESTS
       "Build real OpenGL/Vulkan Flex-on-gCanvas pixel tests" OFF)
if(FLEX_BUILD_GCANVAS_GPU_TESTS AND
   TARGET flex_render_engine_gcanvas AND
   TARGET gCanvas::Window)
  add_executable(test_gcanvas_gpu test_gcanvas_gpu.cpp)
  target_link_libraries(
    test_gcanvas_gpu
    PRIVATE flex flex_render_engine_gcanvas gCanvas::Window
            Salts::TinyTest)
  set_target_properties(test_gcanvas_gpu PROPERTIES FOLDER "flex/tests")
  add_test(NAME test_gcanvas_gpu COMMAND test_gcanvas_gpu)
  set_tests_properties(
    test_gcanvas_gpu PROPERTIES LABELS "gpu;opengl;vulkan")

  add_executable(benchmark_gcanvas_blur benchmark_gcanvas_blur.cpp)
  target_link_libraries(
    benchmark_gcanvas_blur
    PRIVATE flex flex_render_engine_gcanvas gCanvas::Window
            Salts::TinyTest)
  target_include_directories(
    benchmark_gcanvas_blur PRIVATE ${CMAKE_SOURCE_DIR}/vendor/gCanvas/src)
  set_target_properties(
    benchmark_gcanvas_blur PROPERTIES FOLDER "flex/benchmarks")
endif()

option(FLEX_BUILD_OPENGL_ENGINE_PARITY_TESTS
       "Build NanoVG/gCanvas same-context migration tests and benchmark" OFF)
if(FLEX_BUILD_OPENGL_ENGINE_PARITY_TESTS)
  find_package(glfw3 CONFIG REQUIRED)
endif()
if(FLEX_BUILD_OPENGL_ENGINE_PARITY_TESTS AND
   TARGET flex_render_engine_gcanvas AND
   TARGET flex_render_engine_nanovg AND
   TARGET gCanvas::OpenGL)
  add_executable(test_opengl_engine_parity test_opengl_engine_parity.cpp)
  target_link_libraries(
    test_opengl_engine_parity
    PRIVATE flex_render_engine_gcanvas flex_render_engine_nanovg glfw
            Salts::TinyTest)
  set_target_properties(test_opengl_engine_parity PROPERTIES
                        FOLDER "flex/tests")
  add_test(NAME test_opengl_engine_parity COMMAND test_opengl_engine_parity)
  set_tests_properties(test_opengl_engine_parity PROPERTIES LABELS "gpu;opengl;parity")

  add_executable(benchmark_opengl_engines benchmark_opengl_engines.cpp)
  target_link_libraries(
    benchmark_opengl_engines
    PRIVATE flex_render_engine_gcanvas flex_render_engine_nanovg glfw
            Salts::TinyTest)
  set_target_properties(benchmark_opengl_engines PROPERTIES
                        FOLDER "flex/benchmarks")
endif()

# ----------------------------------------------------------------------------
# Layout Tests (requires full flex: parser + runtime)
# ----------------------------------------------------------------------------
add_executable(test_layout test_layout.cpp)
target_link_libraries(test_layout PRIVATE flex Salts::TinyTest)
if(TARGET flex_backend_opengl)
  target_link_libraries(test_layout PRIVATE flex_backend_opengl)
endif()
if(TARGET flex_backend_d2d)
  target_link_libraries(test_layout PRIVATE flex_backend_d2d)
endif()
target_include_directories(test_layout PRIVATE ../include)
set_target_properties(test_layout PROPERTIES FOLDER "flex/tests")
add_test(NAME test_layout COMMAND test_layout)

# ----------------------------------------------------------------------------
# Binary Format Tests (requires flex_binary + flex)
# ----------------------------------------------------------------------------
add_executable(test_binary test_binary.cpp)
target_link_libraries(test_binary PRIVATE flex_binary flex Salts::TinyTest)
target_include_directories(test_binary PRIVATE ../include)
set_target_properties(test_binary PROPERTIES FOLDER "flex/tests")
add_test(NAME test_binary COMMAND test_binary)
