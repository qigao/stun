# ============================================================================
# Test Suites - Link only required targets for faster compilation
# ============================================================================

enable_testing()

# ----------------------------------------------------------------------------
# Foundation Tests (no scene, DSL, window, or GPU dependency)
# ----------------------------------------------------------------------------
add_executable(test_animation test_animation.cpp)
target_link_libraries(test_animation PRIVATE flex_animation TurboUtils::TinyTest)
set_target_properties(test_animation PROPERTIES FOLDER "flex/tests")
add_test(NAME test_animation COMMAND test_animation)

add_executable(test_render test_render.cpp)
target_link_libraries(test_render PRIVATE flex_render TurboUtils::TinyTest)
set_target_properties(test_render PROPERTIES FOLDER "flex/tests")
add_test(NAME test_render COMMAND test_render)

# ----------------------------------------------------------------------------
# Compiler Tests (lexer/parser only)
# ----------------------------------------------------------------------------
add_executable(test_lexer_parser test_lexer_parser.cpp)
target_link_libraries(test_lexer_parser PRIVATE flex_compiler TurboUtils::TinyTest)
target_include_directories(test_lexer_parser PRIVATE ../include)
set_target_properties(test_lexer_parser PROPERTIES FOLDER "flex/tests")
add_test(NAME test_lexer_parser COMMAND test_lexer_parser)

# ----------------------------------------------------------------------------
# Runtime Tests (runtime system only) - Clean version
# ----------------------------------------------------------------------------
add_executable(test_runtime test_runtime.cpp)
target_link_libraries(test_runtime PRIVATE flex_runtime TurboUtils::TinyTest)
target_include_directories(test_runtime PRIVATE ../include)
set_target_properties(test_runtime PROPERTIES FOLDER "flex/tests")
add_test(NAME test_runtime COMMAND test_runtime)

# ----------------------------------------------------------------------------
# Behavior Tests (render/layout/animation contract)
# ----------------------------------------------------------------------------
add_executable(test_behavior test_behavior.cpp)
target_link_libraries(test_behavior PRIVATE flex_runtime TurboUtils::TinyTest)
target_include_directories(test_behavior PRIVATE ../include)
set_target_properties(test_behavior PROPERTIES FOLDER "flex/tests")
add_test(NAME test_behavior COMMAND test_behavior)

# ----------------------------------------------------------------------------
# Standard Trace Tests (canonical backend-neutral scene contract)
# ----------------------------------------------------------------------------
add_executable(test_standard test_standard.cpp)
target_link_libraries(test_standard PRIVATE flex TurboUtils::TinyTest)
target_compile_definitions(
  test_standard PRIVATE FLEX_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples")
if(TARGET flex_backend_tui)
  target_link_libraries(test_standard PRIVATE flex_backend_tui)
endif()
if(TARGET flex_backend_thorvg)
  target_link_libraries(test_standard PRIVATE flex_backend_thorvg)
endif()
if(TARGET flex_backend_nanovg)
  target_link_libraries(test_standard PRIVATE flex_backend_nanovg)
  target_compile_definitions(test_standard PRIVATE FLEX_HAS_NANOVG=1)
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

# ----------------------------------------------------------------------------
# Layout Tests (requires full flex: parser + runtime)
# ----------------------------------------------------------------------------
add_executable(test_layout test_layout.cpp)
target_link_libraries(test_layout PRIVATE flex TurboUtils::TinyTest)
if(TARGET flex_backend_thorvg)
  target_link_libraries(test_layout PRIVATE flex_backend_thorvg)
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
target_link_libraries(test_binary PRIVATE flex_binary flex TurboUtils::TinyTest)
target_include_directories(test_binary PRIVATE ../include)
set_target_properties(test_binary PROPERTIES FOLDER "flex/tests")
add_test(NAME test_binary COMMAND test_binary)
