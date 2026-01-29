/*
 * flexUI - Markdown Demo
 */

#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/bridge/renderer.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <iostream>
#include <memory>
#include <fstream>
#include <vector>

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
"```cpp\n"
"auto* md = box->create_widget<MarkdownWidget>(\"div\", \"md1\", \"# Hello\");\n"
"root->append(md);\n"
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
        height: 100%;
        padding: 40px;
        background-color: #1e1e1e;
        display: flex;
        flex-direction: column;
    }

    #markdown {
        width: 100%;
        display: flex;
        flex-direction: column;
    }

    .md-p, .md-li, .md-header {
        display: flex;
        flex-direction: row;
        flex-wrap: nowrap; /* flexUI doesn't support wrap yet, so keep on one row */
        align-items: center;
        width: 100%;
    }

    .md-p {
        margin-bottom: 15px;
        color: #abb2bf;
    }

    .h1 { font-size: 32px; border-bottom: 1px solid #3e4451; padding-bottom: 10px; margin-bottom: 20px; font-weight: 700; color: #61afef; }
    .h2 { font-size: 24px; margin-top: 20px; margin-bottom: 10px; font-weight: 700; color: #61afef; }
    .h3 { font-size: 20px; margin-top: 15px; margin-bottom: 10px; font-weight: 700; color: #61afef; }

    .md-quote {
        display: flex;
        flex-direction: column;
        border-left: 4px solid #528bff;
        padding: 10px 15px;
        margin-bottom: 15px;
        font-style: italic;
        background-color: #2c313a;
    }

    .md-code-block {
        display: flex;
        flex-direction: column;
        background-color: #282c34;
        padding: 15px;
        border-radius: 5px;
        margin-bottom: 15px;
        font-family: "Courier New";
        color: #d19a66;
    }

    .md-code {
        background-color: #3e4451;
        padding: 2px 5px;
        border-radius: 3px;
        font-family: "Courier New";
        color: #e06c75;
    }

    .md-hr {
        height: 1px;
        background-color: #3e4451;
        margin: 20px 0;
    }

    .md-strong {
        color: #e06c75;
        font-weight: 700;
        margin: 0 4px;
    }

    .md-em {
        font-style: italic;
        margin: 0 4px;
    }
    
    span {
        display: flex;
    }
)";

bool load_font(const char* name, const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << path << std::endl;
        return false;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        std::cerr << "Failed to read " << path << std::endl;
        return false;
    }

    if (tvg::Text::load(name, buffer.data(), static_cast<uint32_t>(size), "ttf", true) != tvg::Result::Success) {
        std::cerr << "Failed to load " << name << " font" << std::endl;
        return false;
    }

    std::cout << "Font loaded: " << name << std::endl;
    return true;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_Window* window = SDL_CreateWindow(
        "flexUI Markdown Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, SDL_WINDOW_SHOWN
    );

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    tvg::Initializer::init(0);

    if (!load_font("Arial", "C:/Windows/Fonts/arial.ttf")) {
        std::cerr << "Warning: Could not load Arial font. Text might not render." << std::endl;
    }
    
    auto canvas = std::unique_ptr<tvg::SwCanvas>(tvg::SwCanvas::gen());
    canvas->target(static_cast<uint32_t*>(surface->pixels), surface->w, surface->pitch / 4, surface->h, tvg::ColorSpace::ARGB8888);

    auto flex_renderer = flex::create_thorvg_renderer(canvas.get());
    auto box = std::make_unique<Box>(flex_renderer.get());
    box->set_viewport(800, 600);
    box->load_css(MD_CSS);

    auto* root = box->create("div", "root");
    box->set_root(root);

    auto* md_widget = box->create_widget<MarkdownWidget>("div", "markdown", DEMO_MARKDOWN);
    root->append(md_widget);

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        box->update_time(16.0f);
        box->update();
        SDL_UpdateWindowSurface(window);
        SDL_Delay(16);
    }

    tvg::Initializer::term();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
