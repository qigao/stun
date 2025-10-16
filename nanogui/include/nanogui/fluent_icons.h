/*
    include/nanogui/fluent_icons.h -- Codepoint helpers for Fluent System Icons

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

/**
 * \file fluent_icons.h
 *
 * \brief Convenience macros for Fluent System Icon glyphs.
 *
 * The glyph values are sourced from the Fluent System Icons project:
 * https://github.com/microsoft/fluentui-system-icons
 *
 * Only the subset currently needed by the NanoGUI Fluent theme is listed here.
 * Extend this header (and keep the list alphabetical) as new widgets need
 * additional symbols.
 */

// Navigation
#define FLUENT_ICON_ARROW_RIGHT 0xE3F5            ///< ic_fluent_arrow_right_20_regular
#define FLUENT_ICON_ARROW_ROTATE_CLOCKWISE 0xEE5C ///< ic_fluent_arrow_rotate_clockwise_20_regular
#define FLUENT_ICON_CHEVRON_DOWN 0xE456           ///< ic_fluent_chevron_down_20_regular
#define FLUENT_ICON_CHEVRON_LEFT 0xE45A           ///< ic_fluent_chevron_left_20_regular
#define FLUENT_ICON_CHEVRON_RIGHT 0xE45C          ///< ic_fluent_chevron_right_20_regular
#define FLUENT_ICON_CHEVRON_UP 0xE45E             ///< ic_fluent_chevron_up_20_regular

// Status & actions
#define FLUENT_ICON_CHECKMARK 0xE430        ///< ic_fluent_checkmark_20_regular
#define FLUENT_ICON_CHECKMARK_CIRCLE 0xE432 ///< ic_fluent_checkmark_circle_20_regular
#define FLUENT_ICON_DISMISS 0xE687          ///< ic_fluent_dismiss_20_regular
#define FLUENT_ICON_DISMISS_CIRCLE 0xE689   ///< ic_fluent_dismiss_circle_20_regular
#define FLUENT_ICON_INFO 0xEA02             ///< ic_fluent_info_20_regular
#define FLUENT_ICON_WARNING 0xF473          ///< ic_fluent_warning_20_regular

// Search & input affordances
#define FLUENT_ICON_MIC 0xEBA6    ///< ic_fluent_mic_20_regular
#define FLUENT_ICON_SEARCH 0xEF23 ///< ic_fluent_search_20_regular

// Drawing & editing tools
#define FLUENT_ICON_CIRCLE 0xE45D          ///< ic_fluent_circle_20_regular
#define FLUENT_ICON_CURSOR 0xE962          ///< ic_fluent_cursor_20_regular
#define FLUENT_ICON_DIAMOND 0xE6A5         ///< ic_fluent_diamond_20_regular
#define FLUENT_ICON_HAND 0xE25d            ///< ic_fluent_hand_20_regular
#define FLUENT_ICON_IMAGE 0xEA0C           ///< ic_fluent_image_20_regular
#define FLUENT_ICON_LINE_HORIZONTAL 0xEAF7 ///< ic_fluent_line_horizontal_20_regular
#define FLUENT_ICON_LOCK_CLOSED 0xEA02     ///< ic_fluent_lock_closed_20_regular
#define FLUENT_ICON_LOCK_OPEN 0xEA03       ///< ic_fluent_lock_open_20_regular
#define FLUENT_ICON_PEN 0xEBD7             ///< ic_fluent_pen_20_regular
#define FLUENT_ICON_SQUARE 0xEF27          ///< ic_fluent_square_20_regular
#define FLUENT_ICON_TEXT_FONT 0xF0A8       ///< ic_fluent_text_font_20_regular
#define FLUENT_ICON_TREE 0xF0F5            ///< ic_fluent_tree_20_regular

// Rating control
#define FLUENT_ICON_STAR_FILLED 0xF092  ///< ic_fluent_star_20_filled
#define FLUENT_ICON_STAR_HALF 0xF0A5    ///< ic_fluent_star_half_20_regular
#define FLUENT_ICON_STAR_OUTLINE 0xF093 ///< ic_fluent_star_20_regular
