/*
 * Flex Engine - Physics Implementation
 *
 * Box2D v3.0 (C API) integration.
 */
#include "flex/core/physics.h"
#include "flex/core/shape.h"
#include <box2d/box2d.h>
#include <box2d/math_functions.h>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace flex {

static const float PIXELS_PER_METER = 50.0f;

#ifndef b2_pi
#define b2_pi 3.14159265359f
#endif

// Helper to cast between handles
static b2BodyId to_body_id(uint64_t id) {
    b2BodyId bodyId;
    std::memcpy(&bodyId, &id, sizeof(b2BodyId));
    return bodyId;
}

static uint64_t from_body_id(b2BodyId bodyId) {
    uint64_t id = 0;
    std::memcpy(&id, &bodyId, sizeof(b2BodyId));
    return id;
}

static b2ShapeId to_shape_id(uint64_t id) {
    b2ShapeId shapeId;
    std::memcpy(&shapeId, &id, sizeof(b2ShapeId));
    return shapeId;
}

static uint64_t from_shape_id(b2ShapeId shapeId) {
    uint64_t id = 0;
    std::memcpy(&id, &shapeId, sizeof(b2ShapeId));
    return id;
}

static b2JointId to_joint_id(uint64_t id) {
    b2JointId jointId;
    std::memcpy(&jointId, &id, sizeof(b2JointId));
    return jointId;
}

static uint64_t from_joint_id(b2JointId jointId) {
    uint64_t id = 0;
    std::memcpy(&id, &jointId, sizeof(b2JointId));
    return id;
}

// ============================================================================
// Internal Implementation
// ============================================================================

struct PhysicsImpl {
    b2WorldId world_id;
    std::vector<RigidBody*> bodies;
    std::unordered_map<uint64_t, RigidBody*> shape_to_body;  // shape_id -> RigidBody
    std::vector<Joint*> joints;                              // Active joints
    std::unordered_map<uint64_t, Joint*> joint_id_to_joint;  // joint_id -> Joint
    float gravity_x = 0.0f;
    float gravity_y = 9.8f;
    bool valid = false;

    PhysicsImpl(float gx, float gy) : gravity_x(gx), gravity_y(gy) {
        b2WorldDef def = b2DefaultWorldDef();
        def.gravity = {gx, gy};
        world_id = b2CreateWorld(&def);
        valid = true;
    }

    ~PhysicsImpl() {
        if (valid && b2World_IsValid(world_id)) {
            b2DestroyWorld(world_id);
        }
    }

    RigidBody* find_body_by_shape(uint64_t shape_id) {
        auto it = shape_to_body.find(shape_id);
        return (it != shape_to_body.end()) ? it->second : nullptr;
    }
};

// ============================================================================
// PhysicsWorld
// ============================================================================

PhysicsWorld::PhysicsWorld(float gravity_x, float gravity_y)
    : impl_(std::make_unique<PhysicsImpl>(gravity_x, gravity_y)) {}

PhysicsWorld::~PhysicsWorld() {
    clear();
    // Impl destructor handles world destruction
}

void PhysicsWorld::step(float dt, int velocity_iterations, int position_iterations) {
    (void)position_iterations; // Unused in Box2D v3
    if (!impl_->valid) return;

    b2World_Step(impl_->world_id, dt, velocity_iterations);

    // Process collision events before syncing positions
    process_contact_events();

    // Sync bodies to nodes
    for (size_t i = 0; i < impl_->bodies.size(); ++i) {
        RigidBody* body = impl_->bodies[i];
        if (body && body->node && body->b2_body_id != 0) {
            b2BodyId bodyId = to_body_id(body->b2_body_id);
            if (b2Body_IsValid(bodyId)) {
                b2Vec2 pos = b2Body_GetPosition(bodyId);
                b2Rot rot = b2Body_GetRotation(bodyId);
                float angle_rad = b2Rot_GetAngle(rot);

                body->node->set_position(pos.x * PIXELS_PER_METER, pos.y * PIXELS_PER_METER);
                body->node->set_rotation(angle_rad * 180.0f / b2_pi);
            }
        }
    }
}

