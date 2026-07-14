# Flex Binary Format - Usage Examples

## Example 1: Basic Compilation

```cpp
#include "flex/binary/compiler.h"
#include <iostream>

int main() {
    using namespace flex::binary;

    // Compile .flex to .flexb
    BinaryCompiler compiler;
    if (!compiler.compile_to_file("my_app.flex", "my_app.flexb")) {
        std::cerr << "Compilation failed: " << compiler.error_message() << "\n";
        return 1;
    }

    std::cout << "Compiled successfully!\n";
    return 0;
}
```

## Example 2: Compilation with Statistics

```cpp
#include "flex/binary/compiler.h"
#include <iostream>

int main() {
    using namespace flex::binary;

    BinaryCompiler compiler;
    if (!compiler.compile_to_file("app.flex", "app.flexb")) {
        std::cerr << "Error: " << compiler.error_message() << "\n";
        return 1;
    }

    // Print statistics
    const auto& stats = compiler.stats();
    std::cout << "Compilation Statistics:\n";
    std::cout << "  Source size:  " << stats.source_size << " bytes\n";
    std::cout << "  Binary size:  " << stats.binary_size << " bytes\n";
    std::cout << "  Nodes:        " << stats.node_count << "\n";
    std::cout << "  Strings:      " << stats.string_count << "\n";
    std::cout << "  Timelines:    " << stats.timeline_count << "\n";

    if (stats.source_size > 0) {
        float reduction = 100.0f * (1.0f - (float)stats.binary_size / (float)stats.source_size);
        std::cout << "  Reduction:    " << reduction << "%\n";
    }

    return 0;
}
```

## Example 3: Loading Binary File

```cpp
#include "flex/binary/reader.h"
#include <iostream>

int main() {
    using namespace flex::binary;

    // Load .flexb file
    BinaryReader reader;
    if (!reader.load_file("app.flexb")) {
        std::cerr << "Load failed: " << reader.error_message() << "\n";
        return 1;
    }

    // Print metadata
    std::cout << "Loaded successfully!\n";
    std::cout << "  Canvas: " << reader.canvas_width()
              << "x" << reader.canvas_height() << "\n";
    std::cout << "  Nodes: " << reader.node_count() << "\n";
    std::cout << "  Timelines: " << reader.timeline_count() << "\n";

    // Create runtime scene
    auto scene = reader.create_scene();
    if (!scene) {
        std::cerr << "Failed to create scene\n";
        return 1;
    }

    std::cout << "Scene created!\n";
    return 0;
}
```

## Example 4: Complete Round-trip

```cpp
#include "flex/binary/compiler.h"
#include "flex/binary/reader.h"
#include <iostream>
#include <fstream>

int main() {
    using namespace flex::binary;

    // 1. Write .flex source
    const char* source = R"(
scene MyApp {
    width: 800
    height: 600
}
)";

    std::ofstream out("test.flex");
    out << source;
    out.close();

    // 2. Compile to binary
    BinaryCompiler compiler;
    if (!compiler.compile_to_file("test.flex", "test.flexb")) {
        std::cerr << "Compile error: " << compiler.error_message() << "\n";
        return 1;
    }

    std::cout << "Compiled: " << compiler.stats().binary_size << " bytes\n";

    // 3. Load binary
    BinaryReader reader;
    if (!reader.load_file("test.flexb")) {
        std::cerr << "Load error: " << reader.error_message() << "\n";
        return 1;
    }

    std::cout << "Loaded: " << reader.node_count() << " nodes\n";

    // 4. Create runtime objects
    auto scene = reader.create_scene();
    if (!scene) {
        std::cerr << "Failed to create scene\n";
        return 1;
    }

    std::cout << "Scene: " << scene->width()
              << "x" << scene->height() << "\n";

    return 0;
}
```

## Example 5: Using with Flex Instance

