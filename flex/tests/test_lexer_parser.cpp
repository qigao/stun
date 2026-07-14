/*
 * Flex DSL Lexer & Parser Tests
 * TinyTest version
 */

#include <string>
#include <variant>

#include "tinytest.h"
#include "flex/dsl.h"

using namespace flex;
using namespace flex::parser;

namespace {

inline void check_close(float actual, float expected, float eps = 0.001f) {
  check_float_eq(actual, expected, eps);
}

#define REQUIRE(expr) check(expr)
#define REQUIRE_FALSE(expr) check_false(expr)
#define REQUIRE_THAT(actual, matcher) \
  check_close((actual), (matcher).expected, (matcher).epsilon)
#define REQUIRE_NOTHROW(expr) check_nothrow(expr)
#define TEST_CASE(description, tags) it(description)

}  // namespace

suite("flex::lexer_parser") {

group("lexer") {

// ============================================================================
// LEXER TESTS
// ============================================================================

group("Lexer: Basic tokens") {
  it("Keywords") {
    auto lexer = lexer_create("scene group rect circle text");

    REQUIRE(lex_next_token(lexer).type == TOK_SCENE);
    REQUIRE(lex_next_token(lexer).type == TOK_NODE_TYPE); // group
    REQUIRE(lex_next_token(lexer).type == TOK_NODE_TYPE); // rect
    REQUIRE(lex_next_token(lexer).type == TOK_NODE_TYPE); // circle
    REQUIRE(lex_next_token(lexer).type == TOK_NODE_TYPE); // text
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  it("Animation keywords") {
    auto lexer = lexer_create("anim track keyframe");

    REQUIRE(lex_next_token(lexer).type == TOK_ANIM);
    REQUIRE(lex_next_token(lexer).type == TOK_TRACK);
    REQUIRE(lex_next_token(lexer).type == TOK_KEYFRAME);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  it("State machine keywords") {
    auto lexer = lexer_create("machine layer state transition when");

    REQUIRE(lex_next_token(lexer).type == TOK_MACHINE);
    REQUIRE(lex_next_token(lexer).type == TOK_LAYER);
    REQUIRE(lex_next_token(lexer).type == TOK_STATE);
    REQUIRE(lex_next_token(lexer).type == TOK_TRANSITION);
    REQUIRE(lex_next_token(lexer).type == TOK_WHEN);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  it("Punctuation") {
    auto lexer = lexer_create("{ } : , ->");

    REQUIRE(lex_next_token(lexer).type == TOK_LBRACE);
    REQUIRE(lex_next_token(lexer).type == TOK_RBRACE);
    REQUIRE(lex_next_token(lexer).type == TOK_COLON);
    REQUIRE(lex_next_token(lexer).type == TOK_COMMA);
    REQUIRE(lex_next_token(lexer).type == TOK_ARROW);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  it("Operators") {
    auto lexer = lexer_create("> < == !=");

    REQUIRE(lex_next_token(lexer).type == TOK_GT);
    REQUIRE(lex_next_token(lexer).type == TOK_LT);
    REQUIRE(lex_next_token(lexer).type == TOK_EQ);
    REQUIRE(lex_next_token(lexer).type == TOK_NEQ);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }
}

group("Lexer: Literals") {
  it("Numbers") {
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

  it("Strings") {
    auto lexer = lexer_create("\"hello\" \"world with spaces\"");

    auto tok1 = lex_next_token(lexer);
    REQUIRE(tok1.type == TOK_STRING);
    REQUIRE(tok1.value == "hello");

    auto tok2 = lex_next_token(lexer);
    REQUIRE(tok2.type == TOK_STRING);
    REQUIRE(tok2.value == "world with spaces");

    lexer_destroy(lexer);
  }

  it("Colors") {
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

  it("Hash references are not tokenized as colors") {
    auto lexer = lexer_create("set #box.x: 42 set #abcNode.opacity: 1");

    REQUIRE(lex_next_token(lexer).type == TOK_SET);
    REQUIRE(lex_next_token(lexer).type == TOK_HASH);
    auto node = lex_next_token(lexer);
    REQUIRE(node.type == TOK_IDENTIFIER);
    REQUIRE(node.value == "box");
    REQUIRE(lex_next_token(lexer).type == TOK_DOT);
    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);
    REQUIRE(lex_next_token(lexer).type == TOK_COLON);
    REQUIRE(lex_next_token(lexer).type == TOK_NUMBER);

    REQUIRE(lex_next_token(lexer).type == TOK_SET);
    REQUIRE(lex_next_token(lexer).type == TOK_HASH);
    node = lex_next_token(lexer);
    REQUIRE(node.type == TOK_IDENTIFIER);
    REQUIRE(node.value == "abcNode");
    REQUIRE(lex_next_token(lexer).type == TOK_DOT);
    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);
    REQUIRE(lex_next_token(lexer).type == TOK_COLON);
    REQUIRE(lex_next_token(lexer).type == TOK_NUMBER);
    REQUIRE(lex_next_token(lexer).type == TOK_EOF);

    lexer_destroy(lexer);
  }

  it("Booleans") {
    auto lexer = lexer_create("true false");

    auto tok1 = lex_next_token(lexer);
    REQUIRE(tok1.type == TOK_BOOL);
    REQUIRE(tok1.value == "true");

    auto tok2 = lex_next_token(lexer);
    REQUIRE(tok2.type == TOK_BOOL);
    REQUIRE(tok2.value == "false");

    lexer_destroy(lexer);
  }

  it("Identifiers") {
    auto lexer = lexer_create("myNode _private camelCase snake_case");

    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);
    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);
    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);
    REQUIRE(lex_next_token(lexer).type == TOK_IDENTIFIER);

    lexer_destroy(lexer);
  }
}

group("Lexer: Comments") {
  it("Line comments") {
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

}

group("parser") {

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

TEST_CASE("Parser: Constant math expressions use MIR evaluator", "[parser][expr]") {
  auto program = parse(R"(
        const base = 10 * 2
        const offset = (base + 5) / 5
        scene ExprScene {
            width: base + offset
            height: (base - offset) * 2
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->width == 25.0f);
  REQUIRE(program->scene->height == 30.0f);
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
  REQUIRE(trans1.condition_expr == "counter > 0.000000");

  auto &trans2 = layer.transitions[1];
  REQUIRE(trans2.from_state == "active");
  REQUIRE(trans2.to_state == "idle");
  REQUIRE(trans2.condition_expr == "counter < 0.100000");
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

  REQUIRE(layer.transitions[0].condition_expr.find(">") != std::string::npos);
  REQUIRE(layer.transitions[1].condition_expr.find("<") != std::string::npos);
  REQUIRE(layer.transitions[2].condition_expr.find("==") != std::string::npos);
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

TEST_CASE("Parser: Component template captures props and children", "[parser][component]") {
  auto program = parse(R"(
        component Badge {
            width: 10
            fill: #ff6600

            rect icon {
                width: $width
                height: 12
                fill: $fill
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->components.size() == 1);

  auto &component = program->components[0];
  REQUIRE(component->name == "Badge");
  REQUIRE(component->default_props.size() == 2);
  REQUIRE(std::get<float>(component->default_props["width"]) == 10.0f);
  REQUIRE(std::get<std::string>(component->default_props["fill"]) == "#ff6600");
  REQUIRE(component->children.size() == 1);

  auto &icon = component->children[0];
  REQUIRE(icon->type == "rect");
  REQUIRE(icon->id == "icon");
  REQUIRE(std::get<std::string>(icon->properties["width"]) == "$width");
  REQUIRE(std::get<std::string>(icon->properties["fill"]) == "$fill");
}

TEST_CASE("Parser: Pseudo-class block preserves base style", "[parser][pseudo]") {
  auto program = parse(R"(
        scene PseudoDemo {
            rect badge {
                fill: #111111
                :hover {
                    fill: #222222
                }
            }
        }
    )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 1);

  auto &badge = program->scene->children[0];
  REQUIRE(std::get<std::string>(badge->properties["fill"]) == "#111111");
  REQUIRE(badge->pseudo_classes.size() == 1);
  REQUIRE(badge->pseudo_classes.count(":hover") == 1);
  REQUIRE(std::get<std::string>(badge->pseudo_classes[":hover"]["fill"]) == "#222222");
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
    check(false);
  }
  REQUIRE(program != nullptr);
  if (program && !program->scene) {
    check(false);
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
  if (!program) {
    return;
  }

{
  {
    REQUIRE(program->scene != nullptr);
    if (!program->scene) {
      return;
    }
    REQUIRE(program->scene->name == "counter");
    REQUIRE(program->scene->width == 400.0f);
    REQUIRE(program->scene->height == 300.0f);
    REQUIRE(program->scene->children.size() == 2); // background + mainLayout
    if (program->scene->children.size() < 2) {
      return;
    }
  }

  {
    auto &bg = program->scene->children[0];
    REQUIRE(bg->type == "rect");
    REQUIRE(bg->id == "background");
    REQUIRE(std::get<std::string>(bg->properties["fill"]) == "#1a1a2e");
  }

  {
    auto &mainLayout = program->scene->children[1];
    REQUIRE(mainLayout->type == "group");
    REQUIRE(mainLayout->id == "mainLayout");
    REQUIRE(std::get<std::string>(mainLayout->properties["layout"]) == "flex");
    REQUIRE(std::get<std::string>(mainLayout->properties["flexDirection"]) == "column");
    REQUIRE(std::get<float>(mainLayout->properties["gap"]) == 30.0f);

    // mainLayout has: title, counterBox, buttonRow, statusText
    REQUIRE(mainLayout->children.size() == 4);
  }

  {
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

  {
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

  {
    REQUIRE(program->animations.size() == 6);
    if (program->animations.size() < 6) {
      return;
    }

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

  {
    REQUIRE(program->machines.size() == 1);
    if (program->machines.empty()) {
      return;
    }

    auto &machine = program->machines[0];
    REQUIRE(machine->name == "statusTracker");
    REQUIRE(machine->layers.size() == 1);
    if (machine->layers.empty()) {
      return;
    }

    auto &layer = machine->layers[0];
    REQUIRE(layer.name == "status");

    // 6 states: neutral, positive, high, veryHigh, negative, veryLow
    REQUIRE(layer.states.size() == 6);
    if (layer.states.size() < 6) {
      return;
    }

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
    if (layer.transitions.size() < 8) {
      return;
    }

    // Check first transition: neutral -> positive when counter > 0
    auto &t0 = layer.transitions[0];
    REQUIRE(t0.from_state == "neutral");
    REQUIRE(t0.to_state == "positive");
    REQUIRE(t0.condition_expr == "counter > 0.000000");

    // Check backward transition: veryHigh -> high when counter < 10.1
    auto &t3 = layer.transitions[3];
    REQUIRE(t3.from_state == "veryHigh");
    REQUIRE(t3.to_state == "high");
    REQUIRE(t3.condition_expr.find("<") != std::string::npos);
    REQUIRE(t3.condition_expr.find("10.1") != std::string::npos);

    // Check negative transition with negative value
    auto &t7 = layer.transitions[7];
    REQUIRE(t7.from_state == "negative");
    REQUIRE(t7.to_state == "veryLow");
    REQUIRE(t7.condition_expr.find("-5") != std::string::npos);
  }
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

TEST_CASE("Lexer: Binding syntax ${}", "[lexer][binding]") {
  auto lexer = lexer_create("${item.name} ${index}");

  auto tok1 = lex_next_token(lexer);
  REQUIRE(tok1.type == TOK_BINDING);
  REQUIRE(tok1.value == "${item.name}");

  auto tok2 = lex_next_token(lexer);
  REQUIRE(tok2.type == TOK_BINDING);
  REQUIRE(tok2.value == "${index}");

  REQUIRE(lex_next_token(lexer).type == TOK_EOF);

  lexer_destroy(lexer);
}

TEST_CASE("Lexer: Binding syntax $()", "[lexer][binding]") {
  auto lexer = lexer_create("$(item.name) $(index)");

  auto tok1 = lex_next_token(lexer);
  REQUIRE(tok1.type == TOK_BINDING);
  REQUIRE(tok1.value == "$(item.name)");

  auto tok2 = lex_next_token(lexer);
  REQUIRE(tok2.type == TOK_BINDING);
  REQUIRE(tok2.value == "$(index)");

  REQUIRE(lex_next_token(lexer).type == TOK_EOF);

  lexer_destroy(lexer);
}

TEST_CASE("Parser: Node property named data", "[parser][node]") {
  auto program = parse(R"(
    scene SvgScene {
      svg inlineBadge {
        width: 40
        height: 30
        data: "<svg viewBox='0 0 10 10'></svg>"
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 1);

  auto &svg = program->scene->children[0];
  REQUIRE(svg->type == "svg");
  REQUIRE(std::get<float>(svg->properties["width"]) == 40.0f);
  REQUIRE(std::get<float>(svg->properties["height"]) == 30.0f);
  REQUIRE(std::get<std::string>(svg->properties["data"]) == "<svg viewBox='0 0 10 10'></svg>");
}

TEST_CASE("Parser: Data item property named data", "[parser][for]") {
  auto program = parse(R"(
    data payloads {
      first: {
        data: "inline"
        value: 42
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->data_blocks.size() == 1);
  REQUIRE(program->data_blocks[0]->items.size() == 1);

  auto &item = program->data_blocks[0]->items[0];
  REQUIRE(item.key == "first");
  REQUIRE(std::get<std::string>(item.properties["data"]) == "inline");
  REQUIRE(std::get<float>(item.properties["value"]) == 42.0f);
}

TEST_CASE("Parser: Scene property named data remains invalid", "[parser][scene]") {
  auto program = parse(R"(
    scene InvalidScene {
      data: "top-level"
    }
  )");

  REQUIRE(program == nullptr);
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
          fill: ${color.value}
        }
      }
    }
  )");

  if (!program) {
    check(false);
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
          y: ${index}
          text label {
            content: ${item.label}
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

TEST_CASE("Parser: For loop can use a later top-level data block", "[parser][for][order]") {
  auto program = parse(R"(
    scene OrderedScene {
      for item in products {
        rect item {
          fill: ${item.color}
        }
      }
    }

    data products {
      first: { color: "#ff0000" }
      second: { color: "#00ff00" }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "OrderedScene");
  REQUIRE(program->data_blocks.size() == 1);
  REQUIRE(program->scene->children.size() == 2);

  auto &first = program->scene->children[0];
  REQUIRE(first->type == "rect");
  REQUIRE(first->id == "item0");
  REQUIRE(std::get<std::string>(first->properties["fill"]) == "#ff0000");

  auto &second = program->scene->children[1];
  REQUIRE(second->type == "rect");
  REQUIRE(second->id == "item1");
  REQUIRE(std::get<std::string>(second->properties["fill"]) == "#00ff00");
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
          x: ${pos.x}
          y: ${pos.y}
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

TEST_CASE("Parser: For loop with $() binding syntax", "[parser][for][binding]") {
  auto program = parse(R"(
    data positions {
      p1: { x: 100, y: 50 }
      p2: { x: 200, y: 100 }
    }

    scene BindingParenTest {
      for pos in positions {
        circle pos {
          x: $(pos.x)
          y: $(pos.y)
          radius: $(index) + 20
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 2);

  auto &circle0 = program->scene->children[0];
  REQUIRE(circle0->id == "pos0");
  REQUIRE(std::get<float>(circle0->properties["x"]) == 100.0f);
  REQUIRE(std::get<float>(circle0->properties["y"]) == 50.0f);
  REQUIRE(std::get<float>(circle0->properties["radius"]) == 20.0f);

  auto &circle1 = program->scene->children[1];
  REQUIRE(circle1->id == "pos1");
  REQUIRE(std::get<float>(circle1->properties["x"]) == 200.0f);
  REQUIRE(std::get<float>(circle1->properties["y"]) == 100.0f);
  REQUIRE(std::get<float>(circle1->properties["radius"]) == 21.0f);
}

TEST_CASE("Parser: For loop with ${} binding syntax", "[parser][for][binding]") {
  auto program = parse(R"(
    data colors {
      red: { value: "#ff0000" }
      green: { value: "#00ff00" }
    }

    scene BindingTest {
      for color in colors {
        rect color {
          fill: ${color.value}
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 2);

  // ${} syntax should work the same as $()
  auto &rect0 = program->scene->children[0];
  REQUIRE(rect0->id == "color0");
  REQUIRE(std::get<std::string>(rect0->properties["fill"]) == "#ff0000");

  auto &rect1 = program->scene->children[1];
  REQUIRE(rect1->id == "color1");
  REQUIRE(std::get<std::string>(rect1->properties["fill"]) == "#00ff00");
}

// ============================================================================
// PARSER TESTS: ASSETS
// ============================================================================

TEST_CASE("Lexer: Assets tokens", "[lexer][assets]") {
  auto lexer = lexer_create("assets audio font");

  REQUIRE(lex_next_token(lexer).type == TOK_ASSETS);
  REQUIRE(lex_next_token(lexer).type == TOK_AUDIO);
  REQUIRE(lex_next_token(lexer).type == TOK_FONT);
  REQUIRE(lex_next_token(lexer).type == TOK_EOF);

  lexer_destroy(lexer);
}

TEST_CASE("Parser: Assets block basic", "[parser][assets]") {
  auto program = parse(R"(
    assets {
      audio click: "sounds/click.wav"
      audio hover: "sounds/hover.mp3"
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->assets != nullptr);
  REQUIRE(program->assets->assets.size() == 2);

  // Check first asset
  auto &click = program->assets->assets[0];
  REQUIRE(click.type == "audio");
  REQUIRE(click.id == "click");
  REQUIRE(click.path == "sounds/click.wav");

  // Check second asset
  auto &hover = program->assets->assets[1];
  REQUIRE(hover.type == "audio");
  REQUIRE(hover.id == "hover");
  REQUIRE(hover.path == "sounds/hover.mp3");
}

TEST_CASE("Parser: Assets with options", "[parser][assets]") {
  auto program = parse(R"(
    assets {
      audio bgm: "sounds/background.mp3" {
        loop: true
        volume: 0.5
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->assets != nullptr);
  REQUIRE(program->assets->assets.size() == 1);

  auto &bgm = program->assets->assets[0];
  REQUIRE(bgm.type == "audio");
  REQUIRE(bgm.id == "bgm");
  REQUIRE(bgm.path == "sounds/background.mp3");

  // Check options
  REQUIRE(bgm.options.count("loop") == 1);
  REQUIRE(std::get<bool>(bgm.options["loop"]) == true);

  REQUIRE(bgm.options.count("volume") == 1);
  REQUIRE(std::get<float>(bgm.options["volume"]) == 0.5f);
}

TEST_CASE("Parser: Multiple asset types", "[parser][assets]") {
  auto program = parse(R"(
    assets {
      audio click: "sounds/click.wav"
      image logo: "images/logo.png"
      font main: "fonts/roboto.ttf"
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->assets != nullptr);
  REQUIRE(program->assets->assets.size() == 3);

  REQUIRE(program->assets->assets[0].type == "audio");
  REQUIRE(program->assets->assets[0].id == "click");

  REQUIRE(program->assets->assets[1].type == "image");
  REQUIRE(program->assets->assets[1].id == "logo");

  REQUIRE(program->assets->assets[2].type == "font");
  REQUIRE(program->assets->assets[2].id == "main");
}

TEST_CASE("Parser: Assets with scene", "[parser][assets]") {
  auto program = parse(R"(
    assets {
      audio click: "sounds/click.wav"
    }

    scene TestScene {
      width: 800
      height: 600
      rect bg { fill: #000000 }
    }
  )");

  REQUIRE(program != nullptr);

  // Assets should be parsed
  REQUIRE(program->assets != nullptr);
  REQUIRE(program->assets->assets.size() == 1);

  // Scene should also be parsed
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "TestScene");
  REQUIRE(program->scene->width == 800.0f);
}

// ============================================================================
// PARSER TESTS: STATE MACHINE WITH PLAY/STOP AUDIO
// ============================================================================

TEST_CASE("Parser: State with play audio", "[parser][machine][audio]") {
  auto program = parse(R"(
    machine audioMachine {
      layer main {
        state idle {
          initial: true
        }

        state playing {
          play: bgm
          animation: "toPlaying"
        }

        transition idle -> playing when start > 0
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->machines.size() == 1);

  auto &machine = program->machines[0];
  auto &layer = machine->layers[0];
  REQUIRE(layer.states.size() == 2);

  // Check idle state (no audio)
  auto &idle = layer.states[0];
  REQUIRE(idle.name == "idle");
  REQUIRE(idle.play_audio.empty());
  REQUIRE(idle.stop_audio.empty());

  // Check playing state (has play audio)
  auto &playing = layer.states[1];
  REQUIRE(playing.name == "playing");
  REQUIRE(playing.play_audio == "bgm");
  REQUIRE(playing.animation == "toPlaying");
  REQUIRE(playing.stop_audio.empty());
}

TEST_CASE("Parser: State with stop audio", "[parser][machine][audio]") {
  auto program = parse(R"(
    machine audioMachine {
      layer main {
        state playing {
          initial: true
          play: bgm
        }

        state stopped {
          stop: bgm
        }

        transition playing -> stopped when stop > 0
      }
    }
  )");

  REQUIRE(program != nullptr);
  auto &layer = program->machines[0]->layers[0];

  // Check playing state
  auto &playing = layer.states[0];
  REQUIRE(playing.play_audio == "bgm");
  REQUIRE(playing.stop_audio.empty());

  // Check stopped state
  auto &stopped = layer.states[1];
  REQUIRE(stopped.play_audio.empty());
  REQUIRE(stopped.stop_audio == "bgm");
}

TEST_CASE("Parser: State with both play and stop audio", "[parser][machine][audio]") {
  auto program = parse(R"(
    machine audioMachine {
      layer main {
        state switchTrack {
          stop: track1
          play: track2
          animation: "switchAnim"
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  auto &state = program->machines[0]->layers[0].states[0];

  REQUIRE(state.name == "switchTrack");
  REQUIRE(state.stop_audio == "track1");
  REQUIRE(state.play_audio == "track2");
  REQUIRE(state.animation == "switchAnim");
}

TEST_CASE("Parser: Full audio demo", "[parser][assets][machine][audio]") {
  auto program = parse(R"(
    assets {
      audio click: "sounds/click.wav"
      audio hover: "sounds/hover.wav"
      audio bgm: "sounds/background.mp3" {
        loop: true
        volume: 0.5
      }
    }

    scene audioDemo {
      width: 600
      height: 400
      rect bg { fill: #1a1a2e }
    }

    machine audioController {
      layer main {
        state idle {
          initial: true
        }

        state playing {
          play: bgm
          animation: "toPlaying"
        }

        state stopped {
          stop: bgm
          animation: "toStopped"
        }

        transition idle -> playing when playClicked > 0
        transition playing -> stopped when stopClicked > 0
        transition stopped -> playing when playClicked > 0
      }
    }

    anim "toPlaying" {
      duration: 0.2
      track "#statusText/content" {
        keyframe 0 -> "Playing..."
      }
    }

    anim "toStopped" {
      duration: 0.2
      track "#statusText/content" {
        keyframe 0 -> "Stopped"
      }
    }
  )");

  REQUIRE(program != nullptr);

  // Verify assets
  REQUIRE(program->assets != nullptr);
  REQUIRE(program->assets->assets.size() == 3);

  auto &bgm = program->assets->assets[2];
  REQUIRE(bgm.id == "bgm");
  REQUIRE(bgm.options.count("loop") == 1);

  // Verify scene
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "audioDemo");

  // Verify state machine
  REQUIRE(program->machines.size() == 1);
  auto &machine = program->machines[0];
  REQUIRE(machine->name == "audioController");

  auto &layer = machine->layers[0];
  REQUIRE(layer.states.size() == 3);
  REQUIRE(layer.transitions.size() == 3);

  // Check states
  REQUIRE(layer.states[0].name == "idle");
  REQUIRE(layer.states[0].initial == true);

  REQUIRE(layer.states[1].name == "playing");
  REQUIRE(layer.states[1].play_audio == "bgm");
  REQUIRE(layer.states[1].animation == "toPlaying");

  REQUIRE(layer.states[2].name == "stopped");
  REQUIRE(layer.states[2].stop_audio == "bgm");
  REQUIRE(layer.states[2].animation == "toStopped");

  // Verify animations
  REQUIRE(program->animations.size() == 2);
  REQUIRE(program->animations[0]->name == "toPlaying");
  REQUIRE(program->animations[1]->name == "toStopped");
}

// ============================================================================
// PARSER TESTS: IMPORT
// ============================================================================

TEST_CASE("Lexer: Import token", "[lexer][import]") {
  auto lexer = lexer_create("import");

  auto tok = lex_next_token(lexer);
  REQUIRE(tok.type == TOK_IMPORT);
  REQUIRE(tok.value == "import");
  REQUIRE(lex_next_token(lexer).type == TOK_EOF);

  lexer_destroy(lexer);
}

TEST_CASE("Lexer: Import statement tokens", "[lexer][import]") {
  auto lexer = lexer_create("import \"animations/fade.flex\"");

  REQUIRE(lex_next_token(lexer).type == TOK_IMPORT);

  auto tok = lex_next_token(lexer);
  REQUIRE(tok.type == TOK_STRING);
  REQUIRE(tok.value == "animations/fade.flex");

  REQUIRE(lex_next_token(lexer).type == TOK_EOF);

  lexer_destroy(lexer);
}

TEST_CASE("Parser: Single import statement", "[parser][import]") {
  auto program = parse(R"(
    import "components/button.flex"
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->imports.size() == 1);
  REQUIRE(program->imports[0].path == "components/button.flex");
}

TEST_CASE("Parser: Multiple import statements", "[parser][import]") {
  auto program = parse(R"(
    import "animations/fade.flex"
    import "components/button.flex"
    import "machines/player.flex"
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->imports.size() == 3);
  REQUIRE(program->imports[0].path == "animations/fade.flex");
  REQUIRE(program->imports[1].path == "components/button.flex");
  REQUIRE(program->imports[2].path == "machines/player.flex");
}

TEST_CASE("Parser: Import with scene", "[parser][import]") {
  auto program = parse(R"(
    import "shared/colors.flex"

    scene Main {
      width: 800
      height: 600
      rect bg { fill: #000000 }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->imports.size() == 1);
  REQUIRE(program->imports[0].path == "shared/colors.flex");

  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->name == "Main");
  REQUIRE(program->scene->width == 800.0f);
}

TEST_CASE("Parser: Import with animations and machines", "[parser][import]") {
  auto program = parse(R"(
    import "animations/base.flex"

    anim "localFade" {
      duration: 0.5
      track "opacity" {
        keyframe 0 -> 0
        keyframe 0.5 -> 1
      }
    }

    machine localMachine {
      layer main {
        state idle { initial: true }
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->imports.size() == 1);
  REQUIRE(program->animations.size() == 1);
  REQUIRE(program->animations[0]->name == "localFade");
  REQUIRE(program->machines.size() == 1);
  REQUIRE(program->machines[0]->name == "localMachine");
}

TEST_CASE("Parser: Import preserves line info", "[parser][import]") {
  auto program = parse("import \"test.flex\"");

  REQUIRE(program != nullptr);
  REQUIRE(program->imports.size() == 1);
  REQUIRE(program->imports[0].line == 1);
}

// ============================================================================
// LEXER TESTS: NEW KEYWORDS (set, play, with)
// ============================================================================

TEST_CASE("Lexer: New keywords set, play, with", "[lexer]") {
  auto lexer = lexer_create("set play with");

  REQUIRE(lex_next_token(lexer).type == TOK_SET);
  REQUIRE(lex_next_token(lexer).type == TOK_PLAY);
  REQUIRE(lex_next_token(lexer).type == TOK_WITH);
  REQUIRE(lex_next_token(lexer).type == TOK_EOF);

  lexer_destroy(lexer);
}

// ============================================================================
// PARSER TESTS: EXPRESSION TRANSITION CONDITIONS
// ============================================================================

TEST_CASE("Parser: Transition with binding condition", "[parser][machine][expr]") {
  auto program = parse(R"(
    machine test {
      layer main {
        state idle { initial: true }
        state active {}

        transition idle -> active when ${speed > 5 and health < 50}
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->machines.size() == 1);

  auto &layer = program->machines[0]->layers[0];
  REQUIRE(layer.transitions.size() == 1);

  auto &trans = layer.transitions[0];
  REQUIRE(trans.from_state == "idle");
  REQUIRE(trans.to_state == "active");
  REQUIRE(trans.condition_expr == "speed > 5 and health < 50");
}

TEST_CASE("Parser: Legacy transition backward compat", "[parser][machine][expr]") {
  auto program = parse(R"(
    machine test {
      layer main {
        state a { initial: true }
        state b {}
        transition a -> b when speed > 5
      }
    }
  )");

  REQUIRE(program != nullptr);
  auto &trans = program->machines[0]->layers[0].transitions[0];
  REQUIRE(trans.from_state == "a");
  REQUIRE(trans.to_state == "b");
  // Legacy path synthesizes expression string
  REQUIRE(trans.condition_expr.find("speed") != std::string::npos);
  REQUIRE(trans.condition_expr.find(">") != std::string::npos);
  REQUIRE(trans.condition_expr.find("5") != std::string::npos);
}

// ============================================================================
// PARSER TESTS: STATE ENTRY ACTIONS
// ============================================================================

TEST_CASE("Parser: State with set action", "[parser][machine][actions]") {
  auto program = parse(R"(
    machine test {
      layer main {
        state active {
          initial: true
          set #hand.rotation: ${angle * 6}
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->machines.size() == 1);

  auto &state = program->machines[0]->layers[0].states[0];
  REQUIRE(state.name == "active");
  REQUIRE(state.actions.size() == 1);
  REQUIRE(state.actions[0].node_id == "hand");
  REQUIRE(state.actions[0].property == "rotation");
  REQUIRE(state.actions[0].expression == "angle * 6");
}

TEST_CASE("Parser: Scene and machine with set action", "[parser][scene][machine][actions]") {
  auto program = parse(R"(
    scene Motion {}

    machine test {
      layer main {
        state idle {
          initial: true
        }

        state active {
          set #box.x: 42
        }

        transition idle -> active when ${trigger}
      }
    }
  )");
  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->machines.size() == 1);

  auto &state = program->machines[0]->layers[0].states[1];
  REQUIRE(state.name == "active");
  REQUIRE(state.actions.size() == 1);
  REQUIRE(state.actions[0].node_id == "box");
  REQUIRE(state.actions[0].property == "x");
  REQUIRE(state.actions[0].expression == "42.000000");
}

TEST_CASE("Parser: Scene node and machine with set action", "[parser][scene][machine][actions]") {
  auto program = parse(R"(
    scene Motion {
      rect box {
        x: 0
      }
    }

    machine test {
      layer main {
        state idle {
          initial: true
        }

        state active {
          set #box.x: 42
        }

        transition idle -> active when ${trigger}
      }
    }
  )");
  REQUIRE(program != nullptr);
  REQUIRE(program->scene != nullptr);
  REQUIRE(program->scene->children.size() == 1);
  REQUIRE(program->machines.size() == 1);

  auto &state = program->machines[0]->layers[0].states[1];
  REQUIRE(state.name == "active");
  REQUIRE(state.actions.size() == 1);
  REQUIRE(state.actions[0].node_id == "box");
  REQUIRE(state.actions[0].property == "x");
  REQUIRE(state.actions[0].expression == "42.000000");
}

// ============================================================================
// PARSER TESTS: PARAMETERIZED ANIMATION TRIGGERS
// ============================================================================

TEST_CASE("Parser: State with play and params", "[parser][machine][anim_params]") {
  auto program = parse(R"(
    machine test {
      layer main {
        state active {
          initial: true
          play "rotate" with {
            duration: ${durationInput}
            speed: ${speedInput}
          }
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  REQUIRE(program->machines.size() == 1);

  auto &state = program->machines[0]->layers[0].states[0];
  REQUIRE(state.name == "active");
  REQUIRE(state.animation == "rotate");
  REQUIRE(state.animation_params.size() == 2);
  REQUIRE(state.animation_params.count("duration") == 1);
  REQUIRE(state.animation_params.at("duration") == "durationInput");
  REQUIRE(state.animation_params.count("speed") == 1);
  REQUIRE(state.animation_params.at("speed") == "speedInput");
}

TEST_CASE("Parser: State with play no params", "[parser][machine][anim_params]") {
  auto program = parse(R"(
    machine test {
      layer main {
        state active {
          initial: true
          play "fadeIn"
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  auto &state = program->machines[0]->layers[0].states[0];
  REQUIRE(state.animation == "fadeIn");
  REQUIRE(state.animation_params.empty());
}

TEST_CASE("Parser: State with play colon backward compat", "[parser][machine][audio]") {
  // play: audioId should still work as audio playback
  auto program = parse(R"(
    machine test {
      layer main {
        state active {
          initial: true
          play: bgm
        }
      }
    }
  )");

  REQUIRE(program != nullptr);
  auto &state = program->machines[0]->layers[0].states[0];
  REQUIRE(state.play_audio == "bgm");
}

}

}
