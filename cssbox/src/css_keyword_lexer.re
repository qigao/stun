// re2c -i --lang c -o css_keyword_lexer_gen.c css_keyword_lexer.re
/*
 * CSS Keyword Lexer - re2c scanner for CSS property values
 *
 * High-performance keyword matching using DFA.
 * Case-insensitive, zero allocations.
 *
 * Handles:
 * - Display, Position, Overflow, BoxSizing
 * - Flexbox: flex-direction, flex-wrap, justify-content, align-items, etc.
 * - Text: text-align, vertical-align, font-weight, font-style, text-decoration
 * - Border: border-style
 * - SVG: stroke-linecap, stroke-linejoin
 * - Named colors
 */

#include <stdint.h>
#include <string.h>
#include <stddef.h>

// Type definitions are in css_keyword_lexer.h
// Only enum values are used here for the lexer output

// Display values
#define CSS_DISPLAY_UNKNOWN 0
#define CSS_DISPLAY_BLOCK 1
#define CSS_DISPLAY_INLINE 2
#define CSS_DISPLAY_INLINE_BLOCK 3
#define CSS_DISPLAY_FLEX 4
#define CSS_DISPLAY_INLINE_FLEX 5
#define CSS_DISPLAY_GRID 6
#define CSS_DISPLAY_INLINE_GRID 7
#define CSS_DISPLAY_NONE 8
#define CSS_DISPLAY_CONTENTS 9

// Position values
#define CSS_POSITION_UNKNOWN 0
#define CSS_POSITION_STATIC 1
#define CSS_POSITION_RELATIVE 2
#define CSS_POSITION_ABSOLUTE 3
#define CSS_POSITION_FIXED 4
#define CSS_POSITION_STICKY 5

// Overflow values
#define CSS_OVERFLOW_UNKNOWN 0
#define CSS_OVERFLOW_VISIBLE 1
#define CSS_OVERFLOW_HIDDEN 2
#define CSS_OVERFLOW_SCROLL 3
#define CSS_OVERFLOW_AUTO 4

// BoxSizing values
#define CSS_BOX_SIZING_UNKNOWN 0
#define CSS_BOX_SIZING_CONTENT_BOX 1
#define CSS_BOX_SIZING_BORDER_BOX 2

// FlexDirection values
#define CSS_FLEX_DIR_UNKNOWN 0
#define CSS_FLEX_DIR_ROW 1
#define CSS_FLEX_DIR_ROW_REVERSE 2
#define CSS_FLEX_DIR_COLUMN 3
#define CSS_FLEX_DIR_COLUMN_REVERSE 4

// FlexWrap values
#define CSS_FLEX_WRAP_UNKNOWN 0
#define CSS_FLEX_WRAP_NOWRAP 1
#define CSS_FLEX_WRAP_WRAP 2
#define CSS_FLEX_WRAP_WRAP_REVERSE 3

// JustifyContent values
#define CSS_JUSTIFY_UNKNOWN 0
#define CSS_JUSTIFY_FLEX_START 1
#define CSS_JUSTIFY_FLEX_END 2
#define CSS_JUSTIFY_CENTER 3
#define CSS_JUSTIFY_SPACE_BETWEEN 4
#define CSS_JUSTIFY_SPACE_AROUND 5
#define CSS_JUSTIFY_SPACE_EVENLY 6
#define CSS_JUSTIFY_START 7
#define CSS_JUSTIFY_END 8

// AlignItems values
#define CSS_ALIGN_ITEMS_UNKNOWN 0
#define CSS_ALIGN_ITEMS_FLEX_START 1
#define CSS_ALIGN_ITEMS_FLEX_END 2
#define CSS_ALIGN_ITEMS_CENTER 3
#define CSS_ALIGN_ITEMS_BASELINE 4
#define CSS_ALIGN_ITEMS_STRETCH 5
#define CSS_ALIGN_ITEMS_START 6
#define CSS_ALIGN_ITEMS_END 7

// AlignContent values
#define CSS_ALIGN_CONTENT_UNKNOWN 0
#define CSS_ALIGN_CONTENT_FLEX_START 1
#define CSS_ALIGN_CONTENT_FLEX_END 2
#define CSS_ALIGN_CONTENT_CENTER 3
#define CSS_ALIGN_CONTENT_SPACE_BETWEEN 4
#define CSS_ALIGN_CONTENT_SPACE_AROUND 5
#define CSS_ALIGN_CONTENT_STRETCH 6

// SelfAlign values
#define CSS_SELF_ALIGN_UNKNOWN 0
#define CSS_SELF_ALIGN_AUTO 1
#define CSS_SELF_ALIGN_START 2
#define CSS_SELF_ALIGN_END 3
#define CSS_SELF_ALIGN_CENTER 4
#define CSS_SELF_ALIGN_BASELINE 5
#define CSS_SELF_ALIGN_STRETCH 6
#define CSS_SELF_ALIGN_FLEX_START 7
#define CSS_SELF_ALIGN_FLEX_END 8

// BorderStyle values
#define CSS_BORDER_STYLE_UNKNOWN 0
#define CSS_BORDER_STYLE_NONE 1
#define CSS_BORDER_STYLE_SOLID 2
#define CSS_BORDER_STYLE_DASHED 3
#define CSS_BORDER_STYLE_DOTTED 4
#define CSS_BORDER_STYLE_DOUBLE 5
#define CSS_BORDER_STYLE_HIDDEN 6
#define CSS_BORDER_STYLE_GROOVE 7
#define CSS_BORDER_STYLE_RIDGE 8
#define CSS_BORDER_STYLE_INSET 9
#define CSS_BORDER_STYLE_OUTSET 10

