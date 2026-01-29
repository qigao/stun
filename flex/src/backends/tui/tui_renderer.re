#include <flex/backends/tui/init.h>
#include <cctype>

namespace flex {
namespace tui_backend {

/*
 * Optimized text drawing using re2c for ANSI escape sequences and UTF-8.
 * This is called by TuiRenderer::draw_text.
 */

void parse_sgr_re2c(const char*& p, const char* end, ::tui::Color& fg, ::tui::Color& bg, ::tui::Style& style, const ::tui::Color& base_fg) {
    const char* marker;
    int args[16];
    int arg_count = 0;

    auto parse_param = [&]() {
        int val = 0;
        bool has_digit = false;
        while (p < end && isdigit(*p)) {
            val = val * 10 + (*p - '0');
            p++;
            has_digit = true;
        }
        if (has_digit || (p < end && (*p == ';' || *p == 'm'))) {
            if (arg_count < 16) args[arg_count++] = val;
        }
    };

    while (p < end && *p != 'm') {
        /*!re2c
            re2c:define:YYCTYPE = char;
            re2c:define:YYCURSOR = p;
            re2c:define:YYLIMIT = end;
            re2c:define:YYMARKER = marker;
            re2c:yyfill:enable = 0;

            ";" { 
                if (arg_count < 16) args[arg_count++] = 0; 
                continue; 
            }
            [0-9]+ {
                // Rewind to let manual parser handle the number (simpler for capturing value)
                p--; 
                while(p > text.data() && isdigit(p[-1])) p--; // This is complex with re2c
                // Actually let's use re2c's own value capturing if possible, but it's easier to just use manual for the numbers inside the re2c loop for markers.
            }
        */
        // Let's rewrite this to be a proper re2c scanner
    }
}

} // namespace tui_backend
} // namespace flex
