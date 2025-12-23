# Flex DSL - Rive Gap Closing Design

## Overview

This document outlines the high-level architecture for extending Flex DSL to support Rive-like features using the existing re2c+lemon parser approach.

**Current Flex Capabilities:**
- ✅ Scene graph with groups, shapes, text
- ✅ State machines with conditional transitions
- ✅ Keyframe animations with property tracks
- ✅ Flex layout system
- ✅ Component system

**Gap with Rive:**
- ❌ Skeletal/bones animation
- ❌ Mesh deformation
- ❌ Animation blending (blend states)
- ❌ Constraints (IK, follow path)
- ❌ Path morphing
- ❌ Physics/springs

---

## Phase 1: Skeletal Animation System

### 1.1 DSL Syntax

```flex
// Define a skeleton inside a scene
skeleton playerSkeleton {
    // Root bone - all coordinates relative to skeleton origin
    bone root {
        x: 0, y: 0
        length: 0  // Root has no length

        bone spine {
            rotation: 0
            length: 50

            bone chest {
                rotation: 0
                length: 40

                bone head {
                    rotation: 0
                    length: 30
                }

                bone armL {
                    rotation: -45
                    length: 35

                    bone forearmL {
                        rotation: 0
                        length: 30
                    }
                }

                bone armR {
                    rotation: 45
                    length: 35

                    bone forearmR {
                        rotation: 0
                        length: 30
                    }
                }
            }
        }

        bone legL {
            rotation: -90
            length: 45

            bone shinL {
                rotation: 0
                length: 40
            }
        }

        bone legR {
            rotation: -90
            length: 45

            bone shinR {
                rotation: 0
                length: 40
            }
        }
    }
}

// Bind mesh to skeleton
mesh characterBody {
    skeleton: playerSkeleton

    // Vertices with bone weights
    vertices {
        v0: 0, 0     weights: { root: 1.0 }
        v1: 10, 0    weights: { spine: 0.8, root: 0.2 }
        v2: 10, 50   weights: { chest: 1.0 }
        v3: 0, 50    weights: { chest: 0.7, spine: 0.3 }
    }

    // Triangle indices
    triangles {
        0, 1, 2
        0, 2, 3
    }

    fill: #ff9966
}

// Animate bones
anim "walk" {
    duration: 1.0
    loop: loop

    track "#playerSkeleton/legL/rotation" {
        keyframe 0 -> -60
        keyframe 0.5 -> -120
        keyframe 1.0 -> -60
    }

    track "#playerSkeleton/legR/rotation" {
        keyframe 0 -> -120
        keyframe 0.5 -> -60
        keyframe 1.0 -> -120
    }
}
```

### 1.2 New Tokens (flex_lexer.re)

```c
// Add to keyword detection
"skeleton"      { return TOK_SKELETON; }
"bone"          { return TOK_BONE; }
"mesh"          { return TOK_MESH; }
"vertices"      { return TOK_VERTICES; }
"triangles"     { return TOK_TRIANGLES; }
"weights"       { return TOK_WEIGHTS; }
```

### 1.3 AST Structures (flex_ast.h)

```cpp
struct AstBone {
    std::string name;
    float rotation = 0;
    float length = 0;
    float x = 0, y = 0;  // Only for root
    std::vector<std::shared_ptr<AstBone>> children;
};

struct AstSkeleton {
    std::string name;
    std::shared_ptr<AstBone> root;
};

struct AstVertex {
    float x, y;
    std::map<std::string, float> bone_weights;  // bone_name -> weight
};

struct AstMesh {
    std::string name;
    std::string skeleton_ref;  // Which skeleton to bind to
    std::vector<AstVertex> vertices;
    std::vector<std::array<int, 3>> triangles;
    std::string fill;
};
```

### 1.4 Runtime Structures (skeleton.h)

```cpp
class Bone {
public:
    std::string name;
    float local_rotation = 0;  // Animated property
    float length = 0;

    // Computed world transform
    Mat2D world_transform;

    Bone* parent = nullptr;
    std::vector<std::unique_ptr<Bone>> children;

    void update_world_transform();
    Vec2 tip_position() const;  // End of bone in world space
};

class Skeleton : public Node {
public:
    std::unique_ptr<Bone> root;

    Bone* find_bone(const std::string& name);
    void update();  // Recompute all world transforms
};

class SkinnedMesh : public Node {
public:
    Skeleton* skeleton = nullptr;

    struct Vertex {
        Vec2 bind_position;  // Original position
        Vec2 world_position; // Computed each frame
        std::vector<std::pair<Bone*, float>> weights;
    };

    std::vector<Vertex> vertices;
    std::vector<std::array<int, 3>> triangles;

    void update();  // Recompute vertex positions from bone transforms
    void render(Renderer& renderer) override;
};
```