void PhysicsWorld::process_contact_events() {
    if (!impl_->valid) return;

    b2ContactEvents events = b2World_GetContactEvents(impl_->world_id);

    // Process begin contacts
    if (on_collision_begin_) {
        for (int i = 0; i < events.beginCount; ++i) {
            const b2ContactBeginTouchEvent& e = events.beginEvents[i];
            uint64_t shape_a = from_shape_id(e.shapeIdA);
            uint64_t shape_b = from_shape_id(e.shapeIdB);

            RigidBody* body_a = impl_->find_body_by_shape(shape_a);
            RigidBody* body_b = impl_->find_body_by_shape(shape_b);

            if (body_a && body_b) {
                CollisionEvent event;
                event.body_a = body_a;
                event.body_b = body_b;
                on_collision_begin_(event);
            }
        }
    }

    // Process end contacts
    if (on_collision_end_) {
        for (int i = 0; i < events.endCount; ++i) {
            const b2ContactEndTouchEvent& e = events.endEvents[i];
            uint64_t shape_a = from_shape_id(e.shapeIdA);
            uint64_t shape_b = from_shape_id(e.shapeIdB);

            RigidBody* body_a = impl_->find_body_by_shape(shape_a);
            RigidBody* body_b = impl_->find_body_by_shape(shape_b);

            if (body_a && body_b) {
                CollisionEvent event;
                event.body_a = body_a;
                event.body_b = body_b;
                on_collision_end_(event);
            }
        }
    }

    // Process hit contacts (with impulse)
    if (on_collision_hit_) {
        for (int i = 0; i < events.hitCount; ++i) {
            const b2ContactHitEvent& e = events.hitEvents[i];
            uint64_t shape_a = from_shape_id(e.shapeIdA);
            uint64_t shape_b = from_shape_id(e.shapeIdB);

            RigidBody* body_a = impl_->find_body_by_shape(shape_a);
            RigidBody* body_b = impl_->find_body_by_shape(shape_b);

            if (body_a && body_b) {
                CollisionEvent event;
                event.body_a = body_a;
                event.body_b = body_b;
                event.contact_x = e.point.x * PIXELS_PER_METER;
                event.contact_y = e.point.y * PIXELS_PER_METER;
                event.normal_x = e.normal.x;
                event.normal_y = e.normal.y;
                event.impulse = e.approachSpeed;
                on_collision_hit_(event);
            }
        }
    }
}

void PhysicsWorld::on_collision_begin(CollisionCallback callback) {
    on_collision_begin_ = callback;
}

void PhysicsWorld::on_collision_end(CollisionCallback callback) {
    on_collision_end_ = callback;
}

void PhysicsWorld::on_collision_hit(CollisionCallback callback) {
    on_collision_hit_ = callback;
}

void PhysicsWorld::clear() {
    if (!impl_->valid) return;

    // Clear shape mapping
    impl_->shape_to_body.clear();

    // Clear joints
    for (size_t i = 0; i < impl_->joints.size(); ++i) {
        delete impl_->joints[i];
    }
    impl_->joints.clear();
    impl_->joint_id_to_joint.clear();

    // First destroy all bodies while world is still valid
    for (size_t i = 0; i < impl_->bodies.size(); ++i) {
        RigidBody* body = impl_->bodies[i];
        if (body && body->b2_body_id != 0) {
            b2BodyId bid = to_body_id(body->b2_body_id);
            if (b2Body_IsValid(bid)) {
                b2DestroyBody(bid);
            }
            body->b2_body_id = 0;
            body->b2_shape_id = 0;
        }
        delete body;
    }
    impl_->bodies.clear();

    // Now destroy and recreate world with same gravity
    if (b2World_IsValid(impl_->world_id)) {
        b2DestroyWorld(impl_->world_id);
    }

    b2WorldDef def = b2DefaultWorldDef();
    def.gravity = {impl_->gravity_x, impl_->gravity_y};
    impl_->world_id = b2CreateWorld(&def);
}

