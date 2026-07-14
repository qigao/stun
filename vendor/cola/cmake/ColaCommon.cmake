# Common CMake configurations for all cola libraries

if(WIN32)
    # 统一禁用 Windows.h 的 min/max 宏，解决 std::min/max 冲突
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
    
    # 强制所有库导出所有符号（作为兜底），虽然我们也在用导出宏
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# 统一隐藏符号
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# 辅助宏：为目标添加标准的包含路径并设置 IDE 目录
macro(cola_target_setup target_name)
    target_include_directories(${target_name} PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )
    # 将项目组织到 IDE 的 vendor/cola 文件夹下
    set_target_properties(${target_name} PROPERTIES FOLDER "vendor/cola")
endmacro()
