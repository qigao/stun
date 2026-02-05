#pragma once
#include "engine/video_display.h"
#include <mutex>
#include <vector>
#include <atomic>
#include <cstring>
#include <iostream>

class FlexDisplay : public VideoDisplay {
public:
    FlexDisplay(int width, int height);
    virtual ~FlexDisplay();

    // VideoDisplay interface
    void refresh(std::array<uint8_t*, 3> planes, std::array<size_t, 3> pitches) override;
    void input() override; // No-op, handled by main loop
    void redraw() override; // No-op
    bool get_quit() override;
    bool get_play() override;
    void set_audio_player(class AudioPlayer* player) override;
    void set_progress(double position, double duration) override;
    void set_player(class Player* player) override;
    void show_osd(const std::string& message, float duration) override;

    // Interaction with Main Loop
    void set_quit(bool quit) { quit_ = quit; }
    void set_play(bool play) { play_ = play; }
    
    // Frame access for rendering
    bool is_new_frame_available() const { return new_frame_available_; }
    void get_frame_data(std::vector<uint8_t>& y, std::vector<uint8_t>& u, std::vector<uint8_t>& v,
                        int& y_pitch, int& u_pitch, int& v_pitch);

    int get_width() const { return width_; }
    int get_height() const { return height_; }

    double get_position() const { return position_; }
    double get_duration() const { return duration_; }

private:
    int width_;
    int height_;
    
    std::atomic<bool> quit_{false};
    std::atomic<bool> play_{true};
    std::atomic<bool> new_frame_available_{false};
    
    std::mutex buf_mutex_;
    std::vector<uint8_t> y_buf_, u_buf_, v_buf_;
    int y_pitch_ = 0, u_pitch_ = 0, v_pitch_ = 0;

    class AudioPlayer* audio_player_ = nullptr;
    class Player* player_ = nullptr;
    
    std::atomic<double> position_{0.0};
    std::atomic<double> duration_{0.0};
};
