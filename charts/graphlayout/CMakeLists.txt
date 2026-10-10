cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StunGraphLayout LANGUAGES CXX)
    include(CTest)
endif()

add_library(stun_graphlayout STATIC src/layered.cpp src/orthogonal.cpp src/vpsc.cpp src/projection.cpp src/stress.cpp)
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

if(BUILD_TESTING)
    add_executable(stun_graphlayout_projection_tests tests/projection_test.cpp)
    target_link_libraries(stun_graphlayout_projection_tests PRIVATE Stun::GraphLayout)
    target_compile_features(stun_graphlayout_projection_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphProjectionTests COMMAND stun_graphlayout_projection_tests)
endif()

# Headless consumer qualification without the full parser/UI/render graph.
# Infographic's geometry adapter must compile and execute on the same native
# toolchain matrix as GraphLayout; full STUN_BUILD_CHARTS=ON is a later gate.
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../infographic/src/layout/layout_engine.cpp")
    add_executable(stun_graphlayout_infographic_tests
        tests/infographic_adapter_test.cpp
        ../infographic/src/layout/layout_engine.cpp
        ../infographic/src/ir/unified_infographic.cpp
    )
    target_include_directories(stun_graphlayout_infographic_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../infographic/include/infographic
    )
    target_link_libraries(stun_graphlayout_infographic_tests PRIVATE Stun::GraphLayout)
    target_compile_features(stun_graphlayout_infographic_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphInfographicAdapterTests COMMAND stun_graphlayout_infographic_tests)
endif()

# Nonconvex shortest-path stress optimizer and VPSC projection integration.
if(BUILD_TESTING)
    add_executable(stun_graphlayout_stress_tests tests/stress_test.cpp)
    target_link_libraries(stun_graphlayout_stress_tests PRIVATE Stun::GraphLayout)
    target_compile_features(stun_graphlayout_stress_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphStressTests COMMAND stun_graphlayout_stress_tests)
endif()
