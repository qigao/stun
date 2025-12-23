# Flex Engine - Vision (Theoretical Design)

> **⚠️ IMPORTANT:** This document describes the **theoretical design and future vision** for Flex Engine.
> For the **actual current implementation**, see [ARCHITECTURE.md](ARCHITECTURE.md).
> For the **DSL specification**, see [dsl.md](dsl.md).

---

This is the **Master Technical Specification** for the **Flex Engine**.

It consolidates the Language Specification (DSL), the Runtime Architecture, the Tech Stack, and the Build Strategy into a single execution plan.

---

# Part 1: The High-Level Vision

**Flex** is a portable, high-performance, reactive graphics engine. It fills the gap between lightweight UI libraries (Lottie/Rive) and heavy game engines (Unity/Godot).

*   **Primary Use Cases:** Embedded UI, App Interfaces, 2.5D Games, CAD/Design Tools.
*   **Core Philosophy:**
    1.  **Declarative:** Define *what* happens, not *how* (via DSL).
    2.  **Reactive:** Visuals are pure functions of Data Inputs.
    3.  **Scalable:** Runs on a Watch (100KB) or a Workstation (Billions of items).

---

# Part 2: The Flex Language (`.flex`)

A Flex file defines a **Module**. It is composed of 6 optional blocks.

### 1. Global Concepts
*   **Binding:** `${ input * 5 }` (Inline math).
*   **Tags:** Metadata for querying (`tags: ["enemy"]`).
*   **Selectors:** Targeting groups (`track "@enemy.opacity"`).

### 2. The Grammar Structure

```flex
// I. ASSETS (Resources)
Assets {
    Bitmap "Tex_Atlas"   { src: "atlas.png" }
    Vector "Icon_Home"   { src: "home.svg" }
    Audio  "SFX_Jump"    { src: "jump.wav" }
    File   "Prefab_Btn"  { src: "Button.flex" }
    File   "LogicJS"     { src: "logic.js" }
    // Enterprise: Binary Spatial Index
    DataSource "CityDB"  { type: spatial_binary, source: "map.bin" }
}

// II. INPUTS (Public API)
Inputs {
    float  Speed      = 0.0
    string State      = "Idle"
    trigger OnClick
}

// III. SCRIPT (Logic Bridge)
Script "Bridge" { source: "LogicJS" }

// IV. ARTBOARD (Scene Graph)
Artboard "MainScene" (1920, 1080) {
    
    // Standard Node
    Group "Hero" {
        x: 100, y: 500
        tags: ["player"]
        
        // Physics Attachment
        RigidBody { type: dynamic, mass: 5 }
        Collider  { type: capsule, radius: 20, height: 80 }
        
        // Visual
        Shape "Body" {
            Rect { width: 40, height: 80 }
            Fill { color: #FF0000 }
        }
    }

    // Dynamic Query Node (Auto-Layout)
    Query "EnemyGrid" {
        selector: "@enemy"
        layout: grid { columns: 4, gap: 10 }
    }

    // Virtualized Layer (CAD/Maps)
    VirtualLayer "MapData" {
        source: "CityDB"
        buffer: 500px // Pre-load margin
        // Map binary types to Flex Prefabs
        map { "road" -> "Prefab_Road", "bldg" -> "Prefab_Bldg" }
    }
}

// V. TIMELINES (Animation)
Timeline "RunAnim" {
    loop: loop
    track "Hero.x" { 0s -> 0, 1s -> 100 }
    // Audio Sync
    audio "Steps" { 0s -> play "SFX_Jump" }
    // Logic Trigger
    trigger "Events" { 0.5s -> fire $OnStep }
}

// VI. MACHINE (State Controller)
Machine "Controller" {
    Layer "Movement" {
        State Idle {}
        State Running { play "RunAnim" }
        
        transition Idle -> Running { when $Speed > 0 }
    }
}
```

---

# Part 3: Runtime Architecture (C++)

The engine operates on a strictly ordered **Frame Loop**.

### 1. The Data Model
To save memory, we separate **Definition** from **Instance**.
*   **`FlexDefinition` (Read-Only):** The parsed result of a `.flex` file. Stores the tree structure, keyframes, and logic rules. Loaded once.
*   **`FlexInstance` (Mutable):** A lightweight clone. Stores current state (`CurrentTime`, `InputValues`, `NodeTransforms`).

### 2. The Loop Stages
1.  **Input & Script:**
    *   Update Inputs from Host.
    *   Tick QuickJS (`onUpdate`).
    *   Advance State Machine (Evaluate Transitions).
