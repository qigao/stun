# Data Binding Roadmap

## ✅ Phase 1: IMPLEMENTED (C++ API)

Current implementation using C++ BindingContext API:

```cpp
// C++ code
instance_->set_input("counter", 10.0f);

// Updates propagate to nodes automatically
auto* text = scene->find("counterValue");
text->set_content(std::to_string(counter));
```

**Demo**: `data_binding_demo.cpp` - Interactive counter with reactive UI

---

## 🚧 Phase 2: PLANNED (DSL Syntax)

Future DSL syntax for declarative data binding:

```flex
scene dashboard {
    // Data declarations
    data counter: 0
    data userName: "Alice"
    data score: 0

    // Expression binding with {}
    text counterDisplay {
        content: "{counter}"
        fontSize: 24
        color: {counter > 10 ? #ff0000 : #00ff00}
    }

    text greeting {
        content: "Hello {userName}!"
        fontSize: 20
    }

    // Computed values
    data doubleScore: {score * 2}

    text scoreText {
        content: "Score: {score} (x2 = {doubleScore})"
    }

    // Button with event handler
    group incrementButton {
        rect bg { fill: #00d9ff }
        text label { content: "+" }

        on click -> {
            counter = counter + 1
            score = score + 10
        }
    }
}
```

### Parser Extensions Needed:

1. **data keyword**:
   ```flex
   data variableName: initialValue
   ```

2. **Expression syntax** `{expression}`:
   ```flex
   content: "{counter}"
   color: {value > 10 ? #ff0000 : #00ff00}
   ```

3. **Event handlers**:
   ```flex
   on click -> { statement; statement; }
   ```

4. **Expression evaluation**:
   - Variable references: `{counter}`
   - Arithmetic: `{a + b * 2}`
   - Comparison: `{score > 100}`
   - Ternary: `{condition ? value1 : value2}`
   - String interpolation: `"Hello {name}!"`

---

## 🎯 Phase 3: FUTURE (Advanced Features)

### Computed Properties:
```flex
data firstName: "Alice"
data lastName: "Smith"
data fullName: "{firstName} {lastName}"

data items: [1, 2, 3, 4, 5]
data total: {items.sum()}
```

### Watchers:
```flex
watch counter {
    console.log("Counter changed to: {counter}")
    if (counter > 100) {
        showAlert("You reached 100!")
    }
}
```

### Array Binding:
```flex
data users: [
    {name: "Alice", score: 95},
    {name: "Bob", score: 87}
]

group userList {
    for user in users {
        text {
            content: "{user.name}: {user.score}"
            y: {index * 30}
        }
    }
}
```

---

## 📊 Implementation Priority

| Feature | Complexity | Impact | Priority |
|---------|-----------|--------|----------|
| `data` keyword | Low | High | ⭐⭐⭐⭐⭐ |
| `{expression}` in properties | Medium | High | ⭐⭐⭐⭐⭐ |
| Simple expressions (vars, arithmetic) | Low | High | ⭐⭐⭐⭐ |
| Ternary operator | Low | High | ⭐⭐⭐⭐ |
| `on event` handlers | Medium | High | ⭐⭐⭐⭐ |
| String interpolation | Low | Medium | ⭐⭐⭐ |
| Computed properties | Medium | Medium | ⭐⭐⭐ |
| `for` loops | High | High | ⭐⭐⭐ |
| Array/Object access | Medium | Medium | ⭐⭐ |
| Watchers | Low | Low | ⭐⭐ |

---

## 🔧 Technical Notes

### Current Architecture:
- ✅ `BindingContext` - Manages inputs and bindings
- ✅ `ScriptContext` - Expression evaluation (via script.h)
- ✅ `Binding` struct - Input/Expression bindings
- ✅ `Instance::set_input()` - Update reactive values

### Parser Extensions Required:
1. Tokenizer: Recognize `{`, `}`, `data`, `on`
2. Property value parser: Detect `{expr}` and create Binding
3. Scene block parser: Parse `data` declarations
4. Expression parser: Arithmetic, comparison, ternary, string concat

### Example Parser Flow:
```cpp
// Parsing: content: "{counter + 1}"
if (tok.match_char('{')) {
    std::string expr = tok.read_until('}');
    // Create expression binding
    auto binding = flex::Binding::expr(expr);
    binding.target = node;
    binding.property = "content";
    // Add to context
    bindings->add_binding(node, "content", binding);
}
```

---

## 🎮 Usage Examples

### Counter App:
```flex
scene counter {
    data count: 0

    text display { content: "{count}" }

    group plusBtn {
        on click -> { count = count + 1 }
    }
}
```

### Score Board:
```flex
scene scoreboard {
    data players: [
        {name: "Alice", score: 100},
        {name: "Bob", score: 85}
    ]

    data leader: {players[0].name}

    text title { content: "Leader: {leader}" }

    for player in players {
        text {
            y: {index * 40}
            content: "{player.name}: {player.score}"
            color: {player.score > 90 ? #00ff00 : #ffffff}
        }
    }
}
```

### Dynamic Form:
```flex
scene form {
    data email: ""
    data isValid: {email.contains("@")}

    text input {
        content: "{email}"
        color: {isValid ? #00ff00 : #ff0000}
    }

    text status {
        content: {isValid ? "Valid email" : "Invalid email"}
    }
}
```

---

## 🚀 Next Steps

1. **Extend Tokenizer**: Add `{`, `}`, `data` tokens
2. **Parse `data` declarations**: Store in Definition
3. **Parse `{expression}` values**: Create Bindings
4. **Test with simple demo**: Counter with DSL syntax
5. **Iterate**: Add ternary, string interpolation, etc.

---

## 📝 Notes

- Keep parser simple - no complex grammar
- Expressions use existing `ScriptContext`
- Bindings use existing `BindingContext`
- Zero breaking changes to existing API
- DSL syntax is optional enhancement
