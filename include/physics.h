#ifndef PHYSICS_H
#define PHYSICS_H

#include "components.h"
#include "ecs.h"
#include "memory.h"
#include <stdbool.h>

// Physics system constants
#define PHYSICS_DEFAULT_COLLISION_ITERATIONS 8  // Reduced to prevent over-correction
#define PHYSICS_DEFAULT_DAMPING 0.98f           // Slightly more damping for stability
#define PHYSICS_DEFAULT_BOUNDARY_RADIUS 300.0f
#define PHYSICS_SPATIAL_CELL_SIZE 20.0f
#define PHYSICS_SPATIAL_BUFFER_SIZE 4096  // Static buffer for potential collision candidates per entity
#define PHYSICS_MAX_PENETRATION_RATIO 0.8f
#define PHYSICS_CORRECTION_FACTOR 0.8f
#define PHYSICS_OVERLAP_THRESHOLD 0.001f
#define PHYSICS_DEFAULT_RESTITUTION 0.8f
#define PHYSICS_SLEEP_VELOCITY_THRESHOLD 5.0f    // Velocity below which objects sleep
#define PHYSICS_SLEEP_TIME_THRESHOLD 60          // Frames below threshold before sleeping
#define PHYSICS_WAKE_VELOCITY_THRESHOLD 10.0f    // Velocity above which sleeping objects wake

typedef struct {
    Vec3 velocity;
    Vec3 acceleration;
    Vec3 old_position;
    bool is_sleeping;          // Whether object is in sleep mode
    int sleep_timer;           // Frames the object has been below sleep threshold
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
    Arena spatial_arena;  // Arena for spatial grid allocations
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
void spatial_grid_init(SpatialGrid* grid, Arena* arena, Vec3 origin, float width, float height, float cell_size);
void spatial_grid_cleanup(SpatialGrid* grid);
void spatial_grid_clear(SpatialGrid* grid);
void spatial_grid_insert(SpatialGrid* grid, Arena* arena, Entity entity, Vec3 position, float radius);
void spatial_grid_get_potential_collisions(SpatialGrid* grid, Entity entity, Vec3 position, float radius, Entity** out_entities, int* out_count);

extern PhysicsWorld* g_physics_world;

#endif