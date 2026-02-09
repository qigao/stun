set(TOPOLOGY_SRCS
    cola_topology_addon.cpp
    compute_forces.cpp
    orthogonal_topology.cpp
    resize.cpp
    topology_constraints.cpp
    topology_constraints_constructor.cpp
    topology_graph.cpp
)

add_library(topology SHARED ${TOPOLOGY_SRCS})
cola_target_setup(topology)

target_link_libraries(topology PUBLIC cola vpsc)
