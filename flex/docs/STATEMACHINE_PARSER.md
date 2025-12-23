# State Machine Parser Extension

## Overview

Extended the Flex DSL parser to fully support state machine syntax, including layers, states, transitions, and conditions.

## ✅ What's New

### 1. Complete Machine Parsing
The parser now fully parses `machine` blocks instead of skipping them:

```flex
machine statusTracker {
    layer status {
        state neutral {
            initial: true
            animation: "toNeutral"
        }

        state positive {
            animation: "toPositive"
        }

        transition neutral -> positive when counter > 0
        transition positive -> neutral when counter < 0.1
    }
}
```

### 2. State Properties
Supports parsing state properties:
- `initial: true` - Marks initial state
- `animation: "name"` - Associated animation

### 3. Transition Conditions
Supports parsing transition conditions:
- `when counter > 0`
- `when counter < 5`
- `when counter == 10`

## Files Modified

### `flex/src/flex_parser.cpp`
Extended `parse_top_level()` function to fully parse:
- `machine` blocks
- `layer` definitions
- `state` definitions with properties
- `transition` rules with conditions

Added parsing for:
- State properties (`initial`, `animation`)
- Transition conditions (`when input > value`)

## Test Files Created

### 1. `flex/examples/test_statemachine.flex`
Simple state machine test file:
```flex
scene test {
    rect background { ... }
    text title { ... }
}

machine statusTracker {
    layer status {
        state neutral {
            initial: true
            animation: "toNeutral"
        }

        transition neutral -> positive when counter > 0
    }
}
```

### 2. `flex/examples/test_statemachine_parser.cpp`
Standalone test for state machine parsing:
```cpp
int main() {
    const char* test_code = R"(...)";
    auto program = parser::parse(test_code);

    if (!program) {
        std::cerr << "Parse failed\n";
        return 1;
    }

    std::cout << "Machines: " << program->machines.size() << "\n";
    // Print machine details...
}
```

### 3. `flex/examples/test_parser.cpp` (modified)
Extended to print state machine information:
```cpp
// Print state machines
std::cout << "\nState Machines:\n";
const auto& machines = definition->machines();
for (const auto& machine : machines) {
    std::cout << "  - Machine: \"" << machine->name() << "\"\n";
    // Print layers, states, transitions...
}
```

### 4. `flex/examples/test_statemachine_parser.bat`
Windows batch file to run the test.

## Usage

### Using test_parser
```bash
# Parse and print state machine info
build\Ninja\Msvc\bin\test_parser.exe flex/examples/test_statemachine.flex
```

Output:
```
✅ Parse succeeded!

State Machines:
  Found 1 machine(s)
  - Machine: "statusTracker"
    Layers: 1
      - Layer: "status"
        States: 2
          - State: "neutral" (initial) animation="toNeutral"
          - State: "positive" animation="toPositive"
        Transitions: 2
          - neutral -> positive when counter > 0
          - positive -> neutral when counter < 0.1
```

### Using test_statemachine_parser
```bash
# Standalone test
build\Ninja\Msvc\bin\test_statemachine_parser.exe
```

Output:
```
=================================================
State Machine Parser Test
=================================================

Parsing machine syntax...

✅ Parse succeeded!

State Machines: 1
  - Machine: statusTracker
    Layers: 1
      - Layer: status
        States: 2
          - State: neutral (initial) animation="toNeutral"
          - State: positive animation="toPositive"
        Transitions: 2
          - neutral -> positive when counter > 0
          - positive -> neutral when counter < 0.1

=================================================
✅ State machine parsing works!
=================================================
```

## Parser Implementation Details

### Machine Parsing Flow
1. **Parse machine name**
   ```cpp
   machine statusTracker { ... }
   ```

2. **Parse layers**
   ```cpp
   layer status { ... }
   ```

3. **Parse states**
   ```cpp
   state neutral {
       initial: true
       animation: "toNeutral"
   }
   ```

4. **Parse transitions**
   ```cpp
   transition neutral -> positive when counter > 0
   ```

### AST Structure
The parsed state machine is stored in `AstMachine`:
```cpp
struct AstMachine {
    std::string name;
    std::vector<AstLayer> layers;
};

struct AstLayer {
    std::string name;
    std::vector<AstState> states;
    std::vector<AstTransition> transitions;
};

struct AstState {
    std::string name;
    bool initial = false;
    std::string animation;
};

struct AstTransition {
    std::string from_state;
    std::string to_state;
    std::string condition_var;
    std::string condition_op;  // ">", "<", "==", "!="
    float condition_val = 0;
};
```

## Supported Syntax

### Complete State Machine
```flex
machine machineName {
    layer layerName {
        state stateName {
            initial: true|false
            animation: "animationName"
        }

        transition fromState -> toState when input > value
        transition fromState -> toState when input < value
        transition fromState -> toState when input == value
    }
}
```

### Condition Operators
- `>` - Greater than
- `<` - Less than
- `==` - Equal to
- `!=` - Not equal to

## Testing

### Quick Test
```bash
# Run the test
flex\examples\test_statemachine_parser.bat
```

### Custom Test
1. Create a `.flex` file with machine syntax
2. Run:
   ```bash
   build\Ninja\Msvc\bin\test_parser.exe yourfile.flex
   ```

## ✅ Status

**Parser Extension: COMPLETE**

- ✅ Machine parsing implemented
- ✅ Layer parsing implemented
- ✅ State parsing with properties implemented
- ✅ Transition parsing with conditions implemented
- ✅ Test files created
- ✅ Documentation written

The parser now fully supports state machine syntax and can parse all the constructs needed for the unified tinyfsm-based state machine architecture.