---

## Phase 2: Animation Blending

### 2.1 DSL Syntax

```flex
// Blend tree for smooth transitions
machine locomotion {
    // Blend layer - multiple animations can play simultaneously
    blendLayer movement {
        // 1D blend space based on speed input
        blend1D speedBlend {
            parameter: speed

            // Positions on the blend axis
            pose idle at 0 { animation: "idle" }
            pose walk at 1 { animation: "walk" }
            pose run at 2 { animation: "run" }
        }
    }

    // Additive layer - adds on top of base
    additiveLayer upperBody {
        state relaxed { initial: true }
        state aiming { animation: "aim_offset" }

        transition relaxed -> aiming when isAiming == true
        transition aiming -> relaxed when isAiming == false
    }
}

// 2D blend space for directional movement
machine directionalMovement {
    blendLayer direction {
        blend2D moveBlend {
            parameterX: moveX
            parameterY: moveY

            // 2D positions
            pose idle at 0, 0 { animation: "idle" }
            pose forward at 0, 1 { animation: "walkForward" }
            pose backward at 0, -1 { animation: "walkBackward" }
            pose left at -1, 0 { animation: "strafeLeft" }
            pose right at 1, 0 { animation: "strafeRight" }
        }
    }
}
```

### 2.2 New Tokens

```c
"blendLayer"    { return TOK_BLEND_LAYER; }
"additiveLayer" { return TOK_ADDITIVE_LAYER; }
"blend1D"       { return TOK_BLEND_1D; }
"blend2D"       { return TOK_BLEND_2D; }
"pose"          { return TOK_POSE; }
"parameter"     { return TOK_PARAMETER; }
"at"            { return TOK_AT_POS; }  // Different from TOK_AT (@)
```

### 2.3 AST Structures

```cpp
struct AstPose {
    std::string name;
    float position_x = 0;
    float position_y = 0;  // Only for blend2D
    std::string animation;
};

struct AstBlend1D {
    std::string name;
    std::string parameter;
    std::vector<AstPose> poses;
};

struct AstBlend2D {
    std::string name;
    std::string parameter_x;
    std::string parameter_y;
    std::vector<AstPose> poses;
};

struct AstBlendLayer {
    std::string name;
    bool is_additive = false;
    std::variant<AstBlend1D, AstBlend2D, std::vector<AstState>> content;
    std::vector<AstTransition> transitions;
};
```

### 2.4 Runtime Structures

```cpp
class AnimationPose {
public:
    std::map<std::string, float> bone_rotations;
    std::map<std::string, Vec2> bone_positions;

    // Blend two poses
    static AnimationPose lerp(const AnimationPose& a, const AnimationPose& b, float t);

    // Add poses (for additive blending)
    static AnimationPose add(const AnimationPose& base, const AnimationPose& additive);
};

class Blend1D {
public:
    std::string parameter;
    std::vector<std::pair<float, Animation*>> poses;  // position -> animation

    AnimationPose sample(float param_value, float time);
};

class Blend2D {
public:
    std::string parameter_x, parameter_y;
    std::vector<std::tuple<Vec2, Animation*>> poses;  // position -> animation

    AnimationPose sample(float x, float y, float time);
};

class BlendLayer {
public:
    bool is_additive = false;
    std::variant<Blend1D, Blend2D, StateMachine> content;

    AnimationPose evaluate(const InputMap& inputs, float time);
};
```

---

## Phase 3: Constraints System

### 3.1 DSL Syntax

