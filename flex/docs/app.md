This is the ultimate application of the **Flex Engine**.

Because Flex supports **Scripting**, **Input Events**, and **Dynamic Scene Graph manipulation**, you do not need to build these tools in C++ or React. You can build them **entirely inside Flex**.

This is the **"Dogfooding"** approach: The Editor is just a complex Flex file (`Editor.flex`) that loads and manipulates other Flex files.

Here is the architectural blueprint for building all three tools using one common core.

---

# 1. The Common Architecture: "The Meta-Editor"

All three tools (UI, SVG, Whiteboard) share the same skeleton.

### The Scene Structure (`Editor.flex`)
The editor is composed of three layers:
1.  **The Canvas (Viewport):** Renders the user's document.
2.  **The Gizmos (Overlay):** Selection boxes, transform handles, snap lines.
3.  **The Chrome (UI):** Toolbars, property panels, layers list.

```flex
// Editor.flex
Assets {
    File "Logic" { src: "editor_core.js" }
    // Import UI components for the editor itself
    File "UI_Button" { src: "ui/Button.flex" }
    File "UI_Panel"  { src: "ui/Panel.flex" }
}

Script "Core" { source: "Logic" }

Artboard "AppWindow" {
    
    // --- LAYER 1: The User's Document ---
    Group "Viewport" {
        // Transformations for Pan/Zoom
        x: $CamX, y: $CamY, scale: $Zoom
        
        // This group acts as the "Root" of the file being edited.
        // JS injects nodes here.
        Group "DocumentRoot" { id: "DocRoot" }
    }

    // --- LAYER 2: Gizmos (The "Invisible" Helpers) ---
    Group "GizmoLayer" {
        x: $CamX, y: $CamY, scale: $Zoom
        
        // The Blue Selection Box
        Shape "SelectionBox" {
            visible: $IsSelecting
            Rect { width: $SelW, height: $SelH }
            Stroke { color: #0099FF }
            Fill   { color: #0099FF, opacity: 0.1 }
        }
        
        // Transform Handles (Corners)
        Group "Handles" {
             visible: $HasSelection
             // 4 squares at corners of selection...
        }
    }

    // --- LAYER 3: The UI Chrome ---
    Group "HUD" {
        // Toolbar (Left)
        Instance "Toolbar" { source: "UI_Panel", x: 0, y: 0 ... }
        
        // Property Inspector (Right)
        Instance "Inspector" { source: "UI_Panel", x: 1600 ... }
    }
}
```

---

# 2. Tool A: The SVG Designer (Vector Path Editing)

**Key Challenge:** Bezier Path Manipulation (The Pen Tool).
**Flex Solution:** Use **QuickJS** to math-generate the path string, and **Gizmos** for handles.

### The Logic (`vector_tool.js`)

1.  **Node Structure:** When drawing, we create a `Shape` with a `Path` primitive.
2.  **Handle Rendering:** For every point in the path, we spawn a temporary "Gizmo Circle" in the `GizmoLayer`.
3.  **Interaction:** Dragging a Gizmo updates the coordinate in the Path string.

```javascript
// JS Logic
let activePath = []; // [{x,y, h1x, h1y, h2x, h2y}, ...]

function updatePathNode(nodeID) {
    // Convert logic points to SVG string
    let d = "M " + activePath[0].x + " " + activePath[0].y;
    for(let i=1; i<activePath.length; i++) {
        let p = activePath[i];
        d += ` C ${p.h1x} ${p.h1y}, ${p.h2x} ${p.h2y}, ${p.x} ${p.y}`;
    }
    // Update Engine
    flex.setPathData(nodeID, d);
}

function onHandleDrag(handleID, newX, newY) {
    let pointIndex = flex.getProperty(handleID, "meta_index");
    activePath[pointIndex].x = newX;
    activePath[pointIndex].y = newY;
    
    updatePathNode(selectedNodeID);
    
    // Also move the visual handle
    flex.setPosition(handleID, newX, newY);
}
```

---

# 3. Tool B: The UI Designer (Figma-like)

**Key Challenge:** Layout Constraints and Component Properties.
**Flex Solution:** Flex's native **`Query`** node (Auto-Layout) and **`Instance`** node.

### The Auto-Layout Feature
In Figma, "Auto Layout" creates a stack. In Flex, we just wrap the selection in a `Query` node.