```cpp
#include <flex.h>
#include "flex/binary/reader.h"
#include "backends/thorvg/init.h"
#include <iostream>
#include <vector>
#include <thorvg.h>

int main() {
    // Initialize backend integration
    flex::thorvg_backend::init();
    flex::thorvg_backend::register_backend();

    // Option 1: Load .flexb directly with Definition
    auto definition = flex::Definition::load_binary("app.flexb");
    if (definition->has_error()) {
        std::cerr << "Error: " << definition->error_message() << "\n";
        return 1;
    }

    // Option 2: Use BinaryReader for more control
    flex::binary::BinaryReader reader;
    if (!reader.load_file("app.flexb")) {
        std::cerr << "Error: " << reader.error_message() << "\n";
        return 1;
    }

    // Create instance
    auto instance = flex::Instance::create(definition);

    // Render
    auto canvas = tvg::SwCanvas::gen();
    std::vector<uint32_t> buffer(800 * 600);
    canvas->target(buffer.data(), 800, 800, 600, tvg::ColorSpace::ARGB8888);

    auto renderer = flex::create_renderer(static_cast<flex::CanvasHandle>(canvas.get()));
    if (!renderer) {
        std::cerr << "Failed to create renderer\n";
        return 1;
    }

    renderer->begin_frame(800.0f, 600.0f, 1.0f);
    renderer->clear(instance->scene()->background());
    instance->render(*renderer);
    renderer->end_frame();

    // Cleanup
    flex::thorvg_backend::shutdown();

    return 0;
}
```

## Command-line Usage

```bash
# Basic compilation
flex-compiler app.flex -o app.flexb

# With compression
flex-compiler app.flex -o app.flexb --compress

# With statistics
flex-compiler app.flex -o app.flexb --compress --stats

# Output:
# Compiling: app.flex → app.flexb (compressed)
#
# Compilation Statistics:
#   Source size:  1234 bytes
#   Binary size:  567 bytes
#   Nodes:        42
#   Strings:      15
#   Timelines:    2
#   Reduction:    54.05%
#
# Success! Output: app.flexb
```

## Error Handling Best Practices

```cpp
#include "flex/binary/compiler.h"
#include "flex/binary/reader.h"
#include <iostream>

bool compile_and_load(const char* flex_file, const char* flexb_file) {
    using namespace flex::binary;

    // Compile
    BinaryCompiler compiler;
    if (!compiler.compile_to_file(flex_file, flexb_file)) {
        std::cerr << "Compilation failed:\n";
        std::cerr << "  File: " << flex_file << "\n";
        std::cerr << "  Error: " << compiler.error_message() << "\n";
        return false;
    }

    // Verify by loading
    BinaryReader reader;
    if (!reader.load_file(flexb_file)) {
        std::cerr << "Binary verification failed:\n";
        std::cerr << "  File: " << flexb_file << "\n";
        std::cerr << "  Error: " << reader.error_message() << "\n";
        return false;
    }

    // Verify scene creation
    auto scene = reader.create_scene();
    if (!scene) {
        std::cerr << "Scene creation failed\n";
        return false;
    }

    std::cout << "Success!\n";
    std::cout << "  Binary: " << reader.node_count() << " nodes, "
              << reader.canvas_width() << "x" << reader.canvas_height() << "\n";

    return true;
}

int main() {
    if (!compile_and_load("app.flex", "app.flexb")) {
        return 1;
    }
    return 0;
}
```

## Performance Tips

1. **Precompile for production**: Compile `.flex` to `.flexb` during build time, ship only `.flexb`
2. **Verify integrity**: Binary files include CRC32 checksums automatically
3. **String deduplication**: Automatically applied during compilation
4. **Memory efficiency**: Use `load_memory()` if you already have the file in memory

## Example 6: Using Compression

```cpp
#include "flex/binary/compiler.h"
#include "flex/binary/reader.h"
#include <iostream>

int main() {
    using namespace flex::binary;

    const char* source = R"(
scene MyApp {
    width: 1920
    height: 1080
}
)";

    // Compile without compression
    BinaryCompiler compiler_uncompressed;
    std::vector<uint8_t> binary_uncompressed = compiler_uncompressed.compile_source(source);

    // Compile with compression
    BinaryCompiler compiler_compressed;
    compiler_compressed.set_compress(true);
    std::vector<uint8_t> binary_compressed = compiler_compressed.compile_source(source);

    std::cout << "Uncompressed: " << binary_uncompressed.size() << " bytes\n";
    std::cout << "Compressed:   " << binary_compressed.size() << " bytes\n";

    float reduction = 100.0f * (1.0f - (float)binary_compressed.size() / (float)binary_uncompressed.size());
    std::cout << "Reduction:    " << reduction << "%\n";

    // Both load and work identically
    BinaryReader reader;
    reader.load_memory(binary_compressed.data(), binary_compressed.size());
    auto scene = reader.create_scene();

    std::cout << "Scene: " << scene->width() << "x" << scene->height() << "\n";

    return 0;
}
```

## Limitations (Current Version)

- ✅ **Compression**: zstd compression supported (use `set_compress(true)`)
- ❌ **No encryption**: Binary files are not encrypted (future feature)
- ❌ **No timeline serialization**: Only scene/scene graph is saved
- ❌ **No asset embedding**: External assets are not embedded
