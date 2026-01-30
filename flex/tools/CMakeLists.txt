# Flex Compiler Tool
cmake_minimum_required(VERSION 3.16)

# Find cxxopts for command-line parsing
find_package(cxxopts CONFIG REQUIRED)

# ============================================================================
# flex-compiler - Command-line compiler tool
# ============================================================================

add_executable(flex-compiler
    flex-compiler.cpp
)

target_link_libraries(flex-compiler
    PRIVATE flex_binary flex  cxxopts::cxxopts
)

set_target_properties(flex-compiler PROPERTIES
    FOLDER "flex/tools"
    OUTPUT_NAME "flex-compiler"
)

# Install tool
install(TARGETS flex-compiler
    RUNTIME DESTINATION bin
)
