set(TARGET_NAME md_parser)

# 两阶段解析器 - 不再依赖 re2c/lemon
add_library(${TARGET_NAME} STATIC 
    src/md_parser_driver.cpp
    src/md_block_parser.cpp
    src/md_inline_parser.cpp
    src/md_block_processor.cpp
    src/md_extension.cpp
    src/md_adapters.cpp
)

target_include_directories(${TARGET_NAME} 
    PUBLIC include
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${TARGET_NAME} PUBLIC TurboUtils::Core)
# Windows: disable min/max macros that conflict with std::min/std::max
if(WIN32)
    target_compile_definitions(${TARGET_NAME}  PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
# 可选模块支持
if(TARGET flexmaid)
    target_link_libraries(${TARGET_NAME} PRIVATE flexmaid)
    target_compile_definitions(${TARGET_NAME} PRIVATE MD_HAS_FLEXMAID)
endif()

if(TARGET flexchart)
    target_link_libraries(${TARGET_NAME} PRIVATE flexchart)
    target_compile_definitions(${TARGET_NAME} PRIVATE MD_HAS_FLEXCHART)
endif()

if(TARGET infographic)
    target_link_libraries(${TARGET_NAME} PRIVATE infographic)
    target_compile_definitions(${TARGET_NAME} PRIVATE MD_HAS_INFOGRAPHIC)
endif()

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "tools/md")
# ============================================================================
# Unit Tests
# ============================================================================
if(BUILD_TESTS)
    add_subdirectory(test)
endif()
