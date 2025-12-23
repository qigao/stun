# Fix Build Errors - Quick Reference

## ✅ What Was Fixed

### 1. Fixed Incomplete Type Error
**Problem**: `std::unique_ptr<PseudoClassStyleMap>` required complete type
**Solution**: Moved `Node::Node()` constructor to .cpp file

**Changes**:
- `node.h`: Changed `Node() = default;` to `Node();`
- `node.cpp`: Added `Node::Node() = default;` implementation

### 2. Removed Old Machine References
Commented out all old `Machine` system code in `flex.cpp`:
- Definition::Impl::machine
- Definition::machine()
- Instance::Impl::machine
- Instance::set_machine()
- Instance::machine()
- Instance::current_state()
- update_node_machines_recursive()
- clear_node_machine_events_recursive()
- All `node->machine()->fire_event()` calls

---

## ⏳ Remaining Tasks

### Task 1: Comment Out Header Declarations
In `flex/include/flex/flex.h`, comment out:

```cpp
// REMOVED: Old machine system
// Machine::Ptr machine() const;  // In Definition class
// void set_machine(Machine::Ptr machine);  // In Instance class
// Machine* machine() const;
// const char* current_state(const char* layer) const;
```

### Task 2: Fix Parser References
In `flex.cpp`, find and comment out:
- Line ~1142: `auto machine = Machine::create(...)`
- Line ~1236: `node->set_machine(machine);`
- Line ~1330: `Machine::Ptr *out_machine` parameter
- Line ~1697: Machine parsing code

Search pattern:
```bash
grep -n "Machine::create\|set_machine\|out_machine" flex/src/flex.cpp
```

### Task 3: Try Build
```bash
cd build
cmake ..
ninja flex  # Or make flex
```

### Task 4: Fix Any Remaining Errors
If there are errors about undefined `Machine`, add to the top of the file that's failing:
```cpp
#if 0
// Forward declaration for commented code
namespace flex {
    class Machine;
}
#endif
```

---

## 🎯 Quick Test (Once Builds)

After build succeeds, create a minimal test:

```cpp
// test_fsm_basic.cpp
#include "flex/fsm.h"
#include <iostream>

using namespace flex;

struct TestFsm : ComponentFsm<TestFsm> {
    virtual void react(events::MouseDown const&) {}
};

struct StateA : TestFsm {
    void entry() override {
        std::cout << "Entered State A\n";
    }
    void react(events::MouseDown const&) override {
        std::cout << "Transitioning to B\n";
        transit<struct StateB>();
    }
};

struct StateB : TestFsm {
    void entry() override {
        std::cout << "Entered State B\n";
    }
};

FSM_INITIAL_STATE(TestFsm, StateA)

int main() {
    TestFsm::start();
    TestFsm::dispatch(events::MouseDown());
    return 0;
}
```

Compile:
```bash
g++ -std=c++17 -I../flex/include -I../vendor/tinyfsm test_fsm_basic.cpp -o test_fsm
./test_fsm
```

Expected output:
```
Entered State A
Transitioning to B
Entered State B
```

---

## 📋 Summary

**Core fix applied**: Moved Node constructor to .cpp to fix incomplete type error

**Next steps**:
1. Comment out Machine declarations in flex.h (2 min)
2. Comment out Machine parsing code in flex.cpp (5 min)
3. Build and fix any remaining errors (10 min)
4. Test basic FSM functionality (5 min)

**Total estimated time**: ~20 minutes

---

## 🚨 If You Get Stuck

The safest fallback is to temporarily stub out the Machine class:

```cpp
// In flex/include/flex/flex.h (near top)
namespace flex {
    // Temporary stub for old Machine (being removed)
    class Machine {
    public:
        using Ptr = std::shared_ptr<Machine>;
        static Ptr create(const std::string&) { return nullptr; }
        void init() {}
        void update(...) {}
        // ... add any other methods that won't compile
    };
}
```

This will let everything compile while you finish removing the references.

**But the proper solution is to comment out all Machine usage as shown above.**
