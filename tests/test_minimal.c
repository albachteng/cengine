#include "ecs.h"
#include "physics.h"
#include "components.h"
#include <stdio.h>

int main() {
    printf("Starting minimal test...\n");
    
    ECS ecs = {0};  // ZII
    printf("ECS declared\n");
    
    ecs_init(&ecs);
    printf("ECS initialized\n");
    
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    printf("Transform component registered: %d\n", transform_type);
    
    Entity entity = ecs_create_entity(&ecs);
    printf("Entity created: %d\n", entity);
    
    Transform* transform = (Transform*)ecs_add_component(&ecs, entity, transform_type);
    if (transform) {
        printf("Transform component added successfully\n");
        transform->position = vec3_create(1.0f, 2.0f, 3.0f);
        printf("Transform position set to (%.1f, %.1f, %.1f)\n", 
               transform->position.x, transform->position.y, transform->position.z);
    } else {
        printf("Failed to add transform component\n");
        return -1;
    }
    
    printf("Cleaning up...\n");
    ecs_cleanup(&ecs);
    
    printf("Minimal test completed successfully!\n");
    return 0;
}