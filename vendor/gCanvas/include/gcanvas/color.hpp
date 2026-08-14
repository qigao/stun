#ifndef GCANVAS_COLOR_HPP
#define GCANVAS_COLOR_HPP

#include <cstdint>
#include <string>

#include "gcanvas/gcanvas.hpp"

namespace gcanvas
{
    class GCANVAS_API color
    {
    public:
        color() : _r(0.0f), _g(0.0f), _b(0.0f), _a(1.0f)
        {
        }
        color(float r, float g, float b) : _r(r), _g(g), _b(b), _a(1.0f)
        {
        }
        color(float r, float g, float b, float a) : _r(r), _g(g), _b(b), _a(a)
        {
        }
        color(int r, int g, int b);
        color(int r, int g, int b, int a);
        color(int hex);
        color(std::string hex);

        uint8_t r() const;
        uint8_t g() const;
        uint8_t b() const;
        uint8_t a() const;

        float rf() const;
        float gf() const;
        float bf() const;
        float af() const;

        std::string hex();
        std::string rgb();
        std::string rgba();

        static color color_lerp(color a, color b, float t);

        bool operator==(const color& other) const;
        bool operator!=(const color& other) const;

        friend std::ostream& operator<<(std::ostream& os, color c);

    private:
        float _r;
        float _g;
        float _b;
        float _a;
    };
} // namespace gcanvas

#endif // GCANVAS_COLOR_HPP
