set(VPSC_SRCS
    block.cpp
    blocks.cpp
    constraint.cpp
    rectangle.cpp
    solve_VPSC.cpp
    variable.cpp
    cbuffer.cpp
)

add_library(vpsc SHARED ${VPSC_SRCS})
cola_target_setup(vpsc)

# 我们在公共配置里默认关了自动导出，这里显式启用私有导出宏
target_compile_definitions(vpsc PRIVATE LIBVPSC_EXPORTS)
