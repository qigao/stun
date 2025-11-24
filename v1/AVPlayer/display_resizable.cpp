#include "display_resizable.h"
#include "player.h"
#include "filter_presets.h"
#include "audio_track_manager.h"
#include <chrono>
#include <cstdio>
#include <nanogui.h>
#include <nanovg.h>
#include <nfd.h>
#include <thread>


using namespace nanogui;
// Helper function to format time as HH:MM:SS or MM:SS
static std::string format_time(double seconds) {
  int total_seconds = static_cast<int>(seconds);
  int hours = total_seconds / 3600;
  int minutes = (total_seconds % 3600) / 60;
  int secs = total_seconds % 60;

  char buffer[32];
  if (hours > 0) {
    snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, secs);
  } else {
    snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
  }
  return std::string(buffer);
}
VideoCanvas::VideoCanvas(Widget *parent, unsigned video_width, unsigned video_height)
    : Canvas(parent, 1), video_width_(video_width), video_height_(video_height) {

  set_size(Vector2i(video_width, video_height));
  set_fixed_size(Vector2i(video_width, video_height));
  set_draw_border(false);

  video_shader_ = std::make_unique<VideoShader>();
  video_shader_->init(render_pass());
}

void VideoCanvas::update_frame(std::array<uint8_t *, 3> planes, std::array<size_t, 3> pitches) {
  video_shader_->upload_yuv_frame(planes, pitches, video_width_, video_height_);
}

void VideoCanvas::draw_contents() {
  Matrix4f mvp(1.f);
  video_shader_->render(mvp, size());
}

void VideoCanvas::set_canvas_size(const Vector2i &new_size) {
  set_size(new_size);
  set_fixed_size(new_size);
}

ResizableVideoScreen::ResizableVideoScreen(const unsigned video_width, const unsigned video_height)
    : Screen(Vector2i(video_width, video_height), "Video Player", true), video_width_(video_width),
      video_height_(video_height), windowed_size_(video_width, video_height) {

  set_background(Color(0, 0, 0, 255)); // Black background for letterboxing

  canvas_ = new VideoCanvas(this, video_width, video_height);
  canvas_->set_position(Vector2i(0, 0));
  canvas_->set_size(Vector2i(video_width, video_height));

  perform_layout();
  set_visible(true);
}

bool ResizableVideoScreen::resize_event(const Vector2i &new_size) {
  Screen::resize_event(new_size);

  // Calculate new canvas size maintaining aspect ratio
  float window_aspect = static_cast<float>(new_size.x()) / new_size.y();
  float video_aspect = static_cast<float>(video_width_) / video_height_;

  Vector2i canvas_size;
  Vector2i canvas_pos;

  if (window_aspect > video_aspect) {
    // Window is wider - fit to height
    canvas_size.y() = new_size.y();
    canvas_size.x() = static_cast<int>(new_size.y() * video_aspect);
    canvas_pos.x() = (new_size.x() - canvas_size.x()) / 2;
    canvas_pos.y() = 0;
  } else {
    // Window is taller - fit to width
    canvas_size.x() = new_size.x();
    canvas_size.y() = static_cast<int>(new_size.x() / video_aspect);
    canvas_pos.x() = 0;
    canvas_pos.y() = (new_size.y() - canvas_size.y()) / 2;
  }

  // Debug output
  printf("Window: %dx%d, Video: %dx%d, Canvas: %dx%d at (%d,%d)\n", new_size.x(), new_size.y(),
         video_width_, video_height_, canvas_size.x(), canvas_size.y(), canvas_pos.x(),
         canvas_pos.y());

  // Update canvas
  if (canvas_) {
    canvas_->set_position(canvas_pos);
    canvas_->set_canvas_size(canvas_size);
  }

  perform_layout();
  redraw();

  return true;
}

void ResizableVideoScreen::update_frame(std::array<uint8_t *, 3> planes,
                                        std::array<size_t, 3> pitches) {
  // Always update regular canvas for now
  // Split-view rendering needs direct OpenGL implementation
  if (canvas_) {
    canvas_->update_frame(planes, pitches);
  }
  draw_all();
}

