#include "../minunit.h"
#include "../../include/physics.h"
#include "../../include/ecs.h"
#include "../../include/components.h"
#include <math.h>

int tests_run = 0;

// Test that objects start awake
static char* test_objects_start_awake() {
    ECS ecs = {0};
    PhysicsWorld physics_world = {0};
    
    ecs_init(&ecs);
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    physics_world_init(&physics_world, &ecs, transform_type);
    
    Entity entity = physics_create_circle(&physics_world, (Vec3){0, 0, 0}, 10.0f, 1.0f);
    VerletBody* verlet = (VerletBody*)ecs_get_component(&ecs, entity, physics_world.verlet_type);
    
    mu_assert("Object should start awake", !verlet->is_sleeping);
    mu_assert("Sleep timer should start at 0", verlet->sleep_timer == 0);
    
    ecs_cleanup(&ecs);
    arena_cleanup(&physics_world.spatial_arena);
    return 0;
}

// Test sleep transition for stationary object
static char* test_sleep_transition() {
    ECS ecs = {0};
    PhysicsWorld physics_world = {0};
    
    ecs_init(&ecs);
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    physics_world_init(&physics_world, &ecs, transform_type);
    
    Entity entity = physics_create_circle(&physics_world, (Vec3){0, 0, 0}, 10.0f, 1.0f);
    VerletBody* verlet = (VerletBody*)ecs_get_component(&ecs, entity, physics_world.verlet_type);
    Transform* transform = (Transform*)ecs_get_component(&ecs, entity, transform_type);
    
    // Set positions for very small movement (below sleep threshold)
    // Physics integration calculates velocity from position difference
    // For sleep threshold 1.0f, need velocity < 1.0, so position_diff < 1.0 * (1/60) = 0.0167
    transform->position = (Vec3){0.0f, 0.0f, 0.0f};
    verlet->old_position = (Vec3){-0.01f, 0.0f, 0.0f}; // Small movement = velocity 0.6 (below 1.0 threshold)
    verlet->acceleration = (Vec3){0.0f, 0.0f, 0.0f}; // No external forces
    
    // Simulate frames below threshold
    float delta_time = 1.0f/60.0f;
    for (int i = 0; i < PHYSICS_SLEEP_TIME_THRESHOLD - 1; i++) {
        // Keep setting small movement to maintain low velocity
        Vec3 current_pos = transform->position;
        verlet->old_position = (Vec3){current_pos.x - 0.01f, current_pos.y, current_pos.z};
        verlet->acceleration = (Vec3){0.0f, 0.0f, 0.0f}; // Override gravity for test
        
        physics_verlet_integration(&physics_world, delta_time);
        mu_assert("Should not be sleeping yet", !verlet->is_sleeping);
    }
    
    // One more frame should trigger sleep
    Vec3 current_pos = transform->position;
    verlet->old_position = (Vec3){current_pos.x - 0.03f, current_pos.y, current_pos.z};
    verlet->acceleration = (Vec3){0.0f, 0.0f, 0.0f};
    physics_verlet_integration(&physics_world, delta_time);
    mu_assert("Should be sleeping after threshold", verlet->is_sleeping);
    
    ecs_cleanup(&ecs);
    arena_cleanup(&physics_world.spatial_arena);
    return 0;
}

// Test wake up from collision
static char* test_wake_up_from_collision() {
    ECS ecs = {0};
    PhysicsWorld physics_world = {0};
    
    ecs_init(&ecs);
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    physics_world_init(&physics_world, &ecs, transform_type);
    
    Entity entity1 = physics_create_circle(&physics_world, (Vec3){0, 0, 0}, 10.0f, 1.0f);
    Entity entity2 = physics_create_circle(&physics_world, (Vec3){5, 0, 0}, 10.0f, 1.0f);
    
    VerletBody* v1 = (VerletBody*)ecs_get_component(&ecs, entity1, physics_world.verlet_type);
    VerletBody* v2 = (VerletBody*)ecs_get_component(&ecs, entity2, physics_world.verlet_type);
    Transform* t1 = (Transform*)ecs_get_component(&ecs, entity1, transform_type);
    Transform* t2 = (Transform*)ecs_get_component(&ecs, entity2, transform_type);
    CircleCollider* c1 = (CircleCollider*)ecs_get_component(&ecs, entity1, physics_world.collider_type);
    CircleCollider* c2 = (CircleCollider*)ecs_get_component(&ecs, entity2, physics_world.collider_type);
    
    // Put first object to sleep
    v1->is_sleeping = true;
    v1->sleep_timer = PHYSICS_SLEEP_TIME_THRESHOLD;
    
    // Simulate collision
    Vec3 normal = (Vec3){1.0f, 0.0f, 0.0f};
    float penetration = 5.0f;
    resolve_circle_collision(t1, v1, c1, t2, v2, c2, normal, penetration);
    
    // Should wake up after collision
    mu_assert("Should wake up after collision", !v1->is_sleeping);
    mu_assert("Sleep timer should reset", v1->sleep_timer == 0);
    
    ecs_cleanup(&ecs);
    arena_cleanup(&physics_world.spatial_arena);
    return 0;
}

