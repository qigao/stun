# dxf (libdxfrw)

A free, open-source C++ library for reading and writing DXF files in both ASCII and binary formats, with support for reading DWG files from R14 to V2015.

This is a streamlined, CMake-optimized version of the original [libdxfrw](http://sourceforge.net/projects/libdxfrw) library.

## Features

- **DXF Support**: Read and write DXF files in both ASCII and binary formats.
- **DWG Support**: Read DWG files from AutoCAD R14 through 2015.
- **Pure CMake Build**: Modern CMake configuration with IDE integration (Visual Studio, Ninja, etc.).
- **No External Dependencies**: Includes built-in support for major character encodings without requiring `libiconv`.
- **Integrated Test Suite**: Comprehensive tests for entities, tables, blocks, and various DXF versions.

## Requirements

- **C++ Compiler**: C++17 compatible compiler (GCC, Clang, MSVC).
- **CMake**: Version 3.10 or later.

## Building and Integration

The library is designed to be included as a subdirectory in a larger CMake project.

```cmake
# Add to your main CMakeLists.txt
add_subdirectory(vendor/dxf)

# Link against your target
target_link_libraries(your_target PRIVATE dxfrw)
```

### Standalone Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

### Basic Reading Example

```cpp
#include "libdxfrw.h"
#include "drw_interface.h"

class MyInterface : public DRW_Interface {
public:
    virtual void addLine(const DRW_Line& data) {
        printf("Line from (%.2f, %.2f) to (%.2f, %.2f)\n",
               data.basePoint.x, data.basePoint.y,
               data.secPoint.x, data.secPoint.y);
    }
    // Implement other virtual methods in drw_interface.h as needed...
};

int main() {
    MyInterface iface;
    dxfRW dxf("input.dxf");

    if (!dxf.read(&iface, false)) {
        printf("Error reading DXF file\n");
        return 1;
    }
    return 0;
}
```

## Testing

The library includes a comprehensive test suite. When building with CMake, tests are automatically enabled if the `tests` directory is present.

```bash
# Run all tests using ctest
cd build
ctest --output-on-failure

# Or run specific test executables in the build output folder:
./test_entities
./test_polylines
./test_text
./test_tables
./test_versions
```

## Project Structure

```
dxf/
├── CMakeLists.txt     # Library configuration
├── src/               # Core source code
│   └── intern/        # Internal implementation details
├── tests/             # Unit tests
│   └── CMakeLists.txt # Test suite configuration
└── cmake/             # CMake helper scripts and properties
```

## License

This library is free software; you can redistribute it and/or modify it under the terms of the **GNU General Public License** version 2 or later.

See the [COPYING](COPYING) file for the full license text.
