/*
 * flexUI - Markdown Demo (GLFW)
 */
#include <flexUI.h>
#include "glfw_app.h"
#include "host_input_bridge.h"
#include <flexUI/box.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <iostream>
#include <memory>
#include <string>

using namespace flexUI;

const char* DEMO_MARKDOWN = 
"# flexUI Markdown Renderer\n"
"\n"
"This is a demo of the **MarkdownWidget** implemented using [MD4C](https://github.com/mity/md4c).\n"
"\n"
"## Features:\n"
"- **Fast**: Event-driven parsing.\n"
"- **Flexible**: Uses flexUI element hierarchy.\n"
"- **CSS Styled**: Everything can be styled via CSS.\n"
"\n"
"### Code Example:\n"
"\n"
"```c\n"
"#include <stdio.h>\n"
"\n"
"int main(void) {\n"
"    const char* msg = \"Hello, World!\";\n"
"    printf(\"%s\\n\", msg);\n"
"    return 0;\n"
"}\n"
"```\n"
"\n"
"### JSON Example:\n"
"\n"
"```json\n"
"{\n"
"  \"name\": \"flexUI\",\n"
"  \"features\": [\"fast\", \"flexible\", \"modern\"],\n"
"  \"version\": 1.0,\n"
"  \"active\": true\n"
"}\n"
"```\n"
"\n"
"> This is a blockquote showing the support for block structures.\n"
"\n"
"---\n"
"\n"
"Enjoy using Markdown in your TUI applications!";

