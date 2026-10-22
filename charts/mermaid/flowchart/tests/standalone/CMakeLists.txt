cmake_minimum_required(VERSION 3.20)
project(stun_flowchart_ast_smoke LANGUAGES C)

# Test-only parser build using the SAME Lemon grammar, re2c scanner and AST
# implementation as production. Optional installed-SDK lanes also compile
# the production serializer and headless renderer without the desktop stack.
include(CTest)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(STUN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(FLOWCHART_SOURCE "${STUN_ROOT}/charts/mermaid/flowchart")
find_program(RE2C_EXECUTABLE re2c REQUIRED)

add_executable(stun_ci_lemon "${STUN_ROOT}/tools/lemon/lemon.c")

set(GENERATED_GRAMMAR "${CMAKE_CURRENT_BINARY_DIR}/flowchart_parser_gen.y")
set(GENERATED_PARSER "${CMAKE_CURRENT_BINARY_DIR}/flowchart_parser_gen.c")
set(GENERATED_PARSER_HEADER "${CMAKE_CURRENT_BINARY_DIR}/flowchart_parser_gen.h")
set(GENERATED_LEXER "${CMAKE_CURRENT_BINARY_DIR}/flowchart_lexer_gen.c")

add_custom_command(
  OUTPUT "${GENERATED_PARSER}" "${GENERATED_PARSER_HEADER}"
  COMMAND "${CMAKE_COMMAND}" -E copy
          "${FLOWCHART_SOURCE}/src/flowchart_parser.y" "${GENERATED_GRAMMAR}"
  COMMAND "$<TARGET_FILE:stun_ci_lemon>"
          "-T${STUN_ROOT}/tools/lemon/lempar.c" "${GENERATED_GRAMMAR}"
  DEPENDS stun_ci_lemon
          "${FLOWCHART_SOURCE}/src/flowchart_parser.y"
          "${STUN_ROOT}/tools/lemon/lempar.c"
  COMMENT "Generate production Flowchart grammar with repository Lemon"
  VERBATIM
)

add_custom_command(
  OUTPUT "${GENERATED_LEXER}"
  COMMAND "${RE2C_EXECUTABLE}" -o "${GENERATED_LEXER}"
          "${FLOWCHART_SOURCE}/src/flowchart_lexer.re"
  DEPENDS "${FLOWCHART_SOURCE}/src/flowchart_lexer.re"
          "${GENERATED_PARSER_HEADER}"
  COMMENT "Generate production Flowchart scanner with re2c"
  VERBATIM
)

add_executable(stun_flowchart_ast_smoke
  "${GENERATED_PARSER}"
  "${GENERATED_LEXER}"
  "${FLOWCHART_SOURCE}/src/flowchart_parser_wrapper.c"
  "${FLOWCHART_SOURCE}/tests/ci_smoke.c"
)
target_include_directories(stun_flowchart_ast_smoke PRIVATE
  "${FLOWCHART_SOURCE}/include"
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_definitions(stun_flowchart_ast_smoke PRIVATE
  STUN_FLOWCHART_PARSER_ONLY=1
  _POSIX_C_SOURCE=200809L
)

# Repository Lemon generates unavoidable unused-parameter and type-range
# warnings in its template. Do not weaken the warnings on hand-written code.
set_source_files_properties("${GENERATED_PARSER}" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=unused-variable;-Wno-error=unused-parameter;-Wno-error=type-limits"
)

option(STUN_CI_SANITIZERS "Enable ASan and UBSan for AST smoke" ON)
if(STUN_CI_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(stun_flowchart_ast_smoke PRIVATE
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -Wall -Wextra -Werror
  )
  target_link_options(stun_flowchart_ast_smoke PRIVATE
    -fsanitize=address,undefined
  )
endif()

add_test(NAME stun_flowchart_ast_smoke COMMAND stun_flowchart_ast_smoke)
set_tests_properties(stun_flowchart_ast_smoke PROPERTIES TIMEOUT 30)

# Optional isolated JSON golden lane. Unlike the AST smoke, this compiles the
# SAME production SaltsUtils-backed serializer and original 95-case test suite.
# It links only immutable installed Salts/SaltsUtils SDK targets, never a
# second JSON implementation, source checkout, or full FlexUI renderer.
option(STUN_CI_JSON_GOLDEN "Build the actual Flowchart JSON golden contract" OFF)
if(STUN_CI_JSON_GOLDEN)
  # The official fixture set has 95 dynamic Mermaid cases. Reject accidental
  # partial-checkout or missing-fixture "success" before compiling the test.
  file(GLOB _flowchart_golden_inputs
    "${STUN_ROOT}/charts/mermaid/golden/flowchart/*.input.mmd")
  list(LENGTH _flowchart_golden_inputs _flowchart_golden_count)
  if(_flowchart_golden_count LESS 95)
    message(FATAL_ERROR
      "Flowchart JSON conformance requires >=95 real Mermaid fixtures; found ${_flowchart_golden_count}")
  endif()
  message(STATUS "Qualified Flowchart input fixtures: ${_flowchart_golden_count}")
  if(NOT DEFINED ENV{SALTS_ROOT} OR NOT IS_DIRECTORY "$ENV{SALTS_ROOT}")
    message(FATAL_ERROR "Flowchart JSON golden requires the published installed Salts SDK in SALTS_ROOT")
  endif()
  find_package(Salts CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
  find_package(SaltsUtils CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)

  add_executable(stun_flowchart_json_golden
    "${GENERATED_PARSER}"
    "${GENERATED_LEXER}"
    "${FLOWCHART_SOURCE}/src/flowchart_parser_wrapper.c"
    "${FLOWCHART_SOURCE}/tests/test_parser.c"
    "${FLOWCHART_SOURCE}/tests/test_json_v2.c"
  )
  target_include_directories(stun_flowchart_json_golden PRIVATE
    "${FLOWCHART_SOURCE}/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
  )
  target_compile_definitions(stun_flowchart_json_golden PRIVATE
    _POSIX_C_SOURCE=200809L
    GOLDEN_DIR="${STUN_ROOT}/charts/mermaid/golden/flowchart"
    JSON_V2_CONTRACT="${FLOWCHART_SOURCE}/tests/json-v2-contract.json"
  )
  target_link_libraries(stun_flowchart_json_golden PRIVATE
    Salts::JsonParser Salts::TinyTest
  )
  if(STUN_CI_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(stun_flowchart_json_golden PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(stun_flowchart_json_golden PRIVATE
      -fsanitize=address,undefined)
  endif()
  add_test(NAME stun_flowchart_json_golden COMMAND stun_flowchart_json_golden)
  set_tests_properties(stun_flowchart_json_golden PROPERTIES TIMEOUT 120)
endif()

# Compile the actual renderer, Mustache template and native routing adapter
# without FlexUI, a window, a GPU, vcpkg or a Mesa source build.
option(STUN_CI_RENDERER_STYLE "Build production headless edge-style checks" ${STUN_CI_JSON_GOLDEN})
if(STUN_CI_RENDERER_STYLE)
  if(NOT STUN_CI_JSON_GOLDEN)
    message(FATAL_ERROR "Renderer checks require the installed-SDK JSON qualification")
  endif()
  enable_language(CXX)
  add_subdirectory("${STUN_ROOT}/shared/layout" "${CMAKE_CURRENT_BINARY_DIR}/graphlayout" EXCLUDE_FROM_ALL)
  add_executable(stun_flowchart_linkstyle
    "${GENERATED_PARSER}" "${GENERATED_LEXER}"
    "${FLOWCHART_SOURCE}/src/flowchart_parser_wrapper.c"
    "${FLOWCHART_SOURCE}/src/flowchart_renderer.cpp"
    "${FLOWCHART_SOURCE}/src/flowchart_curve.cpp"
    "${FLOWCHART_SOURCE}/src/flowchart_layout.cpp"
    "${FLOWCHART_SOURCE}/tests/test_linkstyle.cpp"
    "${FLOWCHART_SOURCE}/tests/test_curves.cpp")
  target_include_directories(stun_flowchart_linkstyle PRIVATE
    "${FLOWCHART_SOURCE}/include" "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_features(stun_flowchart_linkstyle PRIVATE cxx_std_17)
  target_compile_definitions(stun_flowchart_linkstyle PRIVATE
    STUN_FLOWCHART_PARSER_ONLY=1 _POSIX_C_SOURCE=200809L
    GOLDEN_DIR="${STUN_ROOT}/charts/mermaid/golden/flowchart")
  target_link_libraries(stun_flowchart_linkstyle PRIVATE Salts::Mustache
    Stun::GraphLayered Stun::GraphOrthogonal Stun::GraphPolyline
    Stun::GraphPolylineNudging Stun::GraphNudging)
  if(STUN_CI_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(stun_flowchart_linkstyle PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(stun_flowchart_linkstyle PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME stun_flowchart_linkstyle COMMAND stun_flowchart_linkstyle
    "${CMAKE_CURRENT_BINARY_DIR}/flowchart-linkstyle.svg")
  set_tests_properties(stun_flowchart_linkstyle PROPERTIES TIMEOUT 30)
endif()
