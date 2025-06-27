#include "ecs.h"
#include "physics.h"
#include "components.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_CIRCLES 20  // Try 20 circles first
#define BOUNDARY_RADIUS 100.0f

static float random_float(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

static Color random_color() {
    Color c = {0};  // ZII
    c.r = random_float(0.2f, 1.0f);
    c.g = random_float(0.2f, 1.0f);
    c.b = random_float(0.2f, 1.0f);
    c.a = 1.0f;
    return c;
}

int main() {
    srand((unsigned int)time(NULL));
    
    printf("Testing physics with %d circles and spatial grid...\n", NUM_CIRCLES);
    
    ECS ecs = {0};  // ZII
    ecs_init(&ecs);
    
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    ComponentType verlet_type = ecs_register_component(&ecs, sizeof(VerletBody));
    ComponentType collider_type = ecs_register_component(&ecs, sizeof(CircleCollider));
    ComponentType renderable_type = ecs_register_component(&ecs, sizeof(Renderable));
    
    printf("Components registered\n");
    
    PhysicsWorld physics = {0};  // ZII
    physics.ecs = &ecs;
    physics.transform_type = transform_type;
    physics.verlet_type = verlet_type;
    physics.collider_type = collider_type;
    physics.gravity = vec3_create(0.0f, -200.0f, 0.0f);
    physics.damping = 0.99f;
    physics.collision_iterations = 1;  // Reduce for debugging
    physics.boundary_center = vec3_zero();
    physics.boundary_radius = BOUNDARY_RADIUS;
    
    // Initialize arena with plenty of space
    if (!arena_init(&physics.spatial_arena, 256 * 1024)) {  // Increase to 256KB
        printf("Failed to initialize spatial arena\n");
        return -1;
    }
    
    printf("Physics world initialized\n");
    
    // Initialize spatial grid
    float cell_size = 20.0f;
    float grid_size = physics.boundary_radius * 2.2f;
    Vec3 grid_origin = vec3_create(-grid_size / 2.0f, -grid_size / 2.0f, 0.0f);
    
    printf("Initializing spatial grid...\n");
    spatial_grid_init(&physics.spatial_grid, &physics.spatial_arena, grid_origin, grid_size, grid_size, cell_size);
    
    if (!physics.spatial_grid.cells) {
        printf("ERROR: Spatial grid initialization failed\n");
        arena_cleanup(&physics.spatial_arena);
        ecs_cleanup(&ecs);
        return -1;
    }
    
    ArenaStats stats = {0};
    arena_get_stats(&physics.spatial_arena, &stats);
    printf("Spatial grid initialized, arena usage: %zu/%zu bytes\n", stats.used_bytes, stats.total_size);
    
    printf("Creating %d circles...\n", NUM_CIRCLES);
    
    // Create circles
    for (int i = 0; i < NUM_CIRCLES; i++) {
        float circle_radius = 3.0f;  // Fixed radius for debugging
        float mass = circle_radius * circle_radius * 0.1f;
        
        // Grid pattern for more entities
        int circles_per_row = (int)sqrtf((float)NUM_CIRCLES) + 1;
        int row = i / circles_per_row;
        int col = i % circles_per_row;
        
        Vec3 pos = vec3_create(
            (col - circles_per_row/2.0f) * 15.0f,
            (row - circles_per_row/2.0f) * 15.0f + 30.0f,
            0.0f
        );
        
        if (i % 10 == 0) {
            printf("Creating entity %d at (%.1f, %.1f, %.1f)\n", i+1, pos.x, pos.y, pos.z);
        }
        
        Entity circle = ecs_create_entity(&ecs);
        
        Transform* transform = (Transform*)ecs_add_component(&ecs, circle, transform_type);
        if (!transform) {
            printf("ERROR: Failed to add transform component\n");
            break;
        }
        *transform = transform_create(pos.x, pos.y, pos.z);
        
        VerletBody* verlet = (VerletBody*)ecs_add_component(&ecs, circle, verlet_type);
        if (!verlet) {
            printf("ERROR: Failed to add verlet component\n");
            break;
        }
        verlet->velocity = vec3_zero();
        verlet->acceleration = vec3_zero();
        verlet->old_position = pos;
        
        CircleCollider* collider = (CircleCollider*)ecs_add_component(&ecs, circle, collider_type);
        if (!collider) {
            printf("ERROR: Failed to add collider component\n");
            break;
        }
        collider->radius = circle_radius;
        collider->mass = mass;
        collider->restitution = 0.8f;
        
        Renderable* renderable = (Renderable*)ecs_add_component(&ecs, circle, renderable_type);
        if (!renderable) {
            printf("ERROR: Failed to add renderable component\n");
            break;
        }
        *renderable = renderable_create_circle(circle_radius, random_color());
        
        if (i % 10 == 0) {
            printf("Entity %d created successfully\n", circle);
        }
    }
    
    printf("All entities created, testing physics simulation...\n");
    
    float delta_time = 1.0f / 60.0f;
    
    for (int step = 0; step < 5; step++) {
        printf("\n--- Step %d ---\n", step);
        
        // Physics integration (no spatial grid)
        physics_verlet_integration(&physics, delta_time);
        printf("Verlet integration completed\n");
        
        // Collision detection with spatial grid
        printf("Starting collision detection...\n");
        physics_solve_collisions(&physics);
        printf("Collision detection completed\n");
        
        // Constraints
        physics_apply_constraints(&physics);
        printf("Constraints applied\n");
        
        arena_get_stats(&physics.spatial_arena, &stats);
        printf("Arena usage: %zu/%zu bytes\n", stats.used_bytes, stats.total_size);
    }
    
    printf("\nPhysics simulation completed successfully!\n");
    
    arena_cleanup(&physics.spatial_arena);
    ecs_cleanup(&ecs);
    
    return 0;
}