RigidBody* PhysicsWorld::create_body(Node* node, BodyType type, const PhysicsMaterial& material) {
    if (!impl_->valid || !node) return nullptr;

    b2BodyDef body_def = b2DefaultBodyDef();
    switch (type) {
        case BodyType::Static: body_def.type = b2_staticBody; break;
        case BodyType::Kinematic: body_def.type = b2_kinematicBody; break;
        case BodyType::Dynamic: body_def.type = b2_dynamicBody; break;
    }

    // Convert pixels to meters
    body_def.position = {node->x() / PIXELS_PER_METER, node->y() / PIXELS_PER_METER};
    body_def.rotation = b2MakeRot(node->rotation() * b2_pi / 180.0f);

    // Apply additional body properties
    body_def.linearDamping = material.linear_damping;
    body_def.angularDamping = material.angular_damping;
    body_def.isBullet = material.is_bullet;
    body_def.fixedRotation = material.fixed_rotation;
    body_def.gravityScale = material.gravity_scale;

    b2BodyId body_id = b2CreateBody(impl_->world_id, &body_def);

    // Get actual geometry from Shape node
    float half_w = 50.0f / PIXELS_PER_METER;  // Default 100x100
    float half_h = 50.0f / PIXELS_PER_METER;
    float radius = 50.0f / PIXELS_PER_METER;
    bool use_circle = false;

    if (node->type() == NodeType::Shape) {
        auto* shape = static_cast<Shape*>(node);
        switch (shape->geometry_type()) {
            case GeometryType::Rect:
                half_w = (shape->rect().width / 2.0f) / PIXELS_PER_METER;
                half_h = (shape->rect().height / 2.0f) / PIXELS_PER_METER;
                break;
            case GeometryType::Circle:
                radius = shape->circle().radius / PIXELS_PER_METER;
                use_circle = true;
                break;
            case GeometryType::Ellipse:
                // Approximate ellipse as box
                half_w = shape->ellipse().rx / PIXELS_PER_METER;
                half_h = shape->ellipse().ry / PIXELS_PER_METER;
                break;
            case GeometryType::Polygon:
                radius = shape->polygon().radius / PIXELS_PER_METER;
                use_circle = true;  // Approximate as circle
                break;
            default:
                break;
        }
    }

    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = material.density;
    shape_def.material.friction = material.friction;
    shape_def.material.restitution = material.restitution;
    shape_def.enableContactEvents = true;  // Enable begin/end contact events
    shape_def.enableHitEvents = true;      // Enable hit events with impulse
    shape_def.isSensor = material.is_sensor;  // Enable sensor mode

    shape_def.filter.categoryBits = material.category_bits;
    shape_def.filter.maskBits = material.mask_bits;
    shape_def.filter.groupIndex = material.group_index;

    b2ShapeId shape_id;
    if (use_circle) {
        b2Circle circle = {{0, 0}, radius};
        shape_id = b2CreateCircleShape(body_id, &shape_def, &circle);
    } else {
        b2Polygon box = b2MakeBox(half_w, half_h);
        shape_id = b2CreatePolygonShape(body_id, &shape_def, &box);
    }

    // Create RigidBody wrapper
    auto* rigid_body = new RigidBody();
    rigid_body->node = node;
    rigid_body->b2_body_id = from_body_id(body_id);
    rigid_body->b2_shape_id = from_shape_id(shape_id);

    // Register in lookup maps
    impl_->bodies.push_back(rigid_body);
    impl_->shape_to_body[rigid_body->b2_shape_id] = rigid_body;

    return rigid_body;
}

void PhysicsWorld::destroy_body(RigidBody* body) {
    if (!body || !impl_->valid) return;

    // Remove from shape map
    if (body->b2_shape_id != 0) {
        impl_->shape_to_body.erase(body->b2_shape_id);
    }

    if (body->b2_body_id != 0) {
        b2BodyId bid = to_body_id(body->b2_body_id);
        b2DestroyBody(bid);
        body->b2_body_id = 0;
        body->b2_shape_id = 0;
    }

    // Remove from list
    auto it = std::find(impl_->bodies.begin(), impl_->bodies.end(), body);
    if (it != impl_->bodies.end()) {
        impl_->bodies.erase(it);
    }

    delete body;
}

RigidBody* PhysicsWorld::find_body(Node* node) const {
    if (!node) return nullptr;
    for (size_t i = 0; i < impl_->bodies.size(); ++i) {
        RigidBody* body = impl_->bodies[i];
        if (body && body->node == node) {
            return body;
        }
    }
    return nullptr;
}

// ============================================================================
// Collision Query Implementations
// ============================================================================

