cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StunUICore LANGUAGES C)
    include(CTest)
    find_package(Salts CONFIG REQUIRED)
endif()

add_library(stun_ui_core STATIC src/ui_core.c src/markdown_c11.c src/markdown_entities.c src/markdown_html.c)
add_library(Stun::UICore ALIAS stun_ui_core)

target_compile_features(stun_ui_core PUBLIC c_std_11)
target_include_directories(stun_ui_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
# UTF-8 whitespace in inline-flanking rules belongs to the canonical Unicode
# provider. An installed Salts SDK without this component must fail configure;
# never silently fall back to locale tables or a Stun-local Unicode copy.
target_link_libraries(stun_ui_core PUBLIC Salts::CMeta Salts::Unicode)
set_target_properties(stun_ui_core PROPERTIES EXPORT_NAME UICore)
include(GNUInstallDirs)
install(TARGETS stun_ui_core EXPORT StunUICoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT FlexUICore
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT FlexUICore
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT FlexUICore)
install(EXPORT StunUICoreTargets FILE StunUICoreTargets.cmake
  NAMESPACE Stun:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlexUI"
  COMPONENT FlexUICore)
install(DIRECTORY include/stun DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT FlexUICore)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
