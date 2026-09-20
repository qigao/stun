add_library(flexUI_controller STATIC application.cpp application_command.cpp
                                      application_completion.cpp
                                      application_service_requests.cpp
                                      application_service_dispatcher.cpp
                                      application_host_bridge.cpp controller.cpp
                                      mutation.cpp box_mutation_host.cpp)
add_library(FlexUI::Controller ALIAS flexUI_controller)

target_link_libraries(flexUI_controller PUBLIC FlexUI::Core FlexUI::Services
                                        PRIVATE Salts::Core)
target_include_directories(
  flexUI_controller
  PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>)
set_target_properties(flexUI_controller PROPERTIES FOLDER "flexUI/controller")

install(TARGETS flexUI_controller DESTINATION lib)

if(FLEXUI_ENABLE_TURBOSCRIPT)
  set(_flexui_turboscript_hints)
  if(DEFINED TURBOSCRIPT_ROOT)
    list(APPEND _flexui_turboscript_hints "${TURBOSCRIPT_ROOT}")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      list(PREPEND _flexui_turboscript_hints "${TURBOSCRIPT_ROOT}/debug")
    elseif(CMAKE_BUILD_TYPE)
      list(PREPEND _flexui_turboscript_hints "${TURBOSCRIPT_ROOT}/release")
    endif()
  endif()
  find_package(TurboScript CONFIG REQUIRED HINTS ${_flexui_turboscript_hints})

  get_target_property(
    _flexui_turboscript_include_dirs
    TurboScript::TurboScript INTERFACE_INCLUDE_DIRECTORIES)
  set(_flexui_turboscript_public_header)
  foreach(_flexui_turboscript_include_dir
          IN LISTS _flexui_turboscript_include_dirs)
    if(IS_DIRECTORY "${_flexui_turboscript_include_dir}"
       AND EXISTS "${_flexui_turboscript_include_dir}/turbo_script.h")
      set(_flexui_turboscript_public_header
          "${_flexui_turboscript_include_dir}/turbo_script.h")
      break()
    endif()
  endforeach()
  if(NOT _flexui_turboscript_public_header)
    message(
      FATAL_ERROR
        "TurboScript::TurboScript does not publish a readable turbo_script.h include directory")
  endif()
  file(READ "${_flexui_turboscript_public_header}"
       _flexui_turboscript_public_header_content LIMIT 131072)
  foreach(_flexui_turboscript_required_token
          IN ITEMS TURBO_SCRIPT_HOST_ABI_VERSION turbo_script_string_view_t
                   turbo_script_value_view_t turbo_script_export_handle_t)
    string(FIND "${_flexui_turboscript_public_header_content}"
           "${_flexui_turboscript_required_token}"
           _flexui_turboscript_required_token_offset)
    if(_flexui_turboscript_required_token_offset EQUAL -1)
      message(
        FATAL_ERROR
          "FlexUI ControllerTurboScript requires the TurboScript 3 host ABI v1 public header; ${_flexui_turboscript_public_header} does not define ${_flexui_turboscript_required_token}. Reinstall the matching TurboScript package for this build profile")
    endif()
  endforeach()
  if(NOT _flexui_turboscript_public_header_content MATCHES
     "#define[ \t]+TURBO_SCRIPT_HOST_ABI_VERSION[ \t]+1u")
    message(
      FATAL_ERROR
        "FlexUI ControllerTurboScript requires TURBO_SCRIPT_HOST_ABI_VERSION 1u in ${_flexui_turboscript_public_header}")
  endif()

  add_library(flexUI_controller_turboscript STATIC
              application_turboscript.cpp controller_turboscript.cpp)
  add_library(FlexUI::ControllerTurboScript ALIAS flexUI_controller_turboscript)
  target_link_libraries(
    flexUI_controller_turboscript
    PUBLIC FlexUI::Controller
    PRIVATE TurboScript::TurboScript)
  target_include_directories(
    flexUI_controller_turboscript
    PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
           $<INSTALL_INTERFACE:include>)
  set_target_properties(
    flexUI_controller_turboscript PROPERTIES FOLDER "flexUI/controller")

  if(WIN32)
    foreach(_flexui_turboscript_runtime
            TurboScript::TurboScript
            TurboParser::Parser
            TurboParser::Cron
            TurboParser::Mustache
            TurboNet::CoroNet)
      add_custom_command(
        TARGET flexUI_controller_turboscript POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${_flexui_turboscript_runtime}>
                $<TARGET_FILE_DIR:flexUI_controller_turboscript>)
    endforeach()
  endif()

  install(TARGETS flexUI_controller_turboscript DESTINATION lib)
endif()
