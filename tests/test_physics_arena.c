#include "ecs.h"
#include "physics.h"
#include "components.h"
#include <stdio.h>
#include <assert.h>

int main() {
    printf("Testing ECS with arena allocator...\n");
    
    ECS ecs = {0};  // ZII
    ecs_init(&ecs);
    
    printf("ECS initialized successfully\n");
    
    // Test component registration
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    ComponentType verlet_type = ecs_register_component(&ecs, sizeof(VerletBody));
    ComponentType collider_type = ecs_register_component(&ecs, sizeof(CircleCollider));
    
    printf("Components registered: transform=%d, verlet=%d, collider=%d\n", 
           transform_type, verlet_type, collider_type);
    
    // Test entity creation
    Entity entity = ecs_create_entity(&ecs);
    assert(entity != 0);
    
    printf("Entity created: %d\n", entity);
    
    // Test component addition
    Transform* transform = (Transform*)ecs_add_component(&ecs, entity, transform_type);
    assert(transform != NULL);
    *transform = transform_create(10.0f, 20.0f, 0.0f);
    
    VerletBody* verlet = (VerletBody*)ecs_add_component(&ecs, entity, verlet_type);
    assert(verlet != NULL);
    verlet->velocity = vec3_zero();
    verlet->acceleration = vec3_zero();
    verlet->old_position = transform->position;
    
    CircleCollider* collider = (CircleCollider*)ecs_add_component(&ecs, entity, collider_type);
    assert(collider != NULL);
    collider->radius = 5.0f;
    collider->mass = 1.0f;
    collider->restitution = 0.8f;
    
    printf("Components added successfully\n");
    
    // Test physics world initialization (without graphics dependencies)
    PhysicsWorld physics = {0};  // ZII
    physics.ecs = &ecs;
    physics.transform_type = transform_type;
    physics.verlet_type = verlet_type;
    physics.collider_type = collider_type;
    physics.gravity = vec3_create(0.0f, -200.0f, 0.0f);
    physics.damping = 0.99f;
    physics.collision_iterations = 20;
    physics.boundary_center = vec3_zero();
    physics.boundary_radius = 100.0f;
    
    // Initialize arena
    if (!arena_init(&physics.spatial_arena, 64 * 1024)) {
        printf("Failed to initialize spatial arena\n");
        return -1;
    }
    
    printf("Physics world initialized\n");
    
    // Test spatial grid
    float cell_size = 20.0f;
    float grid_size = physics.boundary_radius * 2.2f;
    Vec3 grid_origin = vec3_create(-grid_size / 2.0f, -grid_size / 2.0f, 0.0f);
    spatial_grid_init(&physics.spatial_grid, &physics.spatial_arena, grid_origin, grid_size, grid_size, cell_size);
    
    printf("Spatial grid initialized\n");
    
    // Test spatial grid insertion
    spatial_grid_insert(&physics.spatial_grid, &physics.spatial_arena, entity, transform->position, collider->radius);
    
    printf("Entity inserted into spatial grid\n");
    
    // Cleanup
    arena_cleanup(&physics.spatial_arena);
    ecs_cleanup(&ecs);
    
    printf("All physics arena tests passed!\n");
    return 0;
}