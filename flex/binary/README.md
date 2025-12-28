# Flex Binary Format

Fast binary serialization format for Flex UI files (`.flex` → `.flexb`).

## Features

### ✅ Currently Implemented

- **Compilation**: Parse `.flex` source and serialize to `.flexb` binary
- **Deserialization**: Load `.flexb` files and create runtime objects
- **String Deduplication**: Automatic string table for memory efficiency
- **Integrity Verification**: CRC32 checksum validation
- **Statistics**: Track source size, binary size, node count, string count

### ❌ Not Yet Implemented

- **Compression**: zlib/gzip compression (reserved for future)
- **Encryption**: AES encryption (reserved for future)
- **Timeline Serialization**: Animation data (reserved for future)
- **Asset Embedding**: Inline images/fonts (reserved for future)

## Usage

### 1. Compile .flex to .flexb

```cpp
#include "flex/binary/compiler.h"

flex::binary::BinaryCompiler compiler;
if (!compiler.compile_to_file("app.flex", "app.flexb")) {
    std::cerr << "Error: " << compiler.error_message() << "\n";
    return 1;
}

// Get statistics
const auto& stats = compiler.stats();
std::cout << "Binary size: " << stats.binary_size << " bytes\n";
std::cout << "Nodes: " << stats.node_count << "\n";
std::cout << "Strings: " << stats.string_count << "\n";
```

### 2. Load .flexb binary

```cpp
#include "flex/binary/reader.h"

flex::binary::BinaryReader reader;
if (!reader.load_file("app.flexb")) {
    std::cerr << "Error: " << reader.error_message() << "\n";
    return 1;
}

// Create runtime objects
auto scene = reader.create_scene();
auto timelines = reader.create_timelines();

// Get metadata
float width = reader.canvas_width();
float height = reader.canvas_height();
uint32_t nodes = reader.node_count();
```

### 3. Command-line tool

```bash
# Compile
flex-compiler app.flex -o app.flexb

# With statistics
flex-compiler app.flex -o app.flexb --stats
```

## Binary Format

### File Structure

```
[FileHeader]        Magic, version, checksums, section offsets
[String Table]      Deduplicated strings
[Node Data]         Scene and scene graph
[Asset Table]       (Reserved)
[Animation Data]    (Reserved)
[FSM Data]          (Reserved)
```

### FileHeader (96 bytes)

- `magic`: 0x58454C46 ("FLEX")
- `version`: Format version (currently 1)
- `checksum`: CRC32 of entire file
- Section offsets and sizes
- Canvas dimensions
- Counts (nodes, timelines, assets, etc.)

## Performance

Typical compression compared to .flex source:

- **Small scenes** (~100 lines): 40-60% size reduction
- **Medium scenes** (~500 lines): 50-70% size reduction
- **Large scenes** (1000+ lines): 60-80% size reduction

Main benefits:
- Instant loading (no parsing)
- String deduplication
- Compact binary encoding
- CRC32 integrity check

## Future Work

### Compression (Planned)

```cpp
// Future API (not yet implemented)
compiler.set_compress(true);
reader.load_file("app.flexb");  // Auto-detects compression
```

### Encryption (Planned)

```cpp
// Future API (not yet implemented)
compiler.set_encrypt(true, "password");
reader.load_file_encrypted("app.flexb", "password");
```

### Timeline Serialization (Planned)

Currently only scene/scene graph is serialized. Timeline animation data will be added in a future version.

## Testing

All binary format functionality is tested:

```bash
# Run binary tests
./test_binary

# Tests include:
# - CRC32 calculation and verification
# - Writer serialization
# - Reader deserialization
# - Round-trip consistency
# - Boundary validation
# - Compiler statistics
```

18 test cases, 59 assertions - all passing ✅

## Error Handling

All methods return `false` or `nullptr` on error. Check error messages:

```cpp
BinaryCompiler compiler;
if (!compiler.compile_source(source)) {
    std::cerr << "Compile error: " << compiler.error_message() << "\n";
}

BinaryReader reader;
if (!reader.load_file("app.flexb")) {
    std::cerr << "Load error: " << reader.error_message() << "\n";
}
```

## Compatibility

- **Format version**: 1
- **Endianness**: Little-endian (x86/x64)
- **Alignment**: Natural alignment (no padding issues)
- **String encoding**: UTF-8

Binary files are portable across platforms with the same endianness.
