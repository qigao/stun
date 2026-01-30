add_executable(agent_cli 
    main.cpp
    agent_model.cpp
    agent_controller.cpp
    agent_view.cpp
    # Headers are optional in add_executable but good for IDEs
    agent_model.h
    agent_controller.h
    agent_view.h
    styles.h
    panels/top_panel.h
    panels/content_panel.h
    panels/bottom_panel.h
)
target_link_libraries(agent_cli PRIVATE tui flexUI)
if(WIN32)
    target_link_libraries(agent_cli PRIVATE winmm)
endif()
set_target_properties(agent_cli PROPERTIES FOLDER tui/agent)