bool ResizableVideoScreen::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (Screen::keyboard_event(key, scancode, action, modifiers))
    return true;

  if (action == NANOGUI_KEY_PRESS) {
    if (key == NANOGUI_KEY_ESCAPE) {
      quit_ = true;
      set_visible(false);
      return true;
    } else if (key == NANOGUI_KEY_SPACE) {
      play_ = !play_;
      show_osd(play_ ? "Playing" : "Paused");
      return true;
    } else if (key == NANOGUI_KEY_F) {
      toggle_fullscreen();
      return true;
    } else if (key == NANOGUI_KEY_M) {
      toggle_mute();
      return true;
    } else if (key == NANOGUI_KEY_UP) {
      adjust_volume(0.05f); // Increase 5%
      return true;
    } else if (key == NANOGUI_KEY_DOWN) {
      adjust_volume(-0.05f); // Decrease 5%
      return true;
    } else if (key == NANOGUI_KEY_LEFT) {
      // Seek backward 5 seconds
      if (player_ && playback_duration_ > 0.0) {
        double new_pos = playback_position_ - 5.0;
        if (new_pos < 0.0)
          new_pos = 0.0;
        player_->seek(new_pos);
        show_osd("Seek: " + format_time(new_pos));
      }
      return true;
    } else if (key == NANOGUI_KEY_RIGHT) {
      // Seek forward 5 seconds
      if (player_ && playback_duration_ > 0.0) {
        double new_pos = playback_position_ + 5.0;
        if (new_pos > playback_duration_)
          new_pos = playback_duration_;
        player_->seek(new_pos);
        show_osd("Seek: " + format_time(new_pos));
      }
      return true;
    } else if (key == NANOGUI_KEY_LEFTBRACKET) {
      // Decrease speed
      if (player_) {
        double current = playback_speed_;
        double new_speed = current;
        
        // Speed presets: 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0
        if (current > 2.0) new_speed = 2.0;
        else if (current > 1.5) new_speed = 1.5;
        else if (current > 1.25) new_speed = 1.25;
        else if (current > 1.0) new_speed = 1.0;
        else if (current > 0.75) new_speed = 0.75;
        else if (current > 0.5) new_speed = 0.5;
        else if (current > 0.25) new_speed = 0.25;
        
        set_playback_speed(new_speed);
      }
      return true;
    } else if (key == NANOGUI_KEY_RIGHTBRACKET) {
      // Increase speed
      if (player_) {
        double current = playback_speed_;
        double new_speed = current;
        
        // Speed presets: 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0
        if (current < 0.5) new_speed = 0.5;
        else if (current < 0.75) new_speed = 0.75;
        else if (current < 1.0) new_speed = 1.0;
        else if (current < 1.25) new_speed = 1.25;
        else if (current < 1.5) new_speed = 1.5;
        else if (current < 2.0) new_speed = 2.0;
        else if (current < 3.0) new_speed = 3.0;
        else if (current < 4.0) new_speed = 4.0;
        
        set_playback_speed(new_speed);
      }
      return true;
    } else if (key == NANOGUI_KEY_S) {
      // Capture screenshot
      capture_screenshot();
      return true;
    }
  }
  return false;
}

void ResizableVideoScreen::set_playback_speed(double speed) {
  playback_speed_ = speed;
  
  // Update player if available
  if (player_) {
    player_->set_speed(speed);
  }
  
  // Show OSD
  char msg[64];
  if (speed == 1.0) {
    snprintf(msg, sizeof(msg), "Speed: Normal");
  } else {
    snprintf(msg, sizeof(msg), "Speed: %.2fx", speed);
  }
  show_osd(msg);
}

bool ResizableVideoScreen::mouse_button_event(const Vector2i &p, int button, bool down,
                                              int modifiers) {
  // Let base class handle first (this will close popups if clicked outside)
  if (Screen::mouse_button_event(p, button, down, modifiers))
    return true;

  // Left-click on progress bar for seeking
  if (button == NANOGUI_MOUSE_BUTTON_LEFT && down && playback_duration_ > 0.0) {
    if (is_mouse_over_progress_bar(p)) {
      // Calculate seek position based on click location
      const float bar_width = m_size.x() - 20.0f;
      const float bar_x = 10.0f;
      float click_x = p.x() - bar_x;

      if (click_x >= 0 && click_x <= bar_width) {
        float progress = click_x / bar_width;
        double seek_pos = progress * playback_duration_;

        if (player_) {
          player_->seek(seek_pos);
          show_osd("Seek: " + format_time(seek_pos));
        }
      }
      return true;
    }
  }

  // Show context menu on right-click
  if (button == NANOGUI_MOUSE_BUTTON_RIGHT && down) {
    show_context_menu(p);
    return true;
  }

  return false;
}

bool ResizableVideoScreen::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                                              int modifiers) {
  if (Screen::mouse_motion_event(p, rel, button, modifiers))
    return true;

  // Change cursor when over progress bar
  if (is_mouse_over_progress_bar(p)) {
    set_cursor(Cursor::Hand);
  } else {
    set_cursor(Cursor::Arrow);
  }

  return false;
}