// TextAlign values
#define CSS_TEXT_ALIGN_UNKNOWN 0
#define CSS_TEXT_ALIGN_LEFT 1
#define CSS_TEXT_ALIGN_CENTER 2
#define CSS_TEXT_ALIGN_RIGHT 3
#define CSS_TEXT_ALIGN_JUSTIFY 4

// VerticalAlign values
#define CSS_VERTICAL_ALIGN_UNKNOWN 0
#define CSS_VERTICAL_ALIGN_TOP 1
#define CSS_VERTICAL_ALIGN_MIDDLE 2
#define CSS_VERTICAL_ALIGN_BOTTOM 3
#define CSS_VERTICAL_ALIGN_BASELINE 4

// FontWeight values
#define CSS_FONT_WEIGHT_UNKNOWN 0
#define CSS_FONT_WEIGHT_NORMAL 1
#define CSS_FONT_WEIGHT_BOLD 2
#define CSS_FONT_WEIGHT_LIGHTER 3
#define CSS_FONT_WEIGHT_BOLDER 4
#define CSS_FONT_WEIGHT_100 5
#define CSS_FONT_WEIGHT_200 6
#define CSS_FONT_WEIGHT_300 7
#define CSS_FONT_WEIGHT_400 8
#define CSS_FONT_WEIGHT_500 9
#define CSS_FONT_WEIGHT_600 10
#define CSS_FONT_WEIGHT_700 11
#define CSS_FONT_WEIGHT_800 12
#define CSS_FONT_WEIGHT_900 13

// FontStyle values
#define CSS_FONT_STYLE_UNKNOWN 0
#define CSS_FONT_STYLE_NORMAL 1
#define CSS_FONT_STYLE_ITALIC 2
#define CSS_FONT_STYLE_OBLIQUE 3

// TextDecoration values
#define CSS_TEXT_DECORATION_UNKNOWN 0
#define CSS_TEXT_DECORATION_NONE 1
#define CSS_TEXT_DECORATION_UNDERLINE 2
#define CSS_TEXT_DECORATION_OVERLINE 3
#define CSS_TEXT_DECORATION_LINE_THROUGH 4

// TextTransform values
#define CSS_TEXT_TRANSFORM_UNKNOWN 0
#define CSS_TEXT_TRANSFORM_NONE 1
#define CSS_TEXT_TRANSFORM_UPPERCASE 2
#define CSS_TEXT_TRANSFORM_LOWERCASE 3
#define CSS_TEXT_TRANSFORM_CAPITALIZE 4

// LineCap values
#define CSS_LINE_CAP_UNKNOWN 0
#define CSS_LINE_CAP_BUTT 1
#define CSS_LINE_CAP_ROUND 2
#define CSS_LINE_CAP_SQUARE 3

// LineJoin values
#define CSS_LINE_JOIN_UNKNOWN 0
#define CSS_LINE_JOIN_MITER 1
#define CSS_LINE_JOIN_ROUND 2
#define CSS_LINE_JOIN_BEVEL 3

// TransitionProperty values
#define CSS_TRANS_PROP_UNKNOWN 0
#define CSS_TRANS_PROP_NONE 1
#define CSS_TRANS_PROP_ALL 2
#define CSS_TRANS_PROP_WIDTH 3
#define CSS_TRANS_PROP_HEIGHT 4
#define CSS_TRANS_PROP_MIN_WIDTH 5
#define CSS_TRANS_PROP_MIN_HEIGHT 6
#define CSS_TRANS_PROP_MAX_WIDTH 7
#define CSS_TRANS_PROP_MAX_HEIGHT 8
#define CSS_TRANS_PROP_PADDING 9
#define CSS_TRANS_PROP_PADDING_TOP 10
#define CSS_TRANS_PROP_PADDING_RIGHT 11
#define CSS_TRANS_PROP_PADDING_BOTTOM 12
#define CSS_TRANS_PROP_PADDING_LEFT 13
#define CSS_TRANS_PROP_MARGIN 14
#define CSS_TRANS_PROP_MARGIN_TOP 15
#define CSS_TRANS_PROP_MARGIN_RIGHT 16
#define CSS_TRANS_PROP_MARGIN_BOTTOM 17
#define CSS_TRANS_PROP_MARGIN_LEFT 18
#define CSS_TRANS_PROP_TOP 19
#define CSS_TRANS_PROP_RIGHT 20
#define CSS_TRANS_PROP_BOTTOM 21
#define CSS_TRANS_PROP_LEFT 22
#define CSS_TRANS_PROP_OPACITY 23
#define CSS_TRANS_PROP_BACKGROUND_COLOR 24
#define CSS_TRANS_PROP_COLOR 25
#define CSS_TRANS_PROP_BORDER_COLOR 26
#define CSS_TRANS_PROP_BORDER_WIDTH 27
#define CSS_TRANS_PROP_BORDER_RADIUS 28
#define CSS_TRANS_PROP_TRANSFORM 29
#define CSS_TRANS_PROP_FLEX_GROW 30
#define CSS_TRANS_PROP_FLEX_SHRINK 31
#define CSS_TRANS_PROP_GAP 32
#define CSS_TRANS_PROP_FILL 33
#define CSS_TRANS_PROP_STROKE 34
#define CSS_TRANS_PROP_STROKE_WIDTH 35
#define CSS_TRANS_PROP_FILTER 36

