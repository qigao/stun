#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <algorithm>

#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex.h>
#include "flex/backends/thorvg/init.h"

// Include Player headers
#include "engine/player.h"
#include "flex_display.h"

// SDL Globals
static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* video_texture = nullptr;
static SDL_Texture* ui_texture = nullptr;
static int WINDOW_WIDTH = 1280;
static int WINDOW_HEIGHT = 720;

// Flex Globals
static flex::Instance::Ptr instance;
static std::unique_ptr<flex::Renderer> flex_renderer;
static tvg::SwCanvas* canvas = nullptr;
static std::vector<uint32_t> ui_buffer;

// Player Globals
static std::unique_ptr<Player> player;
static FlexDisplay* flex_display_ptr = nullptr; // Raw pointer to the display owned by Player
static std::thread player_thread;

// Include Flex event system
#include <flex/runtime/event.h>

// UI Elements
static flex::Shape* progress_fill = nullptr;
static flex::Node* progress_group = nullptr;
static flex::Node* play_btn = nullptr;
static flex::Node* play_icon = nullptr;
static flex::Node* pause_icon = nullptr;
static flex::Node* start_btn = nullptr;
static flex::Node* prev_btn = nullptr;
static flex::Node* next_btn = nullptr;
static flex::Node* end_btn = nullptr;
static flex::Text* time_label = nullptr;
static bool is_scrubbing = false;

std::string format_time(double seconds) {
    int m = static_cast<int>(seconds) / 60;
    int s = static_cast<int>(seconds) % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    return std::string(buf);
}

void init_ui_refs() {
    auto scene = instance->scene();
    if (scene) {
        progress_fill = dynamic_cast<flex::Shape*>(scene->find("progress_fill"));
        progress_group = scene->find("progress");
        play_btn = scene->find("play_btn");
        play_icon = scene->find("play_icon");
        pause_icon = scene->find("pause_icon");
        start_btn = scene->find("start_btn");
        prev_btn = scene->find("prev_btn");
        next_btn = scene->find("next_btn");
        end_btn = scene->find("end_btn");
        time_label = dynamic_cast<flex::Text*>(scene->find("time_label"));

        // Attach interactions
        if (play_btn) {
            play_btn->on_click([]() {
                if (flex_display_ptr) {
                    bool p = flex_display_ptr->get_play();
                    flex_display_ptr->set_play(!p);
                }
            });
        }

        if (start_btn) {
            start_btn->on_click([]() {
                if (player) player->seek(0);
            });
        }

        if (end_btn) {
            end_btn->on_click([]() {
                if (flex_display_ptr && player) {
                    player->seek(flex_display_ptr->get_duration());
                }
            });
        }

        if (prev_btn) {
            prev_btn->on_click([]() {
                if (flex_display_ptr && player) {
                    player->seek(std::max(0.0, flex_display_ptr->get_position() - 5.0));
                }
            });
        }

        if (next_btn) {
            next_btn->on_click([]() {
                if (flex_display_ptr && player) {
                    player->seek(std::min(flex_display_ptr->get_duration(), flex_display_ptr->get_position() + 5.0));
                }
            });
        }

        if (progress_group) {
            auto seek_func = [](flex::PointerEvent& e) {
               if (flex_display_ptr && player) {
                    float w = e.current_target->bounds().width;
                    if (w > 0) {
                        float pct = std::max(0.0f, std::min(1.0f, e.local_x / w));
                        double dur = flex_display_ptr->get_duration();
                        double target = dur * pct;
                        player->seek(target);
                    }
                }
            };

            progress_group->on_pointer_down([seek_func](flex::PointerEvent& e) {
                is_scrubbing = true;
                seek_func(e);
            });
            
            progress_group->on_pointer_up([](flex::PointerEvent& e) {
                is_scrubbing = false;
            });

            progress_group->on_pointer_move([seek_func](flex::PointerEvent& e) {
                if (is_scrubbing) {
                    seek_func(e);
                }
            });
            
            progress_group->on_hover_leave([](flex::PointerEvent& e) {
                is_scrubbing = false;
            });
        }
    }
}

