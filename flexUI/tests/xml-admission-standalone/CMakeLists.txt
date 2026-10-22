cmake_minimum_required(VERSION 3.20...3.31)
project(StunXmlAdmissionQualification LANGUAGES CXX)
include(CTest)
get_filename_component(STUN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Salts CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
find_path(PUGIXML_INCLUDE_DIR pugixml.hpp REQUIRED)
find_path(STB_INCLUDE_DIR stb_sprintf.h PATH_SUFFIXES stb REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Compile the real XML entry points and retained public compiler/application
# regressions. Application publication runtime is outside this isolated gate.
add_library(xml_call_sites OBJECT
  "${STUN_ROOT}/flexUI/src/ui_xml.cpp"
  "${STUN_ROOT}/flexUI/tests/test_ui_xml.cpp"
  "${STUN_ROOT}/flexUI/tests/test_desktop_application.cpp")
add_executable(test_ui_xml_admission
  "${STUN_ROOT}/flexUI/tests/test_ui_xml_admission.cpp"
  "${STUN_ROOT}/flexUI/src/ui_xml_admission.cpp")
foreach(target IN ITEMS xml_call_sites test_ui_xml_admission)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PRIVATE "${STUN_ROOT}/flexUI/include"
    "${STUN_ROOT}/flex/include" "${PUGIXML_INCLUDE_DIR}" "${STB_INCLUDE_DIR}")
  target_link_libraries(${target} PRIVATE Salts::Core Salts::CMeta Salts::Unicode Salts::TinyTest nlohmann_json::nlohmann_json)
  target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endforeach()
add_test(NAME test_ui_xml_admission COMMAND test_ui_xml_admission)
set_tests_properties(test_ui_xml_admission PROPERTIES TIMEOUT 30)