const char* MD_CSS = R"(
    #root {
        width: 100%;
        height: auto;
        padding: 40px;
        background-color: #1e2227;
        color: #abb2bf;
        display: flex;
        flex-direction: column;
    }

    #markdown {
        width: 100%;
        display: flex;
        flex-direction: column;
    }

    .md-p, .md-header {
        display: flex;
        flex-direction: row;
        align-items: center;
        flex-wrap: nowrap;
        margin-bottom: 8px;
        flex-shrink: 0;
    }

    .h1 { 
        font-size: 34px; 
        border-bottom: 2px solid #3e4451; 
        padding-bottom: 15px; 
        margin-top: 10px;
        margin-bottom: 25px; 
        font-weight: 700; 
        color: #61afef; 
    }
    .h2 { 
        font-size: 26px; 
        margin-top: 30px; 
        margin-bottom: 15px; 
        font-weight: 700; 
        color: #98c379; 
    }
    .h3 { 
        font-size: 20px; 
        margin-top: 25px; 
        margin-bottom: 15px; 
        font-weight: 700; 
        color: #d19a66; 
    }

    .md-quote {
        display: flex;
        flex-direction: column;
        border-left: 4px solid #528bff;
        padding: 12px 20px;
        margin-bottom: 15px;
        font-style: italic;
        background-color: #2c313a;
        color: #abb2bf;
    }

    .md-code-block {
        display: flex;
        flex-direction: column;
        background-color: #1e1e2e;
        padding: 16px 20px 20px 20px;
        border-radius: 8px;
        margin-top: 15px;
        margin-bottom: 15px;
        border: 1px solid #45475a;
        color: #cdd6f4;
        font-family: "Consolas";
        font-size: 14px;
        flex-shrink: 0;
        box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
    }

    .md-code-line {
        display: flex;
        flex-shrink: 0;
        height: 22px;
    }

    /* One Dark Pro syntax highlighting */
    .hl-keyword { color: #c678dd; }
    .hl-type { color: #e5c07b; }
    .hl-string { color: #98c379; }
    .hl-number { color: #d19a66; }
    .hl-comment { color: #5c6370; font-style: italic; }
    .hl-preproc { color: #e06c75; }
    .hl-operator { color: #56b6c2; }
    .hl-plain { color: #abb2bf; }

    .md-code {
        background-color: #ae3987ff;
        padding: 2px 6px;
        border-radius: 4px;
        color: #e06c75;
        font-family: "Consolas";
        flex-shrink: 0;
    }

    .md-ul, .md-ol {
        display: flex;
        flex-direction: column;
        margin-bottom: 15px;
        padding-left: 20px;
        flex-shrink: 0;
    }

    .md-li {
        display: flex;
        flex-direction: row;
        align-items: center;
        margin-bottom: 6px;
        gap: 6px;
        flex-shrink: 0;
    }

    .md-hr {
        height: 2px;
        background-color: #3e4451;
        margin: 25px 0;
        flex-shrink: 0;
    }

    .md-strong {
        color: #e06c75;
        font-weight: 700;
        margin: 0 1px;
        flex-shrink: 0;
    }

    .md-em {
        font-style: italic;
        color: #c678dd;
        margin: 0 1px;
        flex-shrink: 0;
    }
    
    span {
        display: flex;
        flex-shrink: 0;
    }
)";

class MarkdownDemoApp : public flex::GlfwApp {
public:
    MarkdownDemoApp() : flex::GlfwApp("flexUI Markdown Demo", 800, 600) {}

protected:
    bool on_init() override {
        if (!load_font("Arial", "C:/Windows/Fonts/arial.ttf")) {
            std::cerr << "Warning: Could not load Arial font." << std::endl;
        }
        if (!load_font("Consolas", "C:/Windows/Fonts/consola.ttf")) {
            std::cerr << "Warning: Could not load Consolas font." << std::endl;
        }

        box_ = std::make_unique<flexUI::Box>(renderer());
        box_->set_viewport((float)width(), (float)height());
        box_->load_css(MD_CSS);

        auto* root = box_->create("div", "root");
        box_->set_root(root);

        auto* md_widget = box_->create_widget<flexUI::MarkdownWidget>("div", "markdown", DEMO_MARKDOWN);
        root->append(md_widget);

        return true;
    }

    void on_update(float dt) override {
        box_->update_time(dt * 1000.0f);
    }

    void on_render() override {
        box_->invalidate(); // Force repaint for demo
        box_->update();
    }

    void on_resize(int w, int h) override {
        flex::GlfwApp::on_resize(w, h);
        if (box_) {
            box_->set_viewport((float)w, (float)h);
        }
    }

    void on_mouse_button(int button, int action, int mods) override {
        double x, y;
        glfwGetCursorPos(window(), &x, &y);
        auto e = (action == GLFW_PRESS)
            ? flexUI::Event::mouse_down((float)x, (float)y, glfw_to_button(button))
            : flexUI::Event::mouse_up((float)x, (float)y, glfw_to_button(button));
        box_->dispatch_event(e);
    }

    void on_cursor_pos(double x, double y) override {
        auto e = flexUI::Event::mouse_move((float)x, (float)y);
        box_->dispatch_event(e);
    }

    void on_scroll(double dx, double dy) override {
        double x, y;
        glfwGetCursorPos(window(), &x, &y);
        auto e = flexUI::Event::mouse_wheel((float)x, (float)y, (float)dx, (float)dy);
        box_->dispatch_event(e);
    }

    void on_char(unsigned int codepoint) override {
        char buf[5] = {};
        if (codepoint < 0x80) buf[0] = (char)codepoint;
        else if (codepoint < 0x800) { buf[0] = 0xC0 | (codepoint >> 6); buf[1] = 0x80 | (codepoint & 0x3F); }
        else { buf[0] = 0xE0 | (codepoint >> 12); buf[1] = 0x80 | ((codepoint >> 6) & 0x3F); buf[2] = 0x80 | (codepoint & 0x3F); }
        flexui_examples::dispatch_text_input_if_focused(box_.get(), buf);
    }

    void on_key(int key, int action, int mods) override {
        auto e = (action == GLFW_PRESS || action == GLFW_REPEAT)
            ? flexUI::Event::key_down(glfw_to_keycode(key), glfw_to_mods(mods))
            : flexUI::Event::key_up(glfw_to_keycode(key), glfw_to_mods(mods));
        box_->dispatch_event(e);
    }

private:
    flexUI::Box* host_box() override { return box_.get(); }
    std::unique_ptr<flexUI::Box> box_;
};

int main(int argc, char** argv) {
    MarkdownDemoApp app;
    if (!app.init()) return 1;
    app.run();
    return 0;
}