// TimingFunction keyword values
#define CSS_TIMING_UNKNOWN 0
#define CSS_TIMING_LINEAR 1
#define CSS_TIMING_EASE 2
#define CSS_TIMING_EASE_IN 3
#define CSS_TIMING_EASE_OUT 4
#define CSS_TIMING_EASE_IN_OUT 5

// AnimationDirection values
#define CSS_ANIM_DIR_UNKNOWN 0
#define CSS_ANIM_DIR_NORMAL 1
#define CSS_ANIM_DIR_REVERSE 2
#define CSS_ANIM_DIR_ALTERNATE 3
#define CSS_ANIM_DIR_ALTERNATE_REVERSE 4

// AnimationFillMode values
#define CSS_ANIM_FILL_UNKNOWN 0
#define CSS_ANIM_FILL_NONE 1
#define CSS_ANIM_FILL_FORWARDS 2
#define CSS_ANIM_FILL_BACKWARDS 3
#define CSS_ANIM_FILL_BOTH 4

// AnimationPlayState values
#define CSS_ANIM_PLAY_UNKNOWN 0
#define CSS_ANIM_PLAY_RUNNING 1
#define CSS_ANIM_PLAY_PAUSED 2

// Transform function values
#define CSS_TRANSFORM_UNKNOWN 0
#define CSS_TRANSFORM_TRANSLATE 1
#define CSS_TRANSFORM_TRANSLATEX 2
#define CSS_TRANSFORM_TRANSLATEY 3
#define CSS_TRANSFORM_ROTATE 4
#define CSS_TRANSFORM_SCALE 5
#define CSS_TRANSFORM_SCALEX 6
#define CSS_TRANSFORM_SCALEY 7
#define CSS_TRANSFORM_SKEW 8
#define CSS_TRANSFORM_SKEWX 9
#define CSS_TRANSFORM_SKEWY 10
#define CSS_TRANSFORM_MATRIX 11

// Length unit values
#define CSS_UNIT_UNKNOWN 0
#define CSS_UNIT_PX 1
#define CSS_UNIT_PERCENT 2
#define CSS_UNIT_EM 3
#define CSS_UNIT_REM 4
#define CSS_UNIT_VW 5
#define CSS_UNIT_VH 6
#define CSS_UNIT_NONE 7

// Keyframe property values
#define CSS_KF_PROP_UNKNOWN 0
#define CSS_KF_PROP_OPACITY 1
#define CSS_KF_PROP_BACKGROUND_COLOR 2
#define CSS_KF_PROP_BACKGROUND 3
#define CSS_KF_PROP_COLOR 4
#define CSS_KF_PROP_TRANSFORM 5
#define CSS_KF_PROP_WIDTH 6
#define CSS_KF_PROP_HEIGHT 7
#define CSS_KF_PROP_BORDER_RADIUS 8
#define CSS_KF_PROP_BORDER_COLOR 9
#define CSS_KF_PROP_BORDER_WIDTH 10

// Element type values (for SVG/HTML elements)
#define CSS_ELEM_UNKNOWN 0
#define CSS_ELEM_TEXT 1
#define CSS_ELEM_SPAN 2
#define CSS_ELEM_LINE 3
#define CSS_ELEM_CIRCLE 4
#define CSS_ELEM_ELLIPSE 5
#define CSS_ELEM_RECT 6
#define CSS_ELEM_PATH 7
#define CSS_ELEM_POLYGON 8
#define CSS_ELEM_POLYLINE 9
#define CSS_ELEM_SVG 10
#define CSS_ELEM_TEXTPATH 11
#define CSS_ELEM_G 12
#define CSS_ELEM_DEFS 13
#define CSS_ELEM_USE 14
#define CSS_ELEM_IMAGE 15
#define CSS_ELEM_DIV 16

// Position keyword values
#define CSS_POS_UNKNOWN 0
#define CSS_POS_LEFT 1
#define CSS_POS_CENTER 2
#define CSS_POS_RIGHT 3
#define CSS_POS_TOP 4
#define CSS_POS_BOTTOM 5

// Named color result struct
typedef struct {
    uint8_t r, g, b, a;
    int valid;
} CSSNamedColorResult;

// ============================================================================
// Display Parser
// ============================================================================

int css_parse_display(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "block"        { return CSS_DISPLAY_BLOCK; }
        "inline-block" { return CSS_DISPLAY_INLINE_BLOCK; }
        "inline-flex"  { return CSS_DISPLAY_INLINE_FLEX; }
        "inline-grid"  { return CSS_DISPLAY_INLINE_GRID; }
        "inline"       { return CSS_DISPLAY_INLINE; }
        "flex"         { return CSS_DISPLAY_FLEX; }
        "grid"         { return CSS_DISPLAY_GRID; }
        "none"         { return CSS_DISPLAY_NONE; }
        "contents"     { return CSS_DISPLAY_CONTENTS; }
        *              { return CSS_DISPLAY_UNKNOWN; }
    */
}

// ============================================================================
// Position Parser
// ============================================================================

int css_parse_position(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "static"   { return CSS_POSITION_STATIC; }
        "relative" { return CSS_POSITION_RELATIVE; }
        "absolute" { return CSS_POSITION_ABSOLUTE; }
        "fixed"    { return CSS_POSITION_FIXED; }
        "sticky"   { return CSS_POSITION_STICKY; }
        *          { return CSS_POSITION_UNKNOWN; }
    */
}

// ============================================================================
// Overflow Parser
// ============================================================================

int css_parse_overflow(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "visible" { return CSS_OVERFLOW_VISIBLE; }
        "hidden"  { return CSS_OVERFLOW_HIDDEN; }
        "scroll"  { return CSS_OVERFLOW_SCROLL; }
        "auto"    { return CSS_OVERFLOW_AUTO; }
        *         { return CSS_OVERFLOW_UNKNOWN; }
    */
}

