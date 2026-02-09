set(COLA_SRCS
    cola.cpp
    colafd.cpp
    conjugate_gradient.cpp
    gradient_projection.cpp
    straightener.cpp
    connected_components.cpp
    convex_hull.cpp
    cluster.cpp
    compound_constraints.cpp
    pseudorandom.cpp
    output_svg.cpp
    cc_clustercontainmentconstraints.cpp
    cc_nonoverlapconstraints.cpp
    box.cpp
    shapepair.cpp
)

add_library(cola SHARED ${COLA_SRCS})
cola_target_setup(cola)

target_link_libraries(cola PUBLIC vpsc)