bool ResizableVideoScreen::is_mouse_over_progress_bar(const Vector2i &p) const {
  if (playback_duration_ <= 0.0)
    return false;

  const float bar_height = 6.0f;
  const float bar_y = m_size.y() - bar_height - 10.0f;
  const float bar_width = m_size.x() - 20.0f;
  const float bar_x = 10.0f;

  // Expand hit area for easier clicking (add 10px padding)
  const float padding = 10.0f;

  return p.x() >= (bar_x - padding) && p.x() <= (bar_x + bar_width + padding) &&
         p.y() >= (bar_y - padding) && p.y() <= (bar_y + bar_height + padding);
}

void ResizableVideoScreen::show_context_menu(const Vector2i &pos) {
  // Close any existing popups first
  for (auto child : m_children) {
    if (auto popup = dynamic_cast<Popup*>(child)) {
      popup->set_visible(false);
    }
  }
  
  // Create popup at mouse position
  Popup *popup = new Popup(this, nullptr);
  popup->set_anchor_pos(pos);
  popup->set_layout(new GroupLayout(10));

  // Play/Pause button
  Button *play_pause_btn = new Button(popup, play_ ? "Pause" : "Play", FA_PLAY);
  play_pause_btn->set_icon(play_ ? FA_PAUSE : FA_PLAY);
  play_pause_btn->set_callback([this, popup]() {
    play_ = !play_;
    show_osd(play_ ? "Playing" : "Paused");
    popup->set_visible(false);
  });

  // Volume control
  Widget *volume_panel = new Widget(popup);
  volume_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  new Label(volume_panel, "Volume:", "sans-bold");
  Slider *volume_slider = new Slider(volume_panel);
  volume_slider->set_value(volume_);
  volume_slider->set_fixed_width(120);
  volume_slider->set_callback([this](float value) { set_volume(value); });

  // Mute button
  Button *mute_btn =
      new Button(popup, muted_ ? "Unmute" : "Mute", muted_ ? FA_VOLUME_MUTE : FA_VOLUME_UP);
  mute_btn->set_callback([this, popup]() {
    toggle_mute();
    popup->set_visible(false);
  });
  
  // Speed control label
  Widget *speed_panel = new Widget(popup);
  speed_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  new Label(speed_panel, "Speed:", "sans-bold");
  std::string speed_text = playback_speed_ == 1.0 ? "Normal" : std::to_string(playback_speed_) + "x";
  new Label(speed_panel, speed_text.c_str(), "sans");
  
  // Speed buttons
  Widget *speed_buttons = new Widget(popup);
  speed_buttons->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 5));
  
  Button *speed_025 = new Button(speed_buttons, "0.25x");
  speed_025->set_fixed_size(Vector2i(60, 25));
  speed_025->set_callback([this, popup]() { set_playback_speed(0.25); popup->set_visible(false); });
  
  Button *speed_050 = new Button(speed_buttons, "0.5x");
  speed_050->set_fixed_size(Vector2i(60, 25));
  speed_050->set_callback([this, popup]() { set_playback_speed(0.5); popup->set_visible(false); });
  
  Button *speed_100 = new Button(speed_buttons, "1.0x");
  speed_100->set_fixed_size(Vector2i(60, 25));
  speed_100->set_callback([this, popup]() { set_playback_speed(1.0); popup->set_visible(false); });
  
  Button *speed_150 = new Button(speed_buttons, "1.5x");
  speed_150->set_fixed_size(Vector2i(60, 25));
  speed_150->set_callback([this, popup]() { set_playback_speed(1.5); popup->set_visible(false); });
  
  Button *speed_200 = new Button(speed_buttons, "2.0x");
  speed_200->set_fixed_size(Vector2i(60, 25));
  speed_200->set_callback([this, popup]() { set_playback_speed(2.0); popup->set_visible(false); });

  // Video Filters Section
  new Label(popup, "Video Filters", "sans-bold");
  
  // None/Disable filter option
  Button *no_filter_btn = new Button(popup, "None");
  if (player_ && player_->get_current_filter().empty()) {
    no_filter_btn->set_icon(FA_CHECK);
  }
  no_filter_btn->set_callback([this, popup]() {
    if (player_) {
      player_->set_filter("");
      show_osd("Filter: None");
    }
    popup->set_visible(false);
  });
  
  // Get filter categories
  auto categories = FilterPresets::get_categories();
  for (const auto& category : categories) {
    auto presets = FilterPresets::get_by_category(category);
    if (presets.empty()) continue;
    
    // Create submenu button for category
    Button *category_btn = new Button(popup, category, FA_FILTER);
    category_btn->set_callback([this, popup, presets, category]() {
      // Create submenu popup
      Popup *submenu = new Popup(popup->parent(), popup);
      submenu->set_anchor_pos(popup->position() + Vector2i(popup->width(), 0));
      submenu->set_layout(new GroupLayout(5));
      
      for (const auto& preset : presets) {
        Button *preset_btn = new Button(submenu, preset.name);
        preset_btn->set_tooltip(preset.description);
        preset_btn->set_callback([this, preset, popup, submenu]() {
          if (player_) {
            player_->set_filter(preset.filter_string);
            show_filter_osd(preset.name);
          }
          submenu->set_visible(false);
          popup->set_visible(false);
        });
      }
      
      submenu->set_visible(true);
      perform_layout();
    });
  }
  
  // Audio Tracks Section
  if (player_) {
    auto audio_tracks = player_->get_audio_tracks();
    if (!audio_tracks.empty()) {
      new Label(popup, "Audio Tracks", "sans-bold");
      
      int current_audio = player_->get_current_audio_track();
      
      // Audio track buttons
      for (const auto& track : audio_tracks) {
        std::string track_name = track.title.empty() ? 
          ("Track " + std::to_string(track.index)) : track.title;
        if (!track.language.empty()) {
          track_name += " (" + track.language + ")";
        }
        
        // Add channel info
        std::string channel_info;
        if (track.channels == 1) channel_info = "Mono";
        else if (track.channels == 2) channel_info = "Stereo";
        else if (track.channels == 6) channel_info = "5.1";
        else if (track.channels == 8) channel_info = "7.1";
        else channel_info = std::to_string(track.channels) + "ch";
        
        track_name += " [" + channel_info;
        if (!track.codec_name.empty()) {
          track_name += ", " + track.codec_name;
        }
        track_name += "]";
        
        Button *track_btn = new Button(popup, track_name);
        if (track.index == current_audio) {
          track_btn->set_icon(FA_CHECK);
        }
        track_btn->set_callback([this, popup, track, track_name]() {
          if (player_) {
            player_->select_audio_track(track.index);
            show_osd("Audio: " + track_name);
          }
          popup->set_visible(false);
        });
      }
    }
    
    // Audio Channel Mode Section
    new Label(popup, "Audio Channels", "sans-bold");
    
    auto current_mode = player_->get_audio_channel_mode();
    
    std::vector<AudioChannelMode> modes = {
      AudioChannelMode::ORIGINAL,
      AudioChannelMode::STEREO,
      AudioChannelMode::MONO,
      AudioChannelMode::LEFT_ONLY,
      AudioChannelMode::RIGHT_ONLY,
      AudioChannelMode::SWAP_CHANNELS
    };
    
    for (auto mode : modes) {
      std::string mode_name = AudioTrackManager::get_channel_mode_name(mode);
      Button *mode_btn = new Button(popup, mode_name);
      if (mode == current_mode) {
        mode_btn->set_icon(FA_CHECK);
      }
      mode_btn->set_callback([this, popup, mode, mode_name]() {
        if (player_) {
          player_->set_audio_channel_mode(mode);
        }
        popup->set_visible(false);
      });
    }
  }
  
  // Subtitle Tracks Section
  if (player_) {
    auto subtitle_tracks = player_->get_subtitle_tracks();
    if (!subtitle_tracks.empty()) {
      new Label(popup, "Subtitles", "sans-bold");
      
      // Disable subtitles option
      Button *no_sub_btn = new Button(popup, "Disabled");
      int current_track = player_->get_current_subtitle_track();
      if (current_track == -1) {
        no_sub_btn->set_icon(FA_CHECK);
      }
      no_sub_btn->set_callback([this, popup]() {
        if (player_) {
          player_->select_subtitle_track(-1);
          show_osd("Subtitles: Disabled");
        }
        popup->set_visible(false);
      });
      
      // Subtitle track buttons
      for (const auto& track : subtitle_tracks) {
        std::string track_name = track.title.empty() ? 
          ("Track " + std::to_string(track.index)) : track.title;
        if (!track.language.empty()) {
          track_name += " (" + track.language + ")";
        }
        
        Button *track_btn = new Button(popup, track_name);
        if (track.index == current_track) {
          track_btn->set_icon(FA_CHECK);
        }
        track_btn->set_callback([this, popup, track, track_name]() {
          if (player_) {
            player_->select_subtitle_track(track.index);
            show_osd("Subtitles: " + track_name);
          }
          popup->set_visible(false);
        });
      }
      
      // Load external subtitle button
      Button *load_sub_btn = new Button(popup, "Load Subtitle File...", FA_FILE);
      load_sub_btn->set_callback([this, popup]() {
        popup->set_visible(false);
        load_external_subtitle();
      });
      
      // Font size button
      Button *font_size_btn = new Button(popup, "Font Size...", FA_TEXT_HEIGHT);
      font_size_btn->set_callback([this, popup]() {
        popup->set_visible(false);
        show_subtitle_font_dialog();
      });
    }
  }

  // Screenshot button
  Button *screenshot_btn = new Button(popup, "Screenshot (S)", FA_CAMERA);
  screenshot_btn->set_callback([this, popup]() {
    capture_screenshot();
    popup->set_visible(false);
  });
  
  // Batch extract button
  Button *batch_extract_btn = new Button(popup, "Batch Extract Frames...", FA_IMAGES);
  batch_extract_btn->set_callback([this, popup]() {
    popup->set_visible(false);
    show_batch_extract_dialog();
  });

  // Fullscreen button
  Button *fullscreen_btn =
      new Button(popup, fullscreen_ ? "Exit Fullscreen" : "Fullscreen", FA_EXPAND);
  fullscreen_btn->set_callback([this, popup]() {
    toggle_fullscreen();
    popup->set_visible(false);
  });

  // Separator
  new Label(popup, "", "sans");

  // Quit button
  Button *quit_btn = new Button(popup, "Quit", FA_TIMES);
  quit_btn->set_callback([this, popup]() {
    quit_ = true;
    set_visible(false);
    popup->set_visible(false);
  });

  // Show the popup
  popup->set_visible(true);
  perform_layout();
}

