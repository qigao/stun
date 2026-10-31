cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StunUICore LANGUAGES C)
    include(CTest)
    find_package(Salts CONFIG REQUIRED)
    find_package(SaltsUtils CONFIG REQUIRED)
endif()

# Consumer uses the ONLY installed native C11 Markdown parser. An old SDK
# without these exact targets is an explicit configure RED, never a fallback.
foreach(md_target IN ITEMS Salts::MarkdownParser Salts::MarkdownHtml)
    if(NOT TARGET "${md_target}")
        message(FATAL_ERROR "Stun requires installed ${md_target} from SaltsUtils")
    endif()
endforeach()

add_library(stun_ui_core STATIC src/ui_core.c src/markdown_adapter.c)
add_library(Stun::UICore ALIAS stun_ui_core)
target_compile_features(stun_ui_core PUBLIC c_std_11)
target_include_directories(stun_ui_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(stun_ui_core PUBLIC
    Salts::CMeta Salts::Unicode Salts::MarkdownParser Salts::MarkdownHtml)

# Export ONLY the adapter in the actual product package. The canonical
# MarkdownParser/Html implementation remains owned by installed SaltsUtils.
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
