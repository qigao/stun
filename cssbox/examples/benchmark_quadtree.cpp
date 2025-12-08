/**
 * Quadtree Layout Engine Benchmark
 *
 * Measures performance of the three-phase layout algorithm:
 * - Tree building
 * - Constraint propagation (phase 1)
 * - Dimension calculation (phase 2)
 * - Position calculation (phase 3)
 */

#include <cssbox.h>
#include <cssbox_internal.h>
#include <cssbox_quadtree.h>
#include <chrono>
#include <cstdio>
#include <vector>

using namespace cssbox;
using Clock = std::chrono::high_resolution_clock;

struct BenchmarkResult {
    const char* name;
    int element_count;
    double build_ms;
    double layout_ms;
    double total_ms;
};

static void print_result(const BenchmarkResult& r) {
    printf("%-40s | %6d elements | build: %8.3f ms | layout: %8.3f ms | total: %8.3f ms | %.0f elem/ms\n",
           r.name, r.element_count, r.build_ms, r.layout_ms, r.total_ms,
           r.element_count / r.total_ms);
}

static void print_header() {
    printf("\n");
    printf("================================================================================\n");
    printf("Quadtree Layout Engine Benchmark\n");
    printf("================================================================================\n\n");
}

// -----------------------------------------------------------------------------
// Benchmark: Flat list of block elements
// -----------------------------------------------------------------------------
static BenchmarkResult benchmark_flat_blocks(int count) {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1920, 1080);

    cssboxElement* root = cssboxCreateElement(renderer, "root", "div");
    root->style.width = Length::percent(100);
    root->style.height = Length::percent(100);

    for (int i = 0; i < count; i++) {
        cssboxElement* child = cssboxCreateElement(renderer, nullptr, "div");
        child->style.width = Length::percent(100);
        child->style.height = Length::px(50);
        cssboxAppendChild(renderer, root, child);
    }

    QuadtreeLayoutEngine engine(1920, 1080);

    auto t0 = Clock::now();
    LayoutNode* tree = engine.build_tree(root, renderer);
    auto t1 = Clock::now();
    engine.compute_layout(tree, renderer);
    auto t2 = Clock::now();

    double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double layout_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    delete tree;
    cssboxDeleteRenderer(renderer);

    return {"Flat blocks", count, build_ms, layout_ms, build_ms + layout_ms};
}

// -----------------------------------------------------------------------------
// Benchmark: Deep nested tree
// -----------------------------------------------------------------------------
static BenchmarkResult benchmark_deep_nesting(int depth) {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1920, 1080);

    cssboxElement* root = cssboxCreateElement(renderer, "root", "div");
    root->style.width = Length::percent(100);
    root->style.height = Length::percent(100);

    cssboxElement* current = root;
    for (int i = 0; i < depth; i++) {
        cssboxElement* child = cssboxCreateElement(renderer, nullptr, "div");
        child->style.width = Length::percent(100);
        child->style.height = Length::px(50);
        child->style.padding[0] = Length::px(5);  // top
        child->style.padding[3] = Length::px(5);  // left
        cssboxAppendChild(renderer, current, child);
        current = child;
    }

    QuadtreeLayoutEngine engine(1920, 1080);

    auto t0 = Clock::now();
    LayoutNode* tree = engine.build_tree(root, renderer);
    auto t1 = Clock::now();
    engine.compute_layout(tree, renderer);
    auto t2 = Clock::now();

    double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double layout_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    delete tree;
    cssboxDeleteRenderer(renderer);

    return {"Deep nesting", depth, build_ms, layout_ms, build_ms + layout_ms};
}

// -----------------------------------------------------------------------------
// Benchmark: Flex row with items
// -----------------------------------------------------------------------------
static BenchmarkResult benchmark_flex_row(int count) {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1920, 1080);

    cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
    container->style.display = Display::FLEX;
    container->style.flex_direction = FlexDirection::ROW;
    container->style.width = Length::percent(100);
    container->style.height = Length::px(100);

    for (int i = 0; i < count; i++) {
        cssboxElement* child = cssboxCreateElement(renderer, nullptr, "div");
        child->style.flex_grow = 1;
        child->style.height = Length::percent(100);
        cssboxAppendChild(renderer, container, child);
    }

    QuadtreeLayoutEngine engine(1920, 1080);

    auto t0 = Clock::now();
    LayoutNode* tree = engine.build_tree(container, renderer);
    auto t1 = Clock::now();
    engine.compute_layout(tree, renderer);
    auto t2 = Clock::now();

    double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double layout_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    delete tree;
    cssboxDeleteRenderer(renderer);

    return {"Flex row", count, build_ms, layout_ms, build_ms + layout_ms};
}