**Action:** User selects 3 items -> Clicks "Auto Layout".

**JS Logic:**
1.  Create a new `Query` node.
2.  Tag the 3 selected items with a unique ID (e.g., `layout_group_1`).
3.  Set the Query selector to `@layout_group_1`.
4.  Set `layout: stack { direction: horizontal }`.

### The Property Inspector
When a user clicks a node, we must update the UI on the right.

```flex
// Inspector.flex (The UI Panel)
Inputs {
    string SelName = ""
    float  SelX = 0
    float  SelOp = 1.0
}
Artboard {
    Text { content: "Name: ${SelName}" }
    // Binding the input field back to logic
    InputText { 
        value: $SelX
        onChange: call "Editor.updateSelectionX(this.value)"
    }
}
```

```javascript
// editor_core.js
function onSelectionChanged(nodeID) {
    // 1. Read values from the engine
    let x = flex.getProperty(nodeID, "x");
    let name = flex.getName(nodeID);
    
    // 2. Push values into the Inspector UI inputs
    flex.setInput("Inspector.SelX", x);
    flex.setInput("Inspector.SelName", name);
}
```

---

# 4. Tool C: The Whiteboard (Excalidraw-like)

**Key Challenge:** "Sloppy" rendering and Infinite Canvas.
**Flex Solution:** **Virtualization** and **Procedural Geometry**.

### The Infinite Canvas
We use the **`VirtualLayer`** architecture we designed for CAD, but pointing to an in-memory database or a WebSocket stream (for collaboration).

```flex
Artboard "Whiteboard" {
    Group "Camera" {
        x: $CamX, y: $CamY, scale: $Zoom
        
        // As you pan, Flex automatically recycles nodes
        VirtualLayer "CanvasContent" {
            source: "InMemoryDB" 
            buffer: 1000px 
        }
    }
}
```

### The "Sloppy" Algorithm
When the user draws a square, we don't draw a straight `Rect`. We use JS to generate a "Rough" path (multiple wobbly lines) and inject that as a `Path`.

```javascript
// whiteboard_logic.js
function createRoughRect(x, y, w, h) {
    let id = flex.createNode("Group", "DocRoot");
    
    // Generate 2 passes of wobbly lines for that "hand drawn" look
    let pathData = RoughJS.rectangle(0, 0, w, h, { roughness: 2 });
    
    let shape = flex.createNode("Shape", id);
    flex.setPathData(shape, pathData);
    flex.setProperty(shape, "Stroke.width", 2);
}
```

---

# 5. The "Undo/Redo" System (Time Travel)

Every editor needs Undo. Since Flex is declarative, Undo is trivial: **State Snapshots.**

**Implementation Strategy:**

1.  **The Command Pattern:**
    In JS, every action (Move, Color Change, Create) is an object:
    ```javascript
    let cmd = {
        type: "MOVE",
        nodeID: 55,
        oldX: 100, newX: 200
    };
    ```

2.  **The Stack:**
    ```javascript
    let undoStack = [];
    let redoStack = [];

    function execute(cmd) {
        apply(cmd);
        undoStack.push(cmd);
        redoStack = []; // Clear redo
    }
    
    function undo() {
        let cmd = undoStack.pop();
        revert(cmd); // Move back to oldX
        redoStack.push(cmd);
    }
    ```

3.  **Serialization (File Save):**
    Because the entire document is a Flex Node Tree, saving is just:
    `flex.serializeNode("DocumentRoot")` -> returns a JSON or `.flex` string.

---

# Summary: One Engine, Three Tools

| Tool | Flex Feature Used | JS Logic Responsibility |
| :--- | :--- | :--- |
| **UI Designer** | `Query` (Layouts), `Instance` (Components) | Parenting, Property Binding, Selection Logic. |
| **Vector Tool** | `Path` (Geometry), `Shape` (Styling) | Bezier Math, Handle dragging, Boolean Ops. |
| **Whiteboard** | `VirtualLayer` (Infinite Scroll) | Smoothing Input, Procedural "Rough" generation. |

By building these tools **inside Flex**, you get:
1.  **Performance:** 60 FPS zooming/panning (C++ backend).
2.  **Portability:** The editor runs on Web (Wasm), iPad, and Desktop without code changes.
3.  **Dogfooding:** The tools are the best test suite for the engine itself.