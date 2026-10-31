# Markdown syntax tests, CommonMark 652/652 and GFM oracles are now
# owned and executed by the installed Salts::MarkdownParser SDK.
# Stun keeps only real CMeta/UI and Tango integration tests.
add_executable(stun_ui_core_test ui_core_test.c)
target_compile_features(stun_ui_core_test PRIVATE c_std_11)
target_link_libraries(stun_ui_core_test PRIVATE Stun::UICore)
add_test(NAME stun_ui_core_test COMMAND stun_ui_core_test)

add_executable(stun_markdown_adapter_test markdown_adapter_test.c)
target_compile_features(stun_markdown_adapter_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_adapter_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_adapter_test COMMAND stun_markdown_adapter_test)

if(TARGET tango)
    add_executable(stun_markdown_tango_candidate_test
                   markdown_tango_candidate_test.c)
    target_compile_features(stun_markdown_tango_candidate_test PRIVATE c_std_11)
    target_link_libraries(stun_markdown_tango_candidate_test
                          PRIVATE Stun::UICore tango)
    add_test(NAME stun_markdown_tango_candidate_test
             COMMAND stun_markdown_tango_candidate_test)
endif()