// -----------------------------------------------------------------------------
// Benchmark: Flex column with items
// -----------------------------------------------------------------------------
static BenchmarkResult benchmark_flex_column(int count) {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1920, 1080);

    cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
    container->style.display = Display::FLEX;
    container->style.flex_direction = FlexDirection::COLUMN;
    container->style.width = Length::percent(100);
    container->style.height = Length::percent(100);

    for (int i = 0; i < count; i++) {
        cssboxElement* child = cssboxCreateElement(renderer, nullptr, "div");
        child->style.flex_grow = 1;
        child->style.width = Length::percent(100);
        cssboxAppendChild(renderer, container, child);
    }

    QuadtreeLayoutEngine engine(1920, 1080);

    auto t0 = Clock::now();
    LayoutNode* tree = engine.build_tree(container, renderer);
    auto t1 = Clock::now();
    engine.compute_layout(tree, renderer);
    auto t2 = Clock::now();

    double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double layout_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    delete tree;
    cssboxDeleteRenderer(renderer);

    return {"Flex column", count, build_ms, layout_ms, build_ms + layout_ms};
}

// -----------------------------------------------------------------------------
// Benchmark: Grid layout
// -----------------------------------------------------------------------------
static BenchmarkResult benchmark_grid(int cols, int rows) {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1920, 1080);

    cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
    container->style.display = Display::GRID;
    container->style.width = Length::percent(100);
    container->style.height = Length::percent(100);

    // Define grid template
    for (int c = 0; c < cols; c++) {
        container->style.grid_template_columns.push_back(GridTrack::fr(1));
    }
    for (int r = 0; r < rows; r++) {
        container->style.grid_template_rows.push_back(GridTrack::fr(1));
    }

    int count = cols * rows;
    for (int i = 0; i < count; i++) {
        cssboxElement* child = cssboxCreateElement(renderer, nullptr, "div");
        cssboxAppendChild(renderer, container, child);
    }

    QuadtreeLayoutEngine engine(1920, 1080);

    auto t0 = Clock::now();
    LayoutNode* tree = engine.build_tree(container, renderer);
    auto t1 = Clock::now();
    engine.compute_layout(tree, renderer);
    auto t2 = Clock::now();

    double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double layout_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    delete tree;
    cssboxDeleteRenderer(renderer);

    char name[64];
    snprintf(name, sizeof(name), "Grid %dx%d", cols, rows);
    return {strdup(name), count, build_ms, layout_ms, build_ms + layout_ms};
}

// -----------------------------------------------------------------------------
// Benchmark: Mixed layout (realistic UI)
// -----------------------------------------------------------------------------
static BenchmarkResult benchmark_mixed_layout(int sections) {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1920, 1080);

    cssboxElement* root = cssboxCreateElement(renderer, "root", "div");
    root->style.display = Display::FLEX;
    root->style.flex_direction = FlexDirection::COLUMN;
    root->style.width = Length::percent(100);
    root->style.height = Length::percent(100);

    int total_elements = 1;

    for (int s = 0; s < sections; s++) {
        // Section header (block)
        cssboxElement* header = cssboxCreateElement(renderer, nullptr, "div");
        header->style.width = Length::percent(100);
        header->style.height = Length::px(60);
        cssboxAppendChild(renderer, root, header);
        total_elements++;

        // Section content (flex row with cards)
        cssboxElement* content = cssboxCreateElement(renderer, nullptr, "div");
        content->style.display = Display::FLEX;
        content->style.flex_direction = FlexDirection::ROW;
        content->style.flex_wrap = FlexWrap::WRAP;
        content->style.width = Length::percent(100);
        content->style.gap = Length::px(16);
        cssboxAppendChild(renderer, root, content);
        total_elements++;

        // Cards
        for (int c = 0; c < 6; c++) {
            cssboxElement* card = cssboxCreateElement(renderer, nullptr, "div");
            card->style.width = Length::px(300);
            card->style.height = Length::px(200);
            card->style.padding[0] = Length::px(16);
            card->style.padding[1] = Length::px(16);
            card->style.padding[2] = Length::px(16);
            card->style.padding[3] = Length::px(16);
            cssboxAppendChild(renderer, content, card);
            total_elements++;

            // Card content
            for (int i = 0; i < 3; i++) {
                cssboxElement* line = cssboxCreateElement(renderer, nullptr, "div");
                line->style.width = Length::percent(100);
                line->style.height = Length::px(20);
                cssboxAppendChild(renderer, card, line);
                total_elements++;
            }
        }
    }

    QuadtreeLayoutEngine engine(1920, 1080);

    auto t0 = Clock::now();
    LayoutNode* tree = engine.build_tree(root, renderer);
    auto t1 = Clock::now();
    engine.compute_layout(tree, renderer);
    auto t2 = Clock::now();

    double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double layout_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    delete tree;
    cssboxDeleteRenderer(renderer);

    return {"Mixed layout (UI-like)", total_elements, build_ms, layout_ms, build_ms + layout_ms};
}

