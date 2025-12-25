/*
 * Flex Engine - Physics System
 *
 * Box2D integration for 2D rigid body simulation.
 * Supports collision callbacks for contact begin/end/hit events.
 */

#pragma once

#include "flex/runtime/types.h"
#include "flex/runtime/node.h"
#include <memory>
#include <vector>
#include <functional>

namespace flex {

// Forward declarations
class PhysicsWorld;
struct RigidBody;
struct IKChain;
struct RayCastResult;
struct QueryFilter;
struct Joint;

// ============================================================================
// Physics Types
// ============================================================================

enum class BodyType {
    Static,
    Kinematic,
    Dynamic
};

struct PhysicsMaterial {
    float density = 1.0f;
    float friction = 0.5f;
    float restitution = 0.0f;
    uint16_t category_bits = 0x0001;      // Collision category
    uint16_t mask_bits = 0xFFFF;          // Collision mask (what it collides with)
    int16_t group_index = 0;              // Collision group (-1 = no collide, 0 = normal, >0 = group)
    bool is_sensor = false;               // Sensor doesn't produce collision response, only events

    // Additional body properties
    float linear_damping = 0.0f;          // Linear velocity damping
    float angular_damping = 0.0f;         // Angular velocity damping
    bool is_bullet = false;               // Enable continuous collision detection for fast objects
    bool fixed_rotation = false;          // Prevent rotation
    float gravity_scale = 1.0f;           // Scale gravity for this body (0 = no gravity)
};

// ============================================================================
// Collision Events
// ============================================================================

struct CollisionEvent {
    RigidBody* body_a = nullptr;
    RigidBody* body_b = nullptr;
    float contact_x = 0;        // Contact point x (in pixels)
    float contact_y = 0;        // Contact point y (in pixels)
    float normal_x = 0;         // Contact normal x
    float normal_y = 0;         // Contact normal y
    float impulse = 0;          // Collision impulse (for hit events)
};

using CollisionCallback = std::function<void(const CollisionEvent&)>;

// ============================================================================
// Internal Implementation Hiding
// ============================================================================

struct PhysicsImpl; // Hides Box2D types

// ============================================================================
// PhysicsWorld
// ============================================================================

class PhysicsWorld {
public:
    PhysicsWorld(float gravity_x = 0.0f, float gravity_y = 9.8f);
    ~PhysicsWorld();

    // Simulation
    void step(float dt, int velocity_iterations = 6, int position_iterations = 2);
    void clear();

    // Body Management
    RigidBody* create_body(Node* node, BodyType type, const PhysicsMaterial& material = PhysicsMaterial());
    void destroy_body(RigidBody* body);

    // Collision Callbacks
    void on_collision_begin(CollisionCallback callback);
    void on_collision_end(CollisionCallback callback);
    void on_collision_hit(CollisionCallback callback);

    // Query
    RigidBody* find_body(Node* node) const;

    // Ray casting (line-of-sight, projectile trajectory, etc.)
    bool ray_cast(float x1, float y1, float x2, float y2, RayCastResult& result, const QueryFilter& filter) const;
    bool ray_cast_callback(float x1, float y1, float x2, float y2, std::function<bool(const RayCastResult&)> callback, const QueryFilter& filter) const;

    // AABB query (find all bodies in a rectangular region)
    template<typename T>
    struct QueryResults {
        std::vector<T> results;
        bool any() const { return !results.empty(); }
        size_t size() const { return results.size(); }
        T& operator[](size_t i) { return results[i]; }
        const T& operator[](size_t i) const { return results[i]; }
    };

    QueryResults<RigidBody*> query_aabb(float x, float y, float w, float h, const QueryFilter& filter) const;

    // Point query (find all bodies at a specific point - mouse picking)
    QueryResults<RigidBody*> query_point(float x, float y, const QueryFilter& filter) const;

