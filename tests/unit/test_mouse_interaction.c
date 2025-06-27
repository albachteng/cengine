#include "../minunit.h"
#include "../../include/physics.h"
#include "../../include/ecs.h" 
#include "../../include/components.h"
#include <math.h>

int tests_run = 0;

// Test that mouse forces wake up sleeping objects
static char* test_mouse_wake_sleeping_objects() {
    ECS ecs = {0};
    PhysicsWorld physics_world = {0};
    
    ecs_init(&ecs);
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    physics_world_init(&physics_world, &ecs, transform_type);
    
    Entity entity = physics_create_circle(&physics_world, (Vec3){0, 0, 0}, 10.0f, 1.0f);
    VerletBody* verlet = (VerletBody*)ecs_get_component(&ecs, entity, physics_world.verlet_type);
    
    // Put object to sleep
    verlet->is_sleeping = true;
    verlet->sleep_timer = 100;
    
    // Apply external acceleration (simulating mouse force)
    verlet->acceleration = (Vec3){500.0f, 0.0f, 0.0f};
    
    // Run physics integration - should wake up due to acceleration
    physics_verlet_integration(&physics_world, 1.0f/60.0f);
    
    mu_assert("Object should wake up when force applied", !verlet->is_sleeping);
    mu_assert("Sleep timer should reset when woken", verlet->sleep_timer == 0);
    
    ecs_cleanup(&ecs);
    arena_cleanup(&physics_world.spatial_arena);
    return 0;
}

// Test mouse force distance calculations
static char* test_mouse_force_distance() {
    // Test distance calculation logic that would be used in mouse interaction
    Vec3 mouse_pos = (Vec3){100.0f, 50.0f, 0.0f};
    Vec3 circle_pos = (Vec3){80.0f, 30.0f, 0.0f};
    
    Vec3 to_mouse = vec3_add(mouse_pos, vec3_multiply(circle_pos, -1.0f));
    float distance = sqrtf(to_mouse.x * to_mouse.x + to_mouse.y * to_mouse.y);
    
    // Expected: sqrt((100-80)^2 + (50-30)^2) = sqrt(400 + 400) = sqrt(800) ≈ 28.28
    mu_assert("Distance calculation should be approximately correct", 
              fabs(distance - 28.28f) < 0.1f);
    
    // Test force direction normalization
    if (distance > 0.1f) {
        Vec3 force_direction = vec3_multiply(to_mouse, 1.0f / distance);
        float magnitude = sqrtf(force_direction.x * force_direction.x + 
                               force_direction.y * force_direction.y);
        mu_assert("Force direction should be normalized", fabs(magnitude - 1.0f) < 0.01f);
    }
    
    return 0;
}

// Test force falloff calculation
static char* test_mouse_force_falloff() {
    float influence_radius = 100.0f;
    float circle_radius = 5.0f;
    
    // Test force at different distances
    float distance1 = 10.0f;  // Close
    float distance2 = 50.0f;  // Medium
    float distance3 = 90.0f;  // Far
    
    // Force factor calculation: 1.0 - (distance / (influence_radius + circle_radius))
    float max_distance = influence_radius + circle_radius;
    
    float factor1 = 1.0f - (distance1 / max_distance);
    float factor2 = 1.0f - (distance2 / max_distance);
    float factor3 = 1.0f - (distance3 / max_distance);
    
    factor1 = factor1 * factor1; // Square for non-linear falloff
    factor2 = factor2 * factor2;
    factor3 = factor3 * factor3;
    
    mu_assert("Close objects should have stronger force", factor1 > factor2);
    mu_assert("Medium objects should have stronger force than far", factor2 > factor3);
    mu_assert("All factors should be positive", factor1 > 0 && factor2 > 0 && factor3 > 0);
    
    return 0;
}

// Test that collision resolution wakes sleeping objects
static char* test_collision_wakes_objects() {
    ECS ecs = {0};
    PhysicsWorld physics_world = {0};
    
    ecs_init(&ecs);
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    physics_world_init(&physics_world, &ecs, transform_type);
    
    Entity entity1 = physics_create_circle(&physics_world, (Vec3){0, 0, 0}, 10.0f, 1.0f);
    Entity entity2 = physics_create_circle(&physics_world, (Vec3){15, 0, 0}, 10.0f, 1.0f);
    
    VerletBody* v1 = (VerletBody*)ecs_get_component(&ecs, entity1, physics_world.verlet_type);
    VerletBody* v2 = (VerletBody*)ecs_get_component(&ecs, entity2, physics_world.verlet_type);
    Transform* t1 = (Transform*)ecs_get_component(&ecs, entity1, transform_type);
    Transform* t2 = (Transform*)ecs_get_component(&ecs, entity2, transform_type);
    CircleCollider* c1 = (CircleCollider*)ecs_get_component(&ecs, entity1, physics_world.collider_type);
    CircleCollider* c2 = (CircleCollider*)ecs_get_component(&ecs, entity2, physics_world.collider_type);
    
    // Put both objects to sleep
    v1->is_sleeping = true;
    v1->sleep_timer = 100;
    v2->is_sleeping = true;
    v2->sleep_timer = 100;
    
    // Simulate collision between them
    Vec3 normal = (Vec3){1.0f, 0.0f, 0.0f};
    float penetration = 5.0f;
    resolve_circle_collision(t1, v1, c1, t2, v2, c2, normal, penetration);
    
    // Both should wake up after collision
    mu_assert("First object should wake up after collision", !v1->is_sleeping);
    mu_assert("Second object should wake up after collision", !v2->is_sleeping);
    mu_assert("First object sleep timer should reset", v1->sleep_timer == 0);
    mu_assert("Second object sleep timer should reset", v2->sleep_timer == 0);
    
    ecs_cleanup(&ecs);
    arena_cleanup(&physics_world.spatial_arena);
    return 0;
}

static char* all_tests() {
    mu_test_suite_start();
    
    mu_run_test(test_mouse_wake_sleeping_objects);
    mu_run_test(test_mouse_force_distance);
    mu_run_test(test_mouse_force_falloff);
    mu_run_test(test_collision_wakes_objects);
    
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    char *result = all_tests();
    mu_test_suite_end(result);
    
    return result != 0;
}