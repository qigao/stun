/*
 * Flex Runtime Tests - TinyTest version
 *
 * Tests only implemented runtime functionality:
 * - Scene creation and properties
 * - Node hierarchy (Group, Shape, Text, Image)
 * - Timeline animation system
 * - State machine transitions
 */

#include "flex/core.h"
#include "flex/core/expr.h"
#include "flex/core/expr_mir.h"
#include "flex/core/expr_value.h"
#include "tinytest.h"

using namespace flex;

namespace {

inline void check_close(float actual, float expected, float eps = 0.001f) {
    check_float_eq(actual, expected, eps);
}

} // namespace

suite("flex::runtime") {
    group("scene") {
        it("creates with dimensions") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);

            check(scene != nullptr);
            check_close(scene->width(), 800.0f);
            check_close(scene->height(), 600.0f);
        }

        it("sets size") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(100.0f, 100.0f, arena);

            scene->set_size(1920.0f, 1080.0f);

            check_close(scene->width(), 1920.0f);
            check_close(scene->height(), 1080.0f);
        }

        it("stores background color") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);

            Color bg = {0.2f, 0.3f, 0.4f, 1.0f};
            scene->set_background(bg);

            const Color& result = scene->background();
            check_close(result.r, 0.2f);
            check_close(result.g, 0.3f);
            check_close(result.b, 0.4f);
            check_close(result.a, 1.0f);
        }

        it("adds children to root") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);

            auto group = Group::create(arena);
            scene->add_child(group);

            check(scene->root() != nullptr);
            check(scene->root()->child_count() == 1);
        }
    }

    group("group") {
        it("creates and adds children") {
            ArenaAllocator arena(4096);
            auto group = Group::create(arena);

            auto child1 = Group::create(arena);
            auto child2 = Group::create(arena);

            group->add_child(child1);
            group->add_child(child2);

            check(group->child_count() == 2);
            check(group->child_at(0) == child1);
            check(group->child_at(1) == child2);
        }

        it("removes child") {
            ArenaAllocator arena(4096);
            auto group = Group::create(arena);
            auto child = Group::create(arena);

            group->add_child(child);
            check(group->child_count() == 1);

            group->remove_child(child);
            check(group->child_count() == 0);
        }
    }

    group("node") {
        it("stores id and visibility") {
            ArenaAllocator arena(4096);
            auto node = Group::create(arena);

            node->set_id("testNode");
            check(node->id() == "testNode");

            check(node->visible() == true);
            node->set_visible(false);
            check(node->visible() == false);
        }

        it("stores opacity") {
            ArenaAllocator arena(4096);
            auto node = Group::create(arena);

            check_close(node->opacity(), 1.0f);

            node->set_opacity(0.5f);
            check_close(node->opacity(), 0.5f);
        }

        it("finds child by id") {
            ArenaAllocator arena(4096);
            auto parent = Group::create(arena);
            auto child = Group::create(arena);
            child->set_id("target");

            parent->add_child(child);

            Node* found = parent->find_child_recursive("target");
            check(found == child);

            Node* not_found = parent->find_child_recursive("nonexistent");
            check(not_found == nullptr);
        }
    }

    group("shape") {
        it("creates with default properties") {
            ArenaAllocator arena(4096);
            auto shape = Shape::create(arena);

            check(shape != nullptr);
            check_close(shape->opacity(), 1.0f);
        }

        it("sets rectangle geometry") {
            ArenaAllocator arena(4096);
            auto shape = Shape::create(arena);

            shape->set_rect(100.0f, 50.0f, 5.0f);
            check(shape->geometry_type() == GeometryType::Rect);

            auto rect = shape->rect();
            check_close(rect.width, 100.0f);
            check_close(rect.height, 50.0f);
            check_close(rect.corner_radius, 5.0f);
        }

        it("sets circle geometry") {
            ArenaAllocator arena(4096);
            auto shape = Shape::create(arena);

            shape->set_circle(25.0f);
            check(shape->geometry_type() == GeometryType::Circle);

            auto circle = shape->circle();
            check_close(circle.radius, 25.0f);
        }

        it("stores fill color") {
            ArenaAllocator arena(4096);
            auto shape = Shape::create(arena);

            Color red = {1.0f, 0.0f, 0.0f, 1.0f};
            shape->set_fill(red);

            check(shape->has_fill());
            Fill fill = shape->fill();
            check_close(fill.color.r, 1.0f);
            check_close(fill.color.g, 0.0f);
            check_close(fill.color.b, 0.0f);
        }

        it("stores stroke properties") {
            ArenaAllocator arena(4096);
            auto shape = Shape::create(arena);

            Color blue = {0.0f, 0.0f, 1.0f, 1.0f};
            shape->set_stroke(blue, 3.0f);

            check(shape->has_stroke());
            Stroke stroke = shape->stroke();
            check_close(stroke.width, 3.0f);
            check_close(stroke.color.b, 1.0f);
        }
    }

    group("text") {
        it("creates with content") {
            ArenaAllocator arena(4096);
            auto text = Text::create(arena);

            text->set_content("Hello World");
            check(text->content() == "Hello World");
        }

        it("stores font properties") {
            ArenaAllocator arena(4096);
            auto text = Text::create(arena);

            text->set_font_family("Arial");
            check(text->font_family() == "Arial");

            text->set_font_size(24.0f);
            check_close(text->font_size(), 24.0f);
        }

        it("stores color") {
            ArenaAllocator arena(4096);
            auto text = Text::create(arena);

            Color black = {0.0f, 0.0f, 0.0f, 1.0f};
            text->set_color(black);

            Color result = text->color();
            check_close(result.r, 0.0f);
            check_close(result.g, 0.0f);
            check_close(result.b, 0.0f);
        }
    }

    group("image") {
        it("creates with source") {
            ArenaAllocator arena(4096);
            auto image = Image::create(arena);

            image->set_src("test.png");
            check(image->src() == "test.png");
        }

        it("stores dimensions") {
            ArenaAllocator arena(4096);
            auto image = Image::create(arena);

            image->set_width(200.0f);
            image->set_height(150.0f);

            check_close(image->width(), 200.0f);
            check_close(image->height(), 150.0f);
        }
    }

    group("timeline") {
        it("creates with name") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("fadeIn", alloc);

            check(std::string(timeline->name()) == "fadeIn");
        }

        it("sets duration and loop mode") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("test", alloc);

            timeline->set_duration(2.0f);
            check_close(timeline->duration(), 2.0f);

            check(timeline->loop_mode() == LoopMode::Once);
            timeline->set_loop_mode(LoopMode::Loop);
            check(timeline->loop_mode() == LoopMode::Loop);
        }

        it("adds track") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("test", alloc);

            timeline->add_track("opacity");

            auto* track = timeline->get_track("opacity");
            check(track != nullptr);
            check(std::string(track->property()) == "opacity");
        }

        it("adds keyframes") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("test", alloc);
            timeline->add_track("opacity");

            auto* track = timeline->get_track("opacity");
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 0.5f);
            track->add_keyframe(2.0f, 1.0f);

            check(track->keyframe_count() == 3);
        }

        it("samples track at time") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("test", alloc);
            timeline->add_track("opacity");

            auto* track = timeline->get_track("opacity");
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(2.0f, 1.0f);

            AnimValue val0 = track->sample(0.0f);
            AnimValue val1 = track->sample(1.0f);
            AnimValue val2 = track->sample(2.0f);

            check_close(std::get<float>(val0), 0.0f);
            check_close(std::get<float>(val1), 0.5f);
            check_close(std::get<float>(val2), 1.0f);
        }

        it("samples float keyframes through a MIR track expression") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("expressionTrack", alloc);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 10.0f);
            track->add_keyframe(2.0f, 30.0f, Easing::linear());

            track->set_numeric_expression(
                "lerp(from, to, progress * progress) + time");
            check(track->has_numeric_expression());
            check_close(std::get<float>(track->sample(1.0f)), 16.0f);

            track->clear_numeric_expression();
            check_false(track->has_numeric_expression());
            check_close(std::get<float>(track->sample(1.0f)), 20.0f);
        }

        it("rejects an invalid MIR track expression when configured") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("invalidExpressionTrack", alloc);
            auto* track = timeline->add_track("x").get();

            check_throws_as(track->set_numeric_expression("progress + ("),
                            std::invalid_argument);
            check_false(track->has_numeric_expression());
        }

        it("lerps override blend weight") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("blendTest", alloc);
            timeline->set_duration(2.0f);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 10.0f);

            auto node = Group::create(alloc);
            TimelinePlayer player(timeline.get(), node);
            player.set_blend_mode(BlendMode::Override);
            player.set_blend_weight(0.5f);
            player.play();
            player.advance(1.0f);
            player.apply();

            check_close(node->x(), 5.0f);
        }

        it("remaps duration override and fires triggers") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("durationParamTest", alloc);
            timeline->set_duration(1.0f);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 10.0f);
            timeline->add_trigger(0.5f, "half");

            auto node = Group::create(alloc);
            TimelinePlayer player(timeline.get(), node);
            player.set_duration_override(2.0f);
            bool fired = false;
            player.set_trigger_callback([&fired](const std::string& event) {
                if (event == "half") {
                    fired = true;
                }
            });
            player.play();

            player.advance(0.9f);
            player.apply();
            check_close(node->x(), 4.5f);
            check(fired == false);

            player.advance(0.2f);
            check(fired);
        }

        it("keeps speed per player") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("speedParamTest", alloc);
            timeline->set_duration(2.0f);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(2.0f, 20.0f);

            auto fast_node = Group::create(alloc);
            auto normal_node = Group::create(alloc);
            TimelinePlayer fast(timeline.get(), fast_node);
            TimelinePlayer normal(timeline.get(), normal_node);
            fast.set_speed(2.0f);

            fast.play();
            normal.play();
            fast.advance(0.5f);
            normal.advance(0.5f);
            fast.apply();
            normal.apply();

            check_close(fast_node->x(), 10.0f);
            check_close(normal_node->x(), 5.0f);
            check_close(timeline->speed(), 1.0f);
        }

        it("fires controller trigger callback") {
            ArenaAllocator alloc(4096);
            AnimationController controller(alloc);
            auto timeline = Timeline::create("triggerTest", alloc);
            timeline->set_duration(1.0f);
            timeline->add_trigger(0.5f, "fire");
            controller.add_timeline(timeline);

            auto node = Group::create(alloc);
            bool fired = false;
            controller.set_trigger_callback([&fired](const std::string& event) {
                if (event == "fire") {
                    fired = true;
                }
            });

            controller.play("triggerTest", node);
            controller.advance(1.0f);

            check(fired);
        }

        it("fires loop triggers for every crossed cycle") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("loopTriggerTest", alloc);
            timeline->set_duration(1.0f);
            timeline->set_loop_mode(LoopMode::Loop);
            timeline->add_trigger(0.5f, "half");

            auto node = Group::create(alloc);
            TimelinePlayer player(timeline.get(), node);
            int fired = 0;
            player.set_trigger_callback([&fired](const std::string& event) {
                if (event == "half") {
                    ++fired;
                }
            });
            player.play();

            player.advance(2.6f);

            check(fired == 3);
            check_close(player.current_time(), 0.6f);
        }

        it("stops before accessing a destroyed shared target") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("destroyedTarget", alloc);
            timeline->set_duration(1.0f);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 10.0f);

            auto target = Group::create();
            TimelinePlayer player(timeline.get(), target.get());
            player.play();
            target.reset();

            check_false(player.advance(0.5f));
            player.apply();
            check_null(player.target());
            check_false(player.is_playing());
            check(player.is_finished());
        }

        it("stops before accessing a reset arena target") {
            ArenaAllocator timeline_alloc(4096);
            ArenaAllocator target_alloc(4096);
            auto timeline = Timeline::create("resetArenaTarget", timeline_alloc);
            timeline->set_duration(1.0f);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 10.0f);

            auto* target = Group::create(target_alloc);
            TimelinePlayer player(timeline.get(), target);
            player.play();
            target_alloc.reset();

            check_false(player.advance(0.5f));
            player.apply();
            check_null(player.target());
            check(player.is_finished());
        }
    }

    group("machine") {
        it("creates and stores name") {
            RuntimeStateMachine machine("testMachine");

            check(machine.name() == "testMachine");
        }

        it("adds layer") {
            RuntimeStateMachine machine("testMachine");

            machine.add_layer("status");

            check(machine.get_layer("status") != nullptr);
            check(machine.get_layer("nonexistent") == nullptr);
        }

        it("stores layer states") {
            RuntimeStateMachine machine("testMachine");
            machine.add_layer("status");

            auto* layer = machine.get_layer("status");
            layer->add_state("idle", true, "toIdle");
            layer->add_state("active", false, "toActive");

            check(layer->current_state() == "idle");
        }

        it("transitions state") {
            RuntimeStateMachine machine("testMachine");
            machine.add_layer("status");

            auto* layer = machine.get_layer("status");
            layer->add_state("idle", true, "");
            layer->add_state("active", false, "");

            layer->add_transition("idle", "active", "trigger > 0");

            check(layer->current_state() == "idle");

            machine.set_input("trigger", 1.0f);
            machine.update(0.016f);

            check(layer->current_state() == "active");
        }

        it("handles multiple transitions") {
            RuntimeStateMachine machine("counter");
            machine.add_layer("status");

            auto* layer = machine.get_layer("status");
            layer->add_state("low", true, "");
            layer->add_state("medium", false, "");
            layer->add_state("high", false, "");

            layer->add_transition("low", "medium", "value > 10");
            layer->add_transition("medium", "high", "value > 50");
            layer->add_transition("high", "medium", "value < 50");
            layer->add_transition("medium", "low", "value < 10");

            check(layer->current_state() == "low");

            machine.set_input("value", 25.0f);
            machine.update(0.016f);
            check(layer->current_state() == "medium");

            machine.set_input("value", 75.0f);
            machine.update(0.016f);
            machine.update(0.016f);
            check(layer->current_state() == "high");

            machine.set_input("value", 30.0f);
            machine.update(0.016f);
            check(layer->current_state() == "medium");

            machine.set_input("value", 5.0f);
            machine.update(0.016f);
            check(layer->current_state() == "low");
        }
    }

    group("binding") {
        it("evaluates numeric vector helpers") {
            auto a = NumericValue::vector({1.0f, 2.0f, 3.0f});
            auto b = NumericValue::vector({4.0f, 5.0f, 6.0f});

            auto dot = numeric_dot(a, b);
            check(dot.has_value());
            check_close(*dot, 32.0f);

            auto norm = numeric_norm(a);
            check(norm.has_value());
            check_close(*norm, std::sqrt(14.0f));
        }

        it("evaluates numeric matrix helpers with row-major boundary data") {
            auto a = NumericValue::matrix(2, 3, {
                1.0f, 2.0f, 3.0f,
                4.0f, 5.0f, 6.0f,
            });
            auto b = NumericValue::matrix(3, 2, {
                7.0f, 8.0f,
                9.0f, 10.0f,
                11.0f, 12.0f,
            });

            auto product = numeric_matmul(a, b);
            check(product.has_value());
            check(product->rows == static_cast<std::size_t>(2));
            check(product->cols == static_cast<std::size_t>(2));
            check_close(product->values[0], 58.0f);
            check_close(product->values[1], 64.0f);
            check_close(product->values[2], 139.0f);
            check_close(product->values[3], 154.0f);

            auto transposed = numeric_transpose(*product);
            check(transposed.has_value());
            check_close(transposed->values[0], 58.0f);
            check_close(transposed->values[1], 139.0f);
            check_close(transposed->values[2], 64.0f);
            check_close(transposed->values[3], 154.0f);
        }

        it("rejects invalid numeric matrix dimensions") {
            auto a = NumericValue::matrix(2, 2, {1.0f, 2.0f, 3.0f, 4.0f});
            auto b = NumericValue::matrix(3, 1, {1.0f, 2.0f, 3.0f});
            check(!numeric_matmul(a, b).has_value());

            auto bad = NumericValue::matrix(2, 2, {1.0f, 2.0f, 3.0f});
            check(!bad.valid());
            check(!numeric_norm(bad).has_value());
        }

        it("evaluates a MIR numeric expression program") {
            auto program = MirExpressionProgram::compile("a * 2 + b / 4", {"a", "b"});
            check(program != nullptr);
            check(program->uses_jit());

            std::unordered_map<Symbol, float, SymbolHash> inputs;
            inputs[Symbol("a")] = 6.0f;
            inputs[Symbol("b")] = 8.0f;

            check_close(program->evaluate(inputs), 14.0f);
        }

        it("evaluates MIR positional inputs in declared name order") {
            auto program = MirExpressionProgram::compile("left * 10 + right",
                                                         {"right", "left"});
            check(program != nullptr);

            const std::vector<float> float_slots{3.0f, 2.0f};
            check_close(program->evaluate_slots(float_slots), 23.0f);

            const std::vector<double> double_slots{7.0, 4.0};
            check(program->evaluate_slots(double_slots) == 47.0);
        }

        it("rejects invalid MIR positional input buffers") {
            auto program = MirExpressionProgram::compile("a + b", {"a", "b"});
            check(program != nullptr);

            const std::vector<double> too_short{1.0};
            check_throws_as(program->evaluate_slots(too_short), std::invalid_argument);
            check_throws_as(program->evaluate_slots(
                                static_cast<const double*>(nullptr), 2),
                            std::invalid_argument);
        }

        it("updates values used by a cached Expr program") {
            Expr expression;
            check(expression.add_variable("x", 2.0f));
            check(expression.add_variable("y", 3.0f));
            check(expression.eval("x * 10 + y") == 23.0);

            expression.set({{"x", 4.0f}, {"y", 7.0f}});
            check(expression.eval("x * 10 + y") == 47.0);
        }

        it("distinguishes a valid zero Expr result from compile failure") {
            Expr expression;
            check(expression.add_variable("x", 4.0f));

            const auto zero = expression.try_eval("x - x");
            check(zero.has_value());
            check(*zero == 0.0);
            check(!expression.try_eval("x + (").has_value());
        }

        it("evaluates a MIR comparison expression program") {
            auto program = MirExpressionProgram::compile("a >= 3 && b < 10", {"a", "b"});
            check(program != nullptr);

            std::unordered_map<Symbol, float, SymbolHash> inputs;
            inputs[Symbol("a")] = 3.0f;
            inputs[Symbol("b")] = 9.0f;
            check_close(program->evaluate(inputs), 1.0f);

            inputs[Symbol("b")] = 12.0f;
            check_close(program->evaluate(inputs), 0.0f);
        }

        it("evaluates MIR logical OR and NOT") {
            auto program = MirExpressionProgram::compile("a || !b", {"a", "b"});
            check(program != nullptr);

            std::unordered_map<Symbol, float, SymbolHash> inputs;
            inputs[Symbol("a")] = 1.0f;
            inputs[Symbol("b")] = 1.0f;
            check_close(program->evaluate(inputs), 1.0f);

            inputs[Symbol("a")] = 0.0f;
            inputs[Symbol("b")] = 1.0f;
            check_close(program->evaluate(inputs), 0.0f);

            inputs[Symbol("b")] = 0.0f;
            check_close(program->evaluate(inputs), 1.0f);
        }

        it("evaluates MIR math functions") {
            auto program = MirExpressionProgram::compile(
                "sin(angle) + cos(angle) + min(a, b) + max(a, b)", {"angle", "a", "b"});
            check(program != nullptr);

            std::unordered_map<Symbol, float, SymbolHash> inputs;
            inputs[Symbol("angle")] = 0.0f;
            inputs[Symbol("a")] = 3.0f;
            inputs[Symbol("b")] = 7.0f;

            check_close(program->evaluate(inputs), 11.0f);
        }

        it("evaluates MIR custom expression helpers") {
            auto program = MirExpressionProgram::compile(
                "clamp(lerp(a, b, t), 0, 10)", {"a", "b", "t"});
            check(program != nullptr);

            std::unordered_map<Symbol, float, SymbolHash> inputs;
            inputs[Symbol("a")] = 2.0f;
            inputs[Symbol("b")] = 18.0f;
            inputs[Symbol("t")] = 0.5f;

            check_close(program->evaluate(inputs), 10.0f);
        }

        it("evaluates MIR word logical operators and power") {
            auto program = MirExpressionProgram::compile(
                "value > 3 and not disabled or pow(value, 2) == 4", {"value", "disabled"});
            check(program != nullptr);

            std::unordered_map<Symbol, float, SymbolHash> inputs;
            inputs[Symbol("value")] = 4.0f;
            inputs[Symbol("disabled")] = 0.0f;
            check_close(program->evaluate(inputs), 1.0f);

            inputs[Symbol("value")] = 2.0f;
            inputs[Symbol("disabled")] = 1.0f;
            check_close(program->evaluate(inputs), 1.0f);

            inputs[Symbol("value")] = 1.0f;
            check_close(program->evaluate(inputs), 0.0f);
        }

        it("updates property from input binding") {
            ArenaAllocator alloc(4096);
            auto node = Group::create(alloc);

            BindingContext ctx;
            Binding binding = Binding::input("alpha");
            ctx.add_binding(node, "opacity", binding);
            ctx.set_input(Symbol("alpha"), 0.25f);
            ctx.evaluate();

            check_close(node->opacity(), 0.25f);
        }

        it("evaluates expression binding") {
            ArenaAllocator alloc(4096);
            auto node = Group::create(alloc);

            BindingContext ctx;
            Binding binding = Binding::exprtk("1 + 1");
            ctx.add_binding(node, "x", binding);
            ctx.evaluate();

            check_close(node->x(), 2.0f);
        }

        it("marks time expression dirty on time advance") {
            ArenaAllocator alloc(4096);
            auto node = Group::create(alloc);

            BindingContext ctx;
            ctx.add_binding(node, "x", Binding::exprtk("time"));
            ctx.evaluate();
            ctx.clear_dirty();

            ctx.advance_time(1.5f);
            check(ctx.is_dirty());
            ctx.evaluate();

            check_close(node->x(), 1.5f);
        }

        it("keeps component props and geometry in sync") {
            auto comp = Component::create("TestBadgeBinding");
            comp->add_prop("width", 10.0f);
            comp->add_prop("height", 10.0f);
            comp->set_builder([](const Props& props) {
                auto shape = std::make_shared<Shape>();
                float w = get_prop_float(props, "width", 10.0f);
                float h = get_prop_float(props, "height", 10.0f);
                shape->set_rect(w, h);
                shape->set_layout_width(w);
                shape->set_layout_height(h);
                return shape;
            });
            ComponentRegistry::instance().register_component(comp);

            ArenaAllocator alloc(4096);
            auto* parent = Group::create(alloc);

            Props base_props;
            base_props["width"] = 10.0f;
            base_props["height"] = 10.0f;
            auto badge_node = comp->instantiate(base_props);
            check(badge_node != nullptr);
            badge_node->set_id("badge");
            badge_node->set_position(10.0f, 20.0f);
            parent->add_child(badge_node.get());

            auto* box = Shape::create(alloc);
            box->set_id("box");
            parent->add_child(box);

            BindingContext ctx;
            ComponentPropBinding binding_def;
            binding_def.component_name = "TestBadgeBinding";
            binding_def.node = badge_node;
            binding_def.base_props = base_props;
            binding_def.prop_bindings["width"] = Binding::input("badgeW");
            binding_def.prop_bindings["height"] = Binding::input("badgeH");
            ctx.add_component_binding(binding_def);

            ctx.add_binding(box, "width", Binding::input("boxW"));
            ctx.add_binding(box, "height", Binding::input("boxH"));

            ctx.set_input(Symbol("badgeW"), 40.0f);
            ctx.set_input(Symbol("badgeH"), 12.0f);
            ctx.set_input(Symbol("boxW"), 30.0f);
            ctx.set_input(Symbol("boxH"), 14.0f);
            ctx.evaluate();

            auto* badge = parent->find("badge");
            check(badge != nullptr);
            auto* badge_shape = dynamic_cast<Shape*>(badge);
            check(badge_shape != nullptr);

            check(box != nullptr);
            check_close(box->layout_width(), 30.0f);
            check_close(box->layout_height(), 14.0f);
            check_close(box->rect().width, 30.0f);
            check_close(box->rect().height, 14.0f);

            ctx.set_input(Symbol("badgeW"), 60.0f);
            ctx.evaluate();

            auto* badge_after = parent->find("badge");
            check(badge_after != nullptr);
            check(badge_after != badge);
            check_close(badge_after->x(), 10.0f);
            check_close(badge_after->y(), 20.0f);
            auto* badge_shape_after = dynamic_cast<Shape*>(badge_after);
            check(badge_shape_after != nullptr);
            check_close(badge_shape_after->rect().width, 60.0f);

            ctx.set_input(Symbol("boxW"), 50.0f);
            ctx.set_input(Symbol("boxH"), 22.0f);
            ctx.evaluate();

            auto* box_after = dynamic_cast<Shape*>(parent->find("box"));
            check(box_after != nullptr);
            check_close(box_after->layout_width(), 50.0f);
            check_close(box_after->layout_height(), 22.0f);
            check_close(box_after->rect().width, 50.0f);
            check_close(box_after->rect().height, 22.0f);
        }
    }

    group("easing") {
        it("evaluates linear") {
            Easing easing = Easing::linear();

            check_close(easing.evaluate(0.0f), 0.0f);
            check_close(easing.evaluate(0.5f), 0.5f);
            check_close(easing.evaluate(1.0f), 1.0f);
        }

        it("evaluates ease in") {
            Easing easing = Easing::ease_in();

            check_close(easing.evaluate(0.0f), 0.0f);
            check(easing.evaluate(0.5f) < 0.5f);
            check_close(easing.evaluate(1.0f), 1.0f);
        }

        it("evaluates ease out") {
            Easing easing = Easing::ease_out();

            check_close(easing.evaluate(0.0f), 0.0f);
            check(easing.evaluate(0.5f) > 0.5f);
            check_close(easing.evaluate(1.0f), 1.0f);
        }

        it("evaluates ease in out") {
            Easing easing = Easing::ease_in_out();

            check_close(easing.evaluate(0.0f), 0.0f);
            check_close(easing.evaluate(0.5f), 0.5f, 0.05f);
            check_close(easing.evaluate(1.0f), 1.0f);
        }
    }

    group("color") {
        it("stores rgba components") {
            Color color = {0.2f, 0.4f, 0.6f, 0.8f};

            check_close(color.r, 0.2f);
            check_close(color.g, 0.4f);
            check_close(color.b, 0.6f);
            check_close(color.a, 0.8f);
        }
    }
}