    // Joints
    uint64_t create_joint(const Joint& joint);
    void destroy_joint(uint64_t joint_id);
    Joint* find_joint(uint64_t joint_id) const;

private:
    std::unique_ptr<PhysicsImpl> impl_;
    CollisionCallback on_collision_begin_;
    CollisionCallback on_collision_end_;
    CollisionCallback on_collision_hit_;

    void process_contact_events();
};

// ============================================================================
// RigidBody - Wrapper around b2Body
// ============================================================================

struct RigidBody {
    Node* node = nullptr;
    uint64_t b2_body_id = 0;    // Opaque storage for b2BodyId
    uint64_t b2_shape_id = 0;   // Opaque storage for primary shape
    void* user_data = nullptr;  // User-defined data

    // Setters
    void set_position(float x, float y);
    void set_rotation(float angle_degrees);
    void set_linear_velocity(float vx, float vy);
    void set_angular_velocity(float omega);
    void apply_force(float fx, float fy);
    void apply_impulse(float ix, float iy);

    // Getters
    void get_position(float& x, float& y) const;
    float get_rotation() const;
};

// ============================================================================
// Inverse Kinematics (IK)
// ============================================================================

struct IKJoint {
    RigidBody* body = nullptr;     // The body that rotates
    float length = 0.0f;           // Distance to next joint
    float min_angle = -180.0f;     // Minimum rotation angle (degrees)
    float max_angle = 180.0f;      // Maximum rotation angle (degrees)
};

struct IKChain {
    std::vector<IKJoint> joints;   // Chain of joints
    RigidBody* end_effector = nullptr;  // Final body in chain
    float target_x = 0.0f;         // Target position
    float target_y = 0.0f;

    // Solve using 2-bone IK (analytical solution)
    bool solve_2bone(float tolerance = 5.0f);

    // Solve using CCD (Cyclic Coordinate Descent) for longer chains
    bool solve_ccd(int max_iterations = 10, float tolerance = 5.0f);
};

// ============================================================================
// Collision Queries
// ============================================================================

struct RayCastResult {
    RigidBody* body = nullptr;
    float x = 0.0f;                // Hit point
    float y = 0.0f;
    float normal_x = 0.0f;         // Surface normal at hit
    float normal_y = 0.0f;
    float fraction = 0.0f;         // 0.0 to 1.0 along the ray
};

// Query filters for collision detection
struct QueryFilter {
    uint16_t category_bits = 0xFFFF;   // Which categories to test
    uint16_t mask_bits = 0xFFFF;       // What categories this collides with
    int16_t group_index = 0;           // Collision group (-1 = no collide, 0 = normal, >0 = group)
};

// ============================================================================
// Joints
// ============================================================================

enum class JointType {
    Revolute,    // Hinge joint (rotation only)
    Distance,    // Keeps two bodies at fixed distance
    Prismatic,   // Slider joint (translation only)
    Fixed        // Weld joint (no movement)
};

struct Joint {
    JointType type = JointType::Revolute;
    RigidBody* body_a = nullptr;
    RigidBody* body_b = nullptr;

    // Anchor points (in pixels)
    float anchor_a_x = 0.0f;
    float anchor_a_y = 0.0f;
    float anchor_b_x = 0.0f;
    float anchor_b_y = 0.0f;

    // Joint limits and motor
    bool enable_limit = false;
    float lower_angle = 0.0f;      // For revolute joints (degrees)
    float upper_angle = 0.0f;      // For revolute joints (degrees)
    float lower_translation = 0.0f; // For prismatic joints (meters)
    float upper_translation = 0.0f; // For prismatic joints (meters)

    bool enable_motor = false;
    float motor_speed = 0.0f;      // Degrees per second (revolute) or m/s (prismatic)
    float max_motor_force = 0.0f;  // Motor strength

    // Frequency and damping for distance joints
    float frequency_hz = 0.0f;     // Spring frequency (Hz)
    float damping_ratio = 0.0f;    // 0.0 = no damping, 1.0 = critical damping
};

} // namespace flex
