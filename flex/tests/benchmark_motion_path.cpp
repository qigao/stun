#include "flex/core/allocator.h"
#include "flex/core/cubic_bezier.h"
#include "flex/core/group.h"
#include "flex/core/shape.h"
#include "flex/core/timeline.h"
#include "tinytest.h"

#include <string>
#include <variant>
#include <vector>

using namespace flex;

spec("Flex motion path performance") {
  bench("cubic Bezier analytic sampling") {
    constexpr size_t kSamplesPerBatch = 128;
    constexpr size_t kBatchCount = 1000;
    const CubicBezier2D curve({0.0f, 0.0f}, {100.0f, 200.0f}, {200.0f, -100.0f}, {300.0f, 0.0f});

    const Vec2 midpoint = curve.position(0.5f);
    check_float_within_abs(midpoint.x, 150.0f, 0.001f);

    volatile float sink = 0.0f;
    benchmark_ops("cubic Bezier position and derivative samples", kBatchCount, kSamplesPerBatch) {
      for (size_t index = 0; index < kSamplesPerBatch; ++index) {
        const float parameter =
            static_cast<float>(index) / static_cast<float>(kSamplesPerBatch - 1);
        const Vec2 position = curve.position(parameter);
        const Vec2 velocity = curve.derivative(parameter);
        sink = sink + position.x + position.y + velocity.x + velocity.y;
      }
    }
    check(sink != 0.0f);
  }

  bench("Catmull-Rom position track sampling") {
    constexpr size_t kSamplesPerBatch = 128;
    constexpr size_t kBatchCount = 1000;

    ArenaAllocator arena(4096);
    auto timeline = Timeline::create("curve", arena);
    auto *track = timeline->add_track("position").get();
    track->set_spatial_interpolation(SpatialInterpolation::CatmullRom);
    track->add_keyframe(0.0f, Vec2{0.0f, 0.0f});
    track->add_keyframe(1.0f, Vec2{100.0f, 80.0f});
    track->add_keyframe(2.0f, Vec2{200.0f, -40.0f});
    track->add_keyframe(3.0f, Vec2{300.0f, 0.0f});
    const AnimationProgram& program = timeline->program();
    check_size_eq(program.keyframe_count(), 4);

    const Vec2 midpoint = std::get<Vec2>(track->sample(1.5f));
    check_float_within_abs(midpoint.x, 150.0f, 0.001f);
    const Vec2 compiled_midpoint = std::get<Vec2>(program.sample(0, 1.5f));
    check_float_within_abs(compiled_midpoint.x, midpoint.x, 0.001f);
    check_float_within_abs(compiled_midpoint.y, midpoint.y, 0.001f);

    volatile float sink = 0.0f;
    benchmark_ops("Catmull-Rom Vec2 samples", kBatchCount, kSamplesPerBatch) {
      for (size_t index = 0; index < kSamplesPerBatch; ++index) {
        const float time = static_cast<float>(index % 96) / 32.0f;
        const Vec2 value = std::get<Vec2>(track->sample(time));
        sink = sink + value.x + value.y;
      }
    }
    check(sink != 0.0f);

    volatile float compiled_sink = 0.0f;
    benchmark_ops("compiled Catmull-Rom Vec2 samples", kBatchCount,
                  kSamplesPerBatch) {
      for (size_t index = 0; index < kSamplesPerBatch; ++index) {
        const float time = static_cast<float>(index % 96) / 32.0f;
        const Vec2 value = std::get<Vec2>(program.sample(0, time));
        compiled_sink = compiled_sink + value.x + value.y;
      }
    }
    check(compiled_sink != 0.0f);
  }

  bench("cubic Bezier position track sampling") {
    constexpr size_t kSamplesPerBatch = 128;
    constexpr size_t kBatchCount = 1000;

    ArenaAllocator arena(4096);
    auto timeline = Timeline::create("bezier", arena);
    auto *track = timeline->add_track("position").get();
    track->set_spatial_interpolation(SpatialInterpolation::CubicBezier);
    track->add_spatial_keyframe(0.0f, Vec2{0.0f, 0.0f},
                                Vec2{0.0f, 0.0f}, Vec2{100.0f, 200.0f});
    track->add_spatial_keyframe(3.0f, Vec2{300.0f, 0.0f},
                                Vec2{-100.0f, -100.0f}, Vec2{0.0f, 0.0f});
    const AnimationProgram& program = timeline->program();
    check_size_eq(program.keyframe_count(), 2);

    const Vec2 midpoint = std::get<Vec2>(track->sample(1.5f));
    check_float_within_abs(midpoint.x, 150.0f, 0.001f);
    const Vec2 compiled_midpoint = std::get<Vec2>(program.sample(0, 1.5f));
    check_float_within_abs(compiled_midpoint.x, midpoint.x, 0.001f);
    check_float_within_abs(compiled_midpoint.y, midpoint.y, 0.001f);

    volatile float sink = 0.0f;
    benchmark_ops("cubic Bezier Vec2 track samples", kBatchCount,
                  kSamplesPerBatch) {
      for (size_t index = 0; index < kSamplesPerBatch; ++index) {
        const float time = static_cast<float>(index % 96) / 32.0f;
        const Vec2 value = std::get<Vec2>(track->sample(time));
        sink = sink + value.x + value.y;
      }
    }
    check(sink != 0.0f);

    volatile float compiled_sink = 0.0f;
    benchmark_ops("compiled cubic Bezier Vec2 samples", kBatchCount,
                  kSamplesPerBatch) {
      for (size_t index = 0; index < kSamplesPerBatch; ++index) {
        const float time = static_cast<float>(index % 96) / 32.0f;
        const Vec2 value = std::get<Vec2>(program.sample(0, time));
        compiled_sink = compiled_sink + value.x + value.y;
      }
    }
    check(compiled_sink != 0.0f);
  }

  bench("timeline descendant property dispatch") {
    constexpr size_t kTrackCount = 32;
    constexpr size_t kAppliesPerBatch = 128;
    constexpr size_t kBatchCount = 500;

    ArenaAllocator arena(64 * 1024);
    auto *root = Group::create(arena);
    auto *child = Shape::create(arena);
    child->set_id("animated");
    root->add_child(child);

    auto timeline = Timeline::create("dispatch", arena);
    for (size_t index = 0; index < kTrackCount; ++index) {
      auto *track = timeline->add_track("#animated/x").get();
      track->add_keyframe(0.0f, 0.0f);
      track->add_keyframe(1.0f, static_cast<float>(index + 1));
    }

    const AnimationProgram& program = timeline->program();
    check_size_eq(program.track_count(), kTrackCount);

    std::vector<AnimValue> sampled_values(program.track_count());
    program.sample_all(0.5f, sampled_values.data(), sampled_values.size());
    check_float_within_abs(std::get<float>(sampled_values.back()), 16.0f,
                           0.001f);

    volatile float sample_sink = 0.0f;
    benchmark_ops("batch sample compiled scalar tracks", kBatchCount,
                  kTrackCount * kAppliesPerBatch) {
      for (size_t index = 0; index < kAppliesPerBatch; ++index) {
        const float time = static_cast<float>(index) /
                           static_cast<float>(kAppliesPerBatch - 1);
        program.sample_all(time, sampled_values.data(), sampled_values.size());
      }
      sample_sink = sample_sink + std::get<float>(sampled_values.back());
    }
    check(sample_sink != 0.0f);

    timeline->apply(root, 0.5f);
    check_float_within_abs(child->x(), 16.0f, 0.001f);

    volatile float sink = 0.0f;
    benchmark_ops("resolve and apply descendant tracks", kBatchCount,
                  kTrackCount * kAppliesPerBatch) {
      for (size_t index = 0; index < kAppliesPerBatch; ++index) {
        const float time = static_cast<float>(index) /
                           static_cast<float>(kAppliesPerBatch - 1);
        timeline->apply(root, time);
      }
      sink = sink + child->x();
    }
    check(sink != 0.0f);

    timeline->set_duration(1.0f);
    timeline->set_loop_mode(LoopMode::Loop);
    TimelinePlayer player(timeline.get(), root);
    player.play();

    volatile float player_sink = 0.0f;
    benchmark_ops("advance and apply timeline player", kBatchCount,
                  kTrackCount * kAppliesPerBatch) {
      for (size_t index = 0; index < kAppliesPerBatch; ++index) {
        player.advance(1.0f / static_cast<float>(kAppliesPerBatch + 1));
        player.apply();
      }
      player_sink = player_sink + child->x();
    }
    check(player_sink != 0.0f);
  }

  bench("timeline multi-target execution") {
    constexpr size_t kTargetCount = 32;
    constexpr size_t kAppliesPerBatch = 128;
    constexpr size_t kBatchCount = 500;

    ArenaAllocator arena(128 * 1024);
    auto *root = Group::create(arena);
    std::vector<Shape *> targets;
    targets.reserve(kTargetCount);

    auto timeline = Timeline::create("multiTarget", arena);
    timeline->set_duration(1.0f);
    timeline->set_loop_mode(LoopMode::Loop);
    for (size_t index = 0; index < kTargetCount; ++index) {
      auto *shape = Shape::create(arena);
      const std::string id = "animated" + std::to_string(index);
      shape->set_id(id);
      root->add_child(shape);
      targets.push_back(shape);

      const std::string property = "#" + id + "/x";
      auto *track = timeline->add_track(property.c_str()).get();
      track->add_keyframe(0.0f, 0.0f);
      track->add_keyframe(1.0f, static_cast<float>(index + 1));
    }

    TimelinePlayer player(timeline.get(), root);
    player.play();
    player.advance(0.5f);
    player.apply();
    check_float_within_abs(targets.back()->x(), 16.0f, 0.001f);

    volatile float sink = 0.0f;
    benchmark_ops("advance and apply multi-target player", kBatchCount,
                  kTargetCount * kAppliesPerBatch) {
      for (size_t index = 0; index < kAppliesPerBatch; ++index) {
        player.advance(1.0f / static_cast<float>(kAppliesPerBatch + 1));
        player.apply();
      }
      sink = sink + targets.back()->x();
    }
    check(sink != 0.0f);
  }
}
