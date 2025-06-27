#include "ecs.h"
#include "physics.h"
#include "components.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Testing arena allocator with ECS integration...\n");
    
    printf("1. Initializing ECS...\n");
    ECS ecs = {0};  // ZII
    ecs_init(&ecs);
    printf("   ECS initialized successfully\n");
    
    printf("2. Registering components...\n");
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    ComponentType verlet_type = ecs_register_component(&ecs, sizeof(VerletBody));
    ComponentType collider_type = ecs_register_component(&ecs, sizeof(CircleCollider));
    printf("   Components registered: transform=%d, verlet=%d, collider=%d\n", 
           transform_type, verlet_type, collider_type);
    
    printf("3. Initializing physics world...\n");
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
    
    printf("4. Initializing spatial arena...\n");
    if (!arena_init(&physics.spatial_arena, 64 * 1024)) {
        printf("ERROR: Failed to initialize spatial arena\n");
        return -1;
    }
    printf("   Spatial arena initialized\n");
    
    printf("5. Initializing spatial grid...\n");
    float cell_size = 20.0f;
    float grid_size = physics.boundary_radius * 2.2f;
    Vec3 grid_origin = vec3_create(-grid_size / 2.0f, -grid_size / 2.0f, 0.0f);
    spatial_grid_init(&physics.spatial_grid, &physics.spatial_arena, grid_origin, grid_size, grid_size, cell_size);
    
    if (!physics.spatial_grid.cells) {
        printf("ERROR: Spatial grid initialization failed\n");
        return -1;
    }
    printf("   Spatial grid initialized\n");
    
    printf("6. Creating test entities...\n");
    for (int i = 0; i < 10; i++) {
        Entity entity = ecs_create_entity(&ecs);
        
        Transform* transform = (Transform*)ecs_add_component(&ecs, entity, transform_type);
        if (!transform) {
            printf("ERROR: Failed to add transform to entity %d\n", entity);
            return -1;
        }
        *transform = transform_create(i * 10.0f, i * 10.0f, 0.0f);
        
        VerletBody* verlet = (VerletBody*)ecs_add_component(&ecs, entity, verlet_type);
        if (!verlet) {
            printf("ERROR: Failed to add verlet to entity %d\n", entity);
            return -1;
        }
        verlet->velocity = vec3_zero();
        verlet->acceleration = vec3_zero();
        verlet->old_position = transform->position;
        
        CircleCollider* collider = (CircleCollider*)ecs_add_component(&ecs, entity, collider_type);
        if (!collider) {
            printf("ERROR: Failed to add collider to entity %d\n", entity);
            return -1;
        }
        collider->radius = 5.0f;
        collider->mass = 1.0f;
        collider->restitution = 0.8f;
        
        printf("   Created entity %d\n", entity);
    }
    
    printf("7. Testing spatial grid insertion...\n");
    // Reset arena for this frame's spatial allocations
    arena_reset(&physics.spatial_arena);
    spatial_grid_clear(&physics.spatial_grid);
    
    // Insert all entities into spatial grid
    for (Entity entity = 1; entity < ecs.next_entity_id; entity++) {
        if (!ecs_entity_active(&ecs, entity)) {
            continue;
        }
        
        Transform *transform = (Transform *)ecs_get_component(&ecs, entity, transform_type);
        CircleCollider *collider = (CircleCollider *)ecs_get_component(&ecs, entity, collider_type);
        
        if (transform && collider) {
            spatial_grid_insert(&physics.spatial_grid, &physics.spatial_arena, entity, transform->position, collider->radius);
            printf("   Inserted entity %d into spatial grid\n", entity);
        }
    }
    
    printf("8. Testing collision detection...\n");
    // Test getting potential collisions
    Entity test_entity = 1;
    Transform *test_transform = (Transform *)ecs_get_component(&ecs, test_entity, transform_type);
    CircleCollider *test_collider = (CircleCollider *)ecs_get_component(&ecs, test_entity, collider_type);
    
    if (test_transform && test_collider) {
        Entity* potential_entities;
        int potential_count;
        spatial_grid_get_potential_collisions(&physics.spatial_grid, test_entity, test_transform->position, test_collider->radius, &potential_entities, &potential_count);
        printf("   Entity %d has %d potential collisions\n", test_entity, potential_count);
    }
    
    printf("9. Cleanup...\n");
    arena_cleanup(&physics.spatial_arena);
    ecs_cleanup(&ecs);
    
    printf("SUCCESS: All arena allocator and ECS integration tests passed!\n");
    return 0;
}