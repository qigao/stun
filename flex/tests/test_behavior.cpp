/*
 * Flex behavior verification tests
 *
 * Verifies backend-neutral runtime contracts:
 * - layout drives final render positions
 * - timeline animation changes rendered output
 * - retained mode reuses cached draw objects and only updates transforms
 */

#include "tinytest.h"
#include "test_render_trace.h"

using namespace flex;
using namespace flex::test_support;

suite("flex::behavior") {
    group("layout rendering") {
        it("produces stable draw positions") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(400.0f, 200.0f, arena);

            auto container = Group::create(arena);
            container->set_position(20.0f, 10.0f);
            container->set_layout_width(200.0f);
            container->set_layout_height(80.0f);
            container->set_layout(LayoutMode::Flex);
            container->set_gap(10.0f);
            container->set_padding(5.0f);
            container->set_clip(true);
            scene->add_child(container);

            auto first = Shape::create(arena);
            first->set_rect(50.0f, 20.0f);
            first->set_layout_width(50.0f);
            first->set_layout_height(20.0f);
            first->set_fill(Color{1.0f, 0.0f, 0.0f, 1.0f});
            container->add_child(first);

            auto second = Shape::create(arena);
            second->set_rect(30.0f, 20.0f);
            second->set_layout_width(30.0f);
            second->set_layout_height(20.0f);
            second->set_fill(Color{0.0f, 0.0f, 1.0f, 1.0f});
            container->add_child(second);

            RecordingRenderer renderer;
            scene->render(renderer);

            check_float_eq(first->x(), 5.0, 0.001);
            check_float_eq(first->y(), 5.0, 0.001);
            check_float_eq(second->x(), 65.0, 0.001);
            check_float_eq(second->y(), 5.0, 0.001);

            auto rects = renderer.find_all("draw_rect");
            check(rects.size() == 2);
            check_float_eq(rects[0].args[0], 25.0, 0.001);
            check_float_eq(rects[0].args[1], 15.0, 0.001);
            check_float_eq(rects[1].args[0], 85.0, 0.001);
            check_float_eq(rects[1].args[1], 15.0, 0.001);

            int clip_index = renderer.first_index("clip_rect");
            int first_rect_index = renderer.first_index("draw_rect");
            check(clip_index >= 0);
            check(first_rect_index > clip_index);
        }

        it("renders fixed children in viewport space under transformed ancestors") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto container = Group::create(arena);
            container->set_position(120.0f, 70.0f);
            container->set_rotation(90.0f);
            container->set_scale(2.0f, 1.5f);
            container->set_clip(true);
            container->set_layout_width(80.0f);
            container->set_layout_height(60.0f);
            scene->add_child(container);

            auto fixed = Shape::create(arena);
            fixed->set_rect(30.0f, 20.0f);
            fixed->set_layout_size(30.0f, 20.0f);
            fixed->set_fill(Color{0.8f, 0.1f, 0.2f, 1.0f});
            fixed->set_position_mode(PositionMode::Fixed);
            fixed->set_position_offsets(25.0f, NAN, NAN, 15.0f);
            container->add_child(fixed);

            container->perform_layout();

            RecordingRenderer renderer;
            scene->render(renderer);

            auto rects = renderer.find_all("draw_rect");
            check(rects.size() == 1);
            if (rects.size() != 1) {
                return;
            }

            check_float_eq(rects[0].args[0], 15.0f, 0.001f);
            check_float_eq(rects[0].args[1], 25.0f, 0.001f);

            auto world = fixed->to_world(Vec2{0.0f, 0.0f});
            check_float_eq(world.x, 15.0f, 0.001f);
            check_float_eq(world.y, 25.0f, 0.001f);
        }
    }

    group("animation rendering") {
        it("changes rendered output after timeline advance") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(200.0f, 100.0f, arena);

            auto shape = Shape::create(arena);
            shape->set_rect(20.0f, 20.0f);
            shape->set_layout_width(20.0f);
            shape->set_layout_height(20.0f);
            shape->set_fill(Color{0.2f, 0.8f, 0.4f, 1.0f});
            scene->add_child(shape);

            AnimationController controller(arena);
            auto timeline = Timeline::create("moveX", arena);
            timeline->set_duration(1.0f);
            auto* track = timeline->add_track("x").get();
            check(track != nullptr);
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 100.0f);
            controller.add_timeline(timeline);

            auto* player = controller.play("moveX", shape);
            check(player != nullptr);
            controller.advance(0.5f);

            RecordingRenderer renderer;
            scene->render(renderer);

            auto rects = renderer.find_all("draw_rect");
            check(rects.size() == 1);
            check_float_eq(shape->x(), 50.0, 0.001);
            check_float_eq(rects[0].args[0], 50.0, 0.001);
            check_float_eq(rects[0].args[1], 0.0, 0.001);
        }
    }

    group("retained mode") {
        it("only rebuilds cached paint when required") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(200.0f, 100.0f, arena);

            auto shape = Shape::create(arena);
            shape->set_rect(40.0f, 12.0f);
            shape->set_fill(Color{0.9f, 0.4f, 0.1f, 1.0f});
            scene->add_child(shape);

            RecordingRenderer renderer;
            renderer.set_retained_mode(true);

            scene->render(renderer);
            check(renderer.count("push_rect") == 1);
            check(renderer.count("update_transform") == 0);
            check(renderer.count("remove_cached") == 0);

            renderer.clear_ops();
            scene->render(renderer);
            check(renderer.count("push_rect") == 0);
            check(renderer.count("update_transform") == 0);
            check(renderer.count("remove_cached") == 0);

            shape->set_position(30.0f, 8.0f);
            renderer.clear_ops();
            scene->render(renderer);
            check(renderer.count("push_rect") == 0);
            check(renderer.count("update_transform") == 1);

            auto updates = renderer.find_all("update_transform");
            check(updates.size() == 1);
            check_float_eq(tx(updates[0].transform), 30.0, 0.001);
            check_float_eq(ty(updates[0].transform), 8.0, 0.001);
        }

        it("reuses the same cached handle for transform-only updates") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(120.0f, 80.0f, arena);

            auto shape = Shape::create(arena);
            shape->set_rect(24.0f, 12.0f);
            shape->set_fill(Color{0.8f, 0.3f, 0.2f, 1.0f});
            scene->add_child(shape);

            RecordingRenderer renderer;
            renderer.set_retained_mode(true);

            scene->render(renderer);
            auto pushes = renderer.find_all("push_rect");
            check(pushes.size() == 1);
            if (pushes.size() != 1) {
                return;
            }
            PaintHandle initial_handle = pushes[0].handle;

            shape->set_position(18.0f, 6.0f);
            renderer.clear_ops();
            scene->render(renderer);

            check(renderer.count("push_rect") == 0);
            check(renderer.count("remove_cached") == 0);
            check(renderer.count("update_transform") == 1);

            auto updates = renderer.find_all("update_transform");
            check(updates.size() == 1);
            if (updates.size() != 1) {
                return;
            }
            check(updates[0].handle == initial_handle);
            check_float_eq(tx(updates[0].transform), 18.0f, 0.001f);
            check_float_eq(ty(updates[0].transform), 6.0f, 0.001f);
        }

        it("rebuilds cached paint on content changes") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(120.0f, 80.0f, arena);

            auto shape = Shape::create(arena);
            shape->set_rect(24.0f, 12.0f);
            shape->set_fill(Color{0.2f, 0.6f, 0.9f, 1.0f});
            scene->add_child(shape);

            RecordingRenderer renderer;
            renderer.set_retained_mode(true);

            scene->render(renderer);
            auto initial_pushes = renderer.find_all("push_rect");
            check(initial_pushes.size() == 1);
            if (initial_pushes.size() != 1) {
                return;
            }
            PaintHandle initial_handle = initial_pushes[0].handle;

            shape->set_rect(30.0f, 12.0f);
            renderer.clear_ops();
            scene->render(renderer);

            check(renderer.count("remove_cached") == 1);
            check(renderer.count("push_rect") == 1);
            check(renderer.count("update_transform") == 0);

            auto removes = renderer.find_all("remove_cached");
            auto pushes = renderer.find_all("push_rect");
            check(removes.size() == 1);
            check(pushes.size() == 1);
            if (removes.size() != 1 || pushes.size() != 1) {
                return;
            }
            check(removes[0].handle == initial_handle);
            check(pushes[0].handle != initial_handle);
        }

        it("rebuilds cached paint when opacity changes") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(120.0f, 80.0f, arena);

            auto shape = Shape::create(arena);
            shape->set_rect(24.0f, 12.0f);
            shape->set_fill(Color{0.4f, 0.8f, 0.3f, 1.0f});
            scene->add_child(shape);

            RecordingRenderer renderer;
            renderer.set_retained_mode(true);

            scene->render(renderer);
            auto initial_pushes = renderer.find_all("push_rect");
            check(initial_pushes.size() == 1);
            if (initial_pushes.size() != 1) {
                return;
            }
            PaintHandle initial_handle = initial_pushes[0].handle;

            shape->set_opacity(0.25f);
            renderer.clear_ops();
            scene->render(renderer);

            check(renderer.count("remove_cached") == 1);
            check(renderer.count("push_rect") == 1);
            check(renderer.count("update_transform") == 0);

            auto removes = renderer.find_all("remove_cached");
            auto pushes = renderer.find_all("push_rect");
            check(removes.size() == 1);
            check(pushes.size() == 1);
            if (removes.size() != 1 || pushes.size() != 1) {
                return;
            }
            check(removes[0].handle == initial_handle);
            check(pushes[0].handle != initial_handle);
            check_float_eq(pushes[0].alpha, 0.25f, 0.001f);
        }
    }
}
