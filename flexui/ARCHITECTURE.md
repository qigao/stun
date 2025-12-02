# flexui Architecture: Multi-Paradigm UI Framework

## 🎯 Design Philosophy

flexui is a **multi-paradigm UI framework** that combines the best of web technologies with native C++ performance. It provides **four complementary APIs** for maximum flexibility:

```
┌─────────────────────────────────────────────────────────────┐
│                        flexui                                │
│  "Web Technologies Meet Native Performance"                 │
└─────────────────────────────────────────────────────────────┘
         │
         ├─── 1. Canvas-like C++ API (Imperative)
         ├─── 2. HTML+CSS Driven (Declarative)
         ├─── 3. SVG Graphics (Vector)
         └─── 4. JavaScript Scripting (Dynamic)
```

---

## 📐 Architecture Overview

### Core Components

```
┌──────────────────────────────────────────────────────────────┐
│                     Application Layer                         │
├──────────────────────────────────────────────────────────────┤
│  C++ API  │  XML/HTML  │  CSS  │  SVG  │  JavaScript        │
├──────────────────────────────────────────────────────────────┤
│                      flexui Core                              │
│  ┌────────────┬──────────────┬─────────────┬──────────────┐ │
│  │  Widget    │  XML Parser  │  JS Engine  │  Event       │ │
│  │  System    │  (pugixml)   │  (QuickJS)  │  System      │ │
│  └────────────┴──────────────┴─────────────┴──────────────┘ │
├──────────────────────────────────────────────────────────────┤
│                    nanovg_css Layer                           │
│  ┌────────────┬──────────────┬─────────────┬──────────────┐ │
│  │  CSS       │  Layout      │  Rendering  │  Spatial     │ │
│  │  Parser    │  Engine      │  Engine     │  Index       │ │
│  └────────────┴──────────────┴─────────────┴──────────────┘ │
├──────────────────────────────────────────────────────────────┤
│                    NanoVG (Canvas)                            │
│  Vector Graphics • Text Rendering • GPU Acceleration         │
├──────────────────────────────────────────────────────────────┤
│                    OpenGL / Metal                             │
└──────────────────────────────────────────────────────────────┘
```

---

## 🎨 API 1: Canvas-like C++ API (Imperative)

### Philosophy
Direct, imperative control similar to HTML5 Canvas or NanoVG. Perfect for custom rendering and game UIs.

### Example
```cpp
#include <flexui.h>

int main() {
    flexui::Screen screen(800, 600, "Canvas Demo");
    
    // Direct widget creation (like canvas drawing)
    auto* button = screen.createWidget<flexui::Button>("btn", "button");
    button->setPosition(100, 100);
    button->setSize(200, 50);
    button->setText("Click Me");
    button->setClickCallback([](Widget* w) {
        std::cout << "Clicked!" << std::endl;
        return true;
    });
    
    // SVG graphics
    auto* circle = screen.createCircle("circle1", 400, 300, 50);
    circle->setFill("#4a90e2");
    circle->setStroke("#2c5aa0", 2);
    
    // Custom drawing
    screen.setCustomDrawCallback([](NVGcontext* vg) {
        nvgBeginPath(vg);
        nvgRect(vg, 50, 50, 100, 100);
        nvgFillColor(vg, nvgRGBA(255, 0, 0, 255));
        nvgFill(vg);
    });
    
    while (screen.pollEvents()) {
        screen.draw();
    }
    
    return 0;
}
```

### Use Cases
- Game UIs
- Data visualization
- Custom graphics
- Real-time rendering

---

## 🌐 API 2: HTML+CSS Driven (Declarative)

### Philosophy
Declarative UI definition using XML/HTML + CSS, similar to web development. Separation of structure and style.

### Example

**UI Definition (XML/HTML-like)**
```xml
<!-- ui.xml -->
<screen>
    <panel id="sidebar" class="sidebar">
        <button id="btn1" class="primary" onclick="handleClick">
            Home
        </button>
        <button id="btn2" class="secondary">
            Settings
        </button>
    </panel>
    
    <panel id="content" class="content">
        <label class="title">Welcome to flexui</label>
        <input id="username" placeholder="Enter username" />
        <button id="submit" class="primary" onclick="handleSubmit">
            Submit
        </button>
    </panel>
</screen>
```

