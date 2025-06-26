#include "ecs.h"
#include "physics.h"
#include "components.h"
#include <stdio.h>

int main() {
    printf("Testing simple physics with arena...\n");
    
    ECS ecs = {0};  // ZII
    ecs_init(&ecs);
    
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    ComponentType verlet_type = ecs_register_component(&ecs, sizeof(VerletBody));
    ComponentType collider_type = ecs_register_component(&ecs, sizeof(CircleCollider));
    
    printf("Components registered\n");
    
    PhysicsWorld physics = {0};  // ZII
    physics.ecs = &ecs;
    physics.transform_type = transform_type;
    physics.verlet_type = verlet_type;
    physics.collider_type = collider_type;
    physics.gravity = vec3_create(0.0f, -200.0f, 0.0f);
    physics.damping = 0.99f;
    physics.collision_iterations = 1;  // Reduce iterations
    physics.boundary_center = vec3_zero();
    physics.boundary_radius = 100.0f;
    
    // Initialize arena with larger size
    if (!arena_init(&physics.spatial_arena, 128 * 1024)) {
        printf("Failed to initialize spatial arena\n");
        return -1;
    }
    
    printf("Physics world initialized\n");
    
    // Create just one entity for testing
    Entity entity = ecs_create_entity(&ecs);
    
    Transform* transform = (Transform*)ecs_add_component(&ecs, entity, transform_type);
    transform->position = vec3_create(0.0f, 10.0f, 0.0f);
    
    VerletBody* verlet = (VerletBody*)ecs_add_component(&ecs, entity, verlet_type);
    verlet->velocity = vec3_zero();
    verlet->acceleration = vec3_zero();
    verlet->old_position = transform->position;
    
    CircleCollider* collider = (CircleCollider*)ecs_add_component(&ecs, entity, collider_type);
    collider->radius = 5.0f;
    collider->mass = 1.0f;
    collider->restitution = 0.8f;
    
    printf("Created one test entity at position (%.1f, %.1f, %.1f)\n", 
           transform->position.x, transform->position.y, transform->position.z);
    
    // Test physics simulation for a few steps without spatial grid first
    for (int step = 0; step < 10; step++) {
        physics_verlet_integration(&physics, 1.0f/60.0f);
        physics_apply_constraints(&physics);
        
        if (step % 5 == 0) {
            printf("Step %d: position (%.1f, %.1f, %.1f)\n", 
                   step, transform->position.x, transform->position.y, transform->position.z);
        }
    }
    
    printf("Physics simulation completed successfully\n");
    
    arena_cleanup(&physics.spatial_arena);
    ecs_cleanup(&ecs);
    
    return 0;
}