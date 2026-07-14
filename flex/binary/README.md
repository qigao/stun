# Flex Binary Format

Fast binary serialization format for Flex UI files (`.flex` → `.flexb`).

## Features

### ✅ Currently Implemented

- **Compilation**: Parse `.flex` source and serialize to `.flexb` binary
- **Deserialization**: Load `.flexb` files and create runtime objects
- **String Deduplication**: Automatic string table for memory efficiency
- **Integrity Verification**: CRC32 checksum validation
- **Compression**: zstd compression with transparent load support
- **Timeline Serialization**: Float/string/color animation timelines
- **Legacy Compatibility**: Backward-compatible SVG and radial gradient payload reads
- **Statistics**: Track source size, binary size, node count, string count

### ❌ Not Yet Implemented

- **Encryption**: AES encryption (reserved for future)
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
[String Table]      Packed [length][utf8-bytes] entries
[Node Data]         Scene and scene graph
[Asset Table]       (Reserved)
[Animation Data]    Timelines and tracks
[FSM Data]          (Reserved)
```

### FileHeader (96 bytes)

- `magic`: 0x58454C46 ("FLEX")
- `version`: Format version (currently 1.0.4 / `0x00010004`)
- `checksum`: CRC32 of entire file
- Section offsets and sizes
- Canvas dimensions
- Counts (nodes, timelines, assets, etc.)

### Version History

| Version | Hex | Reader status | Notes |
|---------|-----|---------------|-------|
| 1.0.1 | `0x00010001` | Supported for legacy reads | SVG nodes use legacy `ImageData` payload; radial gradients do not carry `fx` / `fy` |
| 1.0.2 | `0x00010002` | Supported | Introduces dedicated `SvgData` payload |
| 1.0.3 | `0x00010003` | Supported | Radial gradients add focal coordinates `fx` / `fy` |
| 1.0.4 | `0x00010004` | Current writer/reader version | Adds `Instance` payloads with float/string/bool inputs |

### Reader Compatibility Matrix

| Binary producer version | Current reader (`1.0.4`) | Notes |
|-------------------------|--------------------------|-------|
| `1.0.1` | Compatible | Legacy SVG payloads are read from `ImageData`; radial gradients fall back to `fx == cx`, `fy == cy` |
| `1.0.2` | Compatible | `SvgData` supported; radial gradients still use legacy focal fallback |
| `1.0.3` | Compatible | Full SVG + radial gradient payload support |
| `1.0.4` | Compatible | Full current format support, including `Instance` payloads |

Compatibility is currently exercised only for the versioned payload changes above. Older or future format revisions are not guaranteed to load unless explicitly covered by tests.

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

### Compression

```cpp
// Supported today
compiler.set_compress(true);
reader.load_file("app.flexb");  // Auto-detects compression
```

### Encryption (Planned)

```cpp
// Future API (not yet implemented)
compiler.set_encrypt(true, "password");
reader.load_file_encrypted("app.flexb", "password");
```

### Timeline Serialization

Current binary format serializes timeline duration, loop mode, tracks, and float/string/color keyframes.
Unsupported value types are skipped at write time.

## Reader Contracts

The reader intentionally splits validation into two layers:

- `load_file()` / `load_memory()` validate magic, checksum, compression, encrypted flag rejection, and section bounds.
- `create_scene()` / `create_timelines()` validate section semantics and decide how much valid data can still be preserved when later payloads are corrupted.

### Load-time Rejection

The binary is rejected before scene creation when any of the following is true:

- Invalid magic number, undersized header, or checksum mismatch
- `FLAG_ENCRYPTED` is present on the regular load path
- zstd-compressed payload is corrupted
- String table, node data, animation data, asset table, or FSM section is out of bounds
- String table entries are truncated or misaligned

### Scene Reconstruction

`create_scene()` uses these recovery rules:

- Invalid root node type or invalid root child offset returns an empty scene
- Root child payload truncated at the start returns an empty scene
- Earlier valid top-level nodes are preserved if a later top-level node is corrupted
- A corrupted nested subtree is dropped as a whole; earlier top-level siblings remain
- Shape payloads truncated inside paint or geometry data now invalidate that node instead of partially reading past the node section

### Timeline Reconstruction

`create_timelines()` uses these recovery rules:

- Missing or invalid first timeline header yields no timelines
- Truncation before a full `TimelineHeader` or `TrackHeader` yields no timelines
- Missing timeline name or track property drops the affected timeline
- A corrupted later timeline does not remove earlier valid timelines
- A corrupted later track or keyframe drops the current timeline being parsed

## Testing

Binary format functionality is covered by `test_binary`:

```bash
# Run binary tests
./test_binary

# Windows build output
build\msvc\bin\test_binary.exe

# Tests include:
# - CRC32 calculation and verification
# - Writer serialization
# - Reader deserialization
# - Round-trip consistency
# - Boundary validation
# - Compiler statistics
# - Compression load/reject paths
# - Legacy SVG / radial gradient compatibility
# - Section-boundary and partial-corruption contracts
# - Scene and timeline recovery behavior across malformed sections
```

The exact test/assertion counts evolve with the format; check the current `test_binary` output.

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

- **Format version**: 1.0.4 (`0x00010004`)
- **Endianness**: Little-endian (x86/x64)
- **Alignment**: Natural alignment (no padding issues)
- **String encoding**: UTF-8

Binary files are portable across platforms with the same endianness.