**Styling (CSS)**
```css
/* styles.css */
.sidebar {
    width: 200px;
    background: #2c3e50;
    padding: 20px;
}

.primary {
    background: #3498db;
    color: white;
    padding: 10px 20px;
    border-radius: 5px;
    transition: background 0.3s ease;
}

.primary:hover {
    background: #2980b9;
}

.title {
    font-size: 24px;
    font-weight: bold;
    color: #2c3e50;
}
```

**C++ Code**
```cpp
#include <flexui.h>

int main() {
    flexui::Screen screen(1024, 768, "HTML+CSS Demo");
    
    // Load CSS
    screen.loadCSS(read_file("styles.css"));
    
    // Load XML UI
    screen.loadXML(read_file("ui.xml"));
    
    // Register event handlers
    screen.registerHandler("handleClick", [](Widget* w) {
        std::cout << "Button clicked: " << w->id() << std::endl;
        return true;
    });
    
    screen.registerHandler("handleSubmit", [&screen](Widget* w) {
        auto* input = screen.findWidget("username");
        std::cout << "Username: " << input->getText() << std::endl;
        return true;
    });
    
    while (screen.pollEvents()) {
        screen.draw();
    }
    
    return 0;
}
```

### Use Cases
- Application UIs
- Forms and dialogs
- Settings panels
- Designer-developer collaboration

---

## 🎨 API 3: SVG Graphics (Vector)

### Philosophy
Scalable vector graphics with CSS styling, similar to web SVG. Perfect for icons, diagrams, and illustrations.

### Example

**SVG in XML**
```xml
<!-- diagram.xml -->
<svg width="800" height="600">
    <!-- Shapes -->
    <circle id="node1" cx="200" cy="200" r="50" class="node" />
    <circle id="node2" cx="400" cy="200" r="50" class="node" />
    <circle id="node3" cx="300" cy="350" r="50" class="node" />
    
    <!-- Connections -->
    <line id="edge1" x1="200" y1="200" x2="400" y2="200" class="edge" />
    <line id="edge2" x1="200" y1="200" x2="300" y2="350" class="edge" />
    <line id="edge3" x1="400" y1="200" x2="300" y2="350" class="edge" />
    
    <!-- Labels -->
    <text x="200" y="200" class="label">A</text>
    <text x="400" y="200" class="label">B</text>
    <text x="300" y="350" class="label">C</text>
</svg>
```

**SVG Styling (CSS)**
```css
/* svg_styles.css */
.node {
    fill: #3498db;
    stroke: #2c3e50;
    stroke-width: 3px;
    transition: fill 0.3s ease;
}

.node:hover {
    fill: #e74c3c;
}

.edge {
    stroke: #95a5a6;
    stroke-width: 2px;
    stroke-dasharray: 5, 5;
}

.label {
    fill: white;
    font-size: 20px;
    font-weight: bold;
    text-anchor: middle;
}
```

**C++ Code**
```cpp
#include <flexui.h>

int main() {
    flexui::Screen screen(800, 600, "SVG Demo");
    
    screen.loadCSS(read_file("svg_styles.css"));
    screen.loadXML(read_file("diagram.xml"));
    
    // Programmatic SVG creation
    auto* path = screen.createPath("custom_path");
    path->addPathPoint(100, 100);
    path->addPathPoint(200, 150);
    path->addPathPoint(150, 250);
    path->setStroke("#e74c3c", 3);
    path->setFill("none");
    path->setHandDrawn(true);  // Rough.js style
    
    while (screen.pollEvents()) {
        screen.draw();
    }
    
    return 0;
}
```

### Use Cases
- Diagrams and flowcharts
- Icons and illustrations
- Data visualization
- Interactive graphics

---

## 🚀 API 4: JavaScript Scripting (Dynamic)

### Philosophy
Dynamic behavior and scripting using JavaScript (QuickJS), similar to web browsers. Hot-reload and rapid prototyping.

### Example

