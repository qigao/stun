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
#include <tinytest.hpp>

using namespace flex;

namespace {

inline void check_close(float actual, float expected, float eps = 0.001f) {
    check_within(actual, expected, eps);
}

} // namespace

suite("flex::runtime") {
    group("allocator") {
        it("fails fast when a pool vector grows without an allocator") {
            PoolVector<int> values;

            check_throws_as(values.push_back(1), std::logic_error);
        }

        it("preserves non-trivial values while growing pool storage") {
            ArenaAllocator alloc(4096);
            PoolVector<std::unique_ptr<int>> values(alloc);

            values.push_back(std::make_unique<int>(10));
            values.push_back(std::make_unique<int>(20));
            values.push_back(std::make_unique<int>(30));

            check_equal(values.size(), 3);
            check_equal(*values[0], 10);
            check_equal(*values[1], 20);
            check_equal(*values[2], 30);
        }
    }

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

        it("releases an owned child safely when removing by index") {
            auto group = Group::create();
            auto child = Group::create();
            std::weak_ptr<Group> lifetime = child;

            group->add_child(child);
            child.reset();
            check_false(lifetime.expired());

            group->remove_child_at(0);
            check_equal(group->child_count(), 0);
            check_true(lifetime.expired());
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

    group("animated property metadata") {
        it("resolves canonical names and compatibility aliases") {
            check_equal(static_cast<int>(get_property_id("x")),
                         static_cast<int>(PropertyID::X));
            check_equal(static_cast<int>(get_property_id("fontSize")),
                         static_cast<int>(PropertyID::FontSize));
            check_equal(static_cast<int>(get_property_id("font_size")),
                         static_cast<int>(PropertyID::FontSize));
            check_equal(static_cast<int>(get_property_id("scaleX")),
                         static_cast<int>(PropertyID::ScaleX));
            check_equal(static_cast<int>(get_property_id("scale_x")),
                         static_cast<int>(PropertyID::ScaleX));
            check_equal(static_cast<int>(get_property_id("text.color")),
                         static_cast<int>(PropertyID::TextColor));
        }

        it("rejects empty and unknown property names") {
            check_equal(static_cast<int>(get_property_id(nullptr)),
                         static_cast<int>(PropertyID::Unknown));
            check_equal(static_cast<int>(get_property_id("")),
                         static_cast<int>(PropertyID::Unknown));
            check_equal(static_cast<int>(get_property_id("widht")),
                         static_cast<int>(PropertyID::Unknown));
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

        it("interpolates color keyframes continuously") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("color", alloc);
            auto* track = timeline->add_track("fill").get();
            track->add_keyframe(0.0f, Color{0.0f, 0.0f, 0.0f, 0.0f});
            track->add_keyframe(2.0f, Color{1.0f, 0.5f, 0.25f, 1.0f});

            const Color value = std::get<Color>(track->sample(1.0f));
            check_close(value.r, 0.5f);
            check_close(value.g, 0.25f);
            check_close(value.b, 0.125f);
            check_close(value.a, 0.5f);
        }

        it("samples typed position keyframes without splitting x and y tracks") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("position", alloc);
            auto* track = timeline->add_track("position").get();
            track->add_keyframe(0.0f, Vec2{0.0f, 10.0f});
            track->add_keyframe(2.0f, Vec2{20.0f, 30.0f});

            const Vec2 value = std::get<Vec2>(track->sample(1.0f));
            check_close(value.x, 10.0f);
            check_close(value.y, 20.0f);

            auto node = Group::create(alloc);
            timeline->apply(node, 1.0f);
            check_close(node->x(), 10.0f);
            check_close(node->y(), 20.0f);
        }

        it("resolves a replaced descendant on every apply") {
            ArenaAllocator alloc(8192);
            auto *root = Group::create(alloc);
            auto *first = Shape::create(alloc);
            first->set_id("animated");
            root->add_child(first);

            auto timeline = Timeline::create("replaceTarget", alloc);
            auto *track = timeline->add_track("#animated/x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 20.0f);

            timeline->apply(root, 0.5f);
            check_close(first->x(), 10.0f);

            root->remove_child(first);
            auto *replacement = Shape::create(alloc);
            replacement->set_id("animated");
            root->add_child(replacement);

            timeline->apply(root, 1.0f);
            check_close(first->x(), 10.0f);
            check_close(replacement->x(), 20.0f);
        }

        it("preserves track order while reusing adjacent target resolution") {
            ArenaAllocator alloc(8192);
            auto *root = Group::create(alloc);
            auto *first = Shape::create(alloc);
            auto *second = Shape::create(alloc);
            first->set_id("first");
            second->set_id("second");
            root->add_child(first);
            root->add_child(second);

            auto timeline = Timeline::create("targetRuns", alloc);
            timeline->add_track("#first/x")->add_keyframe(0.0f, 10.0f);
            timeline->add_track("#first/y")->add_keyframe(0.0f, 20.0f);
            timeline->add_track("#second/x")->add_keyframe(0.0f, 30.0f);
            timeline->add_track("#first/opacity")->add_keyframe(0.0f, 0.25f);

            timeline->apply(root, 0.0f);

            check_close(first->x(), 10.0f);
            check_close(first->y(), 20.0f);
            check_close(first->opacity(), 0.25f);
            check_close(second->x(), 30.0f);
        }

        it("rebuilds a player execution plan after target replacement") {
            ArenaAllocator alloc(8192);
            auto *root = Group::create(alloc);
            auto *nested = Group::create(alloc);
            auto *first = Shape::create(alloc);
            first->set_id("animated");
            nested->add_child(first);
            root->add_child(nested);

            auto timeline = Timeline::create("replacePlayerTarget", alloc);
            timeline->set_duration(1.0f);
            auto *track = timeline->add_track("#animated/x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 20.0f);

            TimelinePlayer player(timeline.get(), root);
            player.play();
            player.advance(0.25f);
            player.apply();
            check_close(first->x(), 5.0f);

            nested->remove_child(first);
            auto *replacement = Shape::create(alloc);
            replacement->set_id("animated");
            nested->add_child(replacement);

            player.advance(0.25f);
            player.apply();
            check_close(first->x(), 5.0f);
            check_close(replacement->x(), 10.0f);
        }

        it("invalidates a player execution plan after target id changes") {
            ArenaAllocator alloc(8192);
            auto *root = Group::create(alloc);
            auto *first = Shape::create(alloc);
            first->set_id("animated");
            root->add_child(first);

            auto timeline = Timeline::create("renamePlayerTarget", alloc);
            timeline->set_duration(1.0f);
            auto *track = timeline->add_track("#animated/x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 20.0f);

            TimelinePlayer player(timeline.get(), root);
            player.play();
            player.advance(0.25f);
            player.apply();
            check_close(first->x(), 5.0f);

            first->set_id("renamed");
            player.advance(0.25f);
            player.apply();
            check_close(first->x(), 5.0f);

            auto *replacement = Shape::create(alloc);
            replacement->set_id("animated");
            root->add_child(replacement);
            player.advance(0.25f);
            player.apply();
            check_close(replacement->x(), 15.0f);
        }

        it("extends a player execution plan when tracks are appended") {
            ArenaAllocator alloc(8192);
            auto *root = Group::create(alloc);
            auto *shape = Shape::create(alloc);
            shape->set_id("animated");
            root->add_child(shape);

            auto timeline = Timeline::create("appendTrack", alloc);
            timeline->set_duration(1.0f);
            auto *x_track = timeline->add_track("#animated/x").get();
            x_track->add_keyframe(0.0f, 0.0f);
            x_track->add_keyframe(1.0f, 20.0f);

            TimelinePlayer player(timeline.get(), root);
            player.play();
            player.advance(0.25f);
            player.apply();
            check_close(shape->x(), 5.0f);

            auto *y_track = timeline->add_track("#animated/y").get();
            y_track->add_keyframe(0.0f, 0.0f);
            y_track->add_keyframe(1.0f, 40.0f);

            player.advance(0.25f);
            player.apply();
            check_close(shape->x(), 10.0f);
            check_close(shape->y(), 20.0f);
        }

        it("compiles track metadata into an immutable animation program") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("compiledMetadata", alloc);
            auto* track = timeline->add_track("#animated/x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 10.0f);

            const AnimationProgram& program = timeline->program();
            check_equal(program.track_count(), 1);
            check_equal(program.operations().size(), 1);

            const auto& operation = program.operations()[0];
            check_equal(operation.keyframe_offset, 0);
            check_equal(operation.keyframe_count, 2);
            check_equal(static_cast<int>(operation.value_kind),
                         static_cast<int>(AnimationProgram::ValueKind::Scalar));
            check_equal(static_cast<int>(operation.property_id),
                         static_cast<int>(PropertyID::X));
            check_equal(operation.target_id, "animated");
            check_true(operation.has_target_selector);
            check_equal(program.keyframe_count(), 2);
            check_close(std::get<float>(program.sample(0, 0.5f)), 5.0f);
            check_equal(&timeline->program(), &program);
        }

        it("keeps compiled keyframes isolated until recompilation") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("recompileProgram", alloc);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);

            const AnimationProgram& initial = timeline->program();
            const uint64_t initial_revision = initial.source_revision();
            track->add_keyframe(1.0f, 10.0f);
            check_equal(initial.keyframe_count(), 1);
            check_close(std::get<float>(initial.sample(0, 1.0f)), 0.0f);

            const AnimationProgram& rebuilt = timeline->program();
            check_true(rebuilt.source_revision() > initial_revision);
            check_equal(rebuilt.track_count(), 1);
            check_equal(rebuilt.keyframe_count(), 2);
            check_equal(rebuilt.operations()[0].keyframe_count, 2);
            check_close(std::get<float>(rebuilt.sample(0, 1.0f)), 10.0f);
        }

        it("matches editable sampling for every animation value category") {
            ArenaAllocator alloc(16384);
            auto timeline = Timeline::create("compiledValueParity", alloc);

            auto* number = timeline->add_track("x").get();
            number->add_keyframe(0.0f, 10.0f, Easing::ease_in());
            number->add_keyframe(2.0f, 30.0f);
            number->set_numeric_expression(
                "lerp(from, to, progress * progress) + time");

            auto* color = timeline->add_track("fill").get();
            color->add_keyframe(0.0f, Color{0.0f, 0.0f, 0.0f, 0.0f});
            color->add_keyframe(2.0f, Color{1.0f, 0.5f, 0.25f, 1.0f});

            auto* position = timeline->add_track("position").get();
            position->add_keyframe(0.0f, Vec2{0.0f, 10.0f});
            position->add_keyframe(2.0f, Vec2{20.0f, 30.0f});

            auto* text = timeline->add_track("text").get();
            text->add_keyframe(0.0f, "first");
            text->add_keyframe(2.0f, "second");

            const AnimationProgram& program = timeline->program();
            check_equal(program.track_count(), 4);
            check_equal(program.keyframe_count(), 8);
            check_equal(program.scalar_keyframe_count(), 2);
            check_equal(program.color_keyframe_count(), 2);
            check_equal(program.vec2_keyframe_count(), 2);
            check_equal(program.generic_keyframe_count(), 2);
            for (size_t index = 0; index < program.track_count(); ++index) {
                check_equal(program.operations()[index].keyframe_offset,
                              index * 2);
                check_equal(program.operations()[index].keyframe_count, 2);
            }

            check_close(std::get<float>(program.sample(0, 1.0f)),
                        std::get<float>(number->sample(1.0f)));

            const Color compiled_color = std::get<Color>(program.sample(1, 1.0f));
            const Color editable_color = std::get<Color>(color->sample(1.0f));
            check_close(compiled_color.r, editable_color.r);
            check_close(compiled_color.g, editable_color.g);
            check_close(compiled_color.b, editable_color.b);
            check_close(compiled_color.a, editable_color.a);

            const Vec2 compiled_position =
                std::get<Vec2>(program.sample(2, 1.0f));
            const Vec2 editable_position =
                std::get<Vec2>(position->sample(1.0f));
            check_close(compiled_position.x, editable_position.x);
            check_close(compiled_position.y, editable_position.y);

            const auto compiled_text =
                std::get<std::string>(program.sample(3, 1.0f));
            const auto editable_text =
                std::get<std::string>(text->sample(1.0f));
            check_equal(compiled_text, editable_text);
        }

        it("preserves interpolation inside a heterogeneous generic track") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("compiledGenericParity", alloc);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 10.0f);
            track->add_keyframe(2.0f, "done");

            const AnimationProgram& program = timeline->program();
            check_equal(
                static_cast<int>(program.operations()[0].value_kind),
                static_cast<int>(AnimationProgram::ValueKind::Generic));
            check_equal(program.generic_keyframe_count(), 3);
            check_close(std::get<float>(program.sample(0, 0.5f)), 5.0f);
            const auto sampled_text =
                std::get<std::string>(program.sample(0, 1.5f));
            check_equal(sampled_text, "done");
        }

        it("samples all compiled tracks into caller-owned storage") {
            ArenaAllocator alloc(8192);
            auto timeline = Timeline::create("compiledBatch", alloc);
            auto* number = timeline->add_track("x").get();
            number->add_keyframe(0.0f, 0.0f);
            number->add_keyframe(1.0f, 10.0f);
            auto* position = timeline->add_track("position").get();
            position->add_keyframe(0.0f, Vec2{0.0f, 10.0f});
            position->add_keyframe(1.0f, Vec2{20.0f, 30.0f});
            auto* text = timeline->add_track("text").get();
            text->add_keyframe(0.0f, "first");
            text->add_keyframe(1.0f, "second");

            const AnimationProgram& program = timeline->program();
            std::vector<AnimValue> outputs(program.track_count());
            program.sample_all(0.5f, outputs.data(), outputs.size());
            check_close(std::get<float>(outputs[0]),
                        std::get<float>(program.sample(0, 0.5f)));
            const Vec2 batch_position = std::get<Vec2>(outputs[1]);
            const Vec2 single_position =
                std::get<Vec2>(program.sample(1, 0.5f));
            check_close(batch_position.x, single_position.x);
            check_close(batch_position.y, single_position.y);
            check_equal(std::get<std::string>(outputs[2]), "second");
        }

        it("validates caller-owned batch storage before writing") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("compiledBatchBounds", alloc);
            timeline->add_track("x")->add_keyframe(0.0f, 1.0f);
            const AnimationProgram& program = timeline->program();
            AnimValue unchanged = 42.0f;

            check_throws_as(program.sample_all(0.0f, &unchanged, 0),
                            std::invalid_argument);
            check_close(std::get<float>(unchanged), 42.0f);
            check_throws_as(program.sample_all(0.0f, nullptr, 1),
                            std::invalid_argument);

            auto empty = Timeline::create("compiledEmptyBatch", alloc);
            check_nothrow(empty->program().sample_all(0.0f, nullptr, 0));
        }

        it("retains a compiled MIR expression until program replacement") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("compiledExpressionSnapshot", alloc);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 10.0f);
            track->add_keyframe(2.0f, 30.0f);
            track->set_numeric_expression(
                "lerp(from, to, progress * progress) + time");

            const AnimationProgram& initial = timeline->program();
            check_close(std::get<float>(initial.sample(0, 1.0f)), 16.0f);

            track->clear_numeric_expression();
            check_close(std::get<float>(initial.sample(0, 1.0f)), 16.0f);

            const AnimationProgram& rebuilt = timeline->program();
            check_close(std::get<float>(rebuilt.sample(0, 1.0f)), 20.0f);
        }

        it("rejects sampling an operation outside the compiled program") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("programBounds", alloc);
            timeline->add_track("x")->add_keyframe(0.0f, 1.0f);

            const AnimationProgram& program = timeline->program();
            check_throws_as(program.sample(1, 0.0f), std::out_of_range);
        }

        it("preserves the empty track default in a compiled program") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("emptyCompiledTrack", alloc);
            timeline->add_track("x");

            const AnimationProgram& program = timeline->program();
            check_equal(program.keyframe_count(), 0);
            check_close(std::get<float>(program.sample(0, 0.5f)), 0.0f);
        }

        it("samples a smooth Catmull-Rom motion path through vec2 keyframes") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("curve", alloc);
            auto* track = timeline->add_track("position").get();
            track->set_spatial_interpolation(SpatialInterpolation::CatmullRom);
            track->add_keyframe(0.0f, Vec2{0.0f, 0.0f});
            track->add_keyframe(1.0f, Vec2{10.0f, 10.0f});
            track->add_keyframe(2.0f, Vec2{20.0f, 10.0f});
            track->add_keyframe(3.0f, Vec2{30.0f, 0.0f});

            const Vec2 midpoint = std::get<Vec2>(track->sample(1.5f));
            check_close(midpoint.x, 15.0f);
            check_close(midpoint.y, 11.25f);
            const Vec2 compiled =
                std::get<Vec2>(timeline->program().sample(0, 1.5f));
            check_close(compiled.x, midpoint.x);
            check_close(compiled.y, midpoint.y);
        }

        it("samples explicit cubic Bezier spatial tangents") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("bezierCurve", alloc);
            auto* track = timeline->add_track("position").get();
            track->set_spatial_interpolation(SpatialInterpolation::CubicBezier);
            track->add_spatial_keyframe(0.0f, Vec2{0.0f, 0.0f},
                                        Vec2{0.0f, 0.0f}, Vec2{0.0f, 10.0f});
            track->add_spatial_keyframe(1.0f, Vec2{10.0f, 0.0f},
                                        Vec2{0.0f, 10.0f}, Vec2{0.0f, 0.0f});

            const Vec2 midpoint = std::get<Vec2>(track->sample(0.5f));
            check_close(midpoint.x, 5.0f);
            check_close(midpoint.y, 7.5f);
            const Vec2 compiled =
                std::get<Vec2>(timeline->program().sample(0, 0.5f));
            check_close(compiled.x, midpoint.x);
            check_close(compiled.y, midpoint.y);
        }

        it("rejects missing cubic Bezier spatial tangents at sampling") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("invalidBezierCurve", alloc);
            auto* track = timeline->add_track("position").get();
            track->set_spatial_interpolation(SpatialInterpolation::CubicBezier);
            track->add_keyframe(0.0f, Vec2{0.0f, 0.0f});
            track->add_keyframe(1.0f, Vec2{10.0f, 0.0f});

            check_throws_as(track->sample(0.5f), std::logic_error);
            check_throws_as(timeline->program().sample(0, 0.5f),
                            std::logic_error);
        }

        it("blends a position track as one coherent vector") {
            ArenaAllocator alloc(4096);
            auto timeline = Timeline::create("positionBlend", alloc);
            timeline->set_duration(1.0f);
            auto* track = timeline->add_track("position").get();
            track->add_keyframe(0.0f, Vec2{0.0f, 0.0f});
            track->add_keyframe(1.0f, Vec2{30.0f, 40.0f});

            auto node = Group::create(alloc);
            TimelinePlayer player(timeline.get(), node);
            player.set_blend_weight(0.5f);
            player.play();
            player.advance(1.0f);
            player.apply();

            check_close(node->x(), 15.0f);
            check_close(node->y(), 20.0f);
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

        it("returns active players only for the requested target") {
            ArenaAllocator alloc(8192);
            AnimationController controller(alloc);
            auto first = Timeline::create("firstTargetQuery", alloc);
            auto second = Timeline::create("secondTargetQuery", alloc);
            first->set_duration(1.0f);
            second->set_duration(1.0f);
            controller.add_timeline(first);
            controller.add_timeline(second);

            auto requested = Group::create();
            auto other = Group::create();
            auto* first_player = controller.play(first->name(), requested.get());
            auto* second_player = controller.play(second->name(), requested.get());
            controller.play(first->name(), other.get());

            const auto players =
                controller.get_players_for_target(requested.get());
            check_equal(players.size(), 2);
            check_equal(players[0], first_player);
            check_equal(players[1], second_player);
        }

        it("plays through a default-constructed controller") {
            ArenaAllocator timeline_alloc(4096);
            AnimationController controller;
            auto timeline = Timeline::create("defaultController", timeline_alloc);
            timeline->set_duration(1.0f);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 10.0f);
            controller.add_timeline(timeline);

            auto target = Group::create();
            auto* player = controller.play(timeline->name(), target.get());
            check_not_null(player);

            controller.advance(0.5f);
            check_close(target->x(), 5.0f);
            check_equal(controller.get_players_for_target(target.get()).size(), 1);
        }

        it("excludes stopped players from a target query") {
            ArenaAllocator alloc(8192);
            AnimationController controller(alloc);
            auto stopped = Timeline::create("stoppedTargetQuery", alloc);
            auto playing = Timeline::create("playingTargetQuery", alloc);
            stopped->set_duration(1.0f);
            playing->set_duration(1.0f);
            controller.add_timeline(stopped);
            controller.add_timeline(playing);

            auto target = Group::create();
            controller.play(stopped->name(), target.get());
            auto* playing_player = controller.play(playing->name(), target.get());
            controller.stop(stopped->name());

            const auto players = controller.get_players_for_target(target.get());
            check_equal(players.size(), 1);
            check_equal(players[0], playing_player);
        }

        it("returns no players for null or destroyed targets") {
            ArenaAllocator alloc(4096);
            AnimationController controller(alloc);
            auto timeline = Timeline::create("destroyedTargetQuery", alloc);
            timeline->set_duration(1.0f);
            controller.add_timeline(timeline);

            auto target = Group::create();
            Node* destroyed_target = target.get();
            controller.play(timeline->name(), destroyed_target);
            target.reset();

            check_empty(controller.get_players_for_target(nullptr));
            check_empty(controller.get_players_for_target(destroyed_target));
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

        it("stops before accessing a destroyed shared timeline") {
            ArenaAllocator alloc(4096);
            auto target = Group::create();
            auto timeline = Timeline::create("destroyedTimeline", alloc);
            timeline->set_duration(1.0f);
            auto* track = timeline->add_track("x").get();
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 10.0f);

            TimelinePlayer player(timeline.get(), target.get());
            player.play();
            timeline.reset();

            check_false(player.advance(0.5f));
            player.apply();
            check_null(player.timeline());
            check(player.target() == target.get());
            check_false(player.is_playing());
            check(player.is_finished());
            check_close(player.normalized_time(), 0.0f);
        }

        it("stops before accessing a destroyed stack timeline") {
            ArenaAllocator alloc(4096);
            auto target = Group::create();
            std::unique_ptr<TimelinePlayer> player;
            {
                Timeline timeline("stackTimeline", alloc);
                timeline.set_duration(1.0f);
                auto* track = timeline.add_track("x").get();
                track->add_keyframe(0.0f, 0.0f);
                track->add_keyframe(1.0f, 10.0f);

                player = std::make_unique<TimelinePlayer>(&timeline, target.get());
                player->play();
            }

            check_false(player->advance(0.5f));
            player->apply();
            check_null(player->timeline());
            check(player->target() == target.get());
            check_false(player->is_playing());
            check(player->is_finished());
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

        it("reuses a shared MIR program and reports compilation failure") {
            std::shared_ptr<void> compiled;
            std::unordered_map<Symbol, float, SymbolHash> inputs;
            inputs[Symbol("x")] = 2.0f;

            float result = -1.0f;
            check(evaluate_mir_expression_inputs("x - x", compiled, inputs, result));
            check_close(result, 0.0f);
            void *first_program = compiled.get();

            inputs[Symbol("x")] = 9.0f;
            check(evaluate_mir_expression_inputs("x - x", compiled, inputs, result));
            check(compiled.get() == first_program);
            check_close(result, 0.0f);
            check_false(evaluate_mir_expression_inputs("x + 1", compiled, inputs, result));

            std::shared_ptr<void> invalid;
            check_false(evaluate_mir_expression_inputs("x + (", invalid, inputs, result));
            check(invalid == nullptr);
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

        it("evaluates animation-oriented MIR helpers") {
            auto program = MirExpressionProgram::compile(
                "select(enabled, lerp(from, to, smoothstep(0, 1, progress)), "
                "step(0.5, saturate(progress)))",
                {"enabled", "from", "to", "progress"});
            check(program != nullptr);
            if (!program) {
                return;
            }

            check_close(program->evaluate_slots(
                            std::vector<float>{1.0f, 10.0f, 30.0f, 0.5f}),
                        20.0f);
            check_close(program->evaluate_slots(
                            std::vector<float>{0.0f, 10.0f, 30.0f, 0.75f}),
                        1.0f);
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

        it("compiles MIR derivatives for arithmetic and functions") {
            auto program = MirExpressionProgram::compile(
                "derivative(sin(time * 2) + time ^ 3, time)", {"time"});
            check(program != nullptr);
            if (!program) return;

            check_close(program->evaluate_slots(std::vector<float>{2.0f}),
                        static_cast<float>(2.0 * std::cos(4.0) + 12.0));
        }

        it("supports nested MIR derivatives") {
            auto program = MirExpressionProgram::compile(
                "derivative(derivative(time ^ 4, time), time)", {"time"});
            check(program != nullptr);
            if (!program) return;

            check_close(program->evaluate_slots(std::vector<float>{2.0f}), 48.0f);
        }

        it("differentiates the MIR elementary function set") {
            auto program = MirExpressionProgram::compile(
                "derivative(tan(time) + sqrt(time) + exp(time) + log(time) + "
                "pow(time, time) + pow(time, 0), time)",
                {"time"});
            check(program != nullptr);
            if (!program) return;

            const double expected = 1.0 / (std::cos(1.0) * std::cos(1.0)) +
                                    0.5 + std::exp(1.0) + 1.0 + 1.0;
            check_close(program->evaluate_slots(std::vector<float>{1.0f}),
                        static_cast<float>(expected));
        }

        it("defines abs derivatives at smooth and corner points") {
            auto program = MirExpressionProgram::compile(
                "derivative(abs(time), time)", {"time"});
            check(program != nullptr);
            if (!program) return;

            check_close(program->evaluate_slots(std::vector<float>{0.5f}), 1.0f);
            check_close(program->evaluate_slots(std::vector<float>{-1.0f}), -1.0f);
            check_close(program->evaluate_slots(std::vector<float>{0.0f}), 0.0f);
        }

        it("defines clamp derivatives by active branch") {
            auto program = MirExpressionProgram::compile(
                "derivative(clamp(time, 0, 1), time)", {"time"});
            check(program != nullptr);
            if (!program) return;

            check_close(program->evaluate_slots(std::vector<float>{0.5f}), 1.0f);
            check_close(program->evaluate_slots(std::vector<float>{-1.0f}), 0.0f);
        }

        it("defines smoothstep derivatives by active branch") {
            auto program = MirExpressionProgram::compile(
                "derivative(smoothstep(0, 1, time), time)", {"time"});
            check(program != nullptr);
            if (!program) return;

            check_close(program->evaluate_slots(std::vector<float>{0.5f}), 1.5f);
            check_close(program->evaluate_slots(std::vector<float>{-1.0f}), 0.0f);

            auto equal_edges = MirExpressionProgram::compile(
                "derivative(smoothstep(1, 1, time), time)", {"time"});
            check(equal_edges != nullptr);
            if (equal_edges) {
                check_close(equal_edges->evaluate_slots(std::vector<float>{1.0f}), 0.0f);
            }
        }

        it("rejects discontinuous modulo derivatives and invalid variables") {
            check(MirExpressionProgram::compile(
                      "derivative(fmod(time, 2), time)", {"time"}) == nullptr);
            check(MirExpressionProgram::compile(
                      "derivative(time, time + 0)", {"time"}) == nullptr);
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

        it("keeps one active type for each input name") {
            BindingContext ctx;
            const Symbol value("value");

            ctx.set_input(value, 3.0f);
            check(ctx.has_input(value));
            check_close(ctx.get_float_input(value), 3.0f);

            ctx.set_input(value, std::string("ready"));
            check_close(ctx.get_float_input(value), 0.0f);
            check(ctx.get_string_input(value) == "ready");

            ctx.set_input(value, true);
            check(ctx.get_string_input(value).empty());
            check(ctx.get_bool_input(value));
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
