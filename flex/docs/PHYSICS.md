# Box2D Physics Features for Flex Engine

## Overview

Flex Engine integrates Box2D v3.0 physics with a clean DSL syntax. All Box2D features are properly implemented - no fake placeholders.

## Recent Improvements (December 2024)

### Fixed Issues
- ✅ **Joints now work** - Previously joints were stored but never created in Box2D. Now they are properly instantiated.
- ✅ **QueryFilter now works** - ray_cast, query_aabb, query_point now respect collision filtering.
- ✅ **DSL simplified** - Removed redundant RigidBody/Collider blocks, replaced with single Physics{} block.
- ✅ **Added missing body properties** - linearDamping, angularDamping, bullet, fixedRotation, gravityScale.

### Breaking Changes
- ❌ **RigidBody{} and Collider{} removed** - Use Physics{} instead (see DSL Syntax below).
- ❌ **mass removed** - Use density instead (Box2D calculates mass from density × shape_area).

## DSL Syntax

### Physics Properties

```flex
Group "player" {
    x: 100
    y: 450

    Physics {
        // Body type
        type: "dynamic"          // Options: static, kinematic, dynamic

        // Material properties
        density: 1.0             // kg/m² - Box2D calculates total mass
        friction: 0.5            // 0.0 to 1.0
        restitution: 0.3         // Bounciness: 0.0 (no bounce) to 1.0 (perfect bounce)

        // Collision filtering
        category: 1              // uint16 - This body's collision category
        mask: 0xFFFF             // uint16 - Categories this body collides with
        group: 0                 // int16 - Group index (see Collision Filtering below)

        // Flags
        sensor: false            // If true, no collision response (only events)
        bullet: false            // Enable CCD for fast-moving objects
        fixedRotation: false     // Prevent rotation

        // Damping
        linearDamping: 0.1       // Reduces linear velocity over time
        angularDamping: 0.1      // Reduces angular velocity over time
        gravityScale: 1.0        // Multiplier for gravity (0 = no gravity)
    }

    Shape "torso" {
        Rect { width: 6 height: 144 }
        Fill { color: #ffffff }
    }
}
```

### Collision Filtering

Collision filtering uses 3 mechanisms:

**1. Category Bits** (uint16)
Each body belongs to one or more categories. Example:
```cpp
category: 1        // Player
category: 2        // Enemy
category: 4        // Bullet
category: 8        // Pickup
```

**2. Mask Bits** (uint16)
Specifies which categories this body collides with:
```cpp
// Player collides with enemies and pickups, not bullets
category: 1        // I am player
mask: 0x000A       // I collide with (2 | 8) = enemies + pickups

// Bullet collides with enemies only
category: 4        // I am bullet
mask: 0x0002       // I collide with enemies only
```

**3. Group Index** (int16)
- `group < 0`: Bodies in same group NEVER collide (character limbs)
- `group > 0`: Bodies in same group ALWAYS collide (stacked boxes)
- `group = 0`: Use category/mask rules (default)

**Collision Rule**:
```cpp
bool collides = (bodyA.category & bodyB.mask) && (bodyB.category & bodyA.mask);

// Override by group:
if (bodyA.group != 0 && bodyA.group == bodyB.group) {
    collides = (bodyA.group > 0);
}
```

## Features Implemented

### 1. Inverse Kinematics (IK)

#### 2-Bone IK Solver (`IKChain::solve_2bone`)
- **Purpose**: Solves IK for exactly 2 joints (e.g., arm, leg)
- **Method**: Analytical solution using law of cosines
- **Performance**: Very fast (O(1) time complexity)
- **Use Cases**: Character arms/legs, robotic arms, procedural animation
- **Features**:
  - Automatic reach detection (handles unreachable targets)
  - Angle constraints (min/max rotation limits)
  - Configurable tolerance for convergence

#### CCD IK Solver (`IKChain::solve_ccd`)
- **Purpose**: Solves IK for longer bone chains (3+ joints)
- **Method**: Cyclic Coordinate Descent iterative algorithm
- **Performance**: Slower but more flexible
- **Use Cases**: Complex character rigs, tentacle animation, spine bending
- **Features**:
  - Configurable max iterations
  - Angle constraints per joint
  - Guaranteed convergence (though may be slow for long chains)