// Test velocity threshold accuracy
static char* test_velocity_threshold() {
    ECS ecs = {0};
    PhysicsWorld physics_world = {0};
    
    ecs_init(&ecs);
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    physics_world_init(&physics_world, &ecs, transform_type);
    
    Entity entity = physics_create_circle(&physics_world, (Vec3){0, 0, 0}, 10.0f, 1.0f);
    VerletBody* verlet = (VerletBody*)ecs_get_component(&ecs, entity, physics_world.verlet_type);
    Transform* transform = (Transform*)ecs_get_component(&ecs, entity, transform_type);
    
    float delta_time = 1.0f/60.0f;
    
    // Set position difference for velocity above threshold
    // velocity = position_diff / delta_time, so we need position_diff > threshold * delta_time
    // For velocity > 1.0, need position_diff > 1.0 * (1/60) = 0.0167
    transform->position = (Vec3){0.0f, 0.0f, 0.0f};
    verlet->old_position = (Vec3){-0.025f, 0.0f, 0.0f}; // Creates velocity = 1.5 (above 1.0 threshold)
    verlet->acceleration = (Vec3){0.0f, 0.0f, 0.0f}; // No external forces
    
    physics_verlet_integration(&physics_world, delta_time);
    mu_assert("Timer should reset for fast movement", verlet->sleep_timer == 0);
    
    // Set position difference for velocity below threshold
    Vec3 current_pos = transform->position;
    verlet->old_position = (Vec3){current_pos.x - 0.01f, current_pos.y, current_pos.z}; // Creates velocity = 0.6 (below 1.0 threshold)
    verlet->acceleration = (Vec3){0.0f, 0.0f, 0.0f};
    
    physics_verlet_integration(&physics_world, delta_time);
    mu_assert("Timer should increment for slow movement", verlet->sleep_timer == 1);
    
    ecs_cleanup(&ecs);
    arena_cleanup(&physics_world.spatial_arena);
    return 0;
}

// Test sleeping objects skip physics integration
static char* test_sleeping_objects_skip_integration() {
    ECS ecs = {0};
    PhysicsWorld physics_world = {0};
    
    ecs_init(&ecs);
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    physics_world_init(&physics_world, &ecs, transform_type);
    
    Entity entity = physics_create_circle(&physics_world, (Vec3){0, 100, 0}, 10.0f, 1.0f);
    VerletBody* verlet = (VerletBody*)ecs_get_component(&ecs, entity, physics_world.verlet_type);
    Transform* transform = (Transform*)ecs_get_component(&ecs, entity, transform_type);
    
    // Put object to sleep at height
    verlet->is_sleeping = true;
    Vec3 initial_position = transform->position;
    
    // Run integration - should not move due to gravity when sleeping
    float delta_time = 1.0f/60.0f;
    physics_verlet_integration(&physics_world, delta_time);
    
    mu_assert("X position should not change when sleeping", transform->position.x == initial_position.x);
    mu_assert("Y position should not change when sleeping", transform->position.y == initial_position.y);
    mu_assert("Z position should not change when sleeping", transform->position.z == initial_position.z);
    
    ecs_cleanup(&ecs);
    arena_cleanup(&physics_world.spatial_arena);
    return 0;
}

static char* all_tests() {
    mu_test_suite_start();
    
    mu_run_test(test_objects_start_awake);
    mu_run_test(test_sleep_transition);
    mu_run_test(test_wake_up_from_collision);
    mu_run_test(test_velocity_threshold);
    mu_run_test(test_sleeping_objects_skip_integration);
    
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    char *result = all_tests();
    mu_test_suite_end(result);
    
    return result != 0;
}