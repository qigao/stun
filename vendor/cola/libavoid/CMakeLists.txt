set(AVOID_SRCS
    actioninfo.cpp
    connectionpin.cpp
    connector.cpp
    connend.cpp
    geometry.cpp
    geomtypes.cpp
    graph.cpp
    hyperedge.cpp
    hyperedgeimprover.cpp
    hyperedgetree.cpp
    junction.cpp
    makepath.cpp
    mtst.cpp
    obstacle.cpp
    orthogonal.cpp
    router.cpp
    scanline.cpp
    shape.cpp
    timer.cpp
    vertices.cpp
    viscluster.cpp
    visibility.cpp
    vpsc.cpp
)

add_library(avoid SHARED ${AVOID_SRCS})
cola_target_setup(avoid)

target_compile_definitions(avoid PRIVATE LIBAVOID_EXPORTS)
target_link_libraries(avoid PUBLIC vpsc)
