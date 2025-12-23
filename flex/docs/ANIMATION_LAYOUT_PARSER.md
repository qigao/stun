# Animation & Layout Parser Implementation

## Overview

Successfully extended the Flex DSL parser to fully support animation and layout syntax.

## ✅ What's New

### 1. Animation Parser

Extended `flex/src/flex_parser.cpp` to fully parse animation blocks:

```flex
anim "fadeIn" {
    duration: 0.5s
    loop: once

    track "opacity" {
        keyframe 0s -> 0.0
        keyframe 0.5s -> 1.0
    }

    track "y" {
        keyframe 0s -> 100
        keyframe 0.3s -> 0
    }
}
```

**Supported Features:**
- ✅ Animation name (quoted string)
- ✅ Duration property (e.g., `0.5s`, `2s`, `100ms`)
- ✅ Loop mode property (`once`, `loop`, `pingpong`)
- ✅ Track definitions (property paths like `"opacity"`, `"y"`, `"#node/property"`)
- ✅ Keyframe definitions (`keyframe time -> value`)

### 2. Layout Parser

Layout properties are parsed as regular node properties:

```flex
group container {
    layout: flex
    flexDirection: column
    justifyContent: center
    alignItems: center
    gap: 30
}
```

**Supported Layout Properties:**
- ✅ `layout: flex` - Enable flexbox layout
- ✅ `flexDirection: row|column` - Main axis direction
- ✅ `justifyContent: center|flex-start|flex-end|space-between|space-around` - Main axis alignment
- ✅ `alignItems: center|flex-start|flex-end|stretch|baseline` - Cross axis alignment
- ✅ `alignContent: center|flex-start|flex-end|stretch|space-between|space-around` - Multi-line alignment
- ✅ `gap: 30` - Spacing between items
- ✅ `flexGrow: 1` - Grow factor
- ✅ `flexShrink: 1` - Shrink factor
- ✅ `flexBasis: auto|100px` - Initial main size
- ✅ `flex: 1` - Shorthand for flex-grow, flex-shrink, flex-basis
- ✅ `alignSelf: auto|flex-start|flex-end|center|stretch|baseline` - Override alignment

## Implementation Details

### Modified Files

#### `flex/src/flex_parser.cpp`
- Extended animation parsing (lines 323-414)
- Added support for:
  - Animation properties: `duration`, `loop`
  - Track parsing with property paths
  - Keyframe parsing with time and value

#### No lexer changes needed
- Animation keywords (`anim`, `track`, `keyframe`) already existed
- Layout properties use existing `TOK_IDENTIFIER` token type

### AST Structure

Animations are stored in `AstAnim`:
```cpp
struct AstAnim {
    std::string name;
    float duration = 0;
    std::string loop_mode = "once";
    std::vector<AstTrack> tracks;
};

struct AstTrack {
    std::string property;  // e.g., "opacity", "y", "#node/property"
    std::vector<AstKeyframe> keyframes;
};

struct AstKeyframe {
    float time;
    AstValue value;
};
```

Layout properties are stored in `AstNode::properties` as regular properties:
```cpp
std::unordered_map<std::string, AstValue> properties;
// Layout properties:
// - "layout" -> "flex"
// - "flexDirection" -> "column"
// - "gap" -> 30.0
```

## Test Files

### 1. `flex/examples/test_animation_layout.flex`
Comprehensive test file with:
- Scene with layout containers
- Flexbox layout examples
- Multiple animations with tracks and keyframes
- State machine with animations

### 2. `flex/examples/test_animation_layout_parser.cpp`
Standalone parser test:
```cpp
int main() {
    const char* test_code = R"(...)";
    auto program = parser::parse(test_code);

    if (!program) {
        std::cerr << "Parse failed\n";
        return 1;
    }

    // Print animations
    std::cout << "Animations: " << program->animations.size() << "\n";
    for (const auto& anim : program->animations) {
        std::cout << "  - " << anim->name << "\n";
        // Print details...
    }

    // Print layout properties
    for (const auto& child : program->scene->children) {
        for (const auto& [key, value] : child->properties) {
            if (is_layout_property(key)) {
                std::cout << key << ": " << value << " (LAYOUT)\n";
            }
        }
    }
}
```

### 3. `flex/examples/test_animation_layout_parser.bat`
Windows batch script to run tests.

## Usage Examples

### Example 1: Simple Animation
```flex
anim "fadeIn" {
    duration: 0.5s
    loop: once

    track "opacity" {
        keyframe 0s -> 0.0
        keyframe 0.5s -> 1.0
    }
}
```

### Example 2: Multi-Track Animation
```flex
anim "moveAndFade" {
    duration: 2.0s
    loop: loop

    track "x" {
        keyframe 0s -> 0
        keyframe 1s -> 100
        keyframe 2s -> 0
    }

    track "opacity" {
        keyframe 0s -> 1.0
        keyframe 1s -> 0.5
        keyframe 2s -> 1.0
    }

    track "#statusText/content" {
        keyframe 0s -> "Loading..."
        keyframe 2s -> "Complete!"
    }
}
```