void ResizableVideoScreen::show_filter_osd(const std::string& filter_name) {
  show_osd("Filter: " + filter_name, 2.5f);
}

void ResizableVideoScreen::toggle_fullscreen() {
#if defined(NANOGUI_USE_SDL3)
  if (!fullscreen_) {
    // Save windowed state
    SDL_GetWindowPosition(m_sdl_window, &windowed_pos_.x(), &windowed_pos_.y());
    SDL_GetWindowSize(m_sdl_window, &windowed_size_.x(), &windowed_size_.y());

    // Enter fullscreen
    SDL_SetWindowFullscreen(m_sdl_window, true);
    fullscreen_ = true;
    printf("Entered fullscreen mode\n");
  } else {
    // Exit fullscreen
    SDL_SetWindowFullscreen(m_sdl_window, false);
    SDL_SetWindowPosition(m_sdl_window, windowed_pos_.x(), windowed_pos_.y());
    SDL_SetWindowSize(m_sdl_window, windowed_size_.x(), windowed_size_.y());
    fullscreen_ = false;
    printf("Exited fullscreen mode\n");
  }
#else
  // GLFW fullscreen toggle
  if (!fullscreen_) {
    // Get current monitor
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    // Save windowed state
    glfwGetWindowPos(m_glfw_window, &windowed_pos_.x(), &windowed_pos_.y());
    glfwGetWindowSize(m_glfw_window, &windowed_size_.x(), &windowed_size_.y());

    // Enter fullscreen
    glfwSetWindowMonitor(m_glfw_window, monitor, 0, 0, mode->width, mode->height,
                         mode->refreshRate);
    fullscreen_ = true;
    printf("Entered fullscreen mode\n");
  } else {
    // Exit fullscreen
    glfwSetWindowMonitor(m_glfw_window, nullptr, windowed_pos_.x(), windowed_pos_.y(),
                         windowed_size_.x(), windowed_size_.y(), 0);
    fullscreen_ = false;
    printf("Exited fullscreen mode\n");
  }
#endif
}

