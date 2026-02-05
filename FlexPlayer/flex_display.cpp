#include "flex_display.h"

FlexDisplay::FlexDisplay(int width, int height) 
    : width_(width), height_(height) {
    // Pre-allocate buffers
    y_buf_.resize(width * height);
    u_buf_.resize(width * height / 4);
    v_buf_.resize(width * height / 4);
}

FlexDisplay::~FlexDisplay() {}

void FlexDisplay::refresh(std::array<uint8_t*, 3> planes, std::array<size_t, 3> pitches) {
    std::lock_guard<std::mutex> lock(buf_mutex_);
    
    // Copy Y plane
    if (y_buf_.size() < height_ * width_) y_buf_.resize(height_ * width_);
    for (int i = 0; i < height_; ++i) {
        std::memcpy(y_buf_.data() + i * width_, planes[0] + i * pitches[0], width_);
    }
    y_pitch_ = width_;

    // Copy U plane
    int uv_height = height_ / 2;
    int uv_width = width_ / 2;
    if (u_buf_.size() < uv_height * uv_width) u_buf_.resize(uv_height * uv_width);
    for (int i = 0; i < uv_height; ++i) {
        std::memcpy(u_buf_.data() + i * uv_width, planes[1] + i * pitches[1], uv_width);
    }
    u_pitch_ = uv_width;

    // Copy V plane
    if (v_buf_.size() < uv_height * uv_width) v_buf_.resize(uv_height * uv_width);
    for (int i = 0; i < uv_height; ++i) {
        std::memcpy(v_buf_.data() + i * uv_width, planes[2] + i * pitches[2], uv_width);
    }
    v_pitch_ = uv_width;

    new_frame_available_ = true;
}

void FlexDisplay::input() {
    // Handled by main loop
}

void FlexDisplay::redraw() {
    // Handled by main loop
}

bool FlexDisplay::get_quit() {
    return quit_;
}

bool FlexDisplay::get_play() {
    return play_;
}

void FlexDisplay::set_audio_player(AudioPlayer* player) {
    audio_player_ = player;
}

void FlexDisplay::set_progress(double position, double duration) {
    position_ = position;
    duration_ = duration;
}

void FlexDisplay::set_player(Player* player) {
    player_ = player;
}

void FlexDisplay::show_osd(const std::string& message, float duration) {
    std::cout << "OSD: " << message << std::endl;
}

void FlexDisplay::get_frame_data(std::vector<uint8_t>& y, std::vector<uint8_t>& u, std::vector<uint8_t>& v,
                                 int& y_pitch, int& u_pitch, int& v_pitch) {
    std::lock_guard<std::mutex> lock(buf_mutex_);
    if (new_frame_available_) {
        // Only valid if new frame is available, but we can copy anyway
        y = y_buf_;
        u = u_buf_;
        v = v_buf_;
        y_pitch = y_pitch_;
        u_pitch = u_pitch_;
        v_pitch = v_pitch_;
        new_frame_available_ = false;
    }
}