bool PhysicsWorld::ray_cast(float x1, float y1, float x2, float y2,
                           RayCastResult& result, const QueryFilter& filter) const {
    if (!impl_->valid) return false;

    b2RayCastInput input;
    b2Vec2 p1 = {x1 / PIXELS_PER_METER, y1 / PIXELS_PER_METER};
    b2Vec2 p2 = {x2 / PIXELS_PER_METER, y2 / PIXELS_PER_METER};
    input.origin = p1;
    input.translation = b2Vec2{p2.x - p1.x, p2.y - p1.y};
    input.maxFraction = 1.0f;

    b2CastOutput best_output;
    best_output.fraction = 1.0f;
    best_output.normal = {0, 0};
    b2ShapeId best_shape = b2_nullShapeId;

    // Iterate through all bodies
    for (size_t i = 0; i < impl_->bodies.size(); ++i) {
        RigidBody* body = impl_->bodies[i];
        if (!body || body->b2_shape_id == 0) continue;

        b2ShapeId shape_id = to_shape_id(body->b2_shape_id);

        // Apply collision filter
        b2Filter shape_filter = b2Shape_GetFilter(shape_id);

        // Check if filter allows collision
        bool category_match = (filter.category_bits & shape_filter.categoryBits) != 0;
        bool mask_match = (filter.mask_bits & shape_filter.maskBits) != 0;

        if (!category_match || !mask_match) continue;

        // Group filtering
        if (filter.group_index != 0 && shape_filter.groupIndex != 0) {
            if (filter.group_index != shape_filter.groupIndex) continue;
        }

        b2CastOutput output = b2Shape_RayCast(shape_id, &input);
        if (output.fraction < best_output.fraction) {
            best_output = output;
            best_shape = shape_id;
        }
    }

    if (best_shape.index1 != 0 || best_shape.world0 != 0 || best_shape.generation != 0) {
        // Convert back to our structure
        uint64_t shape_id = from_shape_id(best_shape);
        RigidBody* body = impl_->find_body_by_shape(shape_id);
        if (body) {
            result.body = body;
            result.x = (p1.x + input.translation.x * best_output.fraction) * PIXELS_PER_METER;
            result.y = (p1.y + input.translation.y * best_output.fraction) * PIXELS_PER_METER;
            result.normal_x = best_output.normal.x;
            result.normal_y = best_output.normal.y;
            result.fraction = best_output.fraction;
            return true;
        }
    }

    return false;
}

bool PhysicsWorld::ray_cast_callback(float x1, float y1, float x2, float y2,
                                    std::function<bool(const RayCastResult&)> callback,
                                    const QueryFilter& filter) const {
    if (!impl_->valid || !callback) return false;

    b2RayCastInput input;
    b2Vec2 p1 = {x1 / PIXELS_PER_METER, y1 / PIXELS_PER_METER};
    b2Vec2 p2 = {x2 / PIXELS_PER_METER, y2 / PIXELS_PER_METER};
    input.origin = p1;
    input.translation = b2Vec2{p2.x - p1.x, p2.y - p1.y};
    input.maxFraction = 1.0f;

    // Iterate through all bodies
    for (size_t i = 0; i < impl_->bodies.size(); ++i) {
        RigidBody* body = impl_->bodies[i];
        if (!body || body->b2_shape_id == 0) continue;

        b2ShapeId shape_id = to_shape_id(body->b2_shape_id);

        // Apply collision filter
        b2Filter shape_filter = b2Shape_GetFilter(shape_id);

        bool category_match = (filter.category_bits & shape_filter.categoryBits) != 0;
        bool mask_match = (filter.mask_bits & shape_filter.maskBits) != 0;

        if (!category_match || !mask_match) continue;

        if (filter.group_index != 0 && shape_filter.groupIndex != 0) {
            if (filter.group_index != shape_filter.groupIndex) continue;
        }

        b2CastOutput output = b2Shape_RayCast(shape_id, &input);

        if (output.fraction <= 1.0f) {
            RayCastResult result;
            result.body = body;
            result.x = (p1.x + input.translation.x * output.fraction) * PIXELS_PER_METER;
            result.y = (p1.y + input.translation.y * output.fraction) * PIXELS_PER_METER;
            result.normal_x = output.normal.x;
            result.normal_y = output.normal.y;
            result.fraction = output.fraction;

            if (!callback(result)) {
                return true; // Callback requested to stop
            }
        }
    }

    return false;
}

