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

typedef struct EntityNode {
    Entity entity;
    struct EntityNode* next;
} EntityNode;

typedef struct {
    EntityNode* entities;
} SpatialCell;

typedef struct {
    SpatialCell* cells;
    int grid_width;
    int grid_height;
    float cell_size;
    Vec3 grid_origin;
} SpatialGrid;

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
    
    SpatialGrid spatial_grid;
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

// Spatial partitioning functions
void spatial_grid_init(SpatialGrid* grid, Vec3 origin, float width, float height, float cell_size);
void spatial_grid_cleanup(SpatialGrid* grid);
void spatial_grid_clear(SpatialGrid* grid);
void spatial_grid_insert(SpatialGrid* grid, Entity entity, Vec3 position, float radius);
void spatial_grid_get_potential_collisions(SpatialGrid* grid, Entity entity, Vec3 position, float radius, Entity** out_entities, int* out_count);

extern PhysicsWorld* g_physics_world;

#endif