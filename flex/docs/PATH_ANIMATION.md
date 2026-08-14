# Path Animation System

Flex supports motion paths through typed `position` tracks. The editable
`Timeline` is lowered to an immutable `AnimationProgram` for playback; both the
C++ API and the DSL use the same linear, Catmull-Rom, and explicit cubic Bezier
sampling implementation.

---

## ✅ What's Implemented

### Phase 1: Standalone C++ Helper ✅

**Core Classes**:
```cpp
// Path definition
class Path {
    void add_point(float x, float y, float time = -1.0f);
    PathPoint interpolate(float t) const;  // t: 0.0-1.0
    void set_interpolation_mode(InterpolationMode mode);
};

// Animation controller
class PathAnimation {
    PathAnimation(Path* path, float duration);
    PathPoint update(float dt);
    void play() / pause() / stop() / reset();
    float progress() const;
};
```

**Historical demo**: `examples/legacy/thorvg/rocket_launch_demo.cpp`
- Rocket follows 6-point trajectory
- 8-second launch animation
- Real-time progress, altitude, speed display

The demo is retained as a migration reference and is not part of the active
renderer examples. New applications should use the Timeline API below.

---

### Phase 2: Smooth Catmull-Rom and Cubic Bezier Interpolation ✅

**Interpolation Modes**:
```cpp
enum class InterpolationMode {
    Linear,         // Sharp corners at waypoints
    CatmullRom,     // Smooth curves through all points
    Bezier          // Explicit cubic Bezier control points
};
```

Bezier paths use `P0, C1, C2, P3` followed by zero or more `C1, C2, P3`
groups. Only endpoint times at indices `0, 3, 6, ...` control segment timing:

```cpp
flex::Path curve;
curve.set_interpolation_mode(flex::Path::InterpolationMode::Bezier);
curve.add_point(0.0f, 0.0f, 0.0f);   // P0
curve.add_point(0.0f, 100.0f);        // C1; time ignored
curve.add_point(100.0f, 100.0f);      // C2; time ignored
curve.add_point(100.0f, 0.0f, 1.0f); // P3
```

`CubicBezier2D` exposes allocation-free position, analytic first/second
derivatives, speed, and bounded adaptive arc-length integration. Derivatives
are with respect to the normalized curve parameter.

**Benefits**:
- Natural curved motion
- No sharp corners
- Passes through all control points
- Adjustable tension (future)

**Visual Comparison**:
```
Linear:           Catmull-Rom:
  •──•               •───╮
     │                   ╰──•
     •                      │
                            •
Sharp corners         Smooth curves
```

---

### Phase 3: DSL Motion Tracks ✅

Use a `position` track with `vec2` keyframes. Times are expressed in the same
timeline units as `duration`; the DSL accepts seconds (`s`) and milliseconds
(`ms`). A target selector uses `#nodeId/position`:

```flex
anim "rocketFlight" {
    duration: 2s
    loop: once
    trigger 750ms -> "passedGuide"

    track "#rocket/position" {
        interpolation: catmullRom
        keyframe 0s -> vec2(130, 500)
        keyframe 500ms -> vec2(150, 300)
        keyframe 1.2s -> vec2(300, 120)
        keyframe 2s -> vec2(450, 50)
    }
}
```

Explicit cubic Bezier tracks store the incoming and outgoing tangent as offsets
from each keyframe position. Every keyframe on the track must provide all three
`vec2` arguments:

```flex
anim "guidedCurve" {
    duration: 1s
    track "#cursor/position" {
        interpolation: cubicBezier
        keyframe 0s -> bezier(vec2(0, 0), vec2(0, 0), vec2(0, 10))
        keyframe 1s -> bezier(vec2(10, 0), vec2(0, 10), vec2(0, 0))
    }
}
```

The semantic validator rejects unknown targets/properties, mixed keyframe
types, non-increasing or non-finite times, non-finite vectors/tangents, spatial
interpolation on non-`position` tracks, and incomplete cubic Bezier tangents.

`catmullRom` and `cubicBezier` require at least two `vec2` keyframes. Linear is
the default interpolation mode.

---

## 🎯 Usage Example

### Timeline C++ API (Current)

```cpp
#include "flex/core.h"

int main() {
    flex::ArenaAllocator arena(64 * 1024);
    auto* target = flex::Group::create(arena);
    auto timeline = flex::Timeline::create("guidedMove", arena);
    timeline->set_duration(2.0f);

    auto track = timeline->add_track("position");
    track->set_spatial_interpolation(
        flex::SpatialInterpolation::CatmullRom);
    track->add_keyframe(0.0f, flex::Vec2{0.0f, 10.0f});
    track->add_keyframe(2.0f, flex::Vec2{20.0f, 30.0f});

    flex::TimelinePlayer player(timeline.get(), target);
    player.play();
    player.advance(1.0f);
    player.apply();

    return target->x() == 10.0f && target->y() == 20.0f ? 0 : 1;
}
```