void update_ui() {
    if (flex_display_ptr) {
        double pos = flex_display_ptr->get_position();
        double dur = flex_display_ptr->get_duration();
        bool is_playing = flex_display_ptr->get_play();
        
        if (progress_fill && progress_group && dur > 0) {
            float pct = static_cast<float>(pos / dur);
            if (pct > 1.0f) pct = 1.0f;
            
            float max_w = progress_group->layout_width();
            if (max_w <= 0) max_w = 1240.0f;
            
            progress_fill->set_rect(pct * max_w, 10); 
        }
        
        if (time_label) {
            std::string text = format_time(pos) + " / " + format_time(dur);
            time_label->set_content(text);
        }

        // Toggle Icons
        if (play_icon) play_icon->set_visible(!is_playing);
        if (pause_icon) pause_icon->set_visible(is_playing);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: FlexPlayer <video_file>" << std::endl;
        return 1;
    }
    std::string video_file = argv[1];

    // 1. Init SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // 2. Init Flex
    flex::init();
    if (tvg::Initializer::init(0) != tvg::Result::Success) return 1;

    // Load Flex UI
    auto definition = flex::Definition::load_file("player_gui.flex");
    if (definition->has_error()) {
        std::cerr << "Flex UI Error: " << definition->error_message() << std::endl;
        return 1;
    }
    instance = flex::Instance::create(definition);
    init_ui_refs(); 

    // 3. Create Window
    window = SDL_CreateWindow("FlexPlayer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // Create Textures
    ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                   WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    // ThorVG Canvas
    canvas = tvg::SwCanvas::gen();
    ui_buffer.resize(WINDOW_WIDTH * WINDOW_HEIGHT);
    canvas->target(ui_buffer.data(), WINDOW_WIDTH, WINDOW_WIDTH, WINDOW_HEIGHT, tvg::ColorSpace::ARGB8888);
    flex_renderer = flex::create_thorvg_renderer(canvas);


    // 4. Init Player
    try {
        // Create Player with factory
        auto factory = [](int w, int h) -> std::unique_ptr<VideoDisplay> {
            auto display = std::make_unique<FlexDisplay>(w, h);
            flex_display_ptr = display.get(); // Store reference
            return display;
        };

        player = std::make_unique<Player>(video_file, true, "", factory);
        
        // Start Player Thread
        player_thread = std::thread([&]() {
            try {
                (*player)();
            } catch (const std::exception& e) {
                std::cerr << "Player Error: " << e.what() << std::endl;
            }
        });

    } catch (const std::exception& e) {
        std::cerr << "Failed to start player: " << e.what() << std::endl;
        return 1;
    }

    // 5. Main Loop
    bool running = true;
    while (running) {
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_QUIT) {
                running = false;
                if (flex_display_ptr) flex_display_ptr->set_quit(true);
            } else if (evt.type == SDL_KEYDOWN) {
                if (evt.key.keysym.sym == SDLK_SPACE) {
                    if (flex_display_ptr) {
                        bool p = flex_display_ptr->get_play();
                        flex_display_ptr->set_play(!p);
                    }
                } else if (evt.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                    if (flex_display_ptr) flex_display_ptr->set_quit(true);
                }
            } else if (evt.type == SDL_WINDOWEVENT && evt.window.event == SDL_WINDOWEVENT_RESIZED) {
                 WINDOW_WIDTH = evt.window.data1;
                 WINDOW_HEIGHT = evt.window.data2;
                 
                 SDL_DestroyTexture(ui_texture);
                 ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                                WINDOW_WIDTH, WINDOW_HEIGHT);
                 SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);
                 
                 ui_buffer.resize(WINDOW_WIDTH * WINDOW_HEIGHT);
                 canvas->target(ui_buffer.data(), WINDOW_WIDTH, WINDOW_WIDTH, WINDOW_HEIGHT, tvg::ColorSpace::ARGB8888);
            } else if (evt.type == SDL_MOUSEMOTION) {
                instance->send_pointer_event((float)evt.motion.x, (float)evt.motion.y, (evt.motion.state & SDL_BUTTON_LMASK));
            } else if (evt.type == SDL_MOUSEBUTTONDOWN) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                     instance->send_pointer_event((float)evt.button.x, (float)evt.button.y, true);
                }
            } else if (evt.type == SDL_MOUSEBUTTONUP) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                     instance->send_pointer_event((float)evt.button.x, (float)evt.button.y, false);
                }
            }
        }

        // Render Video
        SDL_RenderClear(renderer);
        
        if (flex_display_ptr && flex_display_ptr->is_new_frame_available()) {
             // Update Video Texture
             std::vector<uint8_t> y, u, v;
             int yp, up, vp;
             flex_display_ptr->get_frame_data(y, u, v, yp, up, vp);
             
             if (!video_texture) {
                 video_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
                                                   flex_display_ptr->get_width(), flex_display_ptr->get_height());
             }
             
             SDL_UpdateYUVTexture(video_texture, nullptr,
                                  y.data(), yp,
                                  u.data(), up,
                                  v.data(), vp);
        }
        
        if (video_texture) {
            SDL_RenderCopy(renderer, video_texture, nullptr, nullptr);
        }

        // Render UI
        update_ui();
        instance->advance(0.016f); // Approx 60fps
        
        std::fill(ui_buffer.begin(), ui_buffer.end(), 0x00000000); 
        canvas->remove( ); // Reset internal state
        flex_renderer->begin_frame(WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f);
        instance->render(*flex_renderer);
        flex_renderer->end_frame();
        canvas->draw();
        canvas->sync();
        
        SDL_UpdateTexture(ui_texture, nullptr, ui_buffer.data(), WINDOW_WIDTH * 4);
        SDL_RenderCopy(renderer, ui_texture, nullptr, nullptr);

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    // Cleanup
    if (player_thread.joinable()) player_thread.join();
    
    if (video_texture) SDL_DestroyTexture(video_texture);
    if (ui_texture) SDL_DestroyTexture(ui_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    
    flex::shutdown();
    SDL_Quit();

    return 0;
}
