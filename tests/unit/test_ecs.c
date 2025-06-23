#include "../minunit.h"
#include "../../include/ecs.h"

int tests_run = 0;

static char* test_ecs_init() {
    ECS ecs;
    ecs_init(&ecs);
    
    mu_assert("ECS should start with entity ID 1", ecs.next_entity_id == 1);
    mu_assert("ECS should have 0 components", ecs.component_count == 0);
    mu_assert("ECS should have 0 systems", ecs.system_count == 0);
    
    ecs_cleanup(&ecs);
    return 0;
}

static char* test_entity_creation() {
    ECS ecs;
    ecs_init(&ecs);
    
    Entity entity1 = ecs_create_entity(&ecs);
    Entity entity2 = ecs_create_entity(&ecs);
    
    mu_assert("First entity should have ID 1", entity1 == 1);
    mu_assert("Second entity should have ID 2", entity2 == 2);
    mu_assert("Entity 1 should be active", ecs_entity_active(&ecs, entity1));
    mu_assert("Entity 2 should be active", ecs_entity_active(&ecs, entity2));
    
    ecs_cleanup(&ecs);
    return 0;
}

static char* test_entity_destroy() {
    ECS ecs;
    ecs_init(&ecs);
    
    Entity entity = ecs_create_entity(&ecs);
    mu_assert("Entity should be active", ecs_entity_active(&ecs, entity));
    
    ecs_destroy_entity(&ecs, entity);
    mu_assert("Entity should be inactive", !ecs_entity_active(&ecs, entity));
    
    ecs_cleanup(&ecs);
    return 0;
}

static char* test_component_registration() {
    ECS ecs;
    ecs_init(&ecs);
    
    ComponentType pos_type = ecs_register_component(&ecs, sizeof(float) * 3);
    ComponentType vel_type = ecs_register_component(&ecs, sizeof(float) * 3);
    
    mu_assert("First component type should be 0", pos_type == 0);
    mu_assert("Second component type should be 1", vel_type == 1);
    mu_assert("ECS should have 2 components", ecs.component_count == 2);
    
    ecs_cleanup(&ecs);
    return 0;
}

typedef struct {
    float x, y, z;
} Position;

static char* test_component_operations() {
    ECS ecs;
    ecs_init(&ecs);
    
    Entity entity = ecs_create_entity(&ecs);
    ComponentType pos_type = ecs_register_component(&ecs, sizeof(Position));
    
    mu_assert("Entity should not have position component", 
              !ecs_has_component(&ecs, entity, pos_type));
    
    Position* pos = (Position*)ecs_add_component(&ecs, entity, pos_type);
    mu_assert("Position component should be added", pos != NULL);
    mu_assert("Entity should have position component", 
              ecs_has_component(&ecs, entity, pos_type));
    
    pos->x = 10.0f;
    pos->y = 20.0f;
    pos->z = 30.0f;
    
    Position* retrieved = (Position*)ecs_get_component(&ecs, entity, pos_type);
    mu_assert("Retrieved position should not be NULL", retrieved != NULL);
    mu_assert("Position X should be 10.0f", retrieved->x == 10.0f);
    mu_assert("Position Y should be 20.0f", retrieved->y == 20.0f);
    mu_assert("Position Z should be 30.0f", retrieved->z == 30.0f);
    
    ecs_remove_component(&ecs, entity, pos_type);
    mu_assert("Entity should not have position component after removal", 
              !ecs_has_component(&ecs, entity, pos_type));
    
    ecs_cleanup(&ecs);
    return 0;
}

static void test_system_func(float delta_time) {
    (void)delta_time;
}

static char* test_system_registration() {
    ECS ecs;
    ecs_init(&ecs);
    
    ComponentMask required = (1ULL << 0) | (1ULL << 1);
    ecs_register_system(&ecs, test_system_func, required);
    
    mu_assert("ECS should have 1 system", ecs.system_count == 1);
    mu_assert("System should be active", ecs.systems[0].active);
    mu_assert("System should have correct required components", 
              ecs.systems[0].required_components == required);
    
    ecs_cleanup(&ecs);
    return 0;
}

static char* all_tests() {
    mu_run_test(test_ecs_init);
    mu_run_test(test_entity_creation);
    mu_run_test(test_entity_destroy);
    mu_run_test(test_component_registration);
    mu_run_test(test_component_operations);
    mu_run_test(test_system_registration);
    return 0;
}

int main() {
    mu_test_suite_start();
    char* result = all_tests();
    mu_test_suite_end(result);
}