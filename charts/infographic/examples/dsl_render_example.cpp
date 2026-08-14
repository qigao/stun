// Infographic DSL -> Flex Runtime rendering example
#include <SDL2/SDL.h>
#include <flex.h>
#include <flex/render/engines/thorvg.h>
#include <ir/unified_infographic.h>
#include <infographic_component.h>
#include <flex/runtime/instance.h>
#include <iostream>
#include <thorvg.h>

namespace flex::modules::infographic {
bool parse_infographic_dsl(const char* source, UnifiedInfographic* infographic, std::string& error_msg);
}

using namespace flex;
using namespace flex::modules::infographic;

static constexpr int WIDTH = 900;
static constexpr int HEIGHT = 700;

const char* DEMO_DSL = R"(
infographic list-grid-badge-card
data {
    title: "Q4 2024 Performance"
    desc: "Key business metrics overview"
    items: [
        { label: "Revenue", desc: "$12.8M", value: 12.8, icon: "mdi/currency-usd" }
        { label: "Users", desc: "45,000", value: 45, icon: "mdi/account" }
        { label: "Growth", desc: "+23%", value: 23, icon: "mdi/chart-line" }
        { label: "NPS Score", desc: "72", value: 72, icon: "mdi/star" }
        { label: "Retention", desc: "94%", value: 94, icon: "mdi/target" }
        { label: "Markets", desc: "12", value: 12, icon: "mdi/briefcase" }
    ]
}
theme {
    palette: #3b82f6 #22c55e #f59e0b #ef4444 #8b5cf6 #06b6d4
}
)";

const char* TIMELINE_DSL = R"(
infographic sequence-timeline-simple
data {
    title: "Product Roadmap 2025"
    items: [
        { label: "Research", desc: "Market analysis", icon: "mdi/lightbulb" }
        { label: "Design", desc: "UX/UI prototypes", icon: "mdi/cog" }
        { label: "Development", desc: "Core features", icon: "mdi/code-tags" }
        { label: "Beta", desc: "User testing", icon: "mdi/account" }
        { label: "Launch", desc: "Public release", icon: "mdi/rocket-launch" }
    ]
}
theme {
    palette: #6366f1 #8b5cf6 #a855f7 #d946ef #ec4899
}
)";

const char* PIE_DSL = R"(
infographic chart-pie-plain-text
data {
    title: "Market Share Analysis"
    items: [
        { label: "Product A", value: 42 }
        { label: "Product B", value: 28 }
        { label: "Product C", value: 18 }
        { label: "Others", value: 12 }
    ]
}
theme {
    palette: #3b82f6 #22c55e #f59e0b #94a3b8
}
)";

const char* FUNNEL_DSL = R"(
infographic sequence-funnel-simple
data {
    title: "Sales Pipeline"
    items: [
        { label: "Visitors", value: 10000 }
        { label: "Leads", value: 3500 }
        { label: "Qualified", value: 1200 }
        { label: "Proposals", value: 450 }
        { label: "Closed", value: 180 }
    ]
}
)";

const char* SWOT_DSL = R"(
infographic compare-swot
data {
    title: "Strategic Analysis"
    items: [
        { label: "Strengths", children: [
            { label: "Strong R&D team" }
            { label: "Market leader" }
        ]}
        { label: "Weaknesses", children: [
            { label: "High costs" }
            { label: "Limited reach" }
        ]}
        { label: "Opportunities", children: [
            { label: "New markets" }
            { label: "AI integration" }
        ]}
        { label: "Threats", children: [
            { label: "Competition" }
            { label: "Regulation" }
        ]}
    ]
}
)";

const char* BAR_DSL = R"(
infographic chart-bar-plain-text
data {
    title: "Monthly Revenue"
    items: [
        { label: "January", value: 120 }
        { label: "February", value: 180 }
        { label: "March", value: 150 }
        { label: "April", value: 220 }
        { label: "May", value: 280 }
    ]
}
theme {
    palette: #3b82f6 #22c55e #f59e0b #ef4444 #8b5cf6
}
)";

const char* TREE_DSL = R"(
infographic hierarchy-tree-tech-style-capsule-item
data {
    title: "Organization"
    items: [
        { label: "CEO", children: [
            { label: "CTO" }
            { label: "CFO" }
            { label: "COO" }
        ]}
    ]
}
)";

const char* VS_DSL = R"(
infographic compare-binary-horizontal-underline-text-vs
data {
    title: "Cloud vs On-Premise"
    items: [
        { label: "Cloud", children: [
            { label: "Scalable" }
            { label: "Pay as you go" }
            { label: "Auto updates" }
        ]}
        { label: "On-Premise", children: [
            { label: "Full control" }
            { label: "One-time cost" }
            { label: "Data privacy" }
        ]}
    ]
}
theme {
    palette: #3b82f6 #ef4444
}
)";

const char* DONUT_DSL = R"(
infographic chart-pie-donut-plain-text
data {
    title: "Revenue by Region"
    items: [
        { label: "North America", value: 45 }
        { label: "Europe", value: 28 }
        { label: "Asia Pacific", value: 18 }
        { label: "Others", value: 9 }
    ]
}
)";

