// Text-on-Path implementation
// This file is included directly into nanovg_css_painter.cpp
// All necessary headers are already included there

// Helper for UTF-8 decoding
static int nvgcss_utf8_next(const char** str) {
    const unsigned char* s = (const unsigned char*)*str;
    int codepoint = 0;
    int bytes = 0;
    
    if (*s == 0) return 0;
    
    if ((*s & 0x80) == 0) {
        codepoint = *s;
        bytes = 1;
    } else if ((*s & 0xE0) == 0xC0) {
        codepoint = (*s & 0x1F) << 6;
        codepoint |= (*(s + 1) & 0x3F);
        bytes = 2;
    } else if ((*s & 0xF0) == 0xE0) {
        codepoint = (*s & 0x0F) << 12;
        codepoint |= (*(s + 1) & 0x3F) << 6;
        codepoint |= (*(s + 2) & 0x3F);
        bytes = 3;
    } else if ((*s & 0xF8) == 0xF0) {
        codepoint = (*s & 0x07) << 18;
        codepoint |= (*(s + 1) & 0x3F) << 12;
        codepoint |= (*(s + 2) & 0x3F) << 6;
        codepoint |= (*(s + 3) & 0x3F);
        bytes = 4;
    } else {
        // Invalid, skip 1 byte
        bytes = 1;
    }
    
    *str += bytes;
    return codepoint;
}

void NVGCSSPainter::paint_text_path(const NVGCSSElement* element, const NVGCSSBox& box) {
    // 1. Get path reference
    std::string path_id;
    auto href_it = element->attributes.find("href");
    if (href_it != element->attributes.end()) {
        path_id = extract_url_id(href_it->second);
    } else {
        auto xlink_it = element->attributes.find("xlink:href");
        if (xlink_it != element->attributes.end()) {
            path_id = extract_url_id(xlink_it->second);
        }
    }

    if (path_id.empty()) return;
    
    // 2. Find path element
    NVGCSSElement* path_element = nvgcssGetElement(renderer_, path_id.c_str());
    if (!path_element) return;

    auto d_it = path_element->inline_style.find("d");
    if (d_it == path_element->inline_style.end() || d_it->second.empty()) return;

    // 3. Sample path
    auto commands = nvgcss::SVGPathParser::parse(d_it->second);
    if (commands.empty()) return;

    // Check for unsupported Arc commands (not yet implemented in path sampler)
    for (const auto& cmd : commands) {
        if (cmd.type == 'A') {
            fprintf(stderr, "Error: Arc commands ('A') are not yet supported in textPath. "
                    "Path ID: '%s'. Use bezier curves instead.\n", path_id.c_str());
            return;
        }
    }

    nvgcss::PathSampler sampler;
    auto samples = sampler.sample_path(d_it->second);
    float total_length = sampler.get_total_length();

    if (total_length <= 0.0f) return;

    // 4. Setup font
    std::string text = element->text_content;
    if (text.empty()) return;
    
    float font_size = element->style.font_size;
    std::string font_face = compute_font_face(element);
    NVGcolor fill_color = element->style.color;
    
    // Check for fill override
    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() && fill_it->second != "none") {
        fill_color = nvgcss_utils::parse_color(fill_it->second);
    }
    
    nvgFontSize(vg_, font_size);
    nvgFontFace(vg_, font_face.c_str());
    nvgTextAlign(vg_, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    nvgFillColor(vg_, fill_color);
    
    // 5. Measure text
    float text_width = nvgTextBounds(vg_, 0, 0, text.c_str(), nullptr, nullptr);
    
    // 6. Calculate start offset
    float start_offset = 0.0f;
    auto offset_it = element->attributes.find("startOffset");
    if (offset_it != element->attributes.end()) {
        start_offset = nvgcss_utils::parse_length(offset_it->second, total_length);
    }
    
    // 7. Adjust for text-anchor
    auto anchor_it = element->inline_style.find("text-anchor");
    if (anchor_it != element->inline_style.end()) {
        if (anchor_it->second == "middle") {
            start_offset -= text_width / 2.0f;
        } else if (anchor_it->second == "end") {
            start_offset -= text_width;
        }
    }
    
    // 8. Render glyphs
    float current_dist = start_offset;
    
    // Iterate UTF-8 characters
    const char* str = text.c_str();
    const char* end = str + text.length();
    const char* iter = str;
    
    while (iter < end) {
        // Get next glyph
        const char* next = iter;
        int codepoint = nvgcss_utf8_next(&next);
        
        if (codepoint == 0) break;
        
        // Measure this glyph
        char glyph_str[5];
        int len = (int)(next - iter);
        strncpy(glyph_str, iter, len);
        glyph_str[len] = '\0';
        
        float glyph_width = nvgTextBounds(vg_, 0, 0, glyph_str, nullptr, nullptr);

        // Skip glyphs with zero width (font not loaded or glyph missing)
        if (glyph_width <= 0.0f) {
            fprintf(stderr, "Warning: glyph %d (U+%04X) has zero width, font may not be loaded - skipping\n",
                    codepoint, codepoint);
            iter = next;
            continue;
        }
        
        // Position at center of glyph
        float sample_dist = current_dist + glyph_width * 0.5f;
        
        if (sample_dist >= 0 && sample_dist <= total_length) {
            nvgcss::PathSample sample = nvgcss::interpolate_path_position(samples, sample_dist);
            
            nvgSave(vg_);
            nvgTranslate(vg_, sample.x, sample.y);
            nvgRotate(vg_, sample.angle);
            
            // Draw glyph centered
            nvgText(vg_, -glyph_width * 0.5f, 0, glyph_str, nullptr);
            
            nvgRestore(vg_);
        }
        
        current_dist += glyph_width;
        iter = next;
    }
}
