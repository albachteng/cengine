#ifndef PHYSICS_H
#define PHYSICS_H

#include "components.h"
#include "ecs.h"

typedef struct {
    Vec3 velocity;
    Vec3 acceleration;
    Vec3 old_position;
} VerletBody;

typedef struct {
    float radius;
    float mass;
    float restitution;
} CircleCollider;

typedef struct {
    ECS* ecs;
    ComponentType transform_type;
    ComponentType verlet_type;
    ComponentType collider_type;
    
    Vec3 gravity;
    float damping;
    int collision_iterations;
    
    Vec3 boundary_center;
    float boundary_radius;
} PhysicsWorld;

void physics_world_init(PhysicsWorld* world, ECS* ecs, ComponentType transform_type);
void physics_world_cleanup(PhysicsWorld* world);

Entity physics_create_circle(PhysicsWorld* world, Vec3 position, float radius, float mass);
void physics_set_boundary(PhysicsWorld* world, Vec3 center, float radius);

void physics_system_update(float delta_time);
void physics_verlet_integration(PhysicsWorld* world, float delta_time);
void physics_solve_collisions(PhysicsWorld* world);
void physics_apply_constraints(PhysicsWorld* world);

bool circle_circle_collision(Vec3 pos1, float r1, Vec3 pos2, float r2, Vec3* normal, float* penetration);
void resolve_circle_collision(Transform* t1, VerletBody* v1, CircleCollider* c1,
                             Transform* t2, VerletBody* v2, CircleCollider* c2,
                             Vec3 normal, float penetration);

extern PhysicsWorld* g_physics_world;

#endif