const char* ZIGZAG_DSL = R"(
infographic sequence-snake-steps-simple
data {
    title: "User Journey"
    items: [
        { label: "Awareness", desc: "Discover product" }
        { label: "Interest", desc: "Learn features" }
        { label: "Decision", desc: "Compare options" }
        { label: "Action", desc: "Make purchase" }
        { label: "Loyalty", desc: "Become advocate" }
    ]
}
)";

const char* CIRCULAR_DSL = R"(
infographic sequence-circular-simple
data {
    title: "Development Cycle"
    items: [
        { label: "Plan", icon: "mdi/lightbulb" }
        { label: "Design", icon: "mdi/cog" }
        { label: "Develop", icon: "mdi/code-tags" }
        { label: "Test", icon: "mdi/shield-check" }
        { label: "Deploy", icon: "mdi/rocket-launch" }
        { label: "Monitor", icon: "mdi/chart-line" }
    ]
}
)";

const char* ILLUS_DSL = R"(
infographic list-grid-badge-card
data {
    title: "Our Services"
    items: [
        { label: "Development", illus: "coding" }
        { label: "Design", illus: "design" }
        { label: "Analytics", illus: "analytics" }
    ]
}
theme {
    palette: #6366f1 #ec4899 #14b8a6
}
)";

class DslRenderExample {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;

        window_ = SDL_CreateWindow("Infographic DSL Demo (1-4 to switch)", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
        sdl_renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        texture_ = SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

        flex::render::engines::thorvg::init();
        canvas_ = tvg::SwCanvas::gen();
        buffer_.resize(WIDTH * HEIGHT);
        canvas_->target(buffer_.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);
        flex::render::engines::thorvg::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf");

        flex_renderer_ = flex::render::engines::thorvg::create_renderer(canvas_);
        load_dsl(DEMO_DSL);
        return true;
    }

    void load_dsl(const char* dsl) {
        UnifiedInfographic info;
        std::string error;
        
        if (!parse_infographic_dsl(dsl, &info, error)) {
            std::cerr << "Parse error: " << error << "\n";
            return;
        }

        instance_ = Instance::create(WIDTH, HEIGHT);
        auto* scene = instance_->scene();
        scene->set_background(Color(0.96f, 0.96f, 0.98f));

        auto* node = InfographicComponent::build(info, *instance_);
        if (node) {
            node->set_position(50, 30);
            scene->root()->add_child(node);
        }
        scene->root()->perform_layout();
    }

    void run() {
        bool running = true;
        SDL_Event event;
        Uint32 last_time = SDL_GetTicks();

        while (running) {
            Uint32 current_time = SDL_GetTicks();
            float dt = (current_time - last_time) / 1000.0f;
            last_time = current_time;
            dt = std::min(dt, 0.1f);

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
                if (event.type == SDL_KEYDOWN) {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE: running = false; break;
                        case SDLK_1: load_dsl(DEMO_DSL); break;
                        case SDLK_2: load_dsl(TIMELINE_DSL); break;
                        case SDLK_3: load_dsl(PIE_DSL); break;
                        case SDLK_4: load_dsl(FUNNEL_DSL); break;
                        case SDLK_5: load_dsl(SWOT_DSL); break;
                        case SDLK_6: load_dsl(BAR_DSL); break;
                        case SDLK_7: load_dsl(TREE_DSL); break;
                        case SDLK_8: load_dsl(VS_DSL); break;
                        case SDLK_9: load_dsl(DONUT_DSL); break;
                        case SDLK_0: load_dsl(ZIGZAG_DSL); break;
                        case SDLK_c: load_dsl(CIRCULAR_DSL); break;
                        case SDLK_i: load_dsl(ILLUS_DSL); break;
                    }
                }
            }

            if (instance_) {
                instance_->advance(dt);
                flex_renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);
                flex_renderer_->clear(instance_->scene()->background());
                instance_->render(*flex_renderer_);
                flex_renderer_->end_frame();
            }

            SDL_UpdateTexture(texture_, nullptr, buffer_.data(), WIDTH * sizeof(uint32_t));
            SDL_RenderClear(sdl_renderer_);
            SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(sdl_renderer_);
            SDL_Delay(16);
        }
    }

    ~DslRenderExample() {
        flex_renderer_.reset();
        instance_.reset();
        flex::render::engines::thorvg::shutdown();
        delete canvas_;
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(sdl_renderer_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    tvg::SwCanvas* canvas_ = nullptr;
    std::vector<uint32_t> buffer_;
    Instance::Ptr instance_;
    std::unique_ptr<Renderer> flex_renderer_;
};

int main(int argc, char* argv[]) {
    DslRenderExample app;
    if (app.init()) {
        std::cout << "Infographic DSL Demo - Press keys to switch:\n";
        std::cout << "  1: Grid    2: Timeline  3: Pie     4: Funnel\n";
        std::cout << "  5: SWOT    6: Bar       7: Tree    8: VS Compare\n";
        std::cout << "  9: Donut   0: Zigzag    C: Circular  I: Illus\n";
        std::cout << "  ESC: Quit\n";
        app.run();
    }
    return 0;
}
