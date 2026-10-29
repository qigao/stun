cmake_minimum_required(VERSION 3.20...3.31)
project(StunUnicodeQualification LANGUAGES CXX)
include(CTest)
get_filename_component(STUN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Salts CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(STB_INCLUDE_DIR stb_sprintf.h PATH_SUFFIXES stb REQUIRED)

# Execute the original Unicode suite against the production implementation.
add_executable(test_text_util_unicode
  "${STUN_ROOT}/flexUI/tests/test_text_util_unicode.cpp"
  "${STUN_ROOT}/flexUI/src/text_util.cpp")
add_executable(test_text_layout_unicode
  "${STUN_ROOT}/flexUI/tests/test_text_layout_unicode.cpp"
  "${STUN_ROOT}/flexUI/src/text_util.cpp"
  "${STUN_ROOT}/flexUI/src/text_layout.cpp"
  "${STUN_ROOT}/flexUI/src/computed_style.cpp"
  "${STUN_ROOT}/flexUI/src/render_command.cpp"
  "${STUN_ROOT}/flexUI/src/render_command_validation.cpp")
# Compile actual layout and widget consumers plus their retained regressions.
# This is not a full widget/application runtime qualification.
add_library(unicode_call_sites OBJECT
  "${STUN_ROOT}/flexUI/src/text_layout.cpp"
  "${STUN_ROOT}/flexUI/src/widgets/input_widget.cpp"
  "${STUN_ROOT}/flexUI/src/widgets/textarea_widget.cpp"
  "${STUN_ROOT}/flexUI/tests/test_widget_contracts.cpp")
foreach(target IN ITEMS test_text_util_unicode test_text_layout_unicode unicode_call_sites)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PRIVATE "${STUN_ROOT}/flexUI/include"
    "${STUN_ROOT}/flex/include" "${STB_INCLUDE_DIR}")
  target_link_libraries(${target} PRIVATE Salts::Core Salts::CMeta Salts::Unicode
    Salts::TinyTest nlohmann_json::nlohmann_json)
  target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endforeach()
add_test(NAME test_text_util_unicode COMMAND test_text_util_unicode)
set_tests_properties(test_text_util_unicode PROPERTIES TIMEOUT 30)
add_test(NAME test_text_layout_unicode COMMAND test_text_layout_unicode)
set_tests_properties(test_text_layout_unicode PROPERTIES TIMEOUT 30)
