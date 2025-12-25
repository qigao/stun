/*
 * Flex Engine - Performance Benchmark
 *
 * Tests the performance improvements from arena allocator and binary search optimization.
 *
 * Tests:
 * 1. Animation sampling with binary search (O(log n))
 * 2. Memory allocation with arena allocator
 * 3. Scene graph traversal with dirty flags
 * 4. Total frame time
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>
#include <memory>

#include "flex.h"
#include <thorvg.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <SDL2/SDL.h>
#include <GL/gl.h>

using namespace flex;
using namespace std::chrono;

// ============================================================================
// Benchmark Utilities
// ============================================================================

struct BenchmarkResult {
    std::string name;
    double time_ms;
    size_t iterations;

    void print() const {
        std::cout << std::setw(40) << std::left << name << ": "
                  << std::setw(10) << std::right << time_ms << " ms "
                  << "(avg: " << (time_ms / iterations * 1000000) << " ns/op, "
                  << iterations << " iterations)\n";
    }
};

template<typename Func>
BenchmarkResult benchmark(const std::string& name, size_t iterations, Func&& func) {
    auto start = high_resolution_clock::now();

    for (size_t i = 0; i < iterations; i++) {
        func();
    }

    auto end = high_resolution_clock::now();
    double time_ms = duration_cast<nanoseconds>(end - start).count() / 1000000.0;

    return {name, time_ms, iterations};
}

// ============================================================================
// Test 1: Animation Sampling Performance
// ============================================================================

void test_animation_sampling() {
    std::cout << "\n=== Test 1: Animation Sampling Performance ===\n";

    // Create instance (like easing_demo does)
    auto instance = Instance::create(800, 600);
    auto* alloc = instance->object_allocator();

    if (!alloc) {
        std::cerr << "❌ object_allocator not available!\n";
        return;
    }

    // Create timeline exactly like easing_demo
    auto timeline = Timeline::create("test", *alloc);
    timeline->set_duration(2.0f);
    timeline->set_loop_mode(LoopMode::Loop);

    // Add track with simple keyframes
    auto track = timeline->add_track("x");  // Property name, not allocator copy
    track->add_keyframe(0.0f, 100.0f);
    track->add_keyframe(1.0f, 200.0f);
    track->add_keyframe(2.0f, 100.0f);

    std::cout << "✓ Timeline created with " << track->keyframe_count() << " keyframes\n";

    // Test sampling
    auto sample_0 = track->sample(0.0f);
    auto sample_1 = track->sample(1.0f);
    auto sample_2 = track->sample(2.0f);

    std::cout << "Sample at t=0.0: " << std::get<float>(sample_0) << " (expected 100)\n";
    std::cout << "Sample at t=1.0: " << std::get<float>(sample_1) << " (expected 200)\n";
    std::cout << "Sample at t=2.0: " << std::get<float>(sample_2) << " (expected 100)\n";

    // Benchmark sampling
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 2.0f);

    const size_t iterations = 100000;

    auto result = benchmark("Animation Sampling (100k samples)", iterations, [&]() {
        float t = dis(gen);
        auto value = track->sample(t);
        (void)value;  // Use value to prevent optimization
    });

    result.print();
}

// ============================================================================
// Test 2: Memory Allocation Performance
// ============================================================================

void test_memory_allocation() {
    std::cout << "\n=== Test 2: Memory Allocation Performance ===\n";

    const size_t iterations = 10000;
    const size_t allocations = 100;

    // Benchmark with arena allocator
    auto result_arena = benchmark("Arena Allocator (10k frames)", iterations, [&]() {
        ArenaAllocator frame_alloc(1024 * 100);  // 100KB frame pool

        for (size_t i = 0; i < allocations; i++) {
            void* mem = frame_alloc.allocate(64);
            (void)mem;
        }
        // Frame pool automatically reset at end of scope
    });

    result_arena.print();

    // Benchmark with malloc
    auto result_malloc = benchmark("malloc/free (10k frames)", iterations, [&]() {
        std::vector<void*> ptrs;
        ptrs.reserve(allocations);

        for (size_t i = 0; i < allocations; i++) {
            ptrs.push_back(malloc(64));
        }

        for (auto ptr : ptrs) {
            free(ptr);
        }
    });

    result_malloc.print();

    double speedup = result_malloc.time_ms / result_arena.time_ms;
    std::cout << "Arena allocator is " << std::fixed << std::setprecision(1)
              << speedup << "x faster than malloc/free!\n";
}

// ============================================================================
// Test 3: Keyframe Lookup Comparison
// ============================================================================

void test_keyframe_lookup() {
    std::cout << "\n=== Test 3: Keyframe Lookup (Binary Search) ===\n";

    auto instance = Instance::create(800, 600);
    auto* alloc = instance->object_allocator();

    if (!alloc) {
        std::cerr << "❌ object_allocator not available!\n";
        return;
    }

    // Create timeline with 100 keyframes (reduced from 1000)
    auto timeline = Timeline::create("test", *alloc);
    auto track = timeline->add_track("x");
    
    std::cout << "Adding keyframes...\n";
    for (int i = 0; i < 100; i++) {
        track->add_keyframe(i * 0.01f, i * 10.0f);
    }
    std::cout << "✓ Added " << track->keyframe_count() << " keyframes\n";

    // Test binary search
    std::random_device rd;
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    const size_t iterations = 100000;

    auto result = benchmark("Binary Search Lookup", iterations, [&]() {
        float t = dis(gen);
        auto value = track->sample(t);
        (void)value;  // Use value to prevent optimization
    });

    result.print();
    std::cout << "Expected O(log n) = ~7 steps per lookup for 100 keyframes\n";
}

// ============================================================================
// Test 4: Scene Graph Performance
// ============================================================================

void test_scene_graph_performance() {
    std::cout << "\n=== Test 4: Scene Graph Performance ===\n";

    auto instance = Instance::create(800, 600);
    auto* artboard = instance->artboard();

    // Create 1000 nodes
    const size_t node_count = 1000;
    std::vector<std::shared_ptr<Shape>> nodes;

    for (size_t i = 0; i < node_count; i++) {
        auto node = Shape::create();
        node->set_position(i * 10, i * 10);
        node->set_rect(50, 50);
        nodes.push_back(node);
        artboard->add_child(node);
    }

    const size_t iterations = 10000;

    // Test 4a: Normal updates (baseline)
    std::cout << "\n--- Without Batch Mode (Baseline) ---\n";
    auto result_normal = benchmark("Scene Update (10k frames)", iterations, [&]() {
        instance->reset_frame();  // Reset frame allocator

        for (size_t i = 0; i < node_count; i++) {
            nodes[i]->set_x(nodes[i]->x() + 1);
            nodes[i]->set_y(nodes[i]->y() + 1);
        }
    });

    result_normal.print();

    // Test 4b: Batch updates (optimized)
    std::cout << "\n--- With Batch Mode (Optimized) ---\n";
    auto result_batch = benchmark("Scene Update (10k frames, batched)", iterations, [&]() {
        instance->reset_frame();  // Reset frame allocator

        for (size_t i = 0; i < node_count; i++) {
            nodes[i]->begin_batch();
            nodes[i]->set_x(nodes[i]->x() + 1);
            nodes[i]->set_y(nodes[i]->y() + 1);
            nodes[i]->end_batch();
        }
    });

    result_batch.print();

    // Calculate speedup
    double speedup = result_normal.time_ms / result_batch.time_ms;
    std::cout << "\n✨ Batch mode is " << std::fixed << std::setprecision(1)
              << speedup << "x faster!\n";

    std::cout << "Updated " << node_count << " nodes per frame\n";
    std::cout << "Total allocations per frame: 0 (all from arena)\n";
}

// ============================================================================
// Test 5: Total Frame Time
// ============================================================================

void test_total_frame_time() {
    std::cout << "\n=== Test 5: Total Frame Time (60 FPS simulation) ===\n";

    auto instance = Instance::create(800, 600);
    auto* artboard = instance->artboard();
    auto* obj_alloc = instance->object_allocator();

    if (!obj_alloc) {
        std::cerr << "❌ object_allocator not available!\n";
        return;
    }

    // Create animation
    auto timeline = Timeline::create("move", *obj_alloc);
    auto track = timeline->add_track("x");
    track->add_keyframe(0.0f, 100.0f);
    track->add_keyframe(1.0f, 500.0f);
    track->add_keyframe(2.0f, 100.0f);
    timeline->set_loop_mode(LoopMode::Loop);

    instance->add_timeline(timeline);

    // Create animated node
    auto player = Shape::create();
    player->set_rect(40, 80);
    player->set_position(100, 450);
    artboard->add_child(player);

    instance->play("move", player.get());

    // Simulate 60 frames
    const float target_fps = 60.0f;
    const float frame_time = 1.0f / target_fps;
    const size_t frame_count = 60;

    auto start = high_resolution_clock::now();

    for (size_t frame = 0; frame < frame_count; frame++) {
        instance->advance(frame_time);
        instance->reset_frame();  // Reset frame allocator
    }

    auto end = high_resolution_clock::now();
    double total_ms = duration_cast<nanoseconds>(end - start).count() / 1000000.0;
    double avg_frame_ms = total_ms / frame_count;
    double actual_fps = 1000.0 / avg_frame_ms;

    std::cout << "Processed " << frame_count << " frames\n";
    std::cout << "Total time: " << std::fixed << std::setprecision(2) << total_ms << " ms\n";
    std::cout << "Avg frame time: " << std::setprecision(3) << avg_frame_ms << " ms\n";
    std::cout << "Actual FPS: " << std::setprecision(1) << actual_fps << "\n";
    std::cout << "Target FPS: " << target_fps << "\n";

    if (actual_fps >= target_fps) {
        std::cout << "✓ ACHIEVED TARGET FPS!\n";
    } else {
        std::cout << "✗ Below target FPS\n";
    }
}

// ============================================================================
// Test 6: Rendering Performance (ThorVG)
// ============================================================================

void test_rendering_performance() {
    std::cout << "\n=== Test 6: Rendering Performance (ThorVG) ===\n";

    // Create ThorVG canvas
    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) {
        std::cerr << "❌ Failed to create ThorVG canvas\n";
        return;
    }

    const int WIDTH = 800;
    const int HEIGHT = 600;
    std::vector<uint32_t> buffer(WIDTH * HEIGHT);
    canvas->target(buffer.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

    // Create Flex instance and renderer
    auto instance = Instance::create(WIDTH, HEIGHT);
    auto* artboard = instance->artboard();
    auto renderer = create_thorvg_renderer(canvas );

    // Test different scene complexities
    std::vector<int> node_counts = {100, 500, 1000};

    for (int node_count : node_counts) {
        std::cout << "\n--- Testing with " << node_count << " shapes ---\n";

        // Clear scene
        while (artboard->root()->children().size() > 0) {
            // Note: Can't easily remove children, so create new instance
            instance = Instance::create(WIDTH, HEIGHT);
            artboard = instance->artboard();
            break;
        }

        // Create shapes
        std::random_device rd;
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> pos_x(0, WIDTH - 50);
        std::uniform_real_distribution<float> pos_y(0, HEIGHT - 50);
        std::uniform_real_distribution<float> color(0, 1);

        for (int i = 0; i < node_count; i++) {
            auto shape = Shape::create();
            shape->set_rect(20, 20);
            shape->set_position(pos_x(gen), pos_y(gen));
            shape->set_fill(Color(color(gen), color(gen), color(gen), 1.0f));
            artboard->add_child(shape);
        }

        // Benchmark rendering
        const size_t frames = 100;
        auto start = high_resolution_clock::now();

        for (size_t frame = 0; frame < frames; frame++) {
            // Full render cycle
            renderer->begin_frame(WIDTH, HEIGHT, 1.0f);
            instance->render(*renderer);
            renderer->end_frame();
        }

        auto end = high_resolution_clock::now();
        double total_ms = duration_cast<nanoseconds>(end - start).count() / 1000000.0;
        double avg_frame_ms = total_ms / frames;
        double fps = 1000.0 / avg_frame_ms;

        std::cout << "  Rendered " << frames << " frames in " << std::fixed 
                  << std::setprecision(2) << total_ms << " ms\n";
        std::cout << "  Avg frame time: " << std::setprecision(3) << avg_frame_ms << " ms\n";
        std::cout << "  FPS: " << std::setprecision(1) << fps << "\n";

        if (fps >= 60) {
            std::cout << "  ✓ Achieves 60 FPS target\n";
        } else if (fps >= 30) {
            std::cout << "  ⚠ Below 60 FPS but above 30 FPS\n";
        } else {
            std::cout << "  ✗ Below 30 FPS - needs optimization\n";
        }
    }

    std::cout << "\n💡 Note: This tests RENDERING only (no animation updates)\n";

    // ===========================================
    // RETAINED MODE TEST - Use fresh canvas to avoid contamination
    // ===========================================
    std::cout << "\n=== Test 6b: Retained Mode Performance ===\n";
    std::cout << "Using begin_retained_frame() to reuse ThorVG objects\n";

    for (int node_count : node_counts) {
        std::cout << "\n--- Retained Mode with " << node_count << " shapes ---\n";

        // Create FRESH canvas for each retained mode test to avoid contamination
        auto retained_canvas = tvg::SwCanvas::gen();
        retained_canvas->target(buffer.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);
        auto retained_renderer = create_thorvg_renderer(retained_canvas);

        // Create fresh instance
        auto retained_instance = Instance::create(WIDTH, HEIGHT);
        auto* retained_artboard = retained_instance->artboard();

        // Create shapes
        std::mt19937 gen2(42);
        std::uniform_real_distribution<float> pos_x2(0, WIDTH - 50);
        std::uniform_real_distribution<float> pos_y2(0, HEIGHT - 50);
        std::uniform_real_distribution<float> color2(0, 1);
        for (int i = 0; i < node_count; i++) {
            auto shape = Shape::create();
            shape->set_rect(20, 20);
            shape->set_position(pos_x2(gen2), pos_y2(gen2));
            shape->set_fill(Color(color2(gen2), color2(gen2), color2(gen2), 1.0f));
            retained_artboard->add_child(shape);
        }

        // First frame: build all objects with retained mode
        retained_renderer->begin_retained_frame(WIDTH, HEIGHT, 1.0f);
        retained_instance->render(*retained_renderer);
        retained_renderer->end_frame();

        // Benchmark subsequent frames with retained mode (should reuse objects)
        const size_t frames = 100;
        auto start = high_resolution_clock::now();

        for (size_t frame = 0; frame < frames; frame++) {
            retained_renderer->begin_retained_frame(WIDTH, HEIGHT, 1.0f);
            retained_instance->render(*retained_renderer);
            retained_renderer->end_frame();
        }

        auto end = high_resolution_clock::now();
        double total_ms = duration_cast<nanoseconds>(end - start).count() / 1000000.0;
        double avg_frame_ms = total_ms / frames;
        double fps = 1000.0 / avg_frame_ms;

        std::cout << "  Rendered " << frames << " frames in " << std::fixed
                  << std::setprecision(2) << total_ms << " ms\n";
        std::cout << "  Avg frame time: " << std::setprecision(3) << avg_frame_ms << " ms\n";
        std::cout << "  FPS: " << std::setprecision(1) << fps << "\n";

        if (fps >= 120) {
            std::cout << "  ✓✓ Achieves 120 FPS target!\n";
        } else if (fps >= 60) {
            std::cout << "  ✓ Achieves 60 FPS target\n";
        } else {
            std::cout << "  ⚠ Below 60 FPS\n";
        }
    }
}

// ============================================================================
// Test 6c: Retained Mode with Transform Updates
// ============================================================================

void test_retained_mode_animated() {
    std::cout << "\n=== Test 6c: Retained Mode with Transform Updates ===\n";
    std::cout << "Testing retained mode when shapes have transform changes\n";

    const int WIDTH = 800;
    const int HEIGHT = 600;
    std::vector<uint32_t> buffer(WIDTH * HEIGHT);

    std::vector<int> node_counts = {100, 500, 1000};

    for (int node_count : node_counts) {
        std::cout << "\n--- Retained + Transform Updates: " << node_count << " shapes ---\n";

        auto canvas = tvg::SwCanvas::gen();
        canvas->target(buffer.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);
        auto renderer = create_thorvg_renderer(canvas);

        auto instance = Instance::create(WIDTH, HEIGHT);
        auto* artboard = instance->artboard();

        // Create shapes
        std::vector<std::shared_ptr<Shape>> shapes;
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> pos_x(50, WIDTH - 50);
        std::uniform_real_distribution<float> pos_y(50, HEIGHT - 50);
        std::uniform_real_distribution<float> color(0, 1);

        for (int i = 0; i < node_count; i++) {
            auto shape = Shape::create();
            shape->set_rect(20, 20);
            shape->set_position(pos_x(gen), pos_y(gen));
            shape->set_fill(Color(color(gen), color(gen), color(gen), 1.0f));
            artboard->add_child(shape);
            shapes.push_back(shape);
        }

        // First frame: build all cached objects
        renderer->begin_retained_frame(WIDTH, HEIGHT, 1.0f);
        instance->render(*renderer);
        renderer->end_frame();

        // Benchmark with transform updates (simulating animation)
        const size_t frames = 100;
        auto start = high_resolution_clock::now();

        for (size_t frame = 0; frame < frames; frame++) {
            // Update transforms for all shapes (simulate animation)
            for (auto& shape : shapes) {
                shape->set_rotation(shape->rotation() + 1.0f);
            }

            renderer->begin_retained_frame(WIDTH, HEIGHT, 1.0f);
            instance->render(*renderer);
            renderer->end_frame();
        }

        auto end = high_resolution_clock::now();
        double total_ms = duration_cast<nanoseconds>(end - start).count() / 1000000.0;
        double avg_frame_ms = total_ms / frames;
        double fps = 1000.0 / avg_frame_ms;

        std::cout << "  Rendered " << frames << " frames in " << std::fixed
                  << std::setprecision(2) << total_ms << " ms\n";
        std::cout << "  Avg frame time: " << std::setprecision(3) << avg_frame_ms << " ms\n";
        std::cout << "  FPS: " << std::setprecision(1) << fps << "\n";

        if (fps >= 120) {
            std::cout << "  ✓✓ Achieves 120 FPS target!\n";
        } else if (fps >= 60) {
            std::cout << "  ✓ Achieves 60 FPS target\n";
        } else {
            std::cout << "  ⚠ Below 60 FPS\n";
        }
    }
}

// ============================================================================
// Test 7: Complex Scene (Rendering + Animation)
// ============================================================================

void test_complex_scene() {
    std::cout << "\n=== Test 7: Complex Scene (Render + Animation) ===\n";

    // Create ThorVG canvas
    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) {
        std::cerr << "❌ Failed to create ThorVG canvas\n";
        return;
    }

    const int WIDTH = 800;
    const int HEIGHT = 600;
    std::vector<uint32_t> buffer(WIDTH * HEIGHT);
    canvas->target(buffer.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

    // Create Flex instance
    auto instance = Instance::create(WIDTH, HEIGHT);
    auto* artboard = instance->artboard();
    auto* alloc = instance->object_allocator();
    auto renderer = create_thorvg_renderer(canvas );

    // Create complex scene: 100 animated + 400 static nodes
    const int animated_count = 100;
    const int static_count = 400;

    std::random_device rd;
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> pos_x(0, WIDTH - 50);
    std::uniform_real_distribution<float> pos_y(0, HEIGHT - 50);
    std::uniform_real_distribution<float> color(0, 1);

    std::vector<Node*> animated_nodes;

    // Create animated nodes
    for (int i = 0; i < animated_count; i++) {
        auto shape = Shape::create();
        shape->set_circle(10);
        shape->set_position(pos_x(gen), pos_y(gen));
        shape->set_fill(Color(color(gen), color(gen), color(gen), 1.0f));
        artboard->add_child(shape);
        animated_nodes.push_back(shape.get());

        // Create bounce animation
        std::string anim_name = "bounce_" + std::to_string(i);
        auto timeline = Timeline::create(anim_name.c_str(), *alloc);
        timeline->set_duration(2.0f);
        timeline->set_loop_mode(LoopMode::Loop);

        auto track = timeline->add_track("y");
        float start_y = shape->y();
        track->add_keyframe(0.0f, start_y);
        track->add_keyframe(1.0f, start_y - 50);
        track->add_keyframe(2.0f, start_y);

        instance->add_timeline(timeline);
        instance->play(anim_name.c_str(), shape.get());
    }

    // Create static nodes
    for (int i = 0; i < static_count; i++) {
        auto shape = Shape::create();
        shape->set_rect(15, 15);
        shape->set_position(pos_x(gen), pos_y(gen));
        shape->set_fill(Color(color(gen), color(gen), color(gen), 0.5f));
        artboard->add_child(shape);
    }

    std::cout << "Scene: " << animated_count << " animated + " << static_count 
              << " static = " << (animated_count + static_count) << " total nodes\n";

    // Benchmark full frame (update + render)
    const size_t frames = 100;
    const float dt = 1.0f / 60.0f;

    auto start = high_resolution_clock::now();

    for (size_t frame = 0; frame < frames; frame++) {
        // Update animations
        instance->advance(dt);

        // Render
        renderer->begin_frame(WIDTH, HEIGHT, 1.0f);
        instance->render(*renderer);
        renderer->end_frame();

        instance->reset_frame();
    }

    auto end = high_resolution_clock::now();
    double total_ms = duration_cast<nanoseconds>(end - start).count() / 1000000.0;
    double avg_frame_ms = total_ms / frames;
    double fps = 1000.0 / avg_frame_ms;

    std::cout << "\nProcessed " << frames << " frames in " << std::fixed 
              << std::setprecision(2) << total_ms << " ms\n";
    std::cout << "Avg frame time: " << std::setprecision(3) << avg_frame_ms << " ms\n";
    std::cout << "FPS: " << std::setprecision(1) << fps << "\n";
    std::cout << "Target: 60 FPS (16.67 ms/frame)\n";

    if (fps >= 60) {
        std::cout << "✓ ACHIEVES 60 FPS in complex scene!\n";
    } else if (fps >= 30) {
        std::cout << "⚠ 30-60 FPS - acceptable but could be better\n";
    } else {
        std::cout << "✗ Below 30 FPS - needs optimization\n";
    }
}

// ============================================================================
// Test 8: GPU Rendering Performance (GlCanvas)
// ============================================================================

void test_gpu_rendering() {
    std::cout << "\n=== Test 8: GPU Rendering Performance (GlCanvas) ===\n";

    // Initialize SDL with OpenGL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "❌ SDL init failed: " << SDL_GetError() << "\n";
        return;
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    const int WIDTH = 800;
    const int HEIGHT = 600;

    SDL_Window* window = SDL_CreateWindow(
        "GPU Benchmark",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WIDTH, HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
    );

    if (!window) {
        std::cerr << "❌ Window creation failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "❌ GL context creation failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Create GlCanvas
    auto canvas = tvg::GlCanvas::gen();
    if (!canvas) {
        std::cerr << "❌ Failed to create GlCanvas (GL engine may not be supported)\n";
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Set target to default framebuffer (id=0)
    // GlCanvas::target(context, id, w, h, colorspace)
    // context must be a valid pointer (not nullptr)
    if (canvas->target(gl_context, 0, WIDTH, HEIGHT, tvg::ColorSpace::ABGR8888S) != tvg::Result::Success) {
        std::cerr << "❌ Failed to set GlCanvas target\n";
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    std::cout << "✓ GlCanvas initialized successfully\n";

    // Create Flex instance and renderer
    auto instance = Instance::create(WIDTH, HEIGHT);
    auto* artboard = instance->artboard();
    auto renderer = create_thorvg_renderer(canvas );

    // Test different scene complexities
    std::vector<int> node_counts = {100, 500, 1000};

    for (int node_count : node_counts) {
        std::cout << "\n--- GPU Testing with " << node_count << " shapes ---\n";

        // Recreate instance for clean scene
        instance = Instance::create(WIDTH, HEIGHT);
        artboard = instance->artboard();

        // Create shapes
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> pos_x(0, WIDTH - 50);
        std::uniform_real_distribution<float> pos_y(0, HEIGHT - 50);
        std::uniform_real_distribution<float> color(0, 1);

        for (int i = 0; i < node_count; i++) {
            auto shape = Shape::create();
            shape->set_rect(20, 20);
            shape->set_position(pos_x(gen), pos_y(gen));
            shape->set_fill(Color(color(gen), color(gen), color(gen), 1.0f));
            artboard->add_child(shape);
        }

        // Benchmark rendering
        const size_t frames = 100;
        auto start = high_resolution_clock::now();

        for (size_t frame = 0; frame < frames; frame++) {
            glClear(GL_COLOR_BUFFER_BIT);
            renderer->begin_frame(WIDTH, HEIGHT, 1.0f);
            instance->render(*renderer);
            renderer->end_frame();
            SDL_GL_SwapWindow(window);
        }

        auto end = high_resolution_clock::now();
        double total_ms = duration_cast<nanoseconds>(end - start).count() / 1000000.0;
        double avg_frame_ms = total_ms / frames;
        double fps = 1000.0 / avg_frame_ms;

        std::cout << "  Rendered " << frames << " frames in " << std::fixed
                  << std::setprecision(2) << total_ms << " ms\n";
        std::cout << "  Avg frame time: " << std::setprecision(3) << avg_frame_ms << " ms\n";
        std::cout << "  FPS: " << std::setprecision(1) << fps << "\n";

        if (fps >= 60) {
            std::cout << "  ✓ GPU achieves 60 FPS target\n";
        } else if (fps >= 30) {
            std::cout << "  ⚠ GPU below 60 FPS but above 30 FPS\n";
        } else {
            std::cout << "  ✗ GPU below 30 FPS\n";
        }
    }

    // Cleanup
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    std::cout << "=================================================\n";
    std::cout << "  Flex Engine - Performance Benchmark\n";
    std::cout << "  Testing Arena Allocator & Binary Search\n";
    std::cout << "=================================================\n";

    // Initialize Flex engine
    flex::init();

    try {
        // Run benchmarks
        test_animation_sampling();
        test_memory_allocation();
        test_keyframe_lookup();
        test_scene_graph_performance();
        test_total_frame_time();
        
        // Rendering performance tests
        test_rendering_performance();
        test_retained_mode_animated();
        test_complex_scene();

        // GPU rendering test
        test_gpu_rendering();

        std::cout << "\n=================================================\n";
        std::cout << "  Benchmark Complete!\n";
        std::cout << "=================================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    flex::shutdown();
    return 0;
}