PhysicsWorld::QueryResults<RigidBody*> PhysicsWorld::query_aabb(float x, float y, float w, float h,
                                                                const QueryFilter& filter) const {
    QueryResults<RigidBody*> results;

    if (!impl_->valid) return results;

    b2AABB aabb;
    aabb.lowerBound = {x / PIXELS_PER_METER, y / PIXELS_PER_METER};
    aabb.upperBound = {(x + w) / PIXELS_PER_METER, (y + h) / PIXELS_PER_METER};

    // Iterate through all bodies
    for (size_t i = 0; i < impl_->bodies.size(); ++i) {
        RigidBody* body = impl_->bodies[i];
        if (!body || !body->node || body->b2_body_id == 0) continue;

        b2BodyId body_id = to_body_id(body->b2_body_id);

        // Apply collision filter
        if (body->b2_shape_id != 0) {
            b2ShapeId shape_id = to_shape_id(body->b2_shape_id);
            b2Filter shape_filter = b2Shape_GetFilter(shape_id);

            bool category_match = (filter.category_bits & shape_filter.categoryBits) != 0;
            bool mask_match = (filter.mask_bits & shape_filter.maskBits) != 0;

            if (!category_match || !mask_match) continue;

            if (filter.group_index != 0 && shape_filter.groupIndex != 0) {
                if (filter.group_index != shape_filter.groupIndex) continue;
            }
        }

        b2Vec2 pos = b2Body_GetPosition(body_id);

        // Simple AABB check based on body position
        // For accurate results, we'd need to get the actual shape bounds
        if (pos.x >= aabb.lowerBound.x && pos.x <= aabb.upperBound.x &&
            pos.y >= aabb.lowerBound.y && pos.y <= aabb.upperBound.y) {
            results.results.push_back(body);
        }
    }

    return results;
}

PhysicsWorld::QueryResults<RigidBody*> PhysicsWorld::query_point(float x, float y,
                                                                const QueryFilter& filter) const {
    QueryResults<RigidBody*> results;

    if (!impl_->valid) return results;

    b2Vec2 point = {x / PIXELS_PER_METER, y / PIXELS_PER_METER};

    // Iterate through all bodies
    for (size_t i = 0; i < impl_->bodies.size(); ++i) {
        RigidBody* body = impl_->bodies[i];
        if (!body || !body->node || body->b2_shape_id == 0) continue;

        b2ShapeId shape_id = to_shape_id(body->b2_shape_id);

        // Apply collision filter
        b2Filter shape_filter = b2Shape_GetFilter(shape_id);

        bool category_match = (filter.category_bits & shape_filter.categoryBits) != 0;
        bool mask_match = (filter.mask_bits & shape_filter.maskBits) != 0;

        if (!category_match || !mask_match) continue;

        if (filter.group_index != 0 && shape_filter.groupIndex != 0) {
            if (filter.group_index != shape_filter.groupIndex) continue;
        }

        if (b2Shape_TestPoint(shape_id, point)) {
            results.results.push_back(body);
        }
    }

    return results;
}

// ============================================================================
// Joint Implementations
// ============================================================================

