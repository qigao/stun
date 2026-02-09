add_executable(dwg2dxf
    main.cpp
    dx_iface.cpp
    dx_iface.h
    dx_data.h
)

target_link_libraries(dwg2dxf PRIVATE dxfrw)
target_include_directories(dwg2dxf PRIVATE ../src)
set_target_properties(dwg2dxf PROPERTIES FOLDER "vendor/dxf/tools")
