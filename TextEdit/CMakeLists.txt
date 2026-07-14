add_library(textedit_flexui INTERFACE)
add_library(TextEdit::FlexUI ALIAS textedit_flexui)

target_link_libraries(textedit_flexui INTERFACE flexUI)
target_include_directories(
  textedit_flexui
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include/TextEdit>)
target_compile_features(textedit_flexui INTERFACE cxx_std_17)
set_target_properties(textedit_flexui PROPERTIES FOLDER "TextEdit")

if(BUILD_TESTING)
  add_executable(test_textedit_flexui tests/test_flexui_text_editor.cpp)
  target_link_libraries(
    test_textedit_flexui PRIVATE TextEdit::FlexUI TurboUtils::TinyTest)
  set_target_properties(test_textedit_flexui PROPERTIES FOLDER "TextEdit/tests")
  add_test(NAME test_textedit_flexui COMMAND test_textedit_flexui)
endif()

install(FILES FlexUITextEditor.h DESTINATION include/TextEdit)
install(TARGETS textedit_flexui)
