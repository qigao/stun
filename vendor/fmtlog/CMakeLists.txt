add_library(fmtlog STATIC)
add_library(fmtlog::fmtlog ALIAS fmtlog)

target_include_directories(
    fmtlog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_sources(
    fmtlog
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/fmtlog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/fmtlog-inl.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fmtlog.cpp
)

target_compile_features(fmtlog PUBLIC cxx_std_11)

# Suppress MSVC warnings in fmtlog vendor code
if(MSVC)
    target_compile_options(fmtlog PRIVATE
        /wd4702  # unreachable code
        /wd4267  # conversion from 'size_t' to smaller type
        /wd4244  # conversion from larger to smaller type
    )
endif()

# Find dependencies
find_package(fmt CONFIG REQUIRED)
# Link dependencies
target_link_libraries(
    fmtlog
     PUBLIC
    fmt::fmt-header-only
)
set_target_properties(fmtlog PROPERTIES LINKER_LANGUAGE CXX)

set_target_properties(fmtlog PROPERTIES FOLDER "vendor/fmtlog")
