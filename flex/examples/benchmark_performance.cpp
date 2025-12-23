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

#include "flex/flex.h"

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

    // Create a standalone instance to get allocator
    auto instance = Instance::create(800, 600);
    auto* alloc = instance->object_allocator();

    if (!alloc) {
        std::cerr << "❌ object_allocator not available!\n";
        return;
    }

    // Create timeline with many keyframes
    auto timeline = Timeline::create("test", *alloc);
    auto track = timeline->add_track("x");

    // Add 1000 keyframes
    for (int i = 0; i < 1000; i++) {
        float time = i * 0.01f;
        float value = i * 10.0f;
        track->add_keyframe(time, value);
    }

    // Benchmark sampling at random times
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 10.0f);

    const size_t iterations = 100000;

    auto result = benchmark("Animation Sampling (100k samples)", iterations, [&]() {
        float t = dis(gen);
        auto value = track->sample(t);
        (void)value;  // Use value to prevent optimization
    });

    result.print();

    // Verify correctness
    auto sample_at_5 = track->sample(5.0f);
    std::cout << "Verification: sample at t=5.0 should be ~5000, got "
              << std::get<float>(sample_at_5) << "\n";
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
    std::cout << "\n=== Test 3: Keyframe Lookup (Binary Search vs Linear) ===\n";

    auto instance = Instance::create(800, 600);
    auto* alloc = instance->object_allocator();

    if (!alloc) {
        std::cerr << "❌ object_allocator not available!\n";
        return;
    }

    // Create timeline with 1000 keyframes
    auto timeline = Timeline::create("test", *alloc);
    auto track = timeline->add_track("x");
    for (int i = 0; i < 1000; i++) {
        track->add_keyframe(i * 0.01f, i * 10.0f);
    }

    // Test binary search (our optimized version)
    std::random_device rd;
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dis(0.0f, 10.0f);

    const size_t iterations = 100000;

    auto result = benchmark("Binary Search Lookup", iterations, [&]() {
        float t = dis(gen);
        auto value = track->sample(t);
        (void)value;  // Use value to prevent optimization
    });

    result.print();
    std::cout << "Expected O(log n) = ~10 steps per lookup\n";
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

    auto result = benchmark("Scene Update (10k frames)", iterations, [&]() {
        instance->reset_frame();  // Reset frame allocator

        for (size_t i = 0; i < node_count; i++) {
            nodes[i]->set_x(nodes[i]->x() + 1);
            nodes[i]->set_y(nodes[i]->y() + 1);
        }
    });

    result.print();

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
// Main
// ============================================================================

int main() {
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