**Usage Example**:
```cpp
flex::IKChain chain;
chain.joints.resize(2);
chain.joints[0].body = shoulder_body;
chain.joints[0].length = 80.0f;
chain.joints[1].body = elbow_body;
chain.joints[1].length = 60.0f;
chain.end_effector = hand_body;
chain.target_x = mouse_x;
chain.target_y = mouse_y;

bool reached = chain.solve_2bone(5.0f);  // 5px tolerance
```

### 2. Collision Queries

#### Ray Casting
```cpp
flex::RayCastResult result;
bool hit = world->ray_cast(x1, y1, x2, y2, result);

if (hit) {
    std::cout << "Hit body at (" << result.x << ", " << result.y << ")\n";
    std::cout << "Hit fraction: " << result.fraction << "\n";  // 0.0 to 1.0
}
```

**Use Cases**:
- Line-of-sight checks
- Projectile trajectory testing
- AI vision cones
- Mouse picking (shooting rays)

#### AABB Query
```cpp
auto results = world->query_aabb(x, y, w, h);
for (auto* body : results) {
    // Process bodies in rectangular region
}
```

**Use Cases**:
- Viewport culling
- Area-of-effect detection
- Spatial partitioning queries
- Selection boxes

#### Point Query
```cpp
auto results = world->query_point(mouse_x, mouse_y);
for (auto* body : results) {
    // Bodies at mouse position
}
```

**Use Cases**:
- Mouse picking
- UI interaction
- Touch detection
- Placement validation

### 3. Collision Filtering

Each physics body can be assigned collision categories and masks:

```cpp
flex::PhysicsMaterial mat;
mat.category_bits = 0x0001;  // This is category 1
mat.mask_bits = 0x0002 | 0x0004;  // Collides with categories 2 and 4
mat.group_index = 1;  // Same group bodies collide with each other

auto* body = world->create_body(node, flex::BodyType::Dynamic, mat);
```

**Collision Rules**:
- Bodies collide if: `(category_a & mask_b) && (category_b & mask_a)`
- If `group_index > 0`: Bodies in same group collide
- If `group_index < 0`: Bodies in same group DON'T collide
- If `group_index = 0`: Normal category/mask rules apply

### 4. Sensors

Sensors are special bodies that generate collision events but don't produce physical collision response:

```cpp
flex::PhysicsMaterial mat;
mat.is_sensor = true;  // Enable sensor mode
mat.category_bits = 0x0008;  // Trigger category

auto* sensor = world->create_body(node, flex::BodyType::Static, mat);
```

**Use Cases**:
- Trigger zones
- Area detection
- Pickup detection
- Checkpoints

### 5. Joints (FULLY WORKING!)

All joint types are now properly implemented using Box2D v3 API. Previously joints were only stored without creating actual physics constraints - this has been fixed.

The system supports four types of joints:

#### Revolute Joint (Hinge)
Creates a pivot point between two bodies with optional limits and motor.

```cpp
flex::Joint joint;
joint.type = flex::JointType::Revolute;
joint.body_a = body1;
joint.body_b = body2;
joint.anchor_a_x = 100;  // Anchor point on body A (pixels)
joint.anchor_a_y = 200;
joint.anchor_b_x = 100;  // Anchor point on body B (pixels)
joint.anchor_b_y = 200;

// Optional: Add limits
joint.enable_limit = true;
joint.lower_angle = -45.0f;  // Degrees
joint.upper_angle = 45.0f;

// Optional: Add motor
joint.enable_motor = true;
joint.motor_speed = 30.0f;  // Degrees per second
joint.max_motor_force = 100.0f;  // Motor torque

uint64_t joint_id = world->create_joint(joint);
```

**Use Cases**: Doors, character limbs, wheels, mechanical hinges.

#### Distance Joint (Spring/Rope)
Maintains a fixed distance between two bodies with optional spring behavior.

```cpp
joint.type = flex::JointType::Distance;
joint.body_a = body1;
joint.body_b = body2;
joint.anchor_a_x = 100;
joint.anchor_a_y = 200;
joint.anchor_b_x = 200;
joint.anchor_b_y = 200;

// Optional: Make it springy
joint.frequency_hz = 5.0f;  // Spring frequency (0 = rigid)
joint.damping_ratio = 0.7f;  // Damping (0 = bouncy, 1 = critical)
```

