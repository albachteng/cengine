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

#define NUM_CIRCLES 500
#define BOUNDARY_RADIUS 100.0f
#define SIMULATION_STEPS 100

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
    
    printf("Testing physics performance with %d circles...\n", NUM_CIRCLES);
    
    ECS ecs = {0};  // ZII
    ecs_init(&ecs);
    
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    ComponentType verlet_type = ecs_register_component(&ecs, sizeof(VerletBody));
    ComponentType collider_type = ecs_register_component(&ecs, sizeof(CircleCollider));
    ComponentType renderable_type = ecs_register_component(&ecs, sizeof(Renderable));
    
    PhysicsWorld physics = {0};  // ZII
    physics.ecs = &ecs;
    physics.transform_type = transform_type;
    physics.verlet_type = verlet_type;
    physics.collider_type = collider_type;
    physics.gravity = vec3_create(0.0f, -200.0f, 0.0f);
    physics.damping = 0.99f;
    physics.collision_iterations = 20;
    physics.boundary_center = vec3_zero();
    physics.boundary_radius = BOUNDARY_RADIUS;
    
    // Initialize arena
    if (!arena_init(&physics.spatial_arena, 64 * 1024)) {
        printf("Failed to initialize spatial arena\n");
        return -1;
    }
    
    // Initialize spatial grid
    float cell_size = 20.0f;
    float grid_size = physics.boundary_radius * 2.2f;
    Vec3 grid_origin = vec3_create(-grid_size / 2.0f, -grid_size / 2.0f, 0.0f);
    spatial_grid_init(&physics.spatial_grid, &physics.spatial_arena, grid_origin, grid_size, grid_size, cell_size);
    
    printf("Creating %d circles...\n", NUM_CIRCLES);
    
    // Create circles
    for (int i = 0; i < NUM_CIRCLES; i++) {
        float circle_radius = random_float(2.0f, 5.0f);
        float mass = circle_radius * circle_radius * 0.1f;
        
        // Grid pattern with randomness
        int circles_per_row = (int)sqrtf((float)NUM_CIRCLES) + 1;
        float grid_spacing = (BOUNDARY_RADIUS * 1.5f) / circles_per_row;
        int row = i / circles_per_row;
        int col = i % circles_per_row;
        
        float base_x = (col - circles_per_row / 2.0f) * grid_spacing;
        float base_y = (row - circles_per_row / 2.0f) * grid_spacing;
        
        Vec3 pos = vec3_create(
            base_x + random_float(-grid_spacing * 0.3f, grid_spacing * 0.3f),
            base_y + random_float(-grid_spacing * 0.3f, grid_spacing * 0.3f) + 30.0f,
            0.0f
        );
        
        Entity circle = ecs_create_entity(&ecs);
        
        Transform* transform = (Transform*)ecs_add_component(&ecs, circle, transform_type);
        if (transform) {
            *transform = transform_create(pos.x, pos.y, pos.z);
        }
        
        VerletBody* verlet = (VerletBody*)ecs_add_component(&ecs, circle, verlet_type);
        if (verlet) {
            verlet->velocity = vec3_zero();
            verlet->acceleration = vec3_zero();
            verlet->old_position = pos;
        }
        
        CircleCollider* collider = (CircleCollider*)ecs_add_component(&ecs, circle, collider_type);
        if (collider) {
            collider->radius = circle_radius;
            collider->mass = mass;
            collider->restitution = 0.8f;
        }
        
        Renderable* renderable = (Renderable*)ecs_add_component(&ecs, circle, renderable_type);
        if (renderable) {
            *renderable = renderable_create_circle(circle_radius, random_color());
        }
    }
    
    printf("Starting simulation...\n");
    
    clock_t start_time = clock();
    float delta_time = 1.0f / 60.0f;  // 60 FPS
    
    ArenaStats stats = {0};
    
    for (int step = 0; step < SIMULATION_STEPS; step++) {
        // Physics update
        physics_verlet_integration(&physics, delta_time);
        
        for (int i = 0; i < physics.collision_iterations; i++) {
            physics_solve_collisions(&physics);
            physics_apply_constraints(&physics);
        }
        
        if (step % 10 == 0) {
            arena_get_stats(&physics.spatial_arena, &stats);
            printf("Step %d: Arena usage %.1f%% (%zu/%zu bytes)\\n", 
                   step, 
                   (float)stats.used_bytes / stats.total_size * 100.0f,
                   stats.used_bytes, 
                   stats.total_size);
        }
    }
    
    clock_t end_time = clock();
    double elapsed = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("\\nSimulation completed:\\n");
    printf("- %d simulation steps\\n", SIMULATION_STEPS);
    printf("- %d circles\\n", NUM_CIRCLES);
    printf("- %.3f seconds elapsed\\n", elapsed);
    printf("- %.1f FPS average\\n", SIMULATION_STEPS / elapsed);
    
    arena_get_stats(&physics.spatial_arena, &stats);
    printf("- Final arena usage: %.1f%% (%zu/%zu bytes)\\n", 
           (float)stats.used_bytes / stats.total_size * 100.0f,
           stats.used_bytes, 
           stats.total_size);
    
    arena_cleanup(&physics.spatial_arena);
    ecs_cleanup(&ecs);
    
    return 0;
}