/**
 * @file example_memory_pool.cpp
 * @brief Demonstrates memory pool usage in nanovg_css
 * 
 * Performance comparison:
 * - Traditional new/delete
 * - Memory pool allocation
 * - Zero-copy string operations
 */

#include <nanovg_css_memory.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

using namespace nvgcss;

// ============================================================================
// Example 1: Object Pool for Animation States
// ============================================================================

struct AnimationState {
    float start_time;
    float duration;
    float values[4];
    
    AnimationState() : start_time(0), duration(1.0f) {
        for (int i = 0; i < 4; i++) values[i] = 0.0f;
    }
};

void example_object_pool() {
    std::cout << "\n=== Example 1: Object Pool ===\n";
    
    MemoryArena arena(1024 * 1024);  // 1MB
    ObjectPool<AnimationState> pool(arena);
    
    // Allocate 1000 animation states
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<AnimationState*> states;
    for (int i = 0; i < 1000; i++) {
        auto* state = pool.allocate();
        state->start_time = i * 0.1f;
        states.push_back(state);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Allocated 1000 objects in " << duration.count() << " µs\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";
    std::cout << "Arena peak: " << arena.peak() << " bytes\n";
    
    // Deallocate (return to pool)
    for (auto* state : states) {
        pool.deallocate(state);
    }
    
    std::cout << "After deallocation, arena used: " << arena.used() << " bytes\n";
    std::cout << "(Memory reused, not freed)\n";
}

// ============================================================================
// Example 2: Smart Pointers with Pool
// ============================================================================

void example_smart_pointers() {
    std::cout << "\n=== Example 2: Smart Pointers ===\n";
    
    MemoryArena arena(1024 * 1024);
    ObjectPool<AnimationState> pool(arena);
    
    {
        // RAII: automatically returned to pool on scope exit
        auto ptr1 = make_pooled(pool);
        auto ptr2 = make_pooled(pool);
        auto ptr3 = make_pooled(pool);
        
        ptr1->start_time = 1.0f;
        ptr2->start_time = 2.0f;
        ptr3->start_time = 3.0f;
        
        std::cout << "Created 3 smart pointers\n";
        std::cout << "Arena used: " << arena.used() << " bytes\n";
    }  // Automatically deallocated here
    
    std::cout << "After scope exit, arena used: " << arena.used() << " bytes\n";
    std::cout << "(Objects returned to pool)\n";
}

// ============================================================================
// Example 3: Temporary Allocations with Mark/Rewind
// ============================================================================

void example_mark_rewind() {
    std::cout << "\n=== Example 3: Mark/Rewind ===\n";
    
    MemoryArena arena(1024 * 1024);
    
    std::cout << "Initial arena used: " << arena.used() << " bytes\n";
    
    // Simulate frame rendering
    for (int frame = 0; frame < 3; frame++) {
        MemoryScope scope(arena);  // Auto-rewind on scope exit
        
        // Allocate temporary data for this frame
        float* temp_vertices = arena.allocate_array<float>(1000);
        char* temp_string = arena.allocate_array<char>(256);
        
        // Use temporary data
        for (int i = 0; i < 1000; i++) {
            temp_vertices[i] = i * 0.1f;
        }
        std::snprintf(temp_string, 256, "Frame %d", frame);
        
        std::cout << "Frame " << frame << ": arena used = " 
                  << arena.used() << " bytes\n";
    }  // Auto-rewind after each frame
    
    std::cout << "After 3 frames, arena used: " << arena.used() << " bytes\n";
    std::cout << "(Memory reused every frame)\n";
}

// ============================================================================
// Example 4: STL Containers with Arena Allocator
// ============================================================================

void example_stl_allocator() {
    std::cout << "\n=== Example 4: Arena Array Allocation ===\n";
    
    MemoryArena arena(1024 * 1024);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Allocate array directly from arena
    int* array = arena.allocate_array<int>(10000);
    for (int i = 0; i < 10000; i++) {
        array[i] = i;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Allocated and filled 10000 elements in " << duration.count() << " µs\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";
    std::cout << "(No malloc calls, all from arena)\n";
}

// ============================================================================
// Example 5: Performance Comparison
// ============================================================================

void benchmark_allocation() {
    std::cout << "\n=== Example 5: Performance Benchmark ===\n";
    
    const int ITERATIONS = 10000;
    
    // Benchmark 1: Traditional new/delete
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<AnimationState*> states;
        for (int i = 0; i < ITERATIONS; i++) {
            states.push_back(new AnimationState());
        }
        for (auto* state : states) {
            delete state;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "new/delete: " << duration.count() << " µs\n";
    }
    
    // Benchmark 2: Memory pool
    {
        MemoryArena arena(1024 * 1024);
        ObjectPool<AnimationState> pool(arena);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<AnimationState*> states;
        for (int i = 0; i < ITERATIONS; i++) {
            states.push_back(pool.allocate());
        }
        for (auto* state : states) {
            pool.deallocate(state);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Memory pool: " << duration.count() << " µs\n";
    }
    
    // Benchmark 3: Arena (no deallocation)
    {
        MemoryArena arena(1024 * 1024);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < ITERATIONS; i++) {
            arena.allocate<AnimationState>();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Arena (bump): " << duration.count() << " µs\n";
    }
}

// ============================================================================
// Example 6: Zero-Copy String Operations
// ============================================================================

void example_zero_copy_strings() {
    std::cout << "\n=== Example 6: Zero-Copy Strings ===\n";
    
    MemoryArena arena(1024 * 1024);
    
    // Traditional approach (multiple allocations)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::string result;
        char temp[32];
        for (int i = 0; i < 100; i++) {
            result += "class-";
            std::snprintf(temp, sizeof(temp), "%d", i);
            result += temp;
            result += " ";
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "std::string: " << duration.count() << " µs\n";
    }
    
    // Arena approach (single allocation)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        char* buffer = arena.allocate_array<char>(2048);
        size_t offset = 0;
        
        for (int i = 0; i < 100; i++) {
            offset += std::snprintf(buffer + offset, 2048 - offset, "class-%d ", i);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Arena buffer: " << duration.count() << " µs\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "nanovg_css Memory Pool Examples\n";
    std::cout << "================================\n";
    
    example_object_pool();
    example_smart_pointers();
    example_mark_rewind();
    example_stl_allocator();
    benchmark_allocation();
    example_zero_copy_strings();
    
    std::cout << "\n=== All examples completed ===\n";
    return 0;
}