**JavaScript Logic**
```javascript
// app.js
let counter = 0;

function handleClick() {
    counter++;
    updateUI();
    return true;
}

function updateUI() {
    const label = findWidget("counter_label");
    label.setText("Count: " + counter);
    
    const button = findWidget("increment_btn");
    if (counter >= 10) {
        button.addClass("disabled");
    }
}

function handleReset() {
    counter = 0;
    updateUI();
    return true;
}

// Timer example
setInterval(function() {
    const time = findWidget("time_label");
    time.setText(new Date().toLocaleTimeString());
}, 1000);
```

**XML UI**
```xml
<!-- counter.xml -->
<screen>
    <panel class="container">
        <label id="counter_label" class="counter">Count: 0</label>
        <button id="increment_btn" onclick="handleClick">Increment</button>
        <button onclick="handleReset">Reset</button>
        <label id="time_label" class="time"></label>
    </panel>
</screen>
```

**C++ Code**
```cpp
#include <flexui.h>

int main() {
    flexui::Screen screen(400, 300, "JavaScript Demo");
    
    screen.loadCSS(read_file("styles.css"));
    screen.loadXML(read_file("counter.xml"));
    screen.loadJSFile("app.js");  // Load JavaScript
    
    // C++ can also call JavaScript
    screen.jsEngine()->eval("updateUI()");
    
    while (screen.pollEvents()) {
        screen.draw();
    }
    
    return 0;
}
```

### Use Cases
- Rapid prototyping
- Hot-reload development
- Scripting and automation
- Plugin systems

---

## 🔄 Multi-Paradigm Integration

### Combining All APIs

```cpp
#include <flexui.h>

int main() {
    flexui::Screen screen(1280, 720, "Multi-Paradigm Demo");
    
    // 1. Load CSS (Declarative Styling)
    screen.loadCSS(R"(
        .container { padding: 20px; background: #ecf0f1; }
        .chart { border: 2px solid #34495e; }
    )");
    
    // 2. Load XML UI (Declarative Structure)
    screen.loadXML(R"(
        <screen>
            <panel id="main" class="container">
                <label class="title">Dashboard</label>
                <svg id="chart" class="chart" width="600" height="400"></svg>
            </panel>
        </screen>
    )");
    
    // 3. Add SVG graphics (Vector Graphics)
    auto* svg = screen.findWidget("chart");
    for (int i = 0; i < 10; i++) {
        auto* bar = screen.createRect(
            "bar" + std::to_string(i),
            50 + i * 60, 300 - i * 20,
            40, i * 20
        );
        bar->setFill("#3498db");
        svg->addChild(bar);
    }
    
    // 4. Load JavaScript (Dynamic Behavior)
    screen.loadJS(R"(
        function animateBars() {
            for (let i = 0; i < 10; i++) {
                const bar = findWidget("bar" + i);
                bar.addClass("animated");
            }
        }
        setInterval(animateBars, 2000);
    )");
    
    // 5. Custom Canvas Drawing (Imperative)
    screen.setCustomDrawCallback([](NVGcontext* vg) {
        nvgBeginPath(vg);
        nvgCircle(vg, 100, 100, 30);
        nvgFillColor(vg, nvgRGBA(231, 76, 60, 255));
        nvgFill(vg);
    });
    
    while (screen.pollEvents()) {
        screen.draw();
    }
    
    return 0;
}
```

---

## 🏗️ Professional Architecture Patterns

### 1. **MVC Pattern**
```cpp
// Model
class DataModel {
    std::vector<int> data;
public:
    void update(int value) { data.push_back(value); }
    const auto& getData() const { return data; }
};

// View (XML + CSS)
// view.xml, styles.css

// Controller
class Controller {
    DataModel model;
    flexui::Screen& screen;
public:
    void handleInput() {
        auto* input = screen.findWidget("input");
        model.update(std::stoi(input->getText()));
        updateView();
    }
    
    void updateView() {
        // Update UI based on model
    }
};
```

### 2. **Component-Based Architecture**
```cpp
class Component {
public:
    virtual void render(flexui::Screen& screen) = 0;
    virtual void update(float dt) = 0;
};

class ChartComponent : public Component {
    void render(flexui::Screen& screen) override {
        // Render chart using SVG
    }
};

class FormComponent : public Component {
    void render(flexui::Screen& screen) override {
        // Render form using XML
    }
};
```