// ============================================================================
// BoxSizing Parser
// ============================================================================

int css_parse_box_sizing(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "content-box" { return CSS_BOX_SIZING_CONTENT_BOX; }
        "border-box"  { return CSS_BOX_SIZING_BORDER_BOX; }
        *             { return CSS_BOX_SIZING_UNKNOWN; }
    */
}

// ============================================================================
// FlexDirection Parser
// ============================================================================

int css_parse_flex_direction(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "row-reverse"    { return CSS_FLEX_DIR_ROW_REVERSE; }
        "row"            { return CSS_FLEX_DIR_ROW; }
        "column-reverse" { return CSS_FLEX_DIR_COLUMN_REVERSE; }
        "column"         { return CSS_FLEX_DIR_COLUMN; }
        *                { return CSS_FLEX_DIR_UNKNOWN; }
    */
}

// ============================================================================
// FlexWrap Parser
// ============================================================================

int css_parse_flex_wrap(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "wrap-reverse" { return CSS_FLEX_WRAP_WRAP_REVERSE; }
        "wrap"         { return CSS_FLEX_WRAP_WRAP; }
        "nowrap"       { return CSS_FLEX_WRAP_NOWRAP; }
        *              { return CSS_FLEX_WRAP_UNKNOWN; }
    */
}

// ============================================================================
// JustifyContent Parser
// ============================================================================

int css_parse_justify_content(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "flex-start"    { return CSS_JUSTIFY_FLEX_START; }
        "flex-end"      { return CSS_JUSTIFY_FLEX_END; }
        "center"        { return CSS_JUSTIFY_CENTER; }
        "space-between" { return CSS_JUSTIFY_SPACE_BETWEEN; }
        "space-around"  { return CSS_JUSTIFY_SPACE_AROUND; }
        "space-evenly"  { return CSS_JUSTIFY_SPACE_EVENLY; }
        "start"         { return CSS_JUSTIFY_START; }
        "end"           { return CSS_JUSTIFY_END; }
        *               { return CSS_JUSTIFY_UNKNOWN; }
    */
}

// ============================================================================
// AlignItems Parser
// ============================================================================

int css_parse_align_items(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "flex-start" { return CSS_ALIGN_ITEMS_FLEX_START; }
        "flex-end"   { return CSS_ALIGN_ITEMS_FLEX_END; }
        "center"     { return CSS_ALIGN_ITEMS_CENTER; }
        "baseline"   { return CSS_ALIGN_ITEMS_BASELINE; }
        "stretch"    { return CSS_ALIGN_ITEMS_STRETCH; }
        "start"      { return CSS_ALIGN_ITEMS_START; }
        "end"        { return CSS_ALIGN_ITEMS_END; }
        *            { return CSS_ALIGN_ITEMS_UNKNOWN; }
    */
}

// ============================================================================
// AlignContent Parser
// ============================================================================

int css_parse_align_content(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "flex-start"    { return CSS_ALIGN_CONTENT_FLEX_START; }
        "flex-end"      { return CSS_ALIGN_CONTENT_FLEX_END; }
        "center"        { return CSS_ALIGN_CONTENT_CENTER; }
        "space-between" { return CSS_ALIGN_CONTENT_SPACE_BETWEEN; }
        "space-around"  { return CSS_ALIGN_CONTENT_SPACE_AROUND; }
        "stretch"       { return CSS_ALIGN_CONTENT_STRETCH; }
        *               { return CSS_ALIGN_CONTENT_UNKNOWN; }
    */
}

// ============================================================================
// AlignSelf / JustifySelf Parser
// ============================================================================

int css_parse_self_align(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "auto"       { return CSS_SELF_ALIGN_AUTO; }
        "flex-start" { return CSS_SELF_ALIGN_FLEX_START; }
        "flex-end"   { return CSS_SELF_ALIGN_FLEX_END; }
        "start"      { return CSS_SELF_ALIGN_START; }
        "end"        { return CSS_SELF_ALIGN_END; }
        "center"     { return CSS_SELF_ALIGN_CENTER; }
        "baseline"   { return CSS_SELF_ALIGN_BASELINE; }
        "stretch"    { return CSS_SELF_ALIGN_STRETCH; }
        *            { return CSS_SELF_ALIGN_UNKNOWN; }
    */
}

// ============================================================================
// BorderStyle Parser
// ============================================================================

int css_parse_border_style(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "none"   { return CSS_BORDER_STYLE_NONE; }
        "solid"  { return CSS_BORDER_STYLE_SOLID; }
        "dashed" { return CSS_BORDER_STYLE_DASHED; }
        "dotted" { return CSS_BORDER_STYLE_DOTTED; }
        "double" { return CSS_BORDER_STYLE_DOUBLE; }
        "hidden" { return CSS_BORDER_STYLE_HIDDEN; }
        "groove" { return CSS_BORDER_STYLE_GROOVE; }
        "ridge"  { return CSS_BORDER_STYLE_RIDGE; }
        "inset"  { return CSS_BORDER_STYLE_INSET; }
        "outset" { return CSS_BORDER_STYLE_OUTSET; }
        *        { return CSS_BORDER_STYLE_UNKNOWN; }
    */
}

// ============================================================================
// TextAlign Parser
// ============================================================================

int css_parse_text_align(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "left"    { return CSS_TEXT_ALIGN_LEFT; }
        "center"  { return CSS_TEXT_ALIGN_CENTER; }
        "right"   { return CSS_TEXT_ALIGN_RIGHT; }
        "justify" { return CSS_TEXT_ALIGN_JUSTIFY; }
        *         { return CSS_TEXT_ALIGN_UNKNOWN; }
    */
}

