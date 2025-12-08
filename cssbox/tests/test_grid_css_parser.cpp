/**
 * CSS Grid Parser Unit Tests
 *
 * Tests parsing of CSS grid functions:
 * - GridSizeValue: px, fr, %, auto, min-content, max-content
 * - GridTrack: simple values + minmax(), fit-content()
 * - Track lists: multiple tracks + repeat()
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "cssbox_conversion.h"

using Catch::Matchers::WithinAbs;
using namespace cssbox;
using namespace cssbox::convert;

// ============================================================================
// GridSizeValue Parsing Tests
// ============================================================================

TEST_CASE("parse_grid_size_value - basic values", "[grid][parser]") {
    SECTION("pixels") {
        auto v = parse_grid_size_value("100px");
        REQUIRE(v.type == GridSize::PX);
        REQUIRE_THAT(v.value, WithinAbs(100.0f, 0.1f));
    }

    SECTION("pixels with decimal") {
        auto v = parse_grid_size_value("50.5px");
        REQUIRE(v.type == GridSize::PX);
        REQUIRE_THAT(v.value, WithinAbs(50.5f, 0.1f));
    }

    SECTION("fractional units") {
        auto v = parse_grid_size_value("1fr");
        REQUIRE(v.type == GridSize::FR);
        REQUIRE_THAT(v.value, WithinAbs(1.0f, 0.1f));
    }

    SECTION("fractional with decimal") {
        auto v = parse_grid_size_value("2.5fr");
        REQUIRE(v.type == GridSize::FR);
        REQUIRE_THAT(v.value, WithinAbs(2.5f, 0.1f));
    }

    SECTION("percentage") {
        auto v = parse_grid_size_value("50%");
        REQUIRE(v.type == GridSize::PERCENT);
        REQUIRE_THAT(v.value, WithinAbs(50.0f, 0.1f));
    }

    SECTION("auto") {
        auto v = parse_grid_size_value("auto");
        REQUIRE(v.type == GridSize::AUTO);
    }

    SECTION("min-content") {
        auto v = parse_grid_size_value("min-content");
        REQUIRE(v.type == GridSize::MIN_CONTENT);
    }

    SECTION("max-content") {
        auto v = parse_grid_size_value("max-content");
        REQUIRE(v.type == GridSize::MAX_CONTENT);
    }

    SECTION("whitespace handling") {
        auto v = parse_grid_size_value("  100px  ");
        REQUIRE(v.type == GridSize::PX);
        REQUIRE_THAT(v.value, WithinAbs(100.0f, 0.1f));
    }

    SECTION("empty string returns auto") {
        auto v = parse_grid_size_value("");
        REQUIRE(v.type == GridSize::AUTO);
    }
}

// ============================================================================
// Single Track Parsing Tests
// ============================================================================

TEST_CASE("parse_single_track - simple values", "[grid][parser]") {
    SECTION("pixels") {
        auto t = parse_single_track("200px");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::PX);
        REQUIRE_THAT(t->value, WithinAbs(200.0f, 0.1f));
    }

    SECTION("fractional") {
        auto t = parse_single_track("1fr");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::FR);
        REQUIRE_THAT(t->value, WithinAbs(1.0f, 0.1f));
    }

    SECTION("auto") {
        auto t = parse_single_track("auto");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::AUTO);
    }

    SECTION("min-content") {
        auto t = parse_single_track("min-content");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::MIN_CONTENT);
    }

    SECTION("max-content") {
        auto t = parse_single_track("max-content");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::MAX_CONTENT);
    }
}

TEST_CASE("parse_single_track - minmax()", "[grid][parser]") {
    SECTION("minmax with px values") {
        auto t = parse_single_track("minmax(100px, 200px)");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::MINMAX);
        REQUIRE(t->min_size.type == GridSize::PX);
        REQUIRE_THAT(t->min_size.value, WithinAbs(100.0f, 0.1f));
        REQUIRE(t->max_size.type == GridSize::PX);
        REQUIRE_THAT(t->max_size.value, WithinAbs(200.0f, 0.1f));
    }

    SECTION("minmax with px and fr") {
        auto t = parse_single_track("minmax(100px, 1fr)");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::MINMAX);
        REQUIRE(t->min_size.type == GridSize::PX);
        REQUIRE_THAT(t->min_size.value, WithinAbs(100.0f, 0.1f));
        REQUIRE(t->max_size.type == GridSize::FR);
        REQUIRE_THAT(t->max_size.value, WithinAbs(1.0f, 0.1f));
    }

    SECTION("minmax with min-content and max-content") {
        auto t = parse_single_track("minmax(min-content, max-content)");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::MINMAX);
        REQUIRE(t->min_size.type == GridSize::MIN_CONTENT);
        REQUIRE(t->max_size.type == GridSize::MAX_CONTENT);
    }

    SECTION("minmax with auto") {
        auto t = parse_single_track("minmax(auto, auto)");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::MINMAX);
        REQUIRE(t->min_size.type == GridSize::AUTO);
        REQUIRE(t->max_size.type == GridSize::AUTO);
    }
}

TEST_CASE("parse_single_track - fit-content()", "[grid][parser]") {
    SECTION("fit-content with px") {
        auto t = parse_single_track("fit-content(200px)");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::FIT_CONTENT);
        REQUIRE_THAT(t->value, WithinAbs(200.0f, 0.1f));
    }

    SECTION("fit-content with large value") {
        auto t = parse_single_track("fit-content(500px)");
        REQUIRE(t.has_value());
        REQUIRE(t->type == GridTrack::Type::FIT_CONTENT);
        REQUIRE_THAT(t->value, WithinAbs(500.0f, 0.1f));
    }
}

// ============================================================================
// Track List Parsing Tests
// ============================================================================

TEST_CASE("parse_grid_track_list - simple lists", "[grid][parser]") {
    SECTION("single track") {
        auto tracks = parse_grid_track_list("100px");
        REQUIRE(tracks.size() == 1);
        REQUIRE(tracks[0].type == GridTrack::Type::PX);
        REQUIRE_THAT(tracks[0].value, WithinAbs(100.0f, 0.1f));
    }

    SECTION("multiple px tracks") {
        auto tracks = parse_grid_track_list("100px 200px 300px");
        REQUIRE(tracks.size() == 3);
        REQUIRE_THAT(tracks[0].value, WithinAbs(100.0f, 0.1f));
        REQUIRE_THAT(tracks[1].value, WithinAbs(200.0f, 0.1f));
        REQUIRE_THAT(tracks[2].value, WithinAbs(300.0f, 0.1f));
    }

    SECTION("mixed tracks") {
        auto tracks = parse_grid_track_list("100px 1fr auto");
        REQUIRE(tracks.size() == 3);
        REQUIRE(tracks[0].type == GridTrack::Type::PX);
        REQUIRE(tracks[1].type == GridTrack::Type::FR);
        REQUIRE(tracks[2].type == GridTrack::Type::AUTO);
    }

    SECTION("fr tracks") {
        auto tracks = parse_grid_track_list("1fr 2fr 1fr");
        REQUIRE(tracks.size() == 3);
        REQUIRE_THAT(tracks[0].value, WithinAbs(1.0f, 0.1f));
        REQUIRE_THAT(tracks[1].value, WithinAbs(2.0f, 0.1f));
        REQUIRE_THAT(tracks[2].value, WithinAbs(1.0f, 0.1f));
    }
}

TEST_CASE("parse_grid_track_list - with minmax()", "[grid][parser]") {
    SECTION("single minmax") {
        auto tracks = parse_grid_track_list("minmax(100px, 1fr)");
        REQUIRE(tracks.size() == 1);
        REQUIRE(tracks[0].type == GridTrack::Type::MINMAX);
    }

    SECTION("minmax with other tracks") {
        auto tracks = parse_grid_track_list("100px minmax(200px, 1fr) auto");
        REQUIRE(tracks.size() == 3);
        REQUIRE(tracks[0].type == GridTrack::Type::PX);
        REQUIRE(tracks[1].type == GridTrack::Type::MINMAX);
        REQUIRE(tracks[2].type == GridTrack::Type::AUTO);
    }

    SECTION("multiple minmax") {
        auto tracks = parse_grid_track_list("minmax(100px, 1fr) minmax(200px, 2fr)");
        REQUIRE(tracks.size() == 2);
        REQUIRE(tracks[0].type == GridTrack::Type::MINMAX);
        REQUIRE(tracks[1].type == GridTrack::Type::MINMAX);
    }
}

TEST_CASE("parse_grid_track_list - repeat()", "[grid][parser]") {
    SECTION("repeat with integer count") {
        auto tracks = parse_grid_track_list("repeat(3, 1fr)");
        REQUIRE(tracks.size() == 3);
        for (const auto& t : tracks) {
            REQUIRE(t.type == GridTrack::Type::FR);
            REQUIRE_THAT(t.value, WithinAbs(1.0f, 0.1f));
        }
    }

    SECTION("repeat with px") {
        auto tracks = parse_grid_track_list("repeat(4, 100px)");
        REQUIRE(tracks.size() == 4);
        for (const auto& t : tracks) {
            REQUIRE(t.type == GridTrack::Type::PX);
            REQUIRE_THAT(t.value, WithinAbs(100.0f, 0.1f));
        }
    }

    SECTION("repeat with multiple tracks") {
        auto tracks = parse_grid_track_list("repeat(2, 100px 1fr)");
        REQUIRE(tracks.size() == 4);
        REQUIRE(tracks[0].type == GridTrack::Type::PX);
        REQUIRE(tracks[1].type == GridTrack::Type::FR);
        REQUIRE(tracks[2].type == GridTrack::Type::PX);
        REQUIRE(tracks[3].type == GridTrack::Type::FR);
    }

    SECTION("repeat with other tracks") {
        auto tracks = parse_grid_track_list("200px repeat(2, 1fr) 200px");
        REQUIRE(tracks.size() == 4);
        REQUIRE(tracks[0].type == GridTrack::Type::PX);
        REQUIRE(tracks[1].type == GridTrack::Type::FR);
        REQUIRE(tracks[2].type == GridTrack::Type::FR);
        REQUIRE(tracks[3].type == GridTrack::Type::PX);
    }

    SECTION("repeat with minmax") {
        auto tracks = parse_grid_track_list("repeat(3, minmax(100px, 1fr))");
        REQUIRE(tracks.size() == 3);
        for (const auto& t : tracks) {
            REQUIRE(t.type == GridTrack::Type::MINMAX);
        }
    }
}

TEST_CASE("parse_grid_track_list - auto-fill/auto-fit", "[grid][parser]") {
    SECTION("auto-fill with minmax") {
        auto tracks = parse_grid_track_list("repeat(auto-fill, minmax(200px, 1fr))");
        // For now, auto-fill just returns the inner tracks (layout-time expansion)
        REQUIRE(tracks.size() >= 1);
        REQUIRE(tracks[0].type == GridTrack::Type::MINMAX);
    }

    SECTION("auto-fit with minmax") {
        auto tracks = parse_grid_track_list("repeat(auto-fit, minmax(150px, 1fr))");
        REQUIRE(tracks.size() >= 1);
        REQUIRE(tracks[0].type == GridTrack::Type::MINMAX);
    }
}

TEST_CASE("parse_grid_track_list - complex patterns", "[grid][parser]") {
    SECTION("responsive 12-column grid") {
        auto tracks = parse_grid_track_list("repeat(12, 1fr)");
        REQUIRE(tracks.size() == 12);
    }

    SECTION("holy grail layout") {
        auto tracks = parse_grid_track_list("200px 1fr 200px");
        REQUIRE(tracks.size() == 3);
        REQUIRE(tracks[0].type == GridTrack::Type::PX);
        REQUIRE(tracks[1].type == GridTrack::Type::FR);
        REQUIRE(tracks[2].type == GridTrack::Type::PX);
    }

    SECTION("card layout with gap simulation") {
        auto tracks = parse_grid_track_list("repeat(3, minmax(250px, 1fr))");
        REQUIRE(tracks.size() == 3);
    }

    SECTION("mixed content and intrinsic") {
        auto tracks = parse_grid_track_list("min-content 1fr max-content");
        REQUIRE(tracks.size() == 3);
        REQUIRE(tracks[0].type == GridTrack::Type::MIN_CONTENT);
        REQUIRE(tracks[1].type == GridTrack::Type::FR);
        REQUIRE(tracks[2].type == GridTrack::Type::MAX_CONTENT);
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("parse_grid_track_list - edge cases", "[grid][parser]") {
    SECTION("empty string") {
        auto tracks = parse_grid_track_list("");
        REQUIRE(tracks.empty());
    }

    SECTION("whitespace only") {
        auto tracks = parse_grid_track_list("   ");
        REQUIRE(tracks.empty());
    }

    SECTION("extra whitespace between tracks") {
        auto tracks = parse_grid_track_list("100px    200px     300px");
        REQUIRE(tracks.size() == 3);
    }

    SECTION("bare fr is invalid - returns empty") {
        // Note: "fr" alone is not valid CSS, must be "1fr"
        auto tracks = parse_grid_track_list("fr");
        REQUIRE(tracks.empty());
    }
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST_CASE("find_matching_paren", "[grid][parser]") {
    SECTION("simple parentheses") {
        std::string_view s = "minmax(100px, 1fr)";
        size_t open = s.find('(');
        size_t close = find_matching_paren(s, open);
        REQUIRE(close == s.size() - 1);
    }

    SECTION("nested parentheses") {
        std::string_view s = "repeat(3, minmax(100px, 1fr))";
        size_t open = s.find('(');
        size_t close = find_matching_paren(s, open);
        REQUIRE(close == s.size() - 1);
    }

    SECTION("no opening paren") {
        std::string_view s = "100px";
        size_t close = find_matching_paren(s, 0);
        REQUIRE(close == std::string_view::npos);
    }

    SECTION("unmatched paren") {
        std::string_view s = "minmax(100px, 1fr";
        size_t open = s.find('(');
        size_t close = find_matching_paren(s, open);
        REQUIRE(close == std::string_view::npos);
    }
}
