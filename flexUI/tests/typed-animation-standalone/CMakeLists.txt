cmake_minimum_required(VERSION 3.20...3.31)
project(StunTypedAnimationQualification LANGUAGES CXX)

include(CTest)
get_filename_component(STUN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Salts CONFIG REQUIRED PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH)
find_package(nlohmann_json CONFIG REQUIRED)

# Compile the actual registration/render call sites without a window or GPU.
# The object target is a compilation check, not full renderer runtime coverage.
add_library(animation_call_sites OBJECT
  "${STUN_ROOT}/flexUI/src/box.cpp"
  "${STUN_ROOT}/flexUI/src/render_manager.cpp"
  "${STUN_ROOT}/flexUI/src/style_property_impact.cpp")
target_link_libraries(animation_call_sites PRIVATE Salts::Core Salts::CMeta
  nlohmann_json::nlohmann_json)

# Run the existing production manager suites against the real registry and SDK.
foreach(suite IN ITEMS typed_animation typed_transition)
  add_executable(test_${suite}
    "${STUN_ROOT}/flexUI/tests/test_${suite}.cpp"
    "${STUN_ROOT}/flex/tests/cmeta_types_peer.cpp"
    "${STUN_ROOT}/flexUI/src/transition.cpp"
    "${STUN_ROOT}/flexUI/src/style_property_registry.cpp")
  target_link_libraries(test_${suite} PRIVATE Salts::TinyTest Salts::Core Salts::CMeta)
  add_test(NAME test_${suite} COMMAND test_${suite})
  set_tests_properties(test_${suite} PROPERTIES TIMEOUT 30)
endforeach()

foreach(target IN ITEMS animation_call_sites test_typed_animation test_typed_transition)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PRIVATE
    "${STUN_ROOT}/flexUI/include" "${STUN_ROOT}/flex/include")
  target_compile_options(${target} PRIVATE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endforeach()
