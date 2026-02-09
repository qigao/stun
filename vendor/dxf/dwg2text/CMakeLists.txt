add_executable(dwg2text
    main.cpp
    dx_iface.cpp
    dx_iface.h
    dx_data.h
)

target_link_libraries(dwg2text PRIVATE dxfrw)
target_include_directories(dwg2text PRIVATE ../src)
set_target_properties(dwg2text PROPERTIES FOLDER "vendor/dxf/tools")