```flex
skeleton arm {
    bone upper {
        length: 40

        bone lower {
            length: 35

            bone hand {
                length: 15
            }
        }
    }
}

// IK constraint - make bone chain reach a target
constraint reachTarget {
    type: ik
    chain: arm/upper -> arm/hand  // From root to tip
    target: targetNode            // Node to reach
    iterations: 10
    tolerance: 0.1
}

// Look-at constraint
constraint lookAt {
    type: aim
    bone: head
    target: lookTarget
    axis: y                       // Which axis points at target
    upAxis: z
}

// Follow path constraint
constraint followPath {
    type: path
    node: car
    path: roadPath
    parameter: pathProgress       // 0.0 to 1.0 input
    orient: true                  // Rotate to follow path tangent
}

// Parent constraint with offset
constraint attachToHand {
    type: parent
    node: sword
    target: arm/hand
    maintainOffset: true
}

// Distance constraint (spring-like)
constraint springConnection {
    type: distance
    nodeA: ball
    nodeB: anchor
    distance: 50
    stiffness: 0.8
    damping: 0.1
}
```

### 3.2 New Tokens

```c
"constraint"    { return TOK_CONSTRAINT; }
"type"          { return TOK_TYPE; }
"ik"            { return TOK_IK; }
"aim"           { return TOK_AIM; }
"path"          { return TOK_PATH; }
"parent"        { return TOK_PARENT; }
"distance"      { return TOK_DISTANCE; }
"chain"         { return TOK_CHAIN; }
"target"        { return TOK_TARGET; }
"iterations"    { return TOK_ITERATIONS; }
"stiffness"     { return TOK_STIFFNESS; }
"damping"       { return TOK_DAMPING; }
```

### 3.3 Runtime Structures

```cpp
class Constraint {
public:
    virtual void solve(float dt) = 0;
    virtual ~Constraint() = default;
};

class IKConstraint : public Constraint {
public:
    std::vector<Bone*> chain;  // Bones from root to tip
    Node* target = nullptr;
    int iterations = 10;
    float tolerance = 0.1f;

    void solve(float dt) override;  // FABRIK or CCD algorithm
};

class AimConstraint : public Constraint {
public:
    Bone* bone = nullptr;
    Node* target = nullptr;
    Vec3 aim_axis = {0, 1, 0};
    Vec3 up_axis = {0, 0, 1};

    void solve(float dt) override;
};

class PathConstraint : public Constraint {
public:
    Node* node = nullptr;
    Path* path = nullptr;
    std::string parameter;  // Input name for 0-1 progress
    bool orient = true;

    void solve(float dt) override;
};

class DistanceConstraint : public Constraint {
public:
    Node* node_a = nullptr;
    Node* node_b = nullptr;
    float rest_distance = 0;
    float stiffness = 1.0f;
    float damping = 0.0f;

    Vec2 velocity_a, velocity_b;  // For spring physics

    void solve(float dt) override;
};
```

---

## Phase 4: Path Morphing

### 4.1 DSL Syntax

```flex
// Define morphable path
morphPath flame {
    // Base shape
    base {
        M 0 0
        C 10 -20 20 -40 0 -60
        C -20 -40 -10 -20 0 0
        Z
    }

    // Target shapes for morphing
    target flicker1 {
        M 0 0
        C 15 -25 25 -50 5 -70
        C -15 -45 -5 -15 0 0
        Z
    }

    target flicker2 {
        M 0 0
        C 5 -15 15 -35 -5 -55
        C -25 -35 -15 -25 0 0
        Z
    }
}

// Animate morph weights
anim "flameFlicker" {
    duration: 0.5
    loop: loop

    track "#flame/morph.flicker1" {
        keyframe 0 -> 0
        keyframe 0.25 -> 1
        keyframe 0.5 -> 0
    }

    track "#flame/morph.flicker2" {
        keyframe 0 -> 0.5
        keyframe 0.25 -> 0
        keyframe 0.5 -> 0.5
    }
}
```

### 4.2 Runtime Structures

```cpp
class MorphPath : public Node {
public:
    Path base_path;
    std::map<std::string, Path> targets;
    std::map<std::string, float> weights;  // Animated

    Path computed_path;  // Result of blending

    void update() {
        computed_path = base_path;
        for (const auto& [name, weight] : weights) {
            if (weight > 0 && targets.count(name)) {
                computed_path = Path::lerp(computed_path, targets[name], weight);
            }
        }
    }
};
```

---

## Phase 5: Physics System

### 5.1 DSL Syntax

