cmake_minimum_required(VERSION 3.20)
project(stun_flowchart_ast_smoke LANGUAGES C)

# Test-only parser build using the SAME Lemon grammar, re2c scanner and AST
# implementation as production. The serializer and renderer are intentionally
# absent: their SDK integration / golden JSON conformance is checked elsewhere.
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
