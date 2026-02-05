#pragma once
#include <array>
#include <string>
#include <cstdint>

class VideoDisplay {
public:
    virtual ~VideoDisplay() = default;

    virtual void refresh(std::array<uint8_t*, 3> planes, std::array<size_t, 3> pitches) = 0;
    virtual void input() = 0;
    virtual void redraw() = 0;
    virtual bool get_quit() = 0;
    virtual bool get_play() = 0;
    virtual void set_audio_player(class AudioPlayer* player) = 0;
    virtual void set_progress(double position, double duration) = 0;
    virtual void set_player(class Player* player) = 0;
    virtual void show_osd(const std::string& message, float duration = 2.0f) = 0;
};