// ============================================================================
// VerticalAlign Parser
// ============================================================================

int css_parse_vertical_align(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "top"      { return CSS_VERTICAL_ALIGN_TOP; }
        "middle"   { return CSS_VERTICAL_ALIGN_MIDDLE; }
        "bottom"   { return CSS_VERTICAL_ALIGN_BOTTOM; }
        "baseline" { return CSS_VERTICAL_ALIGN_BASELINE; }
        *          { return CSS_VERTICAL_ALIGN_UNKNOWN; }
    */
}

// ============================================================================
// FontWeight Parser
// ============================================================================

int css_parse_font_weight(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "normal"  { return CSS_FONT_WEIGHT_NORMAL; }
        "bold"    { return CSS_FONT_WEIGHT_BOLD; }
        "lighter" { return CSS_FONT_WEIGHT_LIGHTER; }
        "bolder"  { return CSS_FONT_WEIGHT_BOLDER; }
        "100"     { return CSS_FONT_WEIGHT_100; }
        "200"     { return CSS_FONT_WEIGHT_200; }
        "300"     { return CSS_FONT_WEIGHT_300; }
        "400"     { return CSS_FONT_WEIGHT_400; }
        "500"     { return CSS_FONT_WEIGHT_500; }
        "600"     { return CSS_FONT_WEIGHT_600; }
        "700"     { return CSS_FONT_WEIGHT_700; }
        "800"     { return CSS_FONT_WEIGHT_800; }
        "900"     { return CSS_FONT_WEIGHT_900; }
        *         { return CSS_FONT_WEIGHT_UNKNOWN; }
    */
}

// ============================================================================
// FontStyle Parser
// ============================================================================

int css_parse_font_style(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "normal"  { return CSS_FONT_STYLE_NORMAL; }
        "italic"  { return CSS_FONT_STYLE_ITALIC; }
        "oblique" { return CSS_FONT_STYLE_OBLIQUE; }
        *         { return CSS_FONT_STYLE_UNKNOWN; }
    */
}

// ============================================================================
// TextDecoration Parser
// ============================================================================

int css_parse_text_decoration(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "none"         { return CSS_TEXT_DECORATION_NONE; }
        "underline"    { return CSS_TEXT_DECORATION_UNDERLINE; }
        "overline"     { return CSS_TEXT_DECORATION_OVERLINE; }
        "line-through" { return CSS_TEXT_DECORATION_LINE_THROUGH; }
        *              { return CSS_TEXT_DECORATION_UNKNOWN; }
    */
}

// ============================================================================
// TextTransform Parser
// ============================================================================

int css_parse_text_transform(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "none"       { return CSS_TEXT_TRANSFORM_NONE; }
        "uppercase"  { return CSS_TEXT_TRANSFORM_UPPERCASE; }
        "lowercase"  { return CSS_TEXT_TRANSFORM_LOWERCASE; }
        "capitalize" { return CSS_TEXT_TRANSFORM_CAPITALIZE; }
        *            { return CSS_TEXT_TRANSFORM_UNKNOWN; }
    */
}

// ============================================================================
// SVG LineCap Parser
// ============================================================================

int css_parse_line_cap(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "butt"   { return CSS_LINE_CAP_BUTT; }
        "round"  { return CSS_LINE_CAP_ROUND; }
        "square" { return CSS_LINE_CAP_SQUARE; }
        *        { return CSS_LINE_CAP_UNKNOWN; }
    */
}

// ============================================================================
// SVG LineJoin Parser
// ============================================================================

int css_parse_line_join(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "miter" { return CSS_LINE_JOIN_MITER; }
        "round" { return CSS_LINE_JOIN_ROUND; }
        "bevel" { return CSS_LINE_JOIN_BEVEL; }
        *       { return CSS_LINE_JOIN_UNKNOWN; }
    */
}

// ============================================================================
// Named Color Parser (CSS Level 4 basic colors + extended)
// ============================================================================

