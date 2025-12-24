/*
 * Flex DSL Lexer & Parser Tests
 * Comprehensive test suite using Catch2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flex.h"

using namespace flex;
using namespace flex::parser;
using Catch::Matchers::ContainsSubstring;

// ============================================================================
// LEXER TESTS
// ============================================================================

TEST_CASE("Lexer: Basic tokens", "[lexer]") {
  SECTION("Keywords") {
    auto lexer = lexer_create("scene artboard group rect circle text");

    REQUIRE(lex_next_token(lexer).type == TOK_SCENE);
    REQUIRE(lex_next_token(lexer).type == TOK_ARTBOARD);
    REQUIRE(lex_next_token(lexer).type == TOK_NODE_TYPE); // group
    REQUIRE(lex_next_token(lexer).type == TOK_NODE_TYPE); // rect
    REQUIRE(lex_next_token(lexer).type == TOK_NODE_TYPE); // circle
    REQUIRE(lex_next_token(lexer).type == TOK_NODE_TYPE); // text
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  SECTION("Animation keywords") {
    auto lexer = lexer_create("anim track keyframe");

    REQUIRE(lex_next_token(lexer).type == TOK_ANIM);
    REQUIRE(lex_next_token(lexer).type == TOK_TRACK);
    REQUIRE(lex_next_token(lexer).type == TOK_KEYFRAME);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  SECTION("State machine keywords") {
    auto lexer = lexer_create("machine layer state transition when");

    REQUIRE(lex_next_token(lexer).type == TOK_MACHINE);
    REQUIRE(lex_next_token(lexer).type == TOK_LAYER);
    REQUIRE(lex_next_token(lexer).type == TOK_STATE);
    REQUIRE(lex_next_token(lexer).type == TOK_TRANSITION);
    REQUIRE(lex_next_token(lexer).type == TOK_WHEN);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  SECTION("Punctuation") {
    auto lexer = lexer_create("{ } : , ->");

    REQUIRE(lex_next_token(lexer).type == TOK_LBRACE);
    REQUIRE(lex_next_token(lexer).type == TOK_RBRACE);
    REQUIRE(lex_next_token(lexer).type == TOK_COLON);
    REQUIRE(lex_next_token(lexer).type == TOK_COMMA);
    REQUIRE(lex_next_token(lexer).type == TOK_ARROW);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  SECTION("Operators") {
    auto lexer = lexer_create("> < == !=");

    REQUIRE(lex_next_token(lexer).type == TOK_GT);
    REQUIRE(lex_next_token(lexer).type == TOK_LT);
    REQUIRE(lex_next_token(lexer).type == TOK_EQ);
    REQUIRE(lex_next_token(lexer).type == TOK_NEQ);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }
}

TEST_CASE("Lexer: Literals", "[lexer]") {
  SECTION("Numbers") {
    auto lexer = lexer_create("123 45.67 0.5 100");

    auto tok1 = lex_next_token(lexer);
    REQUIRE(tok1.type == TOK_NUMBER);
    REQUIRE(tok1.value == "123");

    auto tok2 = lex_next_token(lexer);
    REQUIRE(tok2.type == TOK_NUMBER);
    REQUIRE(tok2.value == "45.67");

    auto tok3 = lex_next_token(lexer);
    REQUIRE(tok3.type == TOK_NUMBER);
    REQUIRE(tok3.value == "0.5");

    auto tok4 = lex_next_token(lexer);
    REQUIRE(tok4.type == TOK_NUMBER);
    REQUIRE(tok4.value == "100");

    lexer_destroy(lexer);
  }

  SECTION("Strings") {
    auto lexer = lexer_create("\"hello\" \"world with spaces\"");

    auto tok1 = lex_next_token(lexer);
    REQUIRE(tok1.type == TOK_STRING);
    REQUIRE(tok1.value == "hello");

    auto tok2 = lex_next_token(lexer);
    REQUIRE(tok2.type == TOK_STRING);
    REQUIRE(tok2.value == "world with spaces");

    lexer_destroy(lexer);
  }

  SECTION("Colors") {
    auto lexer = lexer_create("#FF0000 #00ff00 #0000FF88 #abc");

    auto tok1 = lex_next_token(lexer);
    REQUIRE(tok1.type == TOK_COLOR);
    REQUIRE(tok1.value == "#FF0000");

    auto tok2 = lex_next_token(lexer);
    REQUIRE(tok2.type == TOK_COLOR);
    REQUIRE(tok2.value == "#00ff00");

    auto tok3 = lex_next_token(lexer);
    REQUIRE(tok3.type == TOK_COLOR);
    REQUIRE(tok3.value == "#0000FF88");

    auto tok4 = lex_next_token(lexer);
    REQUIRE(tok4.type == TOK_COLOR);
    REQUIRE(tok4.value == "#abc");

    lexer_destroy(lexer);
  }

  SECTION("Booleans") {
    auto lexer = lexer_create("true false");

    auto tok1 = lex_next_token(lexer);
    REQUIRE(tok1.type == TOK_BOOL);
    REQUIRE(tok1.value == "true");

    auto tok2 = lex_next_token(lexer);
    REQUIRE(tok2.type == TOK_BOOL);
    REQUIRE(tok2.value == "false");

    lexer_destroy(lexer);
  }

  SECTION("Identifiers") {
    auto lexer = lexer_create("myNode _private camelCase snake_case");

    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);
    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);
    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);
    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);

    lexer_destroy(lexer);
  }
}

TEST_CASE("Lexer: Comments", "[lexer]") {
  SECTION("Line comments") {
    auto lexer = lexer_create("scene // this is a comment\nTest {}");

    REQUIRE(lex_next_token(lexer).type == TOK_SCENE);
    auto tok = lex_next_token(lexer);
    REQUIRE(tok.type == TOK_IDENTIFIER);
    REQUIRE(tok.value == "Test");

    lexer_destroy(lexer);
  }
}

TEST_CASE("Lexer: Line and column tracking", "[lexer]") {
  auto lexer = lexer_create("scene\nTest {\n  x: 100\n}");

  auto tok1 = lex_next_token(lexer);
  REQUIRE(tok1.line == 1);

  auto tok2 = lex_next_token(lexer);
  REQUIRE(tok2.line == 2);
  REQUIRE(tok2.value == "Test");

  lexer_destroy(lexer);
}

// ============================================================================
// PARSER TESTS: SCENE
// ============================================================================

TEST_CASE("Parser: Empty scene", "[parser][scene]") {
  auto program = parse("scene EmptyScene {}");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "EmptyScene");
  REQUIRE(program->scene->children.empty());
}

TEST_CASE("Parser: Scene with dimensions", "[parser][scene]") {
  auto program = parse(R"(
        scene MyScene {
            width: 1920
            height: 1080
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "MyScene");
  REQUIRE(program->scene->width == 1920.0f);
  REQUIRE(program->scene->height == 1080.0f);
}

TEST_CASE("Parser: Scene with nodes", "[parser][scene]") {
  auto program = parse(R"(
        scene TestScene {
            rect myRect {
                x: 100
                y: 200
                width: 300
                height: 150
            }
            circle myCircle {
                x: 400
                radius: 50
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 2);

  auto &rect = program->scene->children[0];
  REQUIRE(rect->type == "rect");
  REQUIRE(rect->id == "myRect");
  REQUIRE(std::get<float>(rect->properties["x"]) == 100.0f);
  REQUIRE(std::get<float>(rect->properties["y"]) == 200.0f);
  REQUIRE(std::get<float>(rect->properties["width"]) == 300.0f);
  REQUIRE(std::get<float>(rect->properties["height"]) == 150.0f);

  auto &circle = program->scene->children[1];
  REQUIRE(circle->type == "circle");
  REQUIRE(circle->id == "myCircle");
  REQUIRE(std::get<float>(circle->properties["radius"]) == 50.0f);
}

TEST_CASE("Parser: Nested groups", "[parser][scene]") {
  auto program = parse(R"(
        scene NestedScene {
            group container {
                x: 10
                y: 20

                group inner {
                    x: 5

                    rect leaf {
                        width: 50
                    }
                }
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 1);

  auto &container = program->scene->children[0];
  REQUIRE(container->type == "group");
  REQUIRE(container->id == "container");
  REQUIRE(container->children.size() == 1);

  auto &inner = container->children[0];
  REQUIRE(inner->type == "group");
  REQUIRE(inner->id == "inner");
  REQUIRE(inner->children.size() == 1);

  auto &leaf = inner->children[0];
  REQUIRE(leaf->type == "rect");
  REQUIRE(leaf->id == "leaf");
}

TEST_CASE("Parser: Property types", "[parser][scene]") {
  auto program = parse(R"(
        scene PropScene {
            rect test {
                x: 100.5
                fill: #FF0000
                visible: true
                content: "Hello World"
            }
        }
    )");

  REQUIRE(program != nullptr);
  auto &node = program->scene->children[0];

  // Float
  REQUIRE(std::holds_alternative<float>(node->properties["x"]));
  REQUIRE(std::get<float>(node->properties["x"]) == 100.5f);

  // Color (stored as string)
  REQUIRE(std::holds_alternative<std::string>(node->properties["fill"]));
  REQUIRE(std::get<std::string>(node->properties["fill"]) == "#FF0000");

  // Boolean
  REQUIRE(std::holds_alternative<bool>(node->properties["visible"]));
  REQUIRE(std::get<bool>(node->properties["visible"]) == true);

  // String
  REQUIRE(std::holds_alternative<std::string>(node->properties["content"]));
  REQUIRE(std::get<std::string>(node->properties["content"]) == "Hello World");
}

// ============================================================================
// PARSER TESTS: ANIMATION
// ============================================================================

TEST_CASE("Parser: Basic animation", "[parser][anim]") {
  auto program = parse(R"(
        anim "fadeIn" {
            duration: 2.0
            loop: once

            track "opacity" {
                keyframe 0 -> 0.0
                keyframe 2.0 -> 1.0
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->animations.size() == 1);

  auto &anim = program->animations[0];
  REQUIRE(anim->name == "fadeIn");
  REQUIRE(anim->duration == 2.0f);
  REQUIRE(anim->loop_mode == "once");
  REQUIRE(anim->tracks.size() == 1);

  auto &track = anim->tracks[0];
  REQUIRE(track.property == "opacity");
  REQUIRE(track.keyframes.size() == 2);
  REQUIRE(track.keyframes[0].time == 0.0f);
  REQUIRE(std::get<float>(track.keyframes[0].value) == 0.0f);
  REQUIRE(track.keyframes[1].time == 2.0f);
  REQUIRE(std::get<float>(track.keyframes[1].value) == 1.0f);
}

TEST_CASE("Parser: Animation with multiple tracks", "[parser][anim]") {
  auto program = parse(R"(
        anim "moveAndFade" {
            duration: 3.0
            loop: loop

            track "x" {
                keyframe 0 -> 0
                keyframe 1.5 -> 100
                keyframe 3.0 -> 200
            }

            track "opacity" {
                keyframe 0 -> 1.0
                keyframe 3.0 -> 0.0
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->animations.size() == 1);

  auto &anim = program->animations[0];
  REQUIRE(anim->loop_mode == "loop");
  REQUIRE(anim->tracks.size() == 2);

  auto &track_x = anim->tracks[0];
  REQUIRE(track_x.property == "x");
  REQUIRE(track_x.keyframes.size() == 3);

  auto &track_opacity = anim->tracks[1];
  REQUIRE(track_opacity.property == "opacity");
  REQUIRE(track_opacity.keyframes.size() == 2);
}

TEST_CASE("Parser: Animation with path property", "[parser][anim]") {
  auto program = parse(R"(
        anim "updateStatus" {
            duration: 1.0

            track "#statusText/content" {
                keyframe 0 -> "Loading..."
                keyframe 1.0 -> "Complete!"
            }

            track "#statusText/color" {
                keyframe 0 -> #888888
                keyframe 1.0 -> #00FF00
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->animations.size() == 1);

  auto &anim = program->animations[0];
  REQUIRE(anim->tracks.size() == 2);

  auto &track_content = anim->tracks[0];
  REQUIRE(track_content.property == "#statusText/content");
  REQUIRE(std::get<std::string>(track_content.keyframes[0].value) == "Loading...");
  REQUIRE(std::get<std::string>(track_content.keyframes[1].value) == "Complete!");
}

TEST_CASE("Parser: Multiple animations", "[parser][anim]") {
  auto program = parse(R"(
        anim "toPositive" {
            duration: 0.5
            track "color" { keyframe 0 -> #00FF00 }
        }

        anim "toNegative" {
            duration: 0.5
            track "color" { keyframe 0 -> #FF0000 }
        }

        anim "toNeutral" {
            duration: 0.5
            track "color" { keyframe 0 -> #888888 }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->animations.size() == 3);
  REQUIRE(program->animations[0]->name == "toPositive");
  REQUIRE(program->animations[1]->name == "toNegative");
  REQUIRE(program->animations[2]->name == "toNeutral");
}

// ============================================================================
// PARSER TESTS: STATE MACHINE
// ============================================================================

TEST_CASE("Parser: Basic state machine", "[parser][machine]") {
  auto program = parse(R"(
        machine statusTracker {
            layer status {
                state idle {
                    initial: true
                    animation: "toIdle"
                }

                state active {
                    animation: "toActive"
                }

                transition idle -> active when counter > 0
                transition active -> idle when counter < 0.1
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->machines.size() == 1);

  auto &machine = program->machines[0];
  REQUIRE(machine->name == "statusTracker");
  REQUIRE(machine->layers.size() == 1);

  auto &layer = machine->layers[0];
  REQUIRE(layer.name == "status");
  REQUIRE(layer.states.size() == 2);
  REQUIRE(layer.transitions.size() == 2);

  // States
  auto &state_idle = layer.states[0];
  REQUIRE(state_idle.name == "idle");
  REQUIRE(state_idle.initial == true);
  REQUIRE(state_idle.animation == "toIdle");

  auto &state_active = layer.states[1];
  REQUIRE(state_active.name == "active");
  REQUIRE(state_active.initial == false);
  REQUIRE(state_active.animation == "toActive");

  // Transitions
  auto &trans1 = layer.transitions[0];
  REQUIRE(trans1.from_state == "idle");
  REQUIRE(trans1.to_state == "active");
  REQUIRE(trans1.condition_var == "counter");
  REQUIRE(trans1.condition_op == ">");
  REQUIRE(trans1.condition_val == 0.0f);

  auto &trans2 = layer.transitions[1];
  REQUIRE(trans2.from_state == "active");
  REQUIRE(trans2.to_state == "idle");
  REQUIRE(trans2.condition_var == "counter");
  REQUIRE(trans2.condition_op == "<");
  REQUIRE(trans2.condition_val == 0.1f);
}

TEST_CASE("Parser: State machine with multiple layers", "[parser][machine]") {
  auto program = parse(R"(
        machine playerState {
            layer movement {
                state standing { initial: true }
                state walking {}
                state running {}

                transition standing -> walking when speed > 0
                transition walking -> running when speed > 5
                transition running -> walking when speed < 5.1
                transition walking -> standing when speed < 0.1
            }

            layer health {
                state healthy { initial: true }
                state injured {}
                state dead {}

                transition healthy -> injured when hp < 50
                transition injured -> dead when hp < 1
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->machines.size() == 1);

  auto &machine = program->machines[0];
  REQUIRE(machine->layers.size() == 2);

  auto &movement = machine->layers[0];
  REQUIRE(movement.name == "movement");
  REQUIRE(movement.states.size() == 3);
  REQUIRE(movement.transitions.size() == 4);

  auto &health = machine->layers[1];
  REQUIRE(health.name == "health");
  REQUIRE(health.states.size() == 3);
  REQUIRE(health.transitions.size() == 2);
}

TEST_CASE("Parser: Transition operators", "[parser][machine]") {
  auto program = parse(R"(
        machine testOps {
            layer test {
                state a { initial: true }
                state b {}
                state c {}
                state d {}

                transition a -> b when x > 10
                transition b -> c when y < 5
                transition c -> d when z == 0
            }
        }
    )");

  REQUIRE(program != nullptr);
  auto &layer = program->machines[0]->layers[0];

  REQUIRE(layer.transitions[0].condition_op == ">");
  REQUIRE(layer.transitions[1].condition_op == "<");
  REQUIRE(layer.transitions[2].condition_op == "==");
}

// ============================================================================
// PARSER TESTS: COMPONENT
// ============================================================================

TEST_CASE("Parser: Component instantiation", "[parser][component]") {
  auto program = parse(R"(
        scene ComponentDemo {
            Slider volumeSlider {
                x: 100
                y: 50
                value: 0.75
                width: 200
            }

            Button submitBtn {
                x: 100
                y: 150
                label: "Submit"
                color: #0D6EFD
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 2);

  auto &slider = program->scene->children[0];
  REQUIRE(slider->type == "Slider");
  REQUIRE(slider->id == "volumeSlider");
  REQUIRE(std::get<float>(slider->properties["value"]) == 0.75f);

  auto &button = program->scene->children[1];
  REQUIRE(button->type == "Button");
  REQUIRE(button->id == "submitBtn");
  REQUIRE(std::get<std::string>(button->properties["label"]) == "Submit");
}

// ============================================================================
// PARSER TESTS: FULL PROGRAM
// ============================================================================

TEST_CASE("Parser: Complete program", "[parser][integration]") {
  auto program = parse(R"(
        scene CounterApp {
            width: 400
            height: 300

            rect background {
                x: 0, y: 0
                width: 400, height: 300
                fill: #1a1a2e
            }

            group mainLayout {
                x: 0, y: 0
                width: 400, height: 300

                text counterValue {
                    content: "0"
                    fontSize: 48
                    color: #00ff88
                }
            }
        }

        anim "toPositive" {
            duration: 0.1
            track "#counterValue/color" {
                keyframe 0 -> #00ff88
            }
        }

        anim "toNegative" {
            duration: 0.1
            track "#counterValue/color" {
                keyframe 0 -> #ff0000
            }
        }

        machine statusTracker {
            layer status {
                state neutral { initial: true, animation: "toNeutral" }
                state positive { animation: "toPositive" }
                state negative { animation: "toNegative" }

                transition neutral -> positive when counter > 0
                transition positive -> neutral when counter < 0.1
                transition neutral -> negative when counter < 0
                transition negative -> neutral when counter > -0.1
            }
        }
    )");

  REQUIRE(program != nullptr);

  // Scene
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "CounterApp");
  REQUIRE(program->scene->width == 400.0f);
  REQUIRE(program->scene->height == 300.0f);
  REQUIRE(program->scene->children.size() == 2);

  // Animations
  REQUIRE(program->animations.size() == 2);

  // State machine
  REQUIRE(program->machines.size() == 1);
  auto &machine = program->machines[0];
  REQUIRE(machine->layers[0].states.size() == 3);
  REQUIRE(machine->layers[0].transitions.size() == 4);
}

// ============================================================================
// PARSER TESTS: ERROR HANDLING
// ============================================================================

TEST_CASE("Parser: Empty source", "[parser][error]") {
  auto program = parse("");
  REQUIRE(program == nullptr);
  REQUIRE(std::string(get_error()) == "Empty source");
}

TEST_CASE("Parser: loading_animation.flex content", "[parser][example]") {
  // Minimal test case from loading_animation.flex
  auto program = parse(R"(
scene loading {
    width: 800
    height: 600

    rect background {
        x: 0, y: 0
        width: 800, height: 600
        fill: #0f0f23
    }

    group spinner1 {
        x: 200, y: 200

        group rotator {
            x: 0, y: 0
            rotation: 0

            circle arc1 {
                x: 0, y: 0
                radius: 40
                stroke: #00d9ff
                strokeWidth: 6
            }
        }
    }
}

anim "Spin1" {
    duration: 2
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 2 -> 360
    }
}

anim "Pulse2" {
    duration: 1.5
    loop: pingpong

    track "opacity" {
        keyframe 0 -> 1.0
        keyframe 1.5 -> 0.2
    }
}
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "loading");
  REQUIRE(program->animations.size() == 2);
  REQUIRE(program->animations[0]->name == "Spin1");
  REQUIRE(program->animations[1]->name == "Pulse2");
  REQUIRE(program->animations[1]->loop_mode == "pingpong");
}

TEST_CASE("Parser: Null source", "[parser][error]") {
  auto program = parse(nullptr);
  REQUIRE(program == nullptr);
}

// ============================================================================
// PARSER TESTS: EDGE CASES
// ============================================================================

TEST_CASE("Parser: Comma-separated properties", "[parser][edge]") {
  auto program = parse(R"(
        scene Test {
            rect r {
                x: 10, y: 20, width: 100, height: 50
            }
        }
    )");

  REQUIRE(program != nullptr);
  auto &rect = program->scene->children[0];
  REQUIRE(std::get<float>(rect->properties["x"]) == 10.0f);
  REQUIRE(std::get<float>(rect->properties["y"]) == 20.0f);
  REQUIRE(std::get<float>(rect->properties["width"]) == 100.0f);
  REQUIRE(std::get<float>(rect->properties["height"]) == 50.0f);
}

TEST_CASE("Parser: Deeply nested groups", "[parser][edge]") {
  auto program = parse(R"(
        scene Deep {
            group level1 {
                group level2 {
                    group level3 {
                        group level4 {
                            rect leaf { x: 1 }
                        }
                    }
                }
            }
        }
    )");

  REQUIRE(program != nullptr);

  auto *node = program->scene->children[0].get();
  REQUIRE(node->id == "level1");

  node = node->children[0].get();
  REQUIRE(node->id == "level2");

  node = node->children[0].get();
  REQUIRE(node->id == "level3");

  node = node->children[0].get();
  REQUIRE(node->id == "level4");

  node = node->children[0].get();
  REQUIRE(node->id == "leaf");
  REQUIRE(node->type == "rect");
}

TEST_CASE("Parser: Inline child node with properties", "[parser][edge]") {
  // This tests the analog_clock.flex pattern where a group has properties
  // followed by an inline child node on the same line
  auto program = parse(R"(
        scene InlineTest {
            group m12 { rotation: 0, circle c { x: 0, y: -185, radius: 7, fill: #2c2c54 } }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 1);

  auto &group = program->scene->children[0];
  REQUIRE(group->type == "group");
  REQUIRE(group->id == "m12");
  REQUIRE(std::get<float>(group->properties["rotation"]) == 0.0f);

  // Verify the child circle was created
  REQUIRE(group->children.size() == 1);

  auto &circle = group->children[0];
  REQUIRE(circle->type == "circle");
  REQUIRE(circle->id == "c");
  REQUIRE(std::get<float>(circle->properties["x"]) == 0.0f);
  REQUIRE(std::get<float>(circle->properties["y"]) == -185.0f);
  REQUIRE(std::get<float>(circle->properties["radius"]) == 7.0f);
  REQUIRE(std::get<std::string>(circle->properties["fill"]) == "#2c2c54");
}

TEST_CASE("Parser: Multiple inline children (clock markers pattern)", "[parser][edge]") {
  // Tests multiple groups each with inline children - the clock markers pattern
  auto program = parse(R"(
        scene ClockMarkers {
            group markers {
                x: 0, y: 0

                group m12 { rotation: 0,   circle c { x: 0, y: -185, radius: 7, fill: #2c2c54 } }
                group m1  { rotation: 30,  circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
                group m2  { rotation: 60,  circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
                group m3  { rotation: 90,  circle c { x: 0, y: -185, radius: 7, fill: #2c2c54 } }
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 1);

  auto &markers = program->scene->children[0];
  REQUIRE(markers->type == "group");
  REQUIRE(markers->id == "markers");
  REQUIRE(markers->children.size() == 4);

  // Check each marker group has a circle child
  auto &m12 = markers->children[0];
  REQUIRE(m12->id == "m12");
  REQUIRE(std::get<float>(m12->properties["rotation"]) == 0.0f);
  REQUIRE(m12->children.size() == 1);
  REQUIRE(m12->children[0]->type == "circle");
  REQUIRE(std::get<float>(m12->children[0]->properties["radius"]) == 7.0f);

  auto &m1 = markers->children[1];
  REQUIRE(m1->id == "m1");
  REQUIRE(std::get<float>(m1->properties["rotation"]) == 30.0f);
  REQUIRE(m1->children.size() == 1);
  REQUIRE(m1->children[0]->type == "circle");
  REQUIRE(std::get<float>(m1->children[0]->properties["radius"]) == 3.0f);

  auto &m3 = markers->children[3];
  REQUIRE(m3->id == "m3");
  REQUIRE(std::get<float>(m3->properties["rotation"]) == 90.0f);
  REQUIRE(m3->children.size() == 1);
  REQUIRE(std::get<float>(m3->children[0]->properties["radius"]) == 7.0f);
}

TEST_CASE("Parser: Repeat block", "[parser][repeat]") {
  auto program = parse(R"(
        scene RepeatTest {
            repeat 3 {
                rect item@index {
                    y: @index
                    width: 100
                }
            }
        }
    )");

  if (!program) {
    INFO("Parse error: " << get_error());
  }
  REQUIRE(program != nullptr);
  if (program && !program->scene) {
    INFO("Scene is null but program exists - check AstBuilder");
  }
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "RepeatTest");

  // Should have 3 items after expansion
  REQUIRE(program->scene->children.size() == 3);

  // Check first item: item0 with y=0
  auto &item0 = program->scene->children[0];
  REQUIRE(item0->type == "rect");
  REQUIRE(item0->id == "item0");
  REQUIRE(std::get<float>(item0->properties["y"]) == 0.0f);

  // Check second item: item1 with y=1
  auto &item1 = program->scene->children[1];
  REQUIRE(item1->type == "rect");
  REQUIRE(item1->id == "item1");
  REQUIRE(std::get<float>(item1->properties["y"]) == 1.0f);

  // Check third item: item2 with y=2
  auto &item2 = program->scene->children[2];
  REQUIRE(item2->type == "rect");
  REQUIRE(item2->id == "item2");
  REQUIRE(std::get<float>(item2->properties["y"]) == 2.0f);
}

TEST_CASE("Parser: Repeat with nested groups", "[parser][repeat]") {
  auto program = parse(R"(
        scene NestedRepeat {
            group container {
                repeat 2 {
                    group item@index {
                        x: 0
                        y: @index

                        text label@index {
                            content: "Item @index"
                        }
                    }
                }
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);

  // Container group
  REQUIRE(program->scene->children.size() == 1);
  auto &container = program->scene->children[0];
  REQUIRE(container->type == "group");

  // Should have 2 item groups
  REQUIRE(container->children.size() == 2);

  // Check item0
  auto &item0 = container->children[0];
  REQUIRE(item0->id == "item0");
  REQUIRE(item0->children.size() == 1);
  REQUIRE(item0->children[0]->id == "label0");

  // Check item1
  auto &item1 = container->children[1];
  REQUIRE(item1->id == "item1");
  REQUIRE(item1->children.size() == 1);
  REQUIRE(item1->children[0]->id == "label1");
}

TEST_CASE("Parser: All node types", "[parser][edge]") {
  auto program = parse(R"(
        scene AllTypes {
            rect r { width: 100 }
            circle c { radius: 50 }
            ellipse e { width: 100, height: 50 }
            text t { content: "Hello" }
            group g {}
            polygon p { sides: 6 }
            star s { points: 5 }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene->children.size() == 7);

  REQUIRE(program->scene->children[0]->type == "rect");
  REQUIRE(program->scene->children[1]->type == "circle");
  REQUIRE(program->scene->children[2]->type == "ellipse");
  REQUIRE(program->scene->children[3]->type == "text");
  REQUIRE(program->scene->children[4]->type == "group");
  REQUIRE(program->scene->children[5]->type == "polygon");
  REQUIRE(program->scene->children[6]->type == "star");
}

// ============================================================================
// PARSER TESTS: REAL-WORLD EXAMPLE (data_binding.flex)
// ============================================================================

TEST_CASE("Parser: data_binding.flex example", "[parser][example]") {
  const char *source = R"(
// Data Binding Demo
// Simple counter with reactive UI

scene counter {
    width: 400
    height: 300

    // Background
    rect background {
        x: 0, y: 0
        width: 400, height: 300
        fill: #1a1a2e
    }

    // Main layout container
    group mainLayout {
        layout: flex
        flexDirection: column
        justifyContent: center
        alignItems: center
        x: 0, y: 0
        width: 400, height: 300
        gap: 30

        // Title
        text title {
            content: "Data Binding Demo"
            fontSize: 28
            color: #00d9ff
        }

        // Counter display with background
        group counterBox {
            width: 160, height: 80

            rect displayBg {
                x: 0, y: 0
                width: 160, height: 80
                fill: #2c2c54
            }

            group counterLabel {
                layout: flex
                justifyContent: center
                alignItems: center
                width: 160, height: 80

                text counterValue {
                    content: "0"
                    fontSize: 48
                    color: #00ff88
                }
            }
        }

        // Buttons container
        group buttonRow {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40
            width: 400, height: 60

            // Increment button
            group incrementButton {
                width: 80, height: 45

                rect bg {
                    width: 80, height: 45
                    fill: #00d9ff
                }

                group labelContainer {
                    layout: flex
                    justifyContent: center
                    alignItems: center
                    width: 80, height: 45

                    text label {
                        content: "+"
                        fontSize: 32
                        color: #ffffff
                    }
                }
            }

            // Decrement button
            group decrementButton {
                width: 80, height: 45

                rect bg {
                    width: 80, height: 45
                    fill: #ff006e
                }

                group labelContainer {
                    layout: flex
                    justifyContent: center
                    alignItems: center
                    width: 80, height: 45

                    text label {
                        content: "-"
                        fontSize: 32
                        color: #ffffff
                    }
                }
            }
        }

        // Status text
        text statusText {
            content: "Neutral"
            fontSize: 18
            color: #888888
        }
    }
}

// Status Animations
anim "toVeryHigh" {
    duration: 0.1
    track "#statusText/content" { keyframe 0 -> "Very High!" }
    track "#statusText/text.color" { keyframe 0 -> #ff0000 }
}

anim "toHigh" {
    duration: 0.1
    track "#statusText/content" { keyframe 0 -> "High" }
    track "#statusText/text.color" { keyframe 0 -> #ffaa00 }
}

anim "toPositive" {
    duration: 0.1
    track "#statusText/content" { keyframe 0 -> "Positive" }
    track "#statusText/text.color" { keyframe 0 -> #00ff88 }
}

anim "toNeutral" {
    duration: 0.1
    track "#statusText/content" { keyframe 0 -> "Neutral" }
    track "#statusText/text.color" { keyframe 0 -> #888888 }
}

anim "toNegative" {
    duration: 0.1
    track "#statusText/content" { keyframe 0 -> "Negative" }
    track "#statusText/text.color" { keyframe 0 -> #ffaa00 }
}

anim "toVeryLow" {
    duration: 0.1
    track "#statusText/content" { keyframe 0 -> "Very Low!" }
    track "#statusText/text.color" { keyframe 0 -> #ff006e }
}

// Logic State Machine
machine statusTracker {
    layer status {
        state neutral { initial: true, animation: "toNeutral" }
        state positive { animation: "toPositive" }
        state high { animation: "toHigh" }
        state veryHigh { animation: "toVeryHigh" }
        state negative { animation: "toNegative" }
        state veryLow { animation: "toVeryLow" }

        // Forward transitions
        transition neutral -> positive when counter > 0
        transition positive -> high when counter > 5
        transition high -> veryHigh when counter > 10

        // Backward transitions
        transition veryHigh -> high when counter < 10.1
        transition high -> positive when counter < 5.1
        transition positive -> neutral when counter < 0.1

        // Negative transitions
        transition neutral -> negative when counter < 0
        transition negative -> veryLow when counter < -5

        transition veryLow -> negative when counter > -5.1
        transition negative -> neutral when counter > -0.1
    }
}
)";

  auto program = parse(source);

  REQUIRE(program != nullptr);

  SECTION("Scene structure") {
    REQUIRE(program->scene != nullptr);
    REQUIRE(program->scene->name == "counter");
    REQUIRE(program->scene->width == 400.0f);
    REQUIRE(program->scene->height == 300.0f);
    REQUIRE(program->scene->children.size() == 2); // background + mainLayout
  }

  SECTION("Background rect") {
    auto &bg = program->scene->children[0];
    REQUIRE(bg->type == "rect");
    REQUIRE(bg->id == "background");
    REQUIRE(std::get<std::string>(bg->properties["fill"]) == "#1a1a2e");
  }

  SECTION("Main layout group") {
    auto &mainLayout = program->scene->children[1];
    REQUIRE(mainLayout->type == "group");
    REQUIRE(mainLayout->id == "mainLayout");
    REQUIRE(std::get<std::string>(mainLayout->properties["layout"]) == "flex");
    REQUIRE(std::get<std::string>(mainLayout->properties["flexDirection"]) == "column");
    REQUIRE(std::get<float>(mainLayout->properties["gap"]) == 30.0f);

    // mainLayout has: title, counterBox, buttonRow, statusText
    REQUIRE(mainLayout->children.size() == 4);
  }

  SECTION("Nested groups - counterBox") {
    auto &mainLayout = program->scene->children[1];
    auto &counterBox = mainLayout->children[1]; // Second child
    REQUIRE(counterBox->type == "group");
    REQUIRE(counterBox->id == "counterBox");
    REQUIRE(counterBox->children.size() == 2); // displayBg + counterLabel

    auto &counterLabel = counterBox->children[1];
    REQUIRE(counterLabel->type == "group");
    REQUIRE(counterLabel->children.size() == 1); // counterValue text

    auto &counterValue = counterLabel->children[0];
    REQUIRE(counterValue->type == "text");
    REQUIRE(std::get<std::string>(counterValue->properties["content"]) == "0");
  }

  SECTION("Button row structure") {
    auto &mainLayout = program->scene->children[1];
    auto &buttonRow = mainLayout->children[2];
    REQUIRE(buttonRow->type == "group");
    REQUIRE(buttonRow->id == "buttonRow");
    REQUIRE(std::get<std::string>(buttonRow->properties["flexDirection"]) == "row");
    REQUIRE(buttonRow->children.size() == 2); // incrementButton + decrementButton

    // Increment button
    auto &incBtn = buttonRow->children[0];
    REQUIRE(incBtn->id == "incrementButton");
    REQUIRE(incBtn->children.size() == 2); // bg + labelContainer

    // Decrement button
    auto &decBtn = buttonRow->children[1];
    REQUIRE(decBtn->id == "decrementButton");
  }

  SECTION("Animations") {
    REQUIRE(program->animations.size() == 6);

    // Check animation names
    REQUIRE(program->animations[0]->name == "toVeryHigh");
    REQUIRE(program->animations[1]->name == "toHigh");
    REQUIRE(program->animations[2]->name == "toPositive");
    REQUIRE(program->animations[3]->name == "toNeutral");
    REQUIRE(program->animations[4]->name == "toNegative");
    REQUIRE(program->animations[5]->name == "toVeryLow");

    // Check toVeryHigh animation details
    auto &toVeryHigh = program->animations[0];
    REQUIRE(toVeryHigh->duration == 0.1f);
    REQUIRE(toVeryHigh->tracks.size() == 2);
    REQUIRE(toVeryHigh->tracks[0].property == "#statusText/content");
    REQUIRE(toVeryHigh->tracks[1].property == "#statusText/text.color");
    REQUIRE(std::get<std::string>(toVeryHigh->tracks[0].keyframes[0].value) == "Very High!");
  }

  SECTION("State machine") {
    REQUIRE(program->machines.size() == 1);

    auto &machine = program->machines[0];
    REQUIRE(machine->name == "statusTracker");
    REQUIRE(machine->layers.size() == 1);

    auto &layer = machine->layers[0];
    REQUIRE(layer.name == "status");

    // 6 states: neutral, positive, high, veryHigh, negative, veryLow
    REQUIRE(layer.states.size() == 6);

    // Check initial state
    REQUIRE(layer.states[0].name == "neutral");
    REQUIRE(layer.states[0].initial == true);
    REQUIRE(layer.states[0].animation == "toNeutral");

    // Check other states have animations
    REQUIRE(layer.states[1].name == "positive");
    REQUIRE(layer.states[1].animation == "toPositive");
    REQUIRE(layer.states[3].name == "veryHigh");
    REQUIRE(layer.states[3].animation == "toVeryHigh");

    // 10 transitions total
    REQUIRE(layer.transitions.size() == 10);

    // Check first transition: neutral -> positive when counter > 0
    auto &t0 = layer.transitions[0];
    REQUIRE(t0.from_state == "neutral");
    REQUIRE(t0.to_state == "positive");
    REQUIRE(t0.condition_var == "counter");
    REQUIRE(t0.condition_op == ">");
    REQUIRE(t0.condition_val == 0.0f);

    // Check backward transition: veryHigh -> high when counter < 10.1
    auto &t3 = layer.transitions[3];
    REQUIRE(t3.from_state == "veryHigh");
    REQUIRE(t3.to_state == "high");
    REQUIRE(t3.condition_op == "<");
    REQUIRE(t3.condition_val == 10.1f);

    // Check negative transition with negative value
    auto &t7 = layer.transitions[7];
    REQUIRE(t7.from_state == "negative");
    REQUIRE(t7.to_state == "veryLow");
    REQUIRE(t7.condition_val == -5.0f);
  }
}

// ============================================================================
// PARSER TESTS: FOR LOOP
// ============================================================================

TEST_CASE("Lexer: For loop tokens", "[lexer][for]") {
  auto lexer = lexer_create("data for in");

  REQUIRE(lex_next_token(lexer).type == TOK_DATA);
  REQUIRE(lex_next_token(lexer).type == TOK_FOR);
  REQUIRE(lex_next_token(lexer).type == TOK_IN);
  REQUIRE(lex_next_token(lexer).type == TOK_EOF);

  lexer_destroy(lexer);
}

TEST_CASE("Lexer: Dot token", "[lexer][for]") {
  auto lexer = lexer_create("item.name item.price");

  REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER); // item
  REQUIRE(lex_next_token(lexer).type == TOK_DOT);        // .
  REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER); // name
  REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER); // item
  REQUIRE(lex_next_token(lexer).type == TOK_DOT);        // .
  REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER); // price
  REQUIRE(lex_next_token(lexer).type == TOK_EOF);

  lexer_destroy(lexer);
}

TEST_CASE("Parser: Data block", "[parser][for]") {
  auto program = parse(R"(
    data products {
      laptop: { name: "Gaming Laptop", price: 999 }
      mouse: { name: "Wireless Mouse", price: 49 }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->data_blocks.size() == 1);

  auto &data = program->data_blocks[0];
  REQUIRE(data->name == "products");
  REQUIRE(data->items.size() == 2);

  // Check first item
  auto &laptop = data->items[0];
  REQUIRE(laptop.key == "laptop");
  REQUIRE(std::get<std::string>(laptop.properties["name"]) == "Gaming Laptop");
  REQUIRE(std::get<float>(laptop.properties["price"]) == 999.0f);

  // Check second item
  auto &mouse = data->items[1];
  REQUIRE(mouse.key == "mouse");
  REQUIRE(std::get<std::string>(mouse.properties["name"]) == "Wireless Mouse");
  REQUIRE(std::get<float>(mouse.properties["price"]) == 49.0f);
}

TEST_CASE("Parser: For loop basic", "[parser][for]") {
  auto program = parse(R"(
    data colors {
      red: { value: "#ff0000" }
      green: { value: "#00ff00" }
      blue: { value: "#0000ff" }
    }

    scene ForTest {
      for color in colors {
        rect color {
          fill: $(color.value)
        }
      }
    }
  )");

  if (!program) {
    INFO("Parse error: " << get_error());
  }
  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "ForTest");

  // Should have 3 rects after expansion
  REQUIRE(program->scene->children.size() == 3);

  // Check expanded nodes
  auto &rect0 = program->scene->children[0];
  REQUIRE(rect0->type == "rect");
  REQUIRE(rect0->id == "color0");
  REQUIRE(std::get<std::string>(rect0->properties["fill"]) == "#ff0000");

  auto &rect1 = program->scene->children[1];
  REQUIRE(rect1->id == "color1");
  REQUIRE(std::get<std::string>(rect1->properties["fill"]) == "#00ff00");

  auto &rect2 = program->scene->children[2];
  REQUIRE(rect2->id == "color2");
  REQUIRE(std::get<std::string>(rect2->properties["fill"]) == "#0000ff");
}

TEST_CASE("Parser: For loop with index", "[parser][for]") {
  auto program = parse(R"(
    data items {
      a: { label: "Item A" }
      b: { label: "Item B" }
    }

    scene IndexTest {
      for item in items {
        group item {
          y: $(index)
          text label {
            content: $(item.label)
          }
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 2);

  // Check first group
  auto &group0 = program->scene->children[0];
  REQUIRE(group0->type == "group");
  REQUIRE(group0->id == "item0");
  REQUIRE(std::get<float>(group0->properties["y"]) == 0.0f);
  REQUIRE(group0->children.size() == 1);
  REQUIRE(std::get<std::string>(group0->children[0]->properties["content"]) == "Item A");

  // Check second group
  auto &group1 = program->scene->children[1];
  REQUIRE(group1->id == "item1");
  REQUIRE(std::get<float>(group1->properties["y"]) == 1.0f);
  REQUIRE(std::get<std::string>(group1->children[0]->properties["content"]) == "Item B");
}

TEST_CASE("Parser: For loop with numeric properties", "[parser][for]") {
  auto program = parse(R"(
    data positions {
      p1: { x: 100, y: 50 }
      p2: { x: 200, y: 100 }
    }

    scene NumericTest {
      for pos in positions {
        circle pos {
          x: $(pos.x)
          y: $(pos.y)
          radius: 25
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene->children.size() == 2);

  auto &circle0 = program->scene->children[0];
  REQUIRE(std::get<float>(circle0->properties["x"]) == 100.0f);
  REQUIRE(std::get<float>(circle0->properties["y"]) == 50.0f);

  auto &circle1 = program->scene->children[1];
  REQUIRE(std::get<float>(circle1->properties["x"]) == 200.0f);
  REQUIRE(std::get<float>(circle1->properties["y"]) == 100.0f);
}
