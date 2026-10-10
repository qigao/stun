cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StunGraphLayout LANGUAGES CXX)
    include(CTest)
endif()

add_library(stun_graphlayout STATIC src/layered.cpp src/orthogonal.cpp src/vpsc.cpp)
add_library(Stun::GraphLayout ALIAS stun_graphlayout)
target_compile_features(stun_graphlayout PUBLIC cxx_std_17)
target_include_directories(stun_graphlayout PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
set_target_properties(stun_graphlayout PROPERTIES FOLDER "Modules/GraphLayout")

if(BUILD_TESTING)
    add_executable(stun_graphlayout_tests tests/layered_test.cpp)
    target_link_libraries(stun_graphlayout_tests PRIVATE Stun::GraphLayout)
    target_compile_features(stun_graphlayout_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphLayoutTests COMMAND stun_graphlayout_tests)
endif()

if(BUILD_TESTING)
    add_executable(stun_graphlayout_routing_tests tests/orthogonal_test.cpp)
    target_link_libraries(stun_graphlayout_routing_tests PRIVATE Stun::GraphLayout)
    target_compile_features(stun_graphlayout_routing_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphRoutingTests COMMAND stun_graphlayout_routing_tests)
endif()

if(BUILD_TESTING)
    add_executable(stun_graphlayout_vpsc_tests tests/vpsc_test.cpp)
    target_link_libraries(stun_graphlayout_vpsc_tests PRIVATE Stun::GraphLayout)
    target_compile_features(stun_graphlayout_vpsc_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphVpscTests COMMAND stun_graphlayout_vpsc_tests)
endif()
