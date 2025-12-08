/**
 * Grid Parser Performance Benchmark
 *
 * Compares re2c-based parser performance
 */

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>
#include "cssbox_conversion.h"

using Clock = std::chrono::high_resolution_clock;

static void benchmark_parse(const char* name, const char* input, int iterations) {
    auto t0 = Clock::now();

    for (int i = 0; i < iterations; i++) {
        auto tracks = cssbox::convert::parse_grid_track_list(input);
        (void)tracks;  // Prevent optimization
    }

    auto t1 = Clock::now();
    double total_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double per_call = total_us / iterations;

    printf("%-40s | %8.2f us/call | %8.0f calls/ms\n",
           name, per_call, 1000.0 / per_call);
}

int main() {
    const int ITERATIONS = 100000;

    printf("\n");
    printf("=======================================================================\n");
    printf("CSS Grid Track Parser Benchmark (re2c-based)\n");
    printf("=======================================================================\n\n");
    printf("%-40s | %14s | %14s\n", "Input", "Time", "Throughput");
    printf("-----------------------------------------------------------------------\n");

    // Simple cases
    benchmark_parse("Simple: 100px", "100px", ITERATIONS);
    benchmark_parse("Simple: 1fr", "1fr", ITERATIONS);
    benchmark_parse("Simple: auto", "auto", ITERATIONS);

    // Multiple tracks
    benchmark_parse("3 tracks: 100px 200px 300px", "100px 200px 300px", ITERATIONS);
    benchmark_parse("3 tracks: 1fr 2fr 1fr", "1fr 2fr 1fr", ITERATIONS);
    benchmark_parse("Mixed: 100px 1fr auto", "100px 1fr auto", ITERATIONS);

    // Functions
    benchmark_parse("minmax(100px, 1fr)", "minmax(100px, 1fr)", ITERATIONS);
    benchmark_parse("fit-content(200px)", "fit-content(200px)", ITERATIONS);

    // Repeat
    benchmark_parse("repeat(3, 1fr)", "repeat(3, 1fr)", ITERATIONS);
    benchmark_parse("repeat(4, 100px 1fr)", "repeat(4, 100px 1fr)", ITERATIONS);
    benchmark_parse("repeat(auto-fill, minmax(200px,1fr))",
                    "repeat(auto-fill, minmax(200px, 1fr))", ITERATIONS);

    // Complex real-world patterns
    benchmark_parse("12-col grid: repeat(12, 1fr)", "repeat(12, 1fr)", ITERATIONS);
    benchmark_parse("Holy grail: 200px 1fr 200px", "200px 1fr 200px", ITERATIONS);
    benchmark_parse("Dashboard: 250px repeat(3, 1fr) 250px",
                    "250px repeat(3, 1fr) 250px", ITERATIONS);

    // Stress test
    benchmark_parse("Complex: repeat(6, minmax(100px, 1fr))",
                    "repeat(6, minmax(100px, 1fr))", ITERATIONS);

    printf("\n");
    printf("=======================================================================\n");
    printf("Iterations per test: %d\n", ITERATIONS);
    printf("=======================================================================\n\n");

    return 0;
}