uint64_t PhysicsWorld::create_joint(const Joint& joint) {
    if (!impl_->valid || !joint.body_a || !joint.body_b) return 0;

    b2BodyId bodyA = to_body_id(joint.body_a->b2_body_id);
    b2BodyId bodyB = to_body_id(joint.body_b->b2_body_id);

    if (!b2Body_IsValid(bodyA) || !b2Body_IsValid(bodyB)) return 0;

    b2Vec2 anchorA = {joint.anchor_a_x / PIXELS_PER_METER, joint.anchor_a_y / PIXELS_PER_METER};
    b2Vec2 anchorB = {joint.anchor_b_x / PIXELS_PER_METER, joint.anchor_b_y / PIXELS_PER_METER};

    b2JointId jointId;

    switch (joint.type) {
        case JointType::Revolute: {
            b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = anchorA;
            def.localAnchorB = anchorB;

            if (joint.enable_limit) {
                def.enableLimit = true;
                def.lowerAngle = joint.lower_angle * b2_pi / 180.0f;
                def.upperAngle = joint.upper_angle * b2_pi / 180.0f;
            }

            if (joint.enable_motor) {
                def.enableMotor = true;
                def.motorSpeed = joint.motor_speed * b2_pi / 180.0f;
                def.maxMotorTorque = joint.max_motor_force;
            }

            jointId = b2CreateRevoluteJoint(impl_->world_id, &def);
            break;
        }

        case JointType::Distance: {
            b2DistanceJointDef def = b2DefaultDistanceJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = anchorA;
            def.localAnchorB = anchorB;

            // Calculate initial distance if not set
            b2Vec2 posA = b2Body_GetPosition(bodyA);
            b2Vec2 posB = b2Body_GetPosition(bodyB);
            b2Vec2 worldAnchorA = b2Body_GetWorldPoint(bodyA, anchorA);
            b2Vec2 worldAnchorB = b2Body_GetWorldPoint(bodyB, anchorB);
            float dx = worldAnchorB.x - worldAnchorA.x;
            float dy = worldAnchorB.y - worldAnchorA.y;
            def.length = std::sqrt(dx * dx + dy * dy);

            if (joint.frequency_hz > 0.0f) {
                def.hertz = joint.frequency_hz;
                def.dampingRatio = joint.damping_ratio;
            }

            jointId = b2CreateDistanceJoint(impl_->world_id, &def);
            break;
        }

        case JointType::Prismatic: {
            b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = anchorA;
            def.localAnchorB = anchorB;

            // Default local axis (along x-axis of body A)
            def.localAxisA = {1.0f, 0.0f};

            if (joint.enable_limit) {
                def.enableLimit = true;
                def.lowerTranslation = joint.lower_translation;
                def.upperTranslation = joint.upper_translation;
            }

            if (joint.enable_motor) {
                def.enableMotor = true;
                def.motorSpeed = joint.motor_speed;
                def.maxMotorForce = joint.max_motor_force;
            }

            jointId = b2CreatePrismaticJoint(impl_->world_id, &def);
            break;
        }

        case JointType::Fixed: {
            b2WeldJointDef def = b2DefaultWeldJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = anchorA;
            def.localAnchorB = anchorB;

            jointId = b2CreateWeldJoint(impl_->world_id, &def);
            break;
        }

        default:
            return 0;
    }

    if (!b2Joint_IsValid(jointId)) {
        return 0;
    }

    uint64_t joint_id = from_joint_id(jointId);

    // Store joint definition for later queries
    auto* joint_copy = new Joint(joint);
    impl_->joints.push_back(joint_copy);
    impl_->joint_id_to_joint[joint_id] = joint_copy;

    return joint_id;
}

void PhysicsWorld::destroy_joint(uint64_t joint_id) {
    if (!impl_->valid || joint_id == 0) return;

    // Destroy actual Box2D joint
    b2JointId b2_joint_id = to_joint_id(joint_id);
    if (b2Joint_IsValid(b2_joint_id)) {
        b2DestroyJoint(b2_joint_id);
    }

    auto it = impl_->joint_id_to_joint.find(joint_id);
    if (it != impl_->joint_id_to_joint.end()) {
        Joint* joint = it->second;

        // Remove from vector
        impl_->joints.erase(std::remove(impl_->joints.begin(), impl_->joints.end(), joint), impl_->joints.end());

        // Delete and remove from map
        delete joint;
        impl_->joint_id_to_joint.erase(it);
    }
}

Joint* PhysicsWorld::find_joint(uint64_t joint_id) const {
    if (joint_id == 0) return nullptr;
    auto it = impl_->joint_id_to_joint.find(joint_id);
    return (it != impl_->joint_id_to_joint.end()) ? it->second : nullptr;
}

// ============================================================================
// RigidBody Implementation
// ============================================================================

void RigidBody::set_position(float x, float y) {
    if (b2_body_id != 0) {
        b2BodyId bid = to_body_id(b2_body_id);
        b2Vec2 pos = {x / PIXELS_PER_METER, y / PIXELS_PER_METER};
        // Need to get current rotation to preserve it
        b2Rot rot = b2Body_GetRotation(bid);
        b2Body_SetTransform(bid, pos, rot);
    }
}

void RigidBody::set_rotation(float angle_degrees) {
    if (b2_body_id != 0) {
        b2BodyId bid = to_body_id(b2_body_id);
        b2Vec2 pos = b2Body_GetPosition(bid);
        b2Rot rot = b2MakeRot(angle_degrees * b2_pi / 180.0f);
        b2Body_SetTransform(bid, pos, rot);
    }
}