**Use Cases**: Ropes, chains, suspension springs, bungee cords.

**Note**: Distance is calculated automatically from initial anchor positions.

#### Prismatic Joint (Slider)
Allows translation along a single axis with optional limits and motor.

```cpp
joint.type = flex::JointType::Prismatic;
joint.body_a = body1;
joint.body_b = body2;
joint.anchor_a_x = 100;
joint.anchor_a_y = 200;
joint.anchor_b_x = 100;
joint.anchor_b_y = 200;

// Axis is automatically set to body A's local X-axis

// Optional: Add limits
joint.enable_limit = true;
joint.lower_translation = -50.0f;  // Meters
joint.upper_translation = 50.0f;

// Optional: Add motor
joint.enable_motor = true;
joint.motor_speed = 10.0f;  // m/s
joint.max_motor_force = 1000.0f;
```

**Use Cases**: Pistons, elevators, sliding doors, gun recoil.

#### Fixed Joint (Weld)
Rigidly attaches two bodies together.

```cpp
joint.type = flex::JointType::Fixed;
joint.body_a = body1;
joint.body_b = body2;
joint.anchor_a_x = 100;
joint.anchor_a_y = 200;
joint.anchor_b_x = 100;
joint.anchor_b_y = 200;
```

**Use Cases**: Composite objects, attaching equipment to characters, destructible structures.

**Joint Management**:
```cpp
// Destroy joint
world->destroy_joint(joint_id);

// Find joint
flex::Joint* joint = world->find_joint(joint_id);
```

### 6. Collision Events

The system provides three types of collision events:

```cpp
world->on_collision_begin([](const flex::CollisionEvent& e) {
    std::cout << "Collision started!\n";
});

world->on_collision_end([](const flex::CollisionEvent& e) {
    std::cout << "Collision ended!\n";
});

world->on_collision_hit([](const flex::CollisionEvent& e) {
    std::cout << "Collision with impulse: " << e.impulse << "\n";
    std::cout << "Hit at (" << e.contact_x << ", " << e.contact_y << ")\n";
});
```

## Example

See `flex/examples/flex_physics_demo.cpp` for a complete working example that demonstrates:
- 2-bone IK solver following the mouse
- Ray casting for line-of-sight
- AABB and point queries
- Visual debugging

## Building

To build the physics demo:
```bash
cd flex/build
cmake ..
cmake --build . --target flex_physics_demo
```

## Technical Details

### Coordinate System
- Physics uses **meters** internally
- Rendering uses **pixels**
- Conversion factor: 1 meter = 50 pixels (configurable via `PIXELS_PER_METER`)

### Box2D Integration
- Uses Box2D v3.0 C API
- Bodies are wrapped in `RigidBody` struct
- Shape IDs are stored as opaque handles for type safety
- All coordinates are converted between pixels and meters automatically

### Performance Considerations

1. **IK Solvers**:
   - 2-bone: Use for simple cases fast
   - CCD: Use for, very complex chains, can be slow for 5+ joints

2. **Collision Queries**:
   - Ray casting: O(n) where n = number of bodies
   - AABB query: O(n)
   - Point query: O(n)
   - Consider spatial partitioning for large scenes

3. **Joint Count**:
   - Box2D can handle hundreds of joints
   - Each joint adds overhead to simulation
   - Destroy unused joints to free memory

## Future Improvements

1. **Spatial Partitioning**: Add quadtree or grid-based broadphase
2. **Continuous Collision Detection**: Prevent tunneling at high speeds
3. **More Joint Types**: Gear, pulley, mouse joint
4. **Collision Filtering**: Full support when Box2D C API exposes filters
5. **Physics Debug Draw**: Visualize bodies, joints, and collision areas

## References

- [Box2D Documentation](https://box2d.org/documentation/)
- [IK Algorithms](https://www.cs.berkeley.edu/~sequin/CS294/7.pdf)
- [Real-Time Collision Detection](https://www.amazon.com/Real-Time-Collision-Detection-Chris-Erickson/dp/1558607323)
