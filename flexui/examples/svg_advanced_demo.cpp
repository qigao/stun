/*
 * FlexUI Advanced SVG Demo
 *
 * Demonstrates all advanced SVG features in FlexUI:
 * - SVG Path with d attribute (re2c parser)
 * - Text-on-path
 * - SVG Patterns
 * - SVG Markers
 * - Clipping paths
 */

#include <flexui.h>
#include <nanovg_css.h>
#include <iostream>

int main() {
    flexui::Screen screen(1200, 800, "FlexUI - Advanced SVG Features");

    // Load CSS for styling
    screen.loadCSS(R"(
        .label {
            font-size: 18px;
            font-weight: bold;
            color: #2c3e50;
        }

        .path-demo {
            fill: none;
            stroke: #3498db;
            stroke-width: 3px;
        }

        .pattern-rect {
            stroke: #2c3e50;
            stroke-width: 2px;
        }
    )");

    // ========================================================================
    // SECTION 1: SVG Path with 'd' attribute (using re2c parser)
    // ========================================================================

    auto* pathLabel = screen.addWidget("path_label", "div");
    pathLabel->setPosition(50, 20);
    pathLabel->setText("1. SVG Path (d attribute with re2c parser)");
    pathLabel->setInlineStyle("class", "label");

    // Create a path using SVG path string
    auto* svgPath = screen.createPath("svg_path_1");
    svgPath->setInlineStyle("d", "M 50 100 Q 150 50 250 100 T 450 100");
    svgPath->setInlineStyle("stroke", "#3498db");
    svgPath->setInlineStyle("stroke-width", "3px");
    svgPath->setInlineStyle("fill", "none");

    // Star path (compressed format)
    auto* starPath = screen.createPath("star");
    starPath->setInlineStyle("d", "M100,200 L115,245 L162,245 L125,272 L140,318 L100,290 L60,318 L75,272 L38,245 L85,245 Z");
    starPath->setInlineStyle("fill", "#f39c12");
    starPath->setInlineStyle("stroke", "#e67e22");
    starPath->setInlineStyle("stroke-width", "2px");

    // ========================================================================
    // SECTION 2: SVG Patterns
    // ========================================================================

    auto* patternLabel = screen.addWidget("pattern_label", "div");
    patternLabel->setPosition(650, 20);
    patternLabel->setText("2. SVG Patterns");
    patternLabel->setInlineStyle("class", "label");

    // Create a dots pattern
    auto* renderer = screen.renderer();
    auto* dotsPattern = nvgcssCreatePattern(renderer, "dots", 0, 0, 20, 20);
    auto* dot = nvgcssCreateElement(renderer, "pattern_dot", "circle");
    dot->inline_style["cx"] = "10px";
    dot->inline_style["cy"] = "10px";
    dot->inline_style["r"] = "4px";
    dot->inline_style["fill"] = "#4a90e2";
    dot->inline_style["width"] = "20px";
    dot->inline_style["height"] = "20px";
    nvgcssAppendChild(renderer, dotsPattern, dot);

    // Rectangle filled with pattern
    auto* patternRect = screen.createRect("pattern_rect", 650, 50, 200, 150);
    patternRect->setInlineStyle("fill", "url(#dots)");
    patternRect->setInlineStyle("stroke", "#2c3e50");
    patternRect->setInlineStyle("stroke-width", "2px");

    // ========================================================================
    // SECTION 3: Text-on-Path
    // ========================================================================

    auto* textPathLabel = screen.addWidget("textpath_label", "div");
    textPathLabel->setPosition(50, 350);
    textPathLabel->setText("3. Text-on-Path");
    textPathLabel->setInlineStyle("class", "label");

    // Create the path for text
    auto* curvePath = screen.createPath("curve_for_text");
    curvePath->setInlineStyle("d", "M 50 450 Q 300 350 550 450");
    curvePath->setInlineStyle("stroke", "#95a5a6");
    curvePath->setInlineStyle("stroke-width", "2px");
    curvePath->setInlineStyle("fill", "none");
    curvePath->setInlineStyle("stroke-dasharray", "5 3");

    // Create text-on-path element
    auto* textPath = screen.addWidget("text_path_demo", "textPath");
    textPath->setInlineStyle("href", "#curve_for_text");
    textPath->setText("Text flowing along a curved path!");
    textPath->setInlineStyle("fill", "#2c3e50");
    textPath->setInlineStyle("font-size", "24px");

    // ========================================================================
    // SECTION 4: SVG Markers
    // ========================================================================

    auto* markerLabel = screen.addWidget("marker_label", "div");
    markerLabel->setPosition(650, 350);
    markerLabel->setText("4. SVG Markers");
    markerLabel->setInlineStyle("class", "label");

    // Create arrow marker
    auto* arrowMarker = nvgcssCreateMarker(renderer, "arrow", 10, 10, 5, 5, "auto");
    auto* arrowPath = nvgcssCreateElement(renderer, "arrow_shape", "path");
    arrowPath->inline_style["d"] = "M 0 0 L 10 5 L 0 10 Z";
    arrowPath->inline_style["fill"] = "#27ae60";
    nvgcssAppendChild(renderer, arrowMarker, arrowPath);

    // Create dot marker
    auto* dotMarker = nvgcssCreateMarker(renderer, "dot_marker", 8, 8, 4, 4, "0");
    auto* markerCircle = nvgcssCreateElement(renderer, "marker_dot_shape", "circle");
    markerCircle->inline_style["cx"] = "4px";
    markerCircle->inline_style["cy"] = "4px";
    markerCircle->inline_style["r"] = "3px";
    markerCircle->inline_style["fill"] = "#e74c3c";
    markerCircle->inline_style["width"] = "8px";
    markerCircle->inline_style["height"] = "8px";
    nvgcssAppendChild(renderer, dotMarker, markerCircle);

    // Path with markers
    auto* markerPath = screen.createPath("marker_demo_path");
    markerPath->setInlineStyle("d", "M 650 450 L 750 450 L 800 500 L 850 450");
    markerPath->setInlineStyle("stroke", "#27ae60");
    markerPath->setInlineStyle("stroke-width", "3px");
    markerPath->setInlineStyle("fill", "none");
    markerPath->setInlineStyle("marker-start", "url(#dot_marker)");
    markerPath->setInlineStyle("marker-mid", "url(#dot_marker)");
    markerPath->setInlineStyle("marker-end", "url(#arrow)");

    // ========================================================================
    // SECTION 5: Clipping Paths
    // ========================================================================

    auto* clipLabel = screen.addWidget("clip_label", "div");
    clipLabel->setPosition(650, 550);
    clipLabel->setText("5. Clipping Paths");
    clipLabel->setInlineStyle("class", "label");

    // Create clip path (star shape)
    auto* clipPath = nvgcssCreateClipPath(renderer, "star_clip");
    auto* clipShape = nvgcssCreateElement(renderer, "clip_star_shape", "path");
    clipShape->inline_style["d"] = "M 750,620 L 765,665 L 812,665 L 775,692 L 790,738 L 750,710 L 710,738 L 725,692 L 688,665 L 735,665 Z";
    nvgcssAppendChild(renderer, clipPath, clipShape);

    // Circle clipped by star
    auto* clippedCircle = screen.createCircle("clipped_circle", 750, 680, 60);
    clippedCircle->setInlineStyle("fill", "#9b59b6");
    clippedCircle->setInlineStyle("clip-path", "url(#star_clip)");

    // ========================================================================
    // SECTION 6: Complex SVG Path (Heart shape using beziers)
    // ========================================================================

    auto* heartPath = screen.createPath("heart");
    heartPath->setInlineStyle("d",
        "M 350,650 "
        "C 325,630 300,610 300,590 "
        "C 300,575 310,560 325,560 "
        "C 335,560 345,565 350,575 "
        "C 355,565 365,560 375,560 "
        "C 390,560 400,575 400,590 "
        "C 400,610 375,630 350,650 Z"
    );
    heartPath->setInlineStyle("fill", "#e74c3c");
    heartPath->setInlineStyle("stroke", "#c0392b");
    heartPath->setInlineStyle("stroke-width", "2px");

    auto* heartLabel = screen.addWidget("heart_label", "div");
    heartLabel->setPosition(310, 700);
    heartLabel->setText("Heart (Bezier curves)");
    heartLabel->setInlineStyle("font-size", "14px");

    // ========================================================================
    // Instructions
    // ========================================================================

    auto* instructions = screen.addWidget("instructions", "div");
    instructions->setPosition(50, 750);
    instructions->setText("All SVG features use the re2c-based path parser (2.6x faster!)");
    instructions->setInlineStyle("font-size", "16px");
    instructions->setInlineStyle("color", "#7f8c8d");
    instructions->setInlineStyle("font-style", "italic");

    std::cout << "FlexUI Advanced SVG Demo\n";
    std::cout << "========================\n";
    std::cout << "Features demonstrated:\n";
    std::cout << "1. SVG Path with 'd' attribute (re2c parser)\n";
    std::cout << "2. SVG Patterns (dots pattern)\n";
    std::cout << "3. Text-on-Path (curved text)\n";
    std::cout << "4. SVG Markers (arrows and dots)\n";
    std::cout << "5. Clipping Paths (star clip)\n";
    std::cout << "6. Complex paths (heart shape with beziers)\n";
    std::cout << "\nPress ESC or close window to exit\n";

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