CSSNamedColorResult css_parse_named_color(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    CSSNamedColorResult result = {0, 0, 0, 255, 0};
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        // Basic colors
        "black"       { result = (CSSNamedColorResult){0, 0, 0, 255, 1}; return result; }
        "white"       { result = (CSSNamedColorResult){255, 255, 255, 255, 1}; return result; }
        "red"         { result = (CSSNamedColorResult){255, 0, 0, 255, 1}; return result; }
        "green"       { result = (CSSNamedColorResult){0, 128, 0, 255, 1}; return result; }
        "blue"        { result = (CSSNamedColorResult){0, 0, 255, 255, 1}; return result; }
        "yellow"      { result = (CSSNamedColorResult){255, 255, 0, 255, 1}; return result; }
        "cyan"        { result = (CSSNamedColorResult){0, 255, 255, 255, 1}; return result; }
        "magenta"     { result = (CSSNamedColorResult){255, 0, 255, 255, 1}; return result; }
        "gray"        { result = (CSSNamedColorResult){128, 128, 128, 255, 1}; return result; }
        "grey"        { result = (CSSNamedColorResult){128, 128, 128, 255, 1}; return result; }
        
        // Extended colors
        "orange"      { result = (CSSNamedColorResult){255, 165, 0, 255, 1}; return result; }
        "purple"      { result = (CSSNamedColorResult){128, 0, 128, 255, 1}; return result; }
        "brown"       { result = (CSSNamedColorResult){165, 42, 42, 255, 1}; return result; }
        "pink"        { result = (CSSNamedColorResult){255, 192, 203, 255, 1}; return result; }
        "lime"        { result = (CSSNamedColorResult){0, 255, 0, 255, 1}; return result; }
        "navy"        { result = (CSSNamedColorResult){0, 0, 128, 255, 1}; return result; }
        "teal"        { result = (CSSNamedColorResult){0, 128, 128, 255, 1}; return result; }
        "olive"       { result = (CSSNamedColorResult){128, 128, 0, 255, 1}; return result; }
        "maroon"      { result = (CSSNamedColorResult){128, 0, 0, 255, 1}; return result; }
        "aqua"        { result = (CSSNamedColorResult){0, 255, 255, 255, 1}; return result; }
        "silver"      { result = (CSSNamedColorResult){192, 192, 192, 255, 1}; return result; }
        "fuchsia"     { result = (CSSNamedColorResult){255, 0, 255, 255, 1}; return result; }
        "transparent" { result = (CSSNamedColorResult){0, 0, 0, 0, 1}; return result; }
        
        // More extended colors
        "coral"       { result = (CSSNamedColorResult){255, 127, 80, 255, 1}; return result; }
        "crimson"     { result = (CSSNamedColorResult){220, 20, 60, 255, 1}; return result; }
        "gold"        { result = (CSSNamedColorResult){255, 215, 0, 255, 1}; return result; }
        "indigo"      { result = (CSSNamedColorResult){75, 0, 130, 255, 1}; return result; }
        "ivory"       { result = (CSSNamedColorResult){255, 255, 240, 255, 1}; return result; }
        "khaki"       { result = (CSSNamedColorResult){240, 230, 140, 255, 1}; return result; }
        "lavender"    { result = (CSSNamedColorResult){230, 230, 250, 255, 1}; return result; }
        "salmon"      { result = (CSSNamedColorResult){250, 128, 114, 255, 1}; return result; }
        "skyblue"     { result = (CSSNamedColorResult){135, 206, 235, 255, 1}; return result; }
        "tomato"      { result = (CSSNamedColorResult){255, 99, 71, 255, 1}; return result; }
        "turquoise"   { result = (CSSNamedColorResult){64, 224, 208, 255, 1}; return result; }
        "violet"      { result = (CSSNamedColorResult){238, 130, 238, 255, 1}; return result; }
        "wheat"       { result = (CSSNamedColorResult){245, 222, 179, 255, 1}; return result; }
        
        // Gray shades
        "dimgray"     { result = (CSSNamedColorResult){105, 105, 105, 255, 1}; return result; }
        "dimgrey"     { result = (CSSNamedColorResult){105, 105, 105, 255, 1}; return result; }
        "darkgray"    { result = (CSSNamedColorResult){169, 169, 169, 255, 1}; return result; }
        "darkgrey"    { result = (CSSNamedColorResult){169, 169, 169, 255, 1}; return result; }
        "lightgray"   { result = (CSSNamedColorResult){211, 211, 211, 255, 1}; return result; }
        "lightgrey"   { result = (CSSNamedColorResult){211, 211, 211, 255, 1}; return result; }
        "gainsboro"   { result = (CSSNamedColorResult){220, 220, 220, 255, 1}; return result; }
        "whitesmoke"  { result = (CSSNamedColorResult){245, 245, 245, 255, 1}; return result; }
        
        // Blue shades
        "darkblue"    { result = (CSSNamedColorResult){0, 0, 139, 255, 1}; return result; }
        "lightblue"   { result = (CSSNamedColorResult){173, 216, 230, 255, 1}; return result; }
        "steelblue"   { result = (CSSNamedColorResult){70, 130, 180, 255, 1}; return result; }
        "royalblue"   { result = (CSSNamedColorResult){65, 105, 225, 255, 1}; return result; }
        "dodgerblue"  { result = (CSSNamedColorResult){30, 144, 255, 255, 1}; return result; }
        "deepskyblue" { result = (CSSNamedColorResult){0, 191, 255, 255, 1}; return result; }
        "cornflowerblue" { result = (CSSNamedColorResult){100, 149, 237, 255, 1}; return result; }
        
        // Green shades
        "darkgreen"   { result = (CSSNamedColorResult){0, 100, 0, 255, 1}; return result; }
        "lightgreen"  { result = (CSSNamedColorResult){144, 238, 144, 255, 1}; return result; }
        "forestgreen" { result = (CSSNamedColorResult){34, 139, 34, 255, 1}; return result; }
        "seagreen"    { result = (CSSNamedColorResult){46, 139, 87, 255, 1}; return result; }
        "limegreen"   { result = (CSSNamedColorResult){50, 205, 50, 255, 1}; return result; }
        "springgreen" { result = (CSSNamedColorResult){0, 255, 127, 255, 1}; return result; }
        
        // Red/Orange shades
        "darkred"     { result = (CSSNamedColorResult){139, 0, 0, 255, 1}; return result; }
        "firebrick"   { result = (CSSNamedColorResult){178, 34, 34, 255, 1}; return result; }
        "indianred"   { result = (CSSNamedColorResult){205, 92, 92, 255, 1}; return result; }
        "orangered"   { result = (CSSNamedColorResult){255, 69, 0, 255, 1}; return result; }
        "darkorange"  { result = (CSSNamedColorResult){255, 140, 0, 255, 1}; return result; }
        
        *             { return result; }  // result.valid = 0
    */
}

// ============================================================================
// TransitionProperty Parser
// ============================================================================

