cmake_minimum_required(VERSION 3.20...3.31)
project(StunMermaidParserQualification LANGUAGES C CXX)
include(CTest)
get_filename_component(STUN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
find_package(Salts CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
find_package(SaltsUtils CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
find_program(RE2C_EXECUTABLE re2c REQUIRED)
find_package(Threads REQUIRED)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(ENABLE_SANITIZER_ADDRESS ON CACHE BOOL "Qualify actual production sources")
set(ENABLE_SANITIZER_UNDEFINED ON CACHE BOOL "Qualify actual production sources")
include("${STUN_ROOT}/cmake/Sanitizers.cmake")

# Use the normal production targets, including the real vendored MIR JIT.
# Runtime, scene/physics, window and GPU targets must not exist in this build.
add_subdirectory("${STUN_ROOT}/vendor/mir" mir)
set(FLEX_ENGINE_INCLUDE_DIR "${STUN_ROOT}/flex/include")
add_subdirectory("${STUN_ROOT}/flex/modules/expression" expression)
add_executable(test_expr_c "${STUN_ROOT}/flex/tests/test_expr_c.c")
target_link_libraries(test_expr_c PRIVATE Flex::Expression Salts::TinyTest)
set_target_properties(test_expr_c PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME test_expr_c COMMAND test_expr_c)
add_custom_target(stun_parser_qualification DEPENDS test_expr_c)

add_executable(stun_expression_lemon "${STUN_ROOT}/tools/lemon/lemon.c")
set(LEMON_EXECUTABLE "$<TARGET_FILE:stun_expression_lemon>")
set(LEMPAR "${STUN_ROOT}/tools/lemon/lempar.c")
set(MERMAID_GOLDEN_DIR "${STUN_ROOT}/charts/mermaid/golden")
set(STUN_EXPRESSION_PARSERS sankey block kanban journey mindmap xychart quadrant
  CACHE STRING "Production parser targets to qualify")
set(STUN_DIRECT_JSON_PARSERS sequence er gantt statediagram timeline classdiagram c4
  CACHE STRING "Production parsers that consume JsonParser without Expression")

# Keep the executable matrix equal to the production Mermaid JsonParser
# inventory. Flowchart is deliberately owned by its stronger standalone
# AST/JSON/Mustache gate in this workflow; every other consumer runs below.
file(GLOB mermaid_parser_cmake_files
  "${STUN_ROOT}/charts/mermaid/*/CMakeLists.txt")
set(discovered_json_parsers)
foreach(parser_cmake IN LISTS mermaid_parser_cmake_files)
  file(READ "${parser_cmake}" parser_cmake_source)
  if(parser_cmake_source MATCHES "Salts::JsonParser")
    get_filename_component(parser_dir "${parser_cmake}" DIRECTORY)
    get_filename_component(parser_name "${parser_dir}" NAME)
    list(APPEND discovered_json_parsers "${parser_name}")
  endif()
endforeach()
set(expected_json_parsers
  ${STUN_EXPRESSION_PARSERS} ${STUN_DIRECT_JSON_PARSERS} flowchart)
list(SORT discovered_json_parsers)
list(SORT expected_json_parsers)
if(NOT discovered_json_parsers STREQUAL expected_json_parsers)
  message(FATAL_ERROR
    "Mermaid JsonParser inventory changed; update the executable qualification matrix. "
    "Expected: ${expected_json_parsers}; discovered: ${discovered_json_parsers}")
endif()
if("mindmap" IN_LIST STUN_EXPRESSION_PARSERS)
  # Supply the real adapter prerequisite while graph algorithm regressions
  # remain in their independent gate. Only parser targets are built here.
  set(_expression_build_testing "${BUILD_TESTING}")
  set(BUILD_TESTING OFF)
  add_subdirectory("${STUN_ROOT}/shared/layout" graph-layout EXCLUDE_FROM_ALL)
  set(BUILD_TESTING "${_expression_build_testing}")
endif()
foreach(parser IN LISTS STUN_EXPRESSION_PARSERS)
  if(NOT parser MATCHES "^(sankey|block|kanban|journey|mindmap|xychart|quadrant)$")
    message(FATAL_ERROR "Unsupported expression qualification parser: ${parser}")
  endif()
  add_subdirectory("${STUN_ROOT}/charts/mermaid/${parser}" "parsers/${parser}")
  add_dependencies(${parser} stun_expression_lemon)
  add_dependencies(stun_parser_qualification ${parser}_test)
  get_target_property(links ${parser} INTERFACE_LINK_LIBRARIES)
  if(NOT "Flex::Expression" IN_LIST links OR NOT "Salts::JsonParser" IN_LIST links OR
     "flex_runtime" IN_LIST links OR "Flex::Runtime" IN_LIST links)
    message(FATAL_ERROR
      "${parser} must link Expression and JsonParser without Runtime")
  endif()
endforeach()

foreach(parser IN LISTS STUN_DIRECT_JSON_PARSERS)
  if(NOT parser MATCHES "^(sequence|er|gantt|statediagram|timeline|classdiagram|c4)$")
    message(FATAL_ERROR "Unsupported direct JsonParser qualification parser: ${parser}")
  endif()
  add_subdirectory("${STUN_ROOT}/charts/mermaid/${parser}" "parsers/${parser}")
  if(parser STREQUAL "c4")
    set(parser_target mermaid_c4)
  else()
    set(parser_target ${parser})
  endif()
  add_dependencies(${parser_target} stun_expression_lemon)
  add_dependencies(stun_parser_qualification ${parser}_test)
  get_target_property(links ${parser_target} INTERFACE_LINK_LIBRARIES)
  if(NOT "Salts::JsonParser" IN_LIST links OR "Flex::Expression" IN_LIST links OR
     "flex_runtime" IN_LIST links OR "Flex::Runtime" IN_LIST links)
    message(FATAL_ERROR
      "${parser} must link JsonParser directly without Expression or Runtime")
  endif()
endforeach()
if(TARGET flex_runtime OR TARGET Flex::Runtime)
  message(FATAL_ERROR "The parser gate must remain independent of Runtime")
endif()
set_tests_properties(test_expr_c PROPERTIES TIMEOUT 30)