### Example 3: Complex Layout
```flex
group appContainer {
    layout: flex
    flexDirection: column
    justifyContent: center
    alignItems: center
    gap: 20

    group header {
        flex: 0 0 auto
    }

    group content {
        flex: 1 1 auto
        layout: flex
        flexDirection: row
        justifyContent: space-between
        gap: 10
    }

    group footer {
        flex: 0 0 auto
        alignSelf: stretch
    }
}
```

## Testing

### Run Standalone Test
```bash
build\Ninja\Msvc\bin\test_animation_layout_parser.exe
```

Expected Output:
```
=================================================
Animation & Layout Parser Test
=================================================

Parsing animation and layout syntax...

✅ Parse succeeded!

Scene: test
  Children: 2
    - rect "bg"
    - group "container"
      layout: "flex" (LAYOUT PROPERTY)
      flexDirection: "column" (LAYOUT PROPERTY)
      justifyContent: "center" (LAYOUT PROPERTY)
      alignItems: "center" (LAYOUT PROPERTY)
      gap: 30 (LAYOUT PROPERTY)

Animations: 2
  - Animation: "fadeIn"
    Duration: 0.5s
    Loop: once
    Tracks: 1
      - Track: "opacity"
        Keyframes: 2
          - 0s -> 0
          - 0.5s -> 1

  - Animation: "slideUp"
    Duration: 0.3s
    Loop: once
    Tracks: 1
      - Track: "y"
        Keyframes: 2
          - 0s -> 100
          - 0.3s -> 0

State Machines: 1
  - Machine: statusTracker
    Layers: 1
      - Layer: status
        States: 1
          - State: neutral (initial) animation="fadeIn"
        Transitions: 1
          - neutral -> positive when counter > 0

=================================================
✅ Animation & Layout parsing works!
=================================================
```

### Run with test_parser
```bash
build\Ninja\Msvc\bin\test_parser.exe flex/examples/test_animation_layout.flex
```

## Parser Implementation Details

### Animation Parsing Flow
1. **Parse animation name** (quoted string)
   ```cpp
   anim "fadeIn" { ... }
   ```

2. **Parse properties**
   ```cpp
   duration: 0.5s
   loop: once
   ```

3. **Parse tracks**
   ```cpp
   track "opacity" { ... }
   ```

4. **Parse keyframes**
   ```cpp
   keyframe 0s -> 0.0
   keyframe 0.5s -> 1.0
   ```

### Layout Parsing Flow
Layout properties are parsed as regular node properties:
```cpp
group container {
    layout: flex              // TOK_IDENTIFIER : TOK_IDENTIFIER
    flexDirection: column     // TOK_IDENTIFIER : TOK_IDENTIFIER
    gap: 30                   // TOK_IDENTIFIER : TOK_NUMBER
}
```

## Supported Animation Features

### ✅ Completed
- Animation blocks with name
- Duration property (supports `s` and `ms` units)
- Loop mode property (`once`, `loop`, `pingpong`)
- Track definitions with property paths
- Keyframe definitions with time and value
- Multiple tracks per animation
- Multiple keyframes per track

### ⏳ Future Enhancements
- Easing functions (`ease-in`, `ease-out`, `cubic-bezier`)
- Animation delay
- Animation fill mode (`forwards`, `backwards`, `both`)
- Timeline synchronization
- Event-based animations

## Supported Layout Features

### ✅ Flexbox Properties
- Container properties:
  - `layout: flex`
  - `flexDirection: row|column|row-reverse|column-reverse`
  - `flexWrap: nowrap|wrap|wrap-reverse`
  - `justifyContent: flex-start|flex-end|center|space-between|space-around|space-evenly`
  - `alignItems: flex-start|flex-end|center|stretch|baseline`
  - `alignContent: flex-start|flex-end|center|stretch|space-between|space-around|space-evenly`
  - `gap`, `row-gap`, `column-gap`

- Item properties:
  - `flex: <grow> <shrink> <basis>`
  - `flexGrow: <number>`
  - `flexShrink: <number>`
  - `flexBasis: auto|<length>`
  - `alignSelf: auto|flex-start|flex-end|center|stretch|baseline`

## ✅ Status

**Animation Parser: COMPLETE** ✅
- Full animation syntax parsing
- Tracks and keyframes supported
- AST creation working

**Layout Parser: COMPLETE** ✅
- All flexbox properties supported
- Parsed as regular node properties
- No special syntax needed

**Tests: READY** ✅
- Test files created
- Batch scripts provided
- Documentation complete

The parser now fully supports both animation and layout syntax as defined in the Flex DSL specification!