int css_parse_transition_property(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "all"             { return CSS_TRANS_PROP_ALL; }
        "none"            { return CSS_TRANS_PROP_NONE; }
        "width"           { return CSS_TRANS_PROP_WIDTH; }
        "height"          { return CSS_TRANS_PROP_HEIGHT; }
        "min-width"       { return CSS_TRANS_PROP_MIN_WIDTH; }
        "min-height"      { return CSS_TRANS_PROP_MIN_HEIGHT; }
        "max-width"       { return CSS_TRANS_PROP_MAX_WIDTH; }
        "max-height"      { return CSS_TRANS_PROP_MAX_HEIGHT; }
        "padding-top"     { return CSS_TRANS_PROP_PADDING_TOP; }
        "padding-right"   { return CSS_TRANS_PROP_PADDING_RIGHT; }
        "padding-bottom"  { return CSS_TRANS_PROP_PADDING_BOTTOM; }
        "padding-left"    { return CSS_TRANS_PROP_PADDING_LEFT; }
        "padding"         { return CSS_TRANS_PROP_PADDING; }
        "margin-top"      { return CSS_TRANS_PROP_MARGIN_TOP; }
        "margin-right"    { return CSS_TRANS_PROP_MARGIN_RIGHT; }
        "margin-bottom"   { return CSS_TRANS_PROP_MARGIN_BOTTOM; }
        "margin-left"     { return CSS_TRANS_PROP_MARGIN_LEFT; }
        "margin"          { return CSS_TRANS_PROP_MARGIN; }
        "top"             { return CSS_TRANS_PROP_TOP; }
        "right"           { return CSS_TRANS_PROP_RIGHT; }
        "bottom"          { return CSS_TRANS_PROP_BOTTOM; }
        "left"            { return CSS_TRANS_PROP_LEFT; }
        "opacity"         { return CSS_TRANS_PROP_OPACITY; }
        "background-color" { return CSS_TRANS_PROP_BACKGROUND_COLOR; }
        "background"      { return CSS_TRANS_PROP_BACKGROUND_COLOR; }
        "color"           { return CSS_TRANS_PROP_COLOR; }
        "border-color"    { return CSS_TRANS_PROP_BORDER_COLOR; }
        "border-width"    { return CSS_TRANS_PROP_BORDER_WIDTH; }
        "border-radius"   { return CSS_TRANS_PROP_BORDER_RADIUS; }
        "transform"       { return CSS_TRANS_PROP_TRANSFORM; }
        "flex-grow"       { return CSS_TRANS_PROP_FLEX_GROW; }
        "flex-shrink"     { return CSS_TRANS_PROP_FLEX_SHRINK; }
        "gap"             { return CSS_TRANS_PROP_GAP; }
        "fill"            { return CSS_TRANS_PROP_FILL; }
        "stroke-width"    { return CSS_TRANS_PROP_STROKE_WIDTH; }
        "stroke"          { return CSS_TRANS_PROP_STROKE; }
        "filter"          { return CSS_TRANS_PROP_FILTER; }
        *                 { return CSS_TRANS_PROP_UNKNOWN; }
    */
}

// ============================================================================
// TimingFunction Keyword Parser
// ============================================================================

int css_parse_timing_keyword(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "linear"      { return CSS_TIMING_LINEAR; }
        "ease-in-out" { return CSS_TIMING_EASE_IN_OUT; }
        "ease-in"     { return CSS_TIMING_EASE_IN; }
        "ease-out"    { return CSS_TIMING_EASE_OUT; }
        "ease"        { return CSS_TIMING_EASE; }
        *             { return CSS_TIMING_UNKNOWN; }
    */
}

// ============================================================================
// AnimationDirection Parser
// ============================================================================

int css_parse_animation_direction(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "alternate-reverse" { return CSS_ANIM_DIR_ALTERNATE_REVERSE; }
        "alternate"         { return CSS_ANIM_DIR_ALTERNATE; }
        "reverse"           { return CSS_ANIM_DIR_REVERSE; }
        "normal"            { return CSS_ANIM_DIR_NORMAL; }
        *                   { return CSS_ANIM_DIR_UNKNOWN; }
    */
}

// ============================================================================
// AnimationFillMode Parser
// ============================================================================

int css_parse_animation_fill_mode(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "forwards"  { return CSS_ANIM_FILL_FORWARDS; }
        "backwards" { return CSS_ANIM_FILL_BACKWARDS; }
        "both"      { return CSS_ANIM_FILL_BOTH; }
        "none"      { return CSS_ANIM_FILL_NONE; }
        *           { return CSS_ANIM_FILL_UNKNOWN; }
    */
}

// ============================================================================
// AnimationPlayState Parser
// ============================================================================

int css_parse_animation_play_state(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "running" { return CSS_ANIM_PLAY_RUNNING; }
        "paused"  { return CSS_ANIM_PLAY_PAUSED; }
        *         { return CSS_ANIM_PLAY_UNKNOWN; }
    */
}

// ============================================================================
// Transform Function Parser
// ============================================================================

int css_parse_transform_function(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "translatex" { return CSS_TRANSFORM_TRANSLATEX; }
        "translatey" { return CSS_TRANSFORM_TRANSLATEY; }
        "translate"  { return CSS_TRANSFORM_TRANSLATE; }
        "rotate"     { return CSS_TRANSFORM_ROTATE; }
        "scalex"     { return CSS_TRANSFORM_SCALEX; }
        "scaley"     { return CSS_TRANSFORM_SCALEY; }
        "scale"      { return CSS_TRANSFORM_SCALE; }
        "skewx"      { return CSS_TRANSFORM_SKEWX; }
        "skewy"      { return CSS_TRANSFORM_SKEWY; }
        "skew"       { return CSS_TRANSFORM_SKEW; }
        "matrix"     { return CSS_TRANSFORM_MATRIX; }
        *            { return CSS_TRANSFORM_UNKNOWN; }
    */
}