// -----------------------------------------------------------------------------
// Benchmark: Repeated layout (simulate animation frame)
// -----------------------------------------------------------------------------
static void benchmark_repeated_layout(int iterations) {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1920, 1080);

    // Build a moderate complexity tree
    cssboxElement* root = cssboxCreateElement(renderer, "root", "div");
    root->style.display = Display::FLEX;
    root->style.flex_direction = FlexDirection::COLUMN;
    root->style.width = Length::percent(100);
    root->style.height = Length::percent(100);

    for (int i = 0; i < 50; i++) {
        cssboxElement* row = cssboxCreateElement(renderer, nullptr, "div");
        row->style.display = Display::FLEX;
        row->style.flex_direction = FlexDirection::ROW;
        row->style.width = Length::percent(100);
        row->style.height = Length::px(50);
        cssboxAppendChild(renderer, root, row);

        for (int j = 0; j < 10; j++) {
            cssboxElement* cell = cssboxCreateElement(renderer, nullptr, "div");
            cell->style.flex_grow = 1;
            cell->style.height = Length::percent(100);
            cssboxAppendChild(renderer, row, cell);
        }
    }

    QuadtreeLayoutEngine engine(1920, 1080);

    // Build tree once
    LayoutNode* tree = engine.build_tree(root, renderer);

    // Time repeated layouts
    auto t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        engine.compute_layout(tree, renderer);
    }
    auto t1 = Clock::now();

    double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double avg_ms = total_ms / iterations;

    printf("\nRepeated layout benchmark (%d iterations, 550 elements):\n", iterations);
    printf("  Total time: %.3f ms\n", total_ms);
    printf("  Average per layout: %.3f ms (%.1f fps potential)\n", avg_ms, 1000.0 / avg_ms);

    delete tree;
    cssboxDeleteRenderer(renderer);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    print_header();

    std::vector<BenchmarkResult> results;

    // Flat blocks
    printf("Running: Flat blocks...\n");
    results.push_back(benchmark_flat_blocks(100));
    results.push_back(benchmark_flat_blocks(500));
    results.push_back(benchmark_flat_blocks(1000));
    results.push_back(benchmark_flat_blocks(5000));

    // Deep nesting
    printf("Running: Deep nesting...\n");
    results.push_back(benchmark_deep_nesting(10));
    results.push_back(benchmark_deep_nesting(50));
    results.push_back(benchmark_deep_nesting(100));
    results.push_back(benchmark_deep_nesting(500));

    // Flex row
    printf("Running: Flex row...\n");
    results.push_back(benchmark_flex_row(100));
    results.push_back(benchmark_flex_row(500));
    results.push_back(benchmark_flex_row(1000));

    // Flex column
    printf("Running: Flex column...\n");
    results.push_back(benchmark_flex_column(100));
    results.push_back(benchmark_flex_column(500));
    results.push_back(benchmark_flex_column(1000));

    // Grid
    printf("Running: Grid...\n");
    results.push_back(benchmark_grid(10, 10));
    results.push_back(benchmark_grid(20, 20));
    results.push_back(benchmark_grid(50, 50));

    // Mixed layout
    printf("Running: Mixed layout...\n");
    results.push_back(benchmark_mixed_layout(5));
    results.push_back(benchmark_mixed_layout(10));
    results.push_back(benchmark_mixed_layout(20));

    // Print results
    printf("\n");
    printf("================================================================================\n");
    printf("Results\n");
    printf("================================================================================\n\n");

    for (const auto& r : results) {
        print_result(r);
    }

    // Repeated layout (animation simulation)
    printf("\n");
    printf("================================================================================\n");
    printf("Animation Simulation\n");
    printf("================================================================================\n");
    benchmark_repeated_layout(100);
    benchmark_repeated_layout(1000);

    printf("\n");
    return 0;
}