void ResizableVideoScreen::set_volume(float volume) {
  // Clamp to valid range
  if (volume < 0.0f)
    volume = 0.0f;
  if (volume > 1.0f)
    volume = 1.0f;

  volume_ = volume;

  // Update audio player if available
  if (audio_player_) {
    audio_player_->set_volume(volume_);
  }

  // Show OSD
  char msg[64];
  snprintf(msg, sizeof(msg), "Volume: %d%%", (int)(volume_ * 100));
  show_osd(msg);
}

void ResizableVideoScreen::adjust_volume(float delta) { set_volume(volume_ + delta); }

void ResizableVideoScreen::toggle_mute() {
  muted_ = !muted_;

  // Update audio player if available
  if (audio_player_) {
    audio_player_->set_muted(muted_);
  }

  // Show OSD
  show_osd(muted_ ? "Muted" : "Unmuted");
}

void ResizableVideoScreen::show_osd(const std::string &message, float duration) {
  osd_message_ = message;
  osd_timer_ = duration;
  osd_start_time_ = std::chrono::steady_clock::now();
  redraw();
}

void ResizableVideoScreen::update_osd() {
  if (osd_timer_ > 0.0f) {
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - osd_start_time_).count();
    osd_timer_ = std::max(0.0f, osd_timer_ - elapsed);
    osd_start_time_ = now;

    if (osd_timer_ > 0.0f) {
      redraw(); // Keep redrawing while OSD is visible
    }
  }
}