void RigidBody::set_linear_velocity(float vx, float vy) {
    if (b2_body_id != 0) {
        b2BodyId bid = to_body_id(b2_body_id);
        b2Body_SetLinearVelocity(bid, {vx / PIXELS_PER_METER, vy / PIXELS_PER_METER});
    }
}

void RigidBody::set_angular_velocity(float omega) {
    if (b2_body_id != 0) {
        b2BodyId bid = to_body_id(b2_body_id);
        b2Body_SetAngularVelocity(bid, omega * b2_pi / 180.0f);
    }
}

void RigidBody::apply_force(float fx, float fy) {
    if (b2_body_id != 0) {
        b2BodyId bid = to_body_id(b2_body_id);
        b2Body_ApplyForceToCenter(bid, {fx, fy}, true);
    }
}

void RigidBody::apply_impulse(float ix, float iy) {
    if (b2_body_id != 0) {
        b2BodyId bid = to_body_id(b2_body_id);
        b2Body_ApplyLinearImpulseToCenter(bid, {ix, iy}, true);
    }
}

void RigidBody::get_position(float& x, float& y) const {
    if (b2_body_id != 0) {
        b2BodyId bid = to_body_id(b2_body_id);
        b2Vec2 pos = b2Body_GetPosition(bid);
        x = pos.x * PIXELS_PER_METER;
        y = pos.y * PIXELS_PER_METER;
    }
}

float RigidBody::get_rotation() const {
    if (b2_body_id != 0) {
        b2BodyId bid = to_body_id(b2_body_id);
        b2Rot rot = b2Body_GetRotation(bid);
        return b2Rot_GetAngle(rot) * 180.0f / b2_pi;
    }
    return 0.0f;
}

// ============================================================================
// Inverse Kinematics (IK) Implementation
// ============================================================================

bool IKChain::solve_2bone(float tolerance) {
    // Only works with exactly 2 joints
    if (joints.size() != 2) return false;
    if (!joints[0].body || !joints[1].body || !end_effector) return false;

    // Get positions (convert to meters)
    float base_x, base_y;
    joints[0].body->get_position(base_x, base_y);
    base_x /= PIXELS_PER_METER;
    base_y /= PIXELS_PER_METER;

    float mid_x, mid_y;
    joints[1].body->get_position(mid_x, mid_y);
    mid_x /= PIXELS_PER_METER;
    mid_y /= PIXELS_PER_METER;

    float end_x, end_y;
    end_effector->get_position(end_x, end_y);
    end_x /= PIXELS_PER_METER;
    end_y /= PIXELS_PER_METER;

    // Calculate vectors
    float to_target_x = target_x - base_x;
    float to_target_y = target_y - base_y;
    float dist_to_target = std::sqrt(to_target_x * to_target_x + to_target_y * to_target_y);

    float to_end_x = end_x - base_x;
    float to_end_y = end_y - base_y;
    float dist_to_end = std::sqrt(to_end_x * to_end_x + to_end_y * to_end_y);

    float l1 = joints[0].length / PIXELS_PER_METER;
    float l2 = joints[1].length / PIXELS_PER_METER;

    // Check if target is reachable
    float max_reach = l1 + l2;
    float min_reach = (std::abs)(l1 - l2);
    if (dist_to_target > max_reach || dist_to_target < min_reach) {
        // Target out of reach - point in maximum direction
        if (dist_to_target > max_reach) {
            float dir_x = to_target_x / dist_to_target;
            float dir_y = to_target_y / dist_to_target;
            end_x = base_x + dir_x * max_reach;
            end_y = base_y + dir_y * max_reach;
        }
    }

    // Use law of cosines to find angles
    float cos_angle2 = (l1 * l1 + l2 * l2 - dist_to_target * dist_to_target) / (2.0f * l1 * l2);
    cos_angle2 = (std::max)(-1.0f, (std::min)(1.0f, cos_angle2));
    float angle2 = std::acos(cos_angle2);  // Elbow angle

    float cos_angle1 = (l1 * l1 + dist_to_target * dist_to_target - l2 * l2) / (2.0f * l1 * dist_to_target);
    cos_angle1 = (std::max)(-1.0f, (std::min)(1.0f, cos_angle1));
    float angle1 = std::acos(cos_angle1);  // Shoulder angle

    // Calculate shoulder angle
    float base_to_target_angle = std::atan2(to_target_y, to_target_x);
    float shoulder_angle = base_to_target_angle - angle1;

    // Calculate elbow angle (relative to shoulder)
    float elbow_angle = angle2;

    // Apply angle constraints (convert to degrees)
    float shoulder_deg = shoulder_angle * 180.0f / b2_pi;
    float elbow_deg = elbow_angle * 180.0f / b2_pi;

    shoulder_deg = (std::max)(joints[0].min_angle, (std::min)(joints[0].max_angle, shoulder_deg));
    elbow_deg = (std::max)(joints[1].min_angle, (std::min)(joints[1].max_angle, elbow_deg));

    // Set rotations
    joints[0].body->set_rotation(shoulder_deg);
    joints[1].body->set_rotation(shoulder_deg + elbow_deg);

    // Update joint positions (mid and end)
    float mid_new_x = base_x + std::cos(shoulder_angle) * l1;
    float mid_new_y = base_y + std::sin(shoulder_angle) * l1;
    joints[1].body->set_position(mid_new_x * PIXELS_PER_METER, mid_new_y * PIXELS_PER_METER);

    float end_new_x = mid_new_x + std::cos(shoulder_angle + elbow_angle) * l2;
    float end_new_y = mid_new_y + std::sin(shoulder_angle + elbow_angle) * l2;

    end_effector->set_position(end_new_x * PIXELS_PER_METER, end_new_y * PIXELS_PER_METER);

    // Check if we reached the target
    float error = std::sqrt((end_new_x - target_x) * (end_new_x - target_x) +
                           (end_new_y - target_y) * (end_new_y - target_y));
    return error < tolerance / PIXELS_PER_METER;
}

