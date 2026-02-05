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
"\n"
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
        background-color: #21252b;
        padding: 15px;
        border-radius: 6px;
        margin-bottom: 15px;
        border: 1px solid #3e4451;
        color: #d19a66;
        font-family: "Consolas";
        font-size: 14px;
        flex-shrink: 0;
    }

    .md-code {
        background-color: #3e4451;
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

    int width = 800;
    int height = 600;

    SDL_Window* window = SDL_CreateWindow(
        "flexUI Markdown Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    tvg::Initializer::init(0);

    if (!load_font("Arial", "C:/Windows/Fonts/arial.ttf")) {
        std::cerr << "Warning: Could not load Arial font. Text might not render." << std::endl;
    }
    if (!load_font("Consolas", "C:/Windows/Fonts/consola.ttf")) {
        std::cerr << "Warning: Could not load Consolas font. Code blocks might not render." << std::endl;
    }
    
    auto canvas = std::unique_ptr<tvg::SwCanvas>(tvg::SwCanvas::gen());
    canvas->target(static_cast<uint32_t*>(surface->pixels), surface->w, surface->pitch / 4, surface->h, tvg::ColorSpace::ARGB8888);

    auto flex_renderer = flex::create_thorvg_renderer(canvas.get());
    auto box = std::make_unique<Box>(flex_renderer.get());
    box->set_viewport((float)width, (float)height);
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
            if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    width = event.window.data1;
                    height = event.window.data2;
                    
                    // On Windows, the surface might be invalidated on resize
                    surface = SDL_GetWindowSurface(window);
                    if (surface) {
                        canvas->target(static_cast<uint32_t*>(surface->pixels), 
                                      surface->w, surface->pitch / 4, surface->h, 
                                      tvg::ColorSpace::ARGB8888);
                        box->set_viewport((float)width, (float)height);
                    }
                }
            }
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
