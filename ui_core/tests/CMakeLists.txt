add_executable(stun_ui_core_test ui_core_test.c)
target_link_libraries(stun_ui_core_test PRIVATE Stun::UICore)
target_compile_features(stun_ui_core_test PRIVATE c_std_11)
add_test(NAME stun_ui_core_test COMMAND stun_ui_core_test)

add_executable(stun_markdown_c11_test markdown_c11_test.c)
target_compile_features(stun_markdown_c11_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_c11_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_c11_test COMMAND stun_markdown_c11_test)

# #182/#185: strict C11 root paragraph cross-line inline owner and rollback.
add_executable(stun_markdown_paragraph_inline_test markdown_paragraph_inline_test.c)
target_compile_features(stun_markdown_paragraph_inline_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_paragraph_inline_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_paragraph_inline_test
         COMMAND stun_markdown_paragraph_inline_test)

add_executable(stun_markdown_golden_test markdown_golden_test.c)
target_link_libraries(stun_markdown_golden_test PRIVATE Stun::UICore)
target_compile_features(stun_markdown_golden_test PRIVATE c_std_11)
add_test(NAME stun_markdown_golden_test COMMAND stun_markdown_golden_test)

# Test-only semantic introspection. A separate Python process parses the same
# Markdown using Mistune and compares complete normalized RichDoc ASTs.
add_executable(stun_markdown_ast_dump markdown_c11_dump.c)
target_compile_features(stun_markdown_ast_dump PRIVATE c_std_11)
target_link_libraries(stun_markdown_ast_dump PRIVATE Stun::UICore)

add_executable(stun_markdown_html_test markdown_html_test.c)
target_compile_features(stun_markdown_html_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_html_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_html_test COMMAND stun_markdown_html_test)

# Test-only native HTML emitter for independent cmark-gfm oracle.
add_executable(stun_markdown_html_dump markdown_html_dump.c)
target_compile_features(stun_markdown_html_dump PRIVATE c_std_11)
target_link_libraries(stun_markdown_html_dump PRIVATE Stun::UICore)

# CommonMark #177: native typed indented-code AST, HTML and ownership checks.
add_executable(stun_markdown_indented_test markdown_indented_test.c)
target_compile_features(stun_markdown_indented_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_indented_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_indented_test COMMAND stun_markdown_indented_test)

# CommonMark #181 typed list/container metadata, exact HTML and ownership.
add_executable(stun_markdown_containers_test markdown_containers_test.c)
target_compile_features(stun_markdown_containers_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_containers_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_containers_test COMMAND stun_markdown_containers_test)

# #181 second slice: root blockquote lazy continuation and exact owned AST/HTML.
add_executable(stun_markdown_quote_test markdown_quote_test.c)
target_compile_features(stun_markdown_quote_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_quote_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_quote_test COMMAND stun_markdown_quote_test)


add_executable(stun_markdown_nested_quote_test markdown_nested_quote_test.c)
target_compile_features(stun_markdown_nested_quote_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_nested_quote_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_nested_quote_test COMMAND stun_markdown_nested_quote_test)

# CommonMark #181: the quote owns nested lists; the root owns following lists.
add_executable(stun_markdown_quoted_list_test markdown_quoted_list_test.c)
target_compile_features(stun_markdown_quoted_list_test PRIVATE c_std_11)
target_link_libraries(stun_markdown_quoted_list_test PRIVATE Stun::UICore)
add_test(NAME stun_markdown_quoted_list_test COMMAND stun_markdown_quoted_list_test)
