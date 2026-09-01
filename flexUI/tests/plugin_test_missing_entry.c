#if defined(_WIN32)
__declspec(dllexport)
#endif
int flexui_test_plugin_without_entry(void) {
  return 1;
}