```flex
// Soft body / cloth simulation
physics clothSim {
    type: softbody

    // Grid of particles
    particles {
        rows: 10
        cols: 10
        spacing: 5
        mass: 0.1
    }

    // Pin top row
    pins: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

    // Physical properties
    stiffness: 0.9
    damping: 0.02
    gravity: 0, 9.8
}

// Ragdoll physics
physics ragdoll {
    type: ragdoll
    skeleton: playerSkeleton

    // Bone physical properties
    bone spine { mass: 2, angularLimit: -30, 30 }
    bone chest { mass: 3, angularLimit: -20, 20 }
    bone head { mass: 1, angularLimit: -45, 45 }
    bone armL { mass: 0.5, angularLimit: -180, 45 }
    bone armR { mass: 0.5, angularLimit: -45, 180 }
}

// Spring/rope
physics rope {
    type: rope
    segments: 20
    length: 100
    stiffness: 0.95
    gravity: 0, 9.8

    anchorA: hookPoint
    anchorB: weight
}
```

---

## Implementation Phases

### Phase 1: Foundation (Weeks 1-2)
1. Add skeleton/bone tokens to lexer
2. Add bone hierarchy parsing to AstBuilder
3. Implement Skeleton and Bone runtime classes
4. Basic bone visualization for debugging

### Phase 2: Skinned Mesh (Weeks 3-4)
1. Add mesh/vertices/triangles tokens
2. Parse mesh definition with bone weights
3. Implement SkinnedMesh with vertex skinning
4. ThorVG rendering of deformed mesh

### Phase 3: Animation Blending (Weeks 5-6)
1. Add blend layer tokens
2. Parse blend trees (1D/2D)
3. Implement pose sampling and blending
4. Additive layer support

### Phase 4: Constraints (Weeks 7-8)
1. Add constraint tokens
2. Parse constraint definitions
3. Implement IK solver (FABRIK algorithm)
4. Implement aim/path/distance constraints

### Phase 5: Path Morphing (Week 9)
1. Add morphPath tokens
2. Parse base/target paths
3. Implement path vertex interpolation

### Phase 6: Physics (Weeks 10-12)
1. Add physics tokens
2. Implement Verlet integration
3. Soft body simulation
4. Spring/rope physics

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         .flex Source                             │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                    re2c Lexer (flex_lexer.re)                   │
│  NEW: TOK_SKELETON, TOK_BONE, TOK_MESH, TOK_CONSTRAINT, etc.    │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                 Lemon Parser (flex_parser.y)                    │
│  NEW: skeleton_def, bone_list, mesh_def, constraint_def, etc.  │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│              AstBuilder (flex_parser_driver.cpp)                │
│  NEW: AstSkeleton, AstBone, AstMesh, AstConstraint, AstBlend   │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      AST to Runtime                              │
│                                                                  │
│  ┌──────────┐  ┌──────────┐  ┌────────────┐  ┌──────────────┐  │
│  │ Skeleton │  │ Skinned  │  │ Constraint │  │ BlendLayer   │  │
│  │   Tree   │  │   Mesh   │  │   System   │  │   System     │  │
│  └──────────┘  └──────────┘  └────────────┘  └──────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Update Pipeline                              │
│                                                                  │
│  1. Evaluate Inputs                                              │
│  2. Update State Machines                                        │
│  3. Sample Animations / Blend Poses                              │
│  4. Apply Poses to Skeleton                                      │
│  5. Solve Constraints (IK, Aim, etc.)                           │
│  6. Update Skinned Mesh Vertices                                │
│  7. Update Physics (if enabled)                                  │
│  8. Compute Final World Transforms                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ThorVG Renderer                               │
│                                                                  │
│  - Basic shapes (existing)                                       │
│  - Skinned mesh triangles (NEW)                                  │
│  - Debug bone visualization (NEW)                                │
│  - Path morphing (NEW)                                           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## DSL Philosophy Alignment

Following Flex's core principles:

1. **No Special Cases**
   - Bones are just hierarchical transforms - same nesting as groups
   - Constraints are declarative - define what, not how
   - Blend spaces use uniform syntax

2. **Composability**
   - Skeletons can be nested in scenes
   - Multiple blend layers compose naturally
   - Constraints reference existing nodes

3. **Data-Driven**
   - Bone rotations driven by animations or inputs
   - Blend parameters driven by inputs
   - Constraint targets can be animated

4. **Simplicity**
   - Only 6 new core concepts: skeleton, mesh, blend, constraint, morph, physics
   - Unified property syntax: `key: value`
   - Same animation track syntax: `track "path" { keyframe t -> v }`