// ============================================================================
// Length Unit Parser
// ============================================================================

int css_parse_length_unit(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "px"  { return CSS_UNIT_PX; }
        "%"   { return CSS_UNIT_PERCENT; }
        "em"  { return CSS_UNIT_EM; }
        "rem" { return CSS_UNIT_REM; }
        "vw"  { return CSS_UNIT_VW; }
        "vh"  { return CSS_UNIT_VH; }
        ""    { return CSS_UNIT_NONE; }
        *     { return CSS_UNIT_UNKNOWN; }
    */
}

// ============================================================================
// Keyframe Property Parser
// ============================================================================

int css_parse_keyframe_property(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "opacity"          { return CSS_KF_PROP_OPACITY; }
        "background-color" { return CSS_KF_PROP_BACKGROUND_COLOR; }
        "background"       { return CSS_KF_PROP_BACKGROUND; }
        "color"            { return CSS_KF_PROP_COLOR; }
        "transform"        { return CSS_KF_PROP_TRANSFORM; }
        "width"            { return CSS_KF_PROP_WIDTH; }
        "height"           { return CSS_KF_PROP_HEIGHT; }
        "border-radius"    { return CSS_KF_PROP_BORDER_RADIUS; }
        "border-color"     { return CSS_KF_PROP_BORDER_COLOR; }
        "border-width"     { return CSS_KF_PROP_BORDER_WIDTH; }
        *                  { return CSS_KF_PROP_UNKNOWN; }
    */
}

// ============================================================================
// Element Type Parser (SVG/HTML elements)
// ============================================================================

int css_parse_element_type(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "textpath" { return CSS_ELEM_TEXTPATH; }
        "polyline" { return CSS_ELEM_POLYLINE; }
        "polygon"  { return CSS_ELEM_POLYGON; }
        "ellipse"  { return CSS_ELEM_ELLIPSE; }
        "circle"   { return CSS_ELEM_CIRCLE; }
        "image"    { return CSS_ELEM_IMAGE; }
        "text"     { return CSS_ELEM_TEXT; }
        "span"     { return CSS_ELEM_SPAN; }
        "line"     { return CSS_ELEM_LINE; }
        "rect"     { return CSS_ELEM_RECT; }
        "path"     { return CSS_ELEM_PATH; }
        "defs"     { return CSS_ELEM_DEFS; }
        "svg"      { return CSS_ELEM_SVG; }
        "use"      { return CSS_ELEM_USE; }
        "div"      { return CSS_ELEM_DIV; }
        "g"        { return CSS_ELEM_G; }
        *          { return CSS_ELEM_UNKNOWN; }
    */
}

// ============================================================================
// Position Keyword Parser (for background-position, transform-origin, etc.)
// ============================================================================

int css_parse_position_keyword(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "left"   { return CSS_POS_LEFT; }
        "center" { return CSS_POS_CENTER; }
        "right"  { return CSS_POS_RIGHT; }
        "top"    { return CSS_POS_TOP; }
        "bottom" { return CSS_POS_BOTTOM; }
        *        { return CSS_POS_UNKNOWN; }
    */
}

// ============================================================================
// Structural Pseudo-class Parser
// ============================================================================

// Structural pseudo-class values
#define CSS_PSEUDO_UNKNOWN 0
#define CSS_PSEUDO_FIRST_CHILD 1
#define CSS_PSEUDO_LAST_CHILD 2
#define CSS_PSEUDO_ONLY_CHILD 3
#define CSS_PSEUDO_NTH_CHILD 4
#define CSS_PSEUDO_NTH_LAST_CHILD 5
#define CSS_PSEUDO_FIRST_OF_TYPE 6
#define CSS_PSEUDO_LAST_OF_TYPE 7
#define CSS_PSEUDO_ONLY_OF_TYPE 8

int css_parse_structural_pseudo(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "first-child"    { return CSS_PSEUDO_FIRST_CHILD; }
        "last-child"     { return CSS_PSEUDO_LAST_CHILD; }
        "only-child"     { return CSS_PSEUDO_ONLY_CHILD; }
        "nth-child"      { return CSS_PSEUDO_NTH_CHILD; }
        "nth-last-child" { return CSS_PSEUDO_NTH_LAST_CHILD; }
        "first-of-type"  { return CSS_PSEUDO_FIRST_OF_TYPE; }
        "last-of-type"   { return CSS_PSEUDO_LAST_OF_TYPE; }
        "only-of-type"   { return CSS_PSEUDO_ONLY_OF_TYPE; }
        *                { return CSS_PSEUDO_UNKNOWN; }
    */
}

// ============================================================================
// Nth-child Keyword Parser (odd, even)
// ============================================================================

#define CSS_NTH_UNKNOWN 0
#define CSS_NTH_ODD 1
#define CSS_NTH_EVEN 2

int css_parse_nth_keyword(const char* str, size_t len) {
    const char* YYCURSOR = str;
    const char* YYLIMIT = str + len;
    const char* YYMARKER;
    
    /*!re2c
        re2c:define:YYCTYPE = "unsigned char";
        re2c:yyfill:enable = 0;
        re2c:flags:case-insensitive = 1;
        
        "odd"  { return CSS_NTH_ODD; }
        "even" { return CSS_NTH_EVEN; }
        *      { return CSS_NTH_UNKNOWN; }
    */
}