void ResizableVideoScreen::draw_osd(NVGcontext *ctx) {
  if (osd_timer_ <= 0.0f || osd_message_.empty()) {
    return;
  }

  // Calculate alpha based on remaining time (fade out in last 0.5s)
  float alpha = 1.0f;
  if (osd_timer_ < 0.5f) {
    alpha = osd_timer_ / 0.5f;
  }

  // Draw semi-transparent background box
  nvgFontSize(ctx, 24.0f);
  nvgFontFace(ctx, "sans-bold");

  float bounds[4];
  nvgTextBounds(ctx, 0, 0, osd_message_.c_str(), nullptr, bounds);
  float text_width = bounds[2] - bounds[0];
  float text_height = bounds[3] - bounds[1];

  // Center at bottom of screen
  float box_width = text_width + 40;
  float box_height = text_height + 30;
  float box_x = (m_size.x() - box_width) / 2.0f;
  float box_y = m_size.y() - box_height - 80;

  // Draw background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, box_x, box_y, box_width, box_height, 8.0f);
  nvgFillColor(ctx, nvgRGBA(0, 0, 0, (int)(180 * alpha)));
  nvgFill(ctx);

  // Draw text
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, (int)(255 * alpha)));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, box_x + box_width / 2.0f, box_y + box_height / 2.0f, osd_message_.c_str(), nullptr);
}

void ResizableVideoScreen::draw(NVGcontext *ctx) {
  // Call base class draw
  Screen::draw(ctx);

  // Update and draw OSD
  update_osd();
  draw_osd(ctx);

  // Draw progress bar
  draw_progress_bar(ctx);
}

void ResizableVideoScreen::set_progress(double position, double duration) {
  playback_position_ = position;
  playback_duration_ = duration;
}

void ResizableVideoScreen::draw_progress_bar(NVGcontext *ctx) {
  if (playback_duration_ <= 0.0) {
    return; // No duration info yet
  }

  const float bar_height = 6.0f;
  const float bar_y = m_size.y() - bar_height - 10.0f;
  const float bar_width = m_size.x() - 20.0f;
  const float bar_x = 10.0f;

  // Calculate progress
  float progress = static_cast<float>(playback_position_ / playback_duration_);
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;

  // Draw background bar (dark gray)
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, bar_x, bar_y, bar_width, bar_height, bar_height / 2.0f);
  nvgFillColor(ctx, nvgRGBA(40, 40, 40, 200));
  nvgFill(ctx);

  // Draw progress bar (blue/accent color)
  if (progress > 0.0f) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, bar_x, bar_y, bar_width * progress, bar_height, bar_height / 2.0f);
    nvgFillColor(ctx, nvgRGBA(0, 120, 215, 255)); // Windows blue
    nvgFill(ctx);
  }

  // Draw time text (Current / Total)
  std::string time_text = format_time(playback_position_) + " / " + format_time(playback_duration_);

  nvgFontSize(ctx, 14.0f);
  nvgFontFace(ctx, "sans");
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 220));
  nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
  nvgText(ctx, m_size.x() - 15.0f, bar_y - 5.0f, time_text.c_str(), nullptr);
}

ResizableDisplay::ResizableDisplay(const unsigned width, const unsigned height) {
  nanogui::init();
  screen_ = std::make_unique<ResizableVideoScreen>(width, height);
}

void ResizableDisplay::refresh(std::array<uint8_t *, 3> planes, std::array<size_t, 3> pitches) {
  screen_->update_frame(planes, pitches);
  // Note: draw_all() is already called in update_frame()
}

void ResizableDisplay::input() {
  // Process events through the Screen's event handling system
  // This properly dispatches events including resize
  if (!screen_->process_events()) {
    quit_ = true;
    return;
  }

  quit_ = screen_->get_quit();
  play_ = screen_->get_play();
}

void ResizableDisplay::redraw() {
  if (screen_) {
    screen_->redraw();
  }
}

bool ResizableDisplay::get_quit() { return quit_; }

bool ResizableDisplay::get_play() { return play_; }

void ResizableVideoScreen::capture_screenshot() {
  if (player_) {
    player_->capture_screenshot(CaptureFormat::PNG);
  } else {
    show_osd("Screenshot failed: No player available");
  }
}