bool IKChain::solve_ccd(int max_iterations, float tolerance) {
    if (joints.empty() || !end_effector) return false;

    float tolerance_m = tolerance / PIXELS_PER_METER;

    for (int iter = 0; iter < max_iterations; ++iter) {
        // Start from the end effector and work backwards
        for (int i = static_cast<int>(joints.size()) - 1; i >= 0; --i) {
            if (!joints[i].body) continue;

            // Get current positions
            float joint_x, joint_y;
            joints[i].body->get_position(joint_x, joint_y);
            joint_x /= PIXELS_PER_METER;
            joint_y /= PIXELS_PER_METER;

            float eff_x, eff_y;
            end_effector->get_position(eff_x, eff_y);
            eff_x /= PIXELS_PER_METER;
            eff_y /= PIXELS_PER_METER;

            // Vector from joint to end effector
            float to_eff_x = eff_x - joint_x;
            float to_eff_y = eff_y - joint_y;

            // Vector from joint to target
            float to_target_x = target_x - joint_x;
            float to_target_y = target_y - joint_y;

            // Calculate angles
            float angle_to_eff = std::atan2(to_eff_y, to_eff_x);
            float angle_to_target = std::atan2(to_target_y, to_target_x);

            // Calculate rotation needed
            float delta_angle = angle_to_target - angle_to_eff;

            // Normalize to [-PI, PI]
            while (delta_angle > b2_pi) delta_angle -= 2.0f * b2_pi;
            while (delta_angle < -b2_pi) delta_angle += 2.0f * b2_pi;

            // Get current rotation
            float current_rot = joints[i].body->get_rotation() * b2_pi / 180.0f;
            float new_rot = current_rot + delta_angle;

            // Convert back to degrees and apply constraints
            float new_rot_deg = new_rot * 180.0f / b2_pi;
            new_rot_deg = (std::max)(joints[i].min_angle, (std::min)(joints[i].max_angle, new_rot_deg));

            // Apply rotation
            joints[i].body->set_rotation(new_rot_deg);
        }

        // Check if we've reached the target
        float eff_x, eff_y;
        end_effector->get_position(eff_x, eff_y);
        eff_x /= PIXELS_PER_METER;
        eff_y /= PIXELS_PER_METER;

        float error = std::sqrt((eff_x - target_x) * (eff_x - target_x) +
                               (eff_y - target_y) * (eff_y - target_y));

        if (error < tolerance_m) {
            return true;
        }
    }

    return false;
}

} // namespace flex