For explicit tangents, select `SpatialInterpolation::CubicBezier` and add each
point with `Track::add_spatial_keyframe(time, position, in_tangent,
out_tangent)`. The older standalone `Path`/`PathAnimation` helper remains
available for callers that do not need Timeline targeting, triggers, blending,
or compiled playback.

---

## 📊 Technical Details

### Path Interpolation

**Linear Interpolation**:
```cpp
P(t) = P0 + (P1 - P0) * t
```

**Catmull-Rom Spline**:
```cpp
P(t) = 0.5 * (
    (2 * P1) +
    (-P0 + P2) * t +
    (2*P0 - 5*P1 + 4*P2 - P3) * t² +
    (-P0 + 3*P1 - 3*P2 + P3) * t³
)
```
Where P0, P1, P2, P3 are 4 consecutive control points.

**Time Parameterization**:
- Each keyframe has an absolute timeline time; values need not be normalized
- Times must be finite, non-negative, and strictly increasing within a track
- Non-uniform distribution allows speed variation
- Example: `0s, 200ms, 800ms, 1s` produces non-uniform segment durations

---

## 🎮 Real-World Use Cases

### 1. Game Character Patrol Routes
```cpp
flex::Path patrol_route;
patrol_route.add_point(100, 300);
patrol_route.add_point(400, 300);
patrol_route.add_point(400, 500);
patrol_route.add_point(100, 500);
patrol_route.set_interpolation_mode(InterpolationMode::CatmullRom);

auto patrol_anim = std::make_unique<PathAnimation>(&patrol_route, 10.0f);
patrol_anim->set_loop(true);  // Continuous patrol
```

### 2. UI Element Transitions
```cpp
// Slide-in menu animation
flex::Path slide_in;
slide_in.add_point(-200, 100, 0.0f);   // Off-screen
slide_in.add_point(0, 100, 0.3f);       // Bounce past
slide_in.add_point(-20, 100, 0.6f);     // Overshoot
slide_in.add_point(0, 100, 1.0f);       // Final position
```

### 3. Camera Movements
```cpp
// Cinematic camera path
flex::Path camera_path;
camera_path.add_point(0, 0);      // Start
camera_path.add_point(100, 50);   // Pan right
camera_path.add_point(100, 150);  // Tilt down
camera_path.add_point(200, 150);  // Pan right again
camera_path.set_interpolation_mode(InterpolationMode::CatmullRom);
```

### 4. Data Visualization Flow
```cpp
// Particle system along curved path
for (int i = 0; i < 10; i++) {
    auto particle = create_particle();
    auto anim = std::make_unique<PathAnimation>(&data_flow_path, 2.0f);
    anim->reset();
    anim->play();
}
```

---

## 🚀 Future Enhancements

### Short-term:
- [x] Linear interpolation
- [x] Catmull-Rom spline
- [x] Cubic Bezier curves and analytic derivatives
- [x] Per-keyframe easing functions
- [x] Typed compiled Timeline storage
- [ ] Adjustable tension parameter

### Medium-term:
- [x] DSL `position` tracks with `vec2` keyframes
- [x] DSL Catmull-Rom and explicit cubic Bezier tangents
- [ ] Path visualization in editor
- [ ] Path editing tools
- [ ] Speed curve visualization

### Long-term:
- [ ] 3D paths (x, y, z)
- [ ] Orientation along path (auto-rotation)
- [ ] Path constraints (clamp to boundaries)
- [ ] Path morphing (blend between paths)

---

## 📈 Performance

`AnimationProgram` snapshots lower homogeneous scalar, `Vec2`, and `Color`
tracks into contiguous typed storage. Sampling performs an O(log n) keyframe
lookup and O(1) interpolation with O(1) auxiliary space. Sampling does not
allocate scratch storage; generic string results can still allocate when copied
into caller-owned `AnimValue` storage.

Use the repository benchmark instead of fixed machine-dependent FPS claims:

```powershell
cmake --build --preset win-release-user --target benchmark_motion_path
.\build\Msvc-Release\bin\benchmark_motion_path.exe
```

The benchmark covers editable and compiled Catmull-Rom/cubic Bezier sampling,
batch sampling, descendant property dispatch, TimelinePlayer execution, and
multi-target execution.

---

## 🎓 Linus Philosophy Applied

### ✅ "好品味"
- Path是数据，Animation是控制 - 清晰分离
- `interpolate(t)` - 简单的函数签名
- 无特殊情况：所有插值模式用同一接口

### ✅ 实用主义
- Phase 1: C++ API（立即可用）
- Phase 2: 平滑曲线（用户需求）
- Phase 3: DSL typed position tracks（已实现并进入语义校验/lowering）

### ✅ 简洁执念
- Timeline/Track 是唯一可编辑事实源，AnimationProgram 是不可变派生快照
- DSL 与 C++ API 共用同一采样与运行时语义
- position 作为一个 Vec2 通道处理，不拆成不同步的 x/y 轨道

---

Built with 🚀 by Flex Engine Team
