#pragma once

#include <ir/unified_infographic.h>
#include <icons.h>
#include <flex/runtime/allocator.h>
#include <flex/runtime/group.h>
#include <flex/runtime/shape.h>
#include <flex/runtime/text.h>
#include <flex/runtime/svg.h>
#include <flex/runtime/node.h>
#include <string>

namespace flex::modules::infographic {

// Color utilities
flex::Color to_color(const std::string& hex);
flex::Color get_palette_color(const Theme& theme, size_t index);

// Icon/Illus utilities
flex::Svg* create_icon(const std::string& icon_name, float size, const std::string& color, 
                       flex::ArenaAllocator& arena);
flex::Node* create_illus(const std::string& illus_name, float w, float h, 
                         flex::ArenaAllocator& arena);

// Illus directory configuration
void set_illus_directory(const std::string& dir);
const std::string& get_illus_directory();

} // namespace flex::modules::infographic