void ResizableVideoScreen::load_external_subtitle() {
  if (!player_) {
    show_osd("No player available");
    return;
  }
  
  // Use NFD to open file dialog
  nfdchar_t *out_path = nullptr;
  nfdfilteritem_t filters[1] = {{"Subtitle Files", "srt,ass,ssa,sub,vtt"}};
  nfdresult_t result = NFD_OpenDialog(&out_path, filters, 1, nullptr);
  
  if (result == NFD_OKAY) {
    std::string subtitle_file(out_path);
    NFD_FreePath(out_path);
    
    if (player_->load_external_subtitle(subtitle_file)) {
      // Extract filename from path
      size_t last_slash = subtitle_file.find_last_of("/\\");
      std::string filename = (last_slash != std::string::npos) ? 
                             subtitle_file.substr(last_slash + 1) : subtitle_file;
      show_osd("Subtitle loaded: " + filename);
    } else {
      show_osd("Failed to load subtitle");
    }
  } else if (result == NFD_CANCEL) {
    // User cancelled, do nothing
  } else {
    show_osd("Error opening file dialog");
  }
}

void ResizableVideoScreen::show_subtitle_font_dialog() {
  if (!player_) {
    show_osd("No player available");
    return;
  }
  
  // Create popup dialog for font size
  Popup *dialog = new Popup(this, nullptr);
  dialog->set_anchor_pos(Vector2i(m_size.x() / 2 - 150, m_size.y() / 2 - 100));
  dialog->set_layout(new GroupLayout(15));
  
  new Label(dialog, "Subtitle Font Size", "sans-bold");
  
  // Current size display
  Widget *size_panel = new Widget(dialog);
  size_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  new Label(size_panel, "Size:", "sans");
  
  double current_scale = player_->get_subtitle_font_size();
  Label *size_label = new Label(size_panel, std::to_string((int)(current_scale * 100)) + "%", "sans-bold");
  
  // Slider for font size
  Widget *slider_panel = new Widget(dialog);
  slider_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  new Label(slider_panel, "50%", "sans");
  
  Slider *size_slider = new Slider(slider_panel);
  size_slider->set_fixed_width(200);
  size_slider->set_value((current_scale - 0.5) / 2.5);  // Map 0.5-3.0 to 0.0-1.0
  size_slider->set_callback([this, size_label](float value) {
    double scale = 0.5 + value * 2.5;  // Map 0.0-1.0 to 0.5-3.0
    if (player_) {
      player_->set_subtitle_font_size(scale);
      size_label->set_caption(std::to_string((int)(scale * 100)) + "%");
    }
  });
  
  new Label(slider_panel, "300%", "sans");
  
  // Preset buttons
  Widget *preset_panel = new Widget(dialog);
  preset_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 5));
  
  auto add_preset = [this, size_slider, size_label, dialog](Widget* parent, const char* label, double scale) {
    Button *btn = new Button(parent, label);
    btn->set_fixed_size(Vector2i(60, 25));
    btn->set_callback([this, size_slider, size_label, scale]() {
      if (player_) {
        player_->set_subtitle_font_size(scale);
        size_slider->set_value((scale - 0.5) / 2.5);
        size_label->set_caption(std::to_string((int)(scale * 100)) + "%");
      }
    });
  };
  
  add_preset(preset_panel, "Small", 0.75);
  add_preset(preset_panel, "Normal", 1.0);
  add_preset(preset_panel, "Large", 1.5);
  add_preset(preset_panel, "Huge", 2.0);
  
  // Close button
  Button *close_btn = new Button(dialog, "Close", FA_CHECK);
  close_btn->set_callback([dialog]() {
    dialog->set_visible(false);
  });
  
  dialog->set_visible(true);
  perform_layout();
}

