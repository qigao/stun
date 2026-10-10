cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StunGraphLayout LANGUAGES CXX)
    include(CTest)
endif()

# Header-only graph IR, geometry and identity contract. No solver dependency.
add_library(stun_graph_core INTERFACE)
add_library(Stun::GraphCore ALIAS stun_graph_core)
target_compile_features(stun_graph_core INTERFACE cxx_std_17)
target_include_directories(stun_graph_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Each native algorithm is separately buildable/linkable; no monolithic
# Stun::GraphLayout aggregate and no implicit algorithm/provider fallback.
function(stun_graph_algorithm target public_name source)
    add_library(${target} STATIC ${source})
    add_library(Stun::${public_name} ALIAS ${target})
    target_link_libraries(${target} PUBLIC Stun::GraphCore)
    set_target_properties(${target} PROPERTIES FOLDER "Modules/GraphLayout")
endfunction()

stun_graph_algorithm(stun_graph_layered GraphLayered src/layered.cpp)
stun_graph_algorithm(stun_graph_tidy_tree GraphTidyTree src/tidy_tree.cpp)
stun_graph_algorithm(stun_graph_orthogonal GraphOrthogonal src/orthogonal.cpp)
stun_graph_algorithm(stun_graph_vpsc GraphVPSC src/vpsc.cpp)
stun_graph_algorithm(stun_graph_projection GraphProjection src/projection.cpp)
stun_graph_algorithm(stun_graph_stress_core GraphStressCore src/stress_common.cpp)
stun_graph_algorithm(stun_graph_stress_gradient GraphStressGradient src/stress_gradient.cpp)
stun_graph_algorithm(stun_graph_stress_smacof GraphStressSmacof src/stress_smacof.cpp)
stun_graph_algorithm(stun_graph_force GraphForce src/force.cpp)

# Explicit algorithm prerequisites. Orthogonal/Layered are independent;
# projection consumes VPSC, and nonconvex optimizers consume projection.
target_link_libraries(stun_graph_projection PUBLIC Stun::GraphVPSC)
target_link_libraries(stun_graph_stress_core PUBLIC Stun::GraphProjection)
target_link_libraries(stun_graph_stress_gradient PUBLIC Stun::GraphStressCore)
target_link_libraries(stun_graph_stress_smacof PUBLIC Stun::GraphStressCore)
target_link_libraries(stun_graph_force PUBLIC Stun::GraphProjection)

if(BUILD_TESTING)
    add_executable(stun_graphlayout_tests tests/layered_test.cpp)
    target_link_libraries(stun_graphlayout_tests PRIVATE Stun::GraphLayered)
    target_compile_features(stun_graphlayout_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphLayoutTests COMMAND stun_graphlayout_tests)
endif()

if(BUILD_TESTING)
    add_executable(stun_graphlayout_routing_tests tests/orthogonal_test.cpp)
    target_link_libraries(stun_graphlayout_routing_tests PRIVATE Stun::GraphOrthogonal Stun::GraphLayered)
    target_compile_features(stun_graphlayout_routing_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphRoutingTests COMMAND stun_graphlayout_routing_tests)
endif()

if(BUILD_TESTING)
    add_executable(stun_graphlayout_vpsc_tests tests/vpsc_test.cpp)
    target_link_libraries(stun_graphlayout_vpsc_tests PRIVATE Stun::GraphVPSC)
    target_compile_features(stun_graphlayout_vpsc_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphVpscTests COMMAND stun_graphlayout_vpsc_tests)
endif()

if(BUILD_TESTING)
    add_executable(stun_graphlayout_projection_tests tests/projection_test.cpp)
    target_link_libraries(stun_graphlayout_projection_tests PRIVATE Stun::GraphProjection)
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
    target_link_libraries(stun_graphlayout_infographic_tests PRIVATE Stun::GraphProjection Stun::GraphTidyTree)
    target_compile_features(stun_graphlayout_infographic_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphInfographicAdapterTests COMMAND stun_graphlayout_infographic_tests)
endif()

# Nonconvex shortest-path stress optimizer and VPSC projection integration.
if(BUILD_TESTING)
    add_executable(stun_graphlayout_stress_tests tests/stress_test.cpp)
    target_link_libraries(stun_graphlayout_stress_tests PRIVATE Stun::GraphStressGradient)
    target_compile_features(stun_graphlayout_stress_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphStressTests COMMAND stun_graphlayout_stress_tests)
endif()

# The SMACOF majorization implementation is a second optimizer for the exact
# same shortest-path Stress objective, qualified independently from descent.
if(BUILD_TESTING)
    add_executable(stun_graphlayout_smacof_tests tests/smacof_test.cpp)
    target_link_libraries(stun_graphlayout_smacof_tests PRIVATE Stun::GraphStressSmacof)
    target_compile_features(stun_graphlayout_smacof_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphSmacofTests COMMAND stun_graphlayout_smacof_tests)
endif()

# Optional microbenchmark, not part of the native CI qualification matrix.
option(STUN_GRAPHLAYOUT_BUILD_BENCHMARKS "Build native graph Stress comparison" OFF)
if(STUN_GRAPHLAYOUT_BUILD_BENCHMARKS)
    add_executable(stun_graphlayout_stress_compare benchmarks/stress_compare.cpp)
    target_link_libraries(stun_graphlayout_stress_compare PRIVATE Stun::GraphStressGradient Stun::GraphStressSmacof)
    target_compile_features(stun_graphlayout_stress_compare PRIVATE cxx_std_17)
endif()

# Independent softened-Coulomb / spring energy and finite-iterate descent gate.
if(BUILD_TESTING)
    add_executable(stun_graphlayout_force_tests tests/force_test.cpp)
    target_link_libraries(stun_graphlayout_force_tests PRIVATE Stun::GraphForce)
    target_compile_features(stun_graphlayout_force_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphForceTests COMMAND stun_graphlayout_force_tests)
endif()

if(STUN_GRAPHLAYOUT_BUILD_BENCHMARKS)
    add_executable(stun_graphlayout_force_compare benchmarks/force_compare.cpp)
    target_link_libraries(stun_graphlayout_force_compare PRIVATE Stun::GraphForce Stun::GraphStressGradient Stun::GraphStressSmacof)
    target_compile_features(stun_graphlayout_force_compare PRIVATE cxx_std_17)
endif()

# Exercise real DOT/Mermaid AST-to-Graph placement adapters headlessly. Chart
# parsers, UI, Mustache, OpenGL and libavoid are deliberately not linked.
if(BUILD_TESTING AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dotgraph/src/dotgraph_layout.cpp" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../mermaid/flowchart/src/flowchart_layout.cpp")
    add_executable(stun_graphlayout_chart_adapter_tests
        tests/chart_adapter_test.cpp
        ../dotgraph/src/dotgraph_layout.cpp
        ../mermaid/flowchart/src/flowchart_layout.cpp
    )
    target_include_directories(stun_graphlayout_chart_adapter_tests PRIVATE
        ../dotgraph/include ../dotgraph/src
        ../mermaid/flowchart/include ../mermaid/flowchart/src
    )
    target_link_libraries(stun_graphlayout_chart_adapter_tests PRIVATE Stun::GraphLayered)
    add_test(NAME StunGraphChartAdaptersTests COMMAND stun_graphlayout_chart_adapter_tests)
endif()

# Independent ordered-forest layout (unbalanced trees and variable sizes).
if(BUILD_TESTING)
    add_executable(stun_graphlayout_tidy_tree_tests tests/tidy_tree_test.cpp)
    target_link_libraries(stun_graphlayout_tidy_tree_tests PRIVATE Stun::GraphTidyTree)
    add_test(NAME StunGraphTidyTreeTests COMMAND stun_graphlayout_tidy_tree_tests)
endif()

# Real Mermaid Mindmap AST adapter: no parser, UI or rendering dependency.
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../mermaid/mindmap/src/mindmap_layout.cpp")
    add_executable(stun_graphlayout_mindmap_tests
        tests/mindmap_adapter_test.cpp
        ../mermaid/mindmap/src/mindmap_layout.cpp
    )
    target_include_directories(stun_graphlayout_mindmap_tests PRIVATE
        ../mermaid/mindmap/include
    )
    target_link_libraries(stun_graphlayout_mindmap_tests PRIVATE Stun::GraphTidyTree)
    add_test(NAME StunGraphMindmapAdapterTests COMMAND stun_graphlayout_mindmap_tests)
endif()

# Compile and execute the actual tree renderer source against a lightweight
# headless rendering contract; the installed/full Charts build is a separate gate.
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../infographic/tree/src/tree_layout_renderer.cpp")
    add_executable(stun_graphlayout_tree_renderer_tests
        tests/infographic_tree_renderer_test.cpp
        ../infographic/tree/src/tree_layout_renderer.cpp
    )
    target_include_directories(stun_graphlayout_tree_renderer_tests PRIVATE
        tests/stubs
    )
    target_compile_features(stun_graphlayout_tree_renderer_tests PRIVATE cxx_std_17)
    add_test(NAME StunGraphTreeRendererTests COMMAND stun_graphlayout_tree_renderer_tests)
endif()