### 3. **Plugin System**
```cpp
class Plugin {
public:
    virtual void init(flexui::Screen& screen) = 0;
    virtual void loadUI() = 0;
    virtual void loadScripts() = 0;
};

class ThemePlugin : public Plugin {
    void init(flexui::Screen& screen) override {
        screen.loadCSS("themes/dark.css");
    }
};
```

---

## 🎯 Best Practices

### 1. **Separation of Concerns**
```
Structure (XML)  →  Styling (CSS)  →  Behavior (JS/C++)
```

### 2. **Progressive Enhancement**
```cpp
// Start simple (C++)
auto* btn = screen.createWidget<Button>("btn", "button");

// Add styling (CSS)
screen.loadCSS(".button { background: blue; }");

// Add behavior (JavaScript)
screen.loadJS("function onClick() { /* ... */ }");
```

### 3. **Performance Optimization**
- Use spatial index for large UIs (1000+ widgets)
- Leverage CSS animations (GPU accelerated)
- Batch SVG updates
- Cache JavaScript functions

### 4. **Cross-Platform Development**
```cpp
#ifdef _WIN32
    // Windows-specific
#elif __APPLE__
    // macOS-specific
#else
    // Linux-specific
#endif
```

---

## 📊 Comparison with Other Frameworks

| Feature | flexui | Qt | Electron | Dear ImGui |
|---------|--------|----|-----------|-----------| 
| **C++ API** | ✅ | ✅ | ❌ | ✅ |
| **HTML+CSS** | ✅ | ❌ | ✅ | ❌ |
| **SVG** | ✅ | ✅ | ✅ | ❌ |
| **JavaScript** | ✅ | ✅ | ✅ | ❌ |
| **Canvas-like** | ✅ | ✅ | ✅ | ✅ |
| **Retained Mode** | ✅ | ✅ | ✅ | ❌ |
| **GPU Accelerated** | ✅ | ✅ | ✅ | ✅ |
| **Lightweight** | ✅ | ❌ | ❌ | ✅ |
| **Hot Reload** | ✅ | ❌ | ✅ | ❌ |

---

## 🚀 Future Roadmap

### Phase 1: Core Features (Current)
- ✅ Multi-paradigm APIs
- ✅ CSS styling
- ✅ SVG graphics
- ✅ JavaScript engine
- ✅ Spatial indexing

### Phase 2: Advanced Features
- [ ] CSS Grid layout
- [ ] CSS Flexbox (enhanced)
- [ ] WebAssembly support
- [ ] Hot reload (CSS/JS)
- [ ] Developer tools

### Phase 3: Ecosystem
- [ ] Component library
- [ ] Theme marketplace
- [ ] Visual designer
- [ ] Documentation site
- [ ] Community plugins

---

## 📚 Documentation Structure

```
docs/
├── getting-started/
│   ├── installation.md
│   ├── hello-world.md
│   └── concepts.md
├── api-reference/
│   ├── cpp-api.md
│   ├── xml-reference.md
│   ├── css-reference.md
│   ├── svg-reference.md
│   └── javascript-api.md
├── guides/
│   ├── styling-guide.md
│   ├── layout-guide.md
│   ├── animation-guide.md
│   └── performance-guide.md
└── examples/
    ├── basic/
    ├── intermediate/
    └── advanced/
```

---

## 🎓 Conclusion

flexui is a **professional, multi-paradigm UI framework** that combines:

1. **Canvas-like C++ API** - Direct control
2. **HTML+CSS Driven** - Declarative structure
3. **SVG Graphics** - Vector graphics
4. **JavaScript Scripting** - Dynamic behavior

This architecture provides:
- ✅ **Flexibility** - Choose the right tool for each task
- ✅ **Performance** - Native C++ with GPU acceleration
- ✅ **Productivity** - Web-like development experience
- ✅ **Scalability** - From simple UIs to complex applications

**flexui brings web technologies to native C++ development!** 🚀
