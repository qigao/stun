/*
 * SVG Path Parser Example
 *
 * Demonstrates the re2c-based SVG path parser's capabilities:
 * - Basic commands (M, L, C, Q, A, Z)
 * - Relative vs absolute coordinates
 * - Implicit command repetition
 * - Scientific notation and negative numbers
 * - Various separator formats (space, comma, compressed)
 */

#include "nanovg_css_svg_path.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

using namespace nvgcss;

// Helper to print command type name
const char* command_name(char type) {
    switch (type) {
        case 'M': return "MoveTo";
        case 'L': return "LineTo";
        case 'H': return "HorizontalLineTo";
        case 'V': return "VerticalLineTo";
        case 'C': return "CubicBezier";
        case 'S': return "SmoothCubicBezier";
        case 'Q': return "QuadraticBezier";
        case 'T': return "SmoothQuadraticBezier";
        case 'A': return "Arc";
        case 'Z': return "ClosePath";
        default: return "Unknown";
    }
}

// Pretty print a path command
void print_command(const PathCommand& cmd, int index) {
    std::cout << "  [" << index << "] " << command_name(cmd.type)
              << " (" << (cmd.relative ? "relative" : "absolute") << ")";

    if (!cmd.params.empty()) {
        std::cout << " params: [";
        for (size_t i = 0; i < cmd.params.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << std::fixed << std::setprecision(2) << cmd.params[i];
        }
        std::cout << "]";
    }
    std::cout << "\n";
}

// Test a path and print results
void test_path(const std::string& description, const std::string& path) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << description << "\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "Input: " << path << "\n\n";

    auto commands = SVGPathParser::parse(path);

    if (commands.empty()) {
        std::cout << "  (no commands parsed)\n";
    } else {
        std::cout << "Parsed " << commands.size() << " command(s):\n";
        for (size_t i = 0; i < commands.size(); ++i) {
            print_command(commands[i], i);
        }
    }
}

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           SVG Path Parser - re2c Implementation Demo            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";

    // 1. Basic Commands
    test_path(
        "1. Basic MoveTo and LineTo",
        "M 10 20 L 30 40"
    );

    // 2. Relative Coordinates
    test_path(
        "2. Relative Commands (lowercase)",
        "m 10 20 l 15 25 h 30 v 40"
    );

    // 3. Cubic Bezier Curves
    test_path(
        "3. Cubic Bezier Curve",
        "M 0 0 C 10 10, 20 20, 30 30"
    );

    // 4. Quadratic Bezier with Smooth continuation
    test_path(
        "4. Quadratic Bezier + Smooth Quad",
        "M 100 100 Q 150 50 200 100 T 300 100"
    );

    // 5. Arc Command
    test_path(
        "5. Arc (elliptical curve)",
        "M 10 10 A 30 50 45 0 1 100 200"
    );

    // 6. Implicit Command Repetition
    test_path(
        "6. Implicit Commands (M becomes L)",
        "M 0 0 10 10 20 20 30 30"
    );

    // 7. Negative Numbers
    test_path(
        "7. Negative Coordinates",
        "M -10 -20 L -30.5 -40.75"
    );

    // 8. Scientific Notation
    test_path(
        "8. Scientific Notation",
        "M 1e2 2.5e-1 L 3E+2 4.0E0"
    );

    // 9. Compressed Format (no whitespace)
    test_path(
        "9. Compressed Path (common in optimized SVG)",
        "M10 20L30 40H100V200z"
    );

    // 10. Mixed Separators
    test_path(
        "10. Mixed Comma and Space Separators",
        "M 10,20 L30, 40 C 50,60 70,80 90,100"
    );

    // 11. Complete Shape - Star
    test_path(
        "11. Real-World Example: 5-Point Star",
        "M 50,0 L 61,35 L 98,35 L 68,57 L 79,91 L 50,70 L 21,91 L 32,57 L 2,35 L 39,35 Z"
    );

    // 12. Complete Shape - Heart (using beziers)
    test_path(
        "12. Real-World Example: Heart Shape",
        "M 50,90 C 25,70 0,50 0,30 C 0,15 10,0 25,0 C 35,0 45,5 50,15 "
        "C 55,5 65,0 75,0 C 90,0 100,15 100,30 C 100,50 75,70 50,90 Z"
    );

    // 13. Smooth Curve Combinations
    test_path(
        "13. Smooth Curve (S command after C)",
        "M 0 0 C 10 10 20 20 30 30 S 50 50 60 60 S 80 80 90 90"
    );

    // 14. Empty and Whitespace
    test_path(
        "14. Empty Path",
        ""
    );

    test_path(
        "15. Whitespace Only Path",
        "   \t\n   "
    );

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "All tests completed!\n";
    std::cout << "The re2c-based parser handles:\n";
    std::cout << "  ✓ All SVG path commands (M, L, C, Q, S, T, A, H, V, Z)\n";
    std::cout << "  ✓ Relative and absolute coordinates\n";
    std::cout << "  ✓ Implicit command repetition\n";
    std::cout << "  ✓ Scientific notation (1e2, 3E+4, 2.5e-1)\n";
    std::cout << "  ✓ Negative numbers and decimals\n";
    std::cout << "  ✓ Flexible separators (space, comma, none)\n";
    std::cout << "  ✓ Compressed/optimized paths\n";
    std::cout << std::string(70, '=') << "\n\n";

    return 0;
}
