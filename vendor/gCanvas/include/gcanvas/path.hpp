#ifndef GCANVAS_PATH_HPP
#define GCANVAS_PATH_HPP

#include <cstddef>
#include <string_view>
#include <vector>

#include "gcanvas/gcanvas.hpp"

namespace gcanvas
{
    class GCANVAS_API Path
    {
    public:
        enum class CommandType
        {
            MoveTo,
            LineTo,
            CubicTo,
            Close
        };

        struct Command
        {
            CommandType type = CommandType::MoveTo;
            float values[6]{};
        };

        static constexpr std::size_t max_commands = 65536;

        Path& move_to(float x, float y);
        Path& line_to(float x, float y);
        Path& cubic_to(float control1_x, float control1_y, float control2_x,
                       float control2_y, float x, float y);
        Path& quadratic_to(float control_x, float control_y, float x, float y);
        Path& close();
        Path& rect(float x, float y, float width, float height);
        Path& rounded_rect(float x, float y, float width, float height, float radius);
        Path& ellipse(float center_x, float center_y, float radius_x, float radius_y);
        void clear() noexcept;

        bool empty() const noexcept { return commands_.empty(); }
        const std::vector<Command>& commands() const noexcept { return commands_; }

        static Path from_svg(std::string_view data);

    private:
        void append(Command command);
        std::vector<Command> commands_;
        float current_x_ = 0.0f;
        float current_y_ = 0.0f;
        float subpath_x_ = 0.0f;
        float subpath_y_ = 0.0f;
        bool has_current_point_ = false;
    };
}

#endif // GCANVAS_PATH_HPP
