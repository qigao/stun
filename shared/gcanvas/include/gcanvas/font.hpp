#ifndef GCANVAS_FONT_HPP
#define GCANVAS_FONT_HPP

#include <map>
#include <memory>

#include "gcanvas/gcanvas.hpp"
#include "gcanvas/image.hpp"
#include "gcanvas/vec2.hpp"

#define LOADED_HEIGHT 64
//#define NUM_GLYPHS 128

namespace gcanvas
{
    class Context;
    struct GCANVAS_API character
    {
        vec2 size;    // Size of glyph
        vec2 bearing; // Offset from baseline to left/top of glyph
        vec2 origin;
        int advance; // Horizontal offset to advance to next glyph
        bool has_color;
    };

    class GCANVAS_API Font
    {
    public:
        virtual ~Font() = default;
        Font(const Font&) = delete;
        Font& operator=(const Font&) = delete;

        std::map<char32_t, character>& get_characters();
        float get_line_height();
        gcanvas::Image& get_image();
        bool is_color_font();

        std::u32string fit_substring(std::u32string text, int width, int font_size);
        vec2 measure_dimensions(std::u32string text, int font_size);

        static std::string UnicodeToUTF8(unsigned int unicode);

    protected:
        std::string _name;
        bool _is_color_font = false;

        float _line_height = 0;

        std::unique_ptr<Image> _texture_atlas;
        Context* _owner = nullptr;
        std::map<char32_t, character> _characters;

        Font() = default;

        void load_from_file(std::string file_path);
        void load_from_memory(const unsigned char* data, size_t size);
        int fit_one_substring(std::u32string text, int width, int font_size);
        virtual std::unique_ptr<Image> create_atlas(int width, int height, int components,
                                                    const unsigned char* data) = 0;

    private:
        friend class Context;
    };

} // namespace gcanvas

#endif // GCANVAS_FONT_HPP
