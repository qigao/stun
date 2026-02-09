set(PROJECT_SRCS
    project.cpp
    util.cpp
)

add_library(project SHARED ${PROJECT_SRCS})
cola_target_setup(project)

target_link_libraries(project PUBLIC vpsc)