2.  **Simulation:**
    *   **Animation:** Sample Timelines $\to$ Write to Properties.
    *   **Math:** Solve `${...}` bindings.
    *   **Physics:** Step Box2D/Verlet $\to$ Sync `RigidBody` nodes.
3.  **Virtualization (The "Window"):**
    *   Calculate Camera Viewport.
    *   Query `DataSource` (R-Tree).
    *   **Object Pool:** Recycle nodes leaving view; spawn nodes entering view.
4.  **Layout & Transform:**
    *   Solve IK Constraints.
    *   Solve `Query` layouts (Grids/Stacks).
    *   Update World Matrices (Parent * Local).
5.  **Render:**
    *   Cull invisible nodes.
    *   Generate Draw Commands (ThorVG/Skia).
    *   **Scatter:** Issue GPU Instancing calls for massive arrays.

---

# Part 4: Implementation Stack

We use "Game Engine" grade libraries to ensure performance across Mobile, Web, and Desktop.

| Component | Technology | Reasoning |
| :--- | :--- | :--- |
| **Allocator** | **mimalloc** | High-performance, thread-safe memory allocation. |
| **Node Pool** | **std::pmr** | Zero-cost allocation for per-frame scene nodes. |
| **Threading** | **EnkiTS** | Task scheduler for parallel Animation/Layout. |
| **Scripting** | **QuickJS** | Small (600KB), ES2020 compliant, secure. |
| **ECS** | **EnTT** | Entity Component System for CAD scale data. |
| **Physics** | **Box2D v3** | SIMD-optimized 2D physics. |
| **Events** | **NanoSignalSlots** | Fast, header-only event dispatching. |
| **Rendering** | **ThorVG** | Lightweight, portable vector rasterizer. |
| **Profiling** | **Tracy** | Frame profiling telemetry. |

---

# Part 5: Build System (CMake Profiles)

We define three build profiles to tailor the engine binary size.

### A. `FLUX_PROFILE_MICRO` (~100KB)
*   **Target:** Smartwatches, Embedded, Simple Web Icons.
*   **Features:** Scene Graph, Animation, Events.
*   **Disabled:** Physics, Scripting, Audio, ECS, Networking.
*   **Math:** `float`.

### B. `FLUX_PROFILE_STANDARD` (~1.5MB)
*   **Target:** Mobile Apps, 2D Games, Rich Web UI.
*   **Features:** Core + Physics + Scripting + Audio.
*   **Disabled:** ECS (Massive Data), Double Precision.
*   **Math:** `float`.

### C. `FLUX_PROFILE_ENTERPRISE` (~4MB+)
*   **Target:** CAD Tools, Editors, Figma-like Apps.
*   **Features:** **All**. Includes EnTT (ECS) for billions of items, Networking for collaboration.
*   **Math:** `double` (64-bit precision).
*   **Debug:** String introspection enabled (Node Names).

---

# Part 6: Public API Design (C++)

This is how a developer integrates Flex into their app.

```cpp
#include "Flex.h"

// 1. Initialize Engine (Global)
Flex::init(FLUX_PROFILE_STANDARD);

// 2. Load Definition (Once)
auto uiDef = Flex::loadDefinition("menu.flex");

// 3. Create Instance (Per Item)
auto ui = Flex::createInstance(uiDef);

// 4. Game Loop
while (app.running) {
    float dt = app.getDeltaTime();
    
    // Input
    ui->setInput("Speed", player.velocity);
    ui->sendPointerEvent(mouse.x, mouse.y, mouse.isDown);
    
    // Update (Animation/Physics/Script)
    ui->advance(dt);
    
    // Render
    renderer->begin();
    ui->render(renderer); // Emits generic draw commands
    renderer->end();
}
```

---

# Part 7: Execution Roadmap

1.  **Phase 1: The Visual Core**
    *   Setup CMake with profiles.
    *   Implement `Node`, `Group`, `Shape`.
    *   Integrate `ThorVG`.
    *   *Result:* Static vector rendering.

2.  **Phase 2: The Animator**
    *   Implement `Timeline` and `PropertyRegistry`.
    *   Implement `State Machine`.
    *   *Result:* Interactive Rive-like animations.

3.  **Phase 3: The Brain**
    *   Integrate `QuickJS`.
    *   Implement `Physics` bridge.
    *   *Result:* 2.5D Games / Complex Logic.

4.  **Phase 4: The Scale**
    *   Implement `VirtualLayer` and `DataSource` (R-Tree).
    *   Implement `Scatter` (GPU Instancing).
    *   *Result:* CAD/Map Editor capability.

This specification provides a complete blueprint for building **Flex**, a next-generation graphics engine.