void ResizableVideoScreen::show_batch_extract_dialog() {
  if (!player_) {
    show_osd("No player available");
    return;
  }
  
  // Check if video is playing (required for frame extraction)
  if (!play_) {
    show_osd("Please start playback first (press SPACE)");
    return;
  }
  
  double duration = player_->get_duration();
  if (duration <= 0) {
    show_osd("Invalid video duration");
    return;
  }
  
  // Create popup dialog
  Popup *dialog = new Popup(this, nullptr);
  dialog->set_anchor_pos(Vector2i(m_size.x() / 2 - 200, m_size.y() / 2 - 150));
  dialog->set_layout(new GroupLayout(15));
  
  new Label(dialog, "Batch Frame Extraction", "sans-bold");
  
  // Warning label
  Label *warning = new Label(dialog, "Note: This feature is experimental", "sans");
  warning->set_color(Color(200, 150, 0, 255));
  
  // Interval setting
  Widget *interval_panel = new Widget(dialog);
  interval_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  new Label(interval_panel, "Extract every:", "sans");
  
  TextBox *interval_box = new TextBox(interval_panel, "1.0");
  interval_box->set_fixed_width(60);
  interval_box->set_units("sec");
  interval_box->set_default_value("1.0");
  interval_box->set_format("[0-9]*\\.?[0-9]+");
  
  // Time range
  Widget *range_panel = new Widget(dialog);
  range_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  new Label(range_panel, "Time range:", "sans");
  
  TextBox *start_box = new TextBox(range_panel, "0");
  start_box->set_fixed_width(60);
  start_box->set_units("sec");
  start_box->set_format("[0-9]*\\.?[0-9]+");
  
  new Label(range_panel, "to", "sans");
  
  TextBox *end_box = new TextBox(range_panel, std::to_string((int)duration));
  end_box->set_fixed_width(60);
  end_box->set_units("sec");
  end_box->set_format("[0-9]*\\.?[0-9]+");
  
  // Format selection
  Widget *format_panel = new Widget(dialog);
  format_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  new Label(format_panel, "Format:", "sans");
  
  ComboBox *format_combo = new ComboBox(format_panel, {"PNG", "JPEG", "BMP"});
  format_combo->set_fixed_width(100);
  
  // Output prefix
  Widget *prefix_panel = new Widget(dialog);
  prefix_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  new Label(prefix_panel, "Filename prefix:", "sans");
  
  TextBox *prefix_box = new TextBox(prefix_panel, "frame");
  prefix_box->set_fixed_width(120);
  
  // Estimated frame count
  Label *estimate_label = new Label(dialog, "", "sans");
  estimate_label->set_color(Color(150, 150, 150, 255));
  
  // Update estimate when interval changes
  auto update_estimate = [=](const std::string& value) -> bool {
    try {
      double interval = std::stod(interval_box->value());
      double start = std::stod(start_box->value());
      double end = std::stod(end_box->value());
      
      if (interval > 0 && end > start) {
        int count = static_cast<int>((end - start) / interval) + 1;
        estimate_label->set_caption("Will extract ~" + std::to_string(count) + " frames");
      } else {
        estimate_label->set_caption("Invalid parameters");
      }
    } catch (...) {
      estimate_label->set_caption("Invalid parameters");
    }
    return true;
  };
  
  interval_box->set_callback(update_estimate);
  start_box->set_callback(update_estimate);
  end_box->set_callback(update_estimate);
  
  // Initial estimate
  try {
    double interval = std::stod(interval_box->value());
    double start = std::stod(start_box->value());
    double end = std::stod(end_box->value());
    if (interval > 0 && end > start) {
      int count = static_cast<int>((end - start) / interval) + 1;
      estimate_label->set_caption("Will extract ~" + std::to_string(count) + " frames");
    }
  } catch (...) {
    estimate_label->set_caption("Invalid parameters");
  }
  
  // Buttons
  Widget *button_panel = new Widget(dialog);
  button_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 10));
  
  Button *extract_btn = new Button(button_panel, "Extract", FA_CHECK);
  extract_btn->set_background_color(Color(0, 100, 200, 255));
  extract_btn->set_callback([=]() {
    try {
      double interval = std::stod(interval_box->value());
      double start = std::stod(start_box->value());
      double end = std::stod(end_box->value());
      std::string prefix = prefix_box->value();
      
      // Map format
      CaptureFormat format = CaptureFormat::PNG;
      int format_idx = format_combo->selected_index();
      if (format_idx == 1) format = CaptureFormat::JPEG;
      else if (format_idx == 2) format = CaptureFormat::BMP;
      
      dialog->set_visible(false);
      
      // Show progress OSD
      show_osd("Extracting frames...", 0.5f);
      
      // Run extraction in background (simplified - blocks UI)
      // TODO: Run in separate thread for better UX
      int extracted = player_->batch_extract_frames(interval, format, prefix, start, end);
      
      if (extracted > 0) {
        show_osd("Extracted " + std::to_string(extracted) + " frames");
      } else {
        show_osd("Frame extraction failed");
      }
      
    } catch (const std::exception& e) {
      show_osd("Error: Invalid parameters");
    }
  });
  
  Button *cancel_btn = new Button(button_panel, "Cancel");
  cancel_btn->set_callback([dialog]() {
    dialog->set_visible(false);
  });
  
  dialog->set_visible(true);
  perform_layout();
}
