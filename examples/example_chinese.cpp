/*
    examples/chinese_input_test.cpp -- Test Chinese/CJK input support

    Demonstrates:
    - TextBox with Chinese input via IME
    - UTF-8 text rendering
    - Mixed language support (English + Chinese)

    Note: Requires CJK font to display Chinese characters.
    See CHINESE_INPUT_SUPPORT.md for font installation instructions.
*/

#include <iostream>
#include <nanogui/button.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/screen.h>
#include <nanogui/textbox.h>
#include <nanogui/window.h>


using namespace nanogui;

int main(int argc, char **argv) {
  nanogui::init();

  auto *screen = new Screen(Vector2i(800, 600), "Chinese Input Test - 中文输入测试");

  auto *window = new Window(screen, "Text Input Test");
  window->set_position(Vector2i(50, 50));
  window->set_layout(new GroupLayout());

  // Instructions
  new Label(window, "Chinese Input Test", "sans-bold", 24);
  new Label(window, "Use your IME to type Chinese, Japanese, Korean, or emoji", "sans");
  new Label(window, "使用输入法输入中文、日文、韩文或表情符号", "sans");

  // Separator
  new Label(window, "", "sans");

  // Test 1: Basic text input
  new Label(window, "Test 1: Basic Input", "sans-bold");
  auto *textbox1 = new TextBox(window, "");
  textbox1->set_editable(true);
  textbox1->set_placeholder("Type here... 在这里输入...");
  textbox1->set_fixed_width(400);

  auto *result1 = new Label(window, "", "sans");
  textbox1->set_callback([result1](const std::string &value) {
    result1->set_caption("You entered: " + value);
    std::cout << "Input: " << value << std::endl;
    return true;
  });

  // Test 2: Pre-filled Chinese text
  new Label(window, "", "sans");
  new Label(window, "Test 2: Pre-filled Text", "sans-bold");
  auto *textbox2 = new TextBox(window, "你好世界 Hello World 🌍");
  textbox2->set_editable(true);
  textbox2->set_fixed_width(400);

  // Test 3: Mixed content
  new Label(window, "", "sans");
  new Label(window, "Test 3: Mixed Languages", "sans-bold");
  auto *textbox3 = new TextBox(window, "");
  textbox3->set_editable(true);
  textbox3->set_placeholder("English + 中文 + 日本語 + 한국어 + 🎨");
  textbox3->set_fixed_width(400);

  // Test 4: Display area
  new Label(window, "", "sans");
  new Label(window, "Test 4: Display Test", "sans-bold");
  new Label(window, "你好世界 こんにちは 안녕하세요 🚀", "sans");

  // Clear button
  new Label(window, "", "sans");
  auto *clear_btn = new Button(window, "Clear All");
  clear_btn->set_callback([textbox1, textbox2, textbox3, result1]() {
    textbox1->set_value("");
    textbox2->set_value("");
    textbox3->set_value("");
    result1->set_caption("");
  });

  // Info section
  new Label(window, "", "sans");
  new Label(window, "Note: If you see boxes (□) instead of characters,", "sans");
  new Label(window, "you need to add a CJK font. See CHINESE_INPUT_SUPPORT.md", "sans");

  screen->set_visible(true);
  screen->perform_layout();

  std::cout << "=== Chinese Input Test ===" << std::endl;
  std::cout << "1. Click on a text box" << std::endl;
  std::cout << "2. Activate your IME (e.g., press Ctrl+Space on Windows)" << std::endl;
  std::cout << "3. Type pinyin (e.g., 'nihao') and select characters" << std::endl;
  std::cout << "4. The text should appear in the box" << std::endl;
  std::cout << std::endl;
  std::cout << "If characters show as boxes, add a CJK font:" << std::endl;
  std::cout << "See CHINESE_INPUT_SUPPORT.md for instructions" << std::endl;
  std::cout << "=========================" << std::endl;

  nanogui::run();

  nanogui::shutdown();
  return 0;
}
