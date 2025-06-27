#include "../minunit.h"
#include "../../include/components.h"
#include "../../include/ecs.h"

int tests_run = 0;

static char* test_transform_creation() {
    Transform t = {0};
    t.position = (Vec3){10.0f, 20.0f, 30.0f};
    t.scale = vec3_one();
    
    mu_assert("Transform position X should be 10.0f", t.position.x == 10.0f);
    mu_assert("Transform position Y should be 20.0f", t.position.y == 20.0f);
    mu_assert("Transform position Z should be 30.0f", t.position.z == 30.0f);
    mu_assert("Transform scale should be 1.0f", 
              t.scale.x == 1.0f && t.scale.y == 1.0f && t.scale.z == 1.0f);
    
    return 0;
}

static char* test_transform_operations() {
    Transform t = {0};
    t.scale = vec3_one();
    
    transform_translate(&t, (Vec3){5.0f, 10.0f, 15.0f});
    mu_assert("Position after translation", 
              t.position.x == 5.0f && t.position.y == 10.0f && t.position.z == 15.0f);
    
    transform_scale_uniform(&t, 2.0f);
    mu_assert("Scale after uniform scaling", 
              t.scale.x == 2.0f && t.scale.y == 2.0f && t.scale.z == 2.0f);
    
    return 0;
}

static char* test_vec3_operations() {
    Vec3 a = {1.0f, 2.0f, 3.0f};
    Vec3 b = {4.0f, 5.0f, 6.0f};
    Vec3 result = vec3_add(a, b);
    
    mu_assert("Vec3 addition X", result.x == 5.0f);
    mu_assert("Vec3 addition Y", result.y == 7.0f);
    mu_assert("Vec3 addition Z", result.z == 9.0f);
    
    Vec3 scaled = vec3_multiply(a, 3.0f);
    mu_assert("Vec3 multiply X", scaled.x == 3.0f);
    mu_assert("Vec3 multiply Y", scaled.y == 6.0f);
    mu_assert("Vec3 multiply Z", scaled.z == 9.0f);
    
    return 0;
}

static char* test_color_creation() {
    Color red = color_red();
    mu_assert("Red color R component", red.r == 1.0f);
    mu_assert("Red color G component", red.g == 0.0f);
    mu_assert("Red color B component", red.b == 0.0f);
    mu_assert("Red color A component", red.a == 1.0f);
    
    Color custom = {0.5f, 0.6f, 0.7f, 0.8f};
    mu_assert("Custom color R", custom.r == 0.5f);
    mu_assert("Custom color G", custom.g == 0.6f);
    mu_assert("Custom color B", custom.b == 0.7f);
    mu_assert("Custom color A", custom.a == 0.8f);
    
    return 0;
}

static char* test_renderable_creation() {
    Renderable triangle = {0};
    triangle.shape = SHAPE_TRIANGLE;
    triangle.color = color_red();
    triangle.visible = true;
    mu_assert("Triangle shape type", triangle.shape == SHAPE_TRIANGLE);
    mu_assert("Triangle visible", triangle.visible == true);
    mu_assert("Triangle layer", triangle.layer == 0);
    
    Renderable quad = {0};
    quad.shape = SHAPE_QUAD;
    quad.color = color_blue();
    quad.visible = true;
    quad.data.quad.width = 100.0f;
    quad.data.quad.height = 50.0f;
    mu_assert("Quad shape type", quad.shape == SHAPE_QUAD);
    mu_assert("Quad width", quad.data.quad.width == 100.0f);
    mu_assert("Quad height", quad.data.quad.height == 50.0f);
    
    Renderable circle = {0};
    circle.shape = SHAPE_CIRCLE;
    circle.color = color_green();
    circle.visible = true;
    circle.data.circle.radius = 25.0f;
    mu_assert("Circle shape type", circle.shape == SHAPE_CIRCLE);
    mu_assert("Circle radius", circle.data.circle.radius == 25.0f);
    
    return 0;
}

static char* test_ecs_component_integration() {
    ECS ecs;
    ecs_init(&ecs);
    
    ComponentType transform_type = ecs_register_component(&ecs, sizeof(Transform));
    ComponentType renderable_type = ecs_register_component(&ecs, sizeof(Renderable));
    
    Entity entity = ecs_create_entity(&ecs);
    
    Transform* transform = (Transform*)ecs_add_component(&ecs, entity, transform_type);
    *transform = (Transform){0};
    transform->position = (Vec3){10.0f, 20.0f, 30.0f};
    transform->scale = vec3_one();
    
    Renderable* renderable = (Renderable*)ecs_add_component(&ecs, entity, renderable_type);
    *renderable = (Renderable){0};
    renderable->shape = SHAPE_QUAD;
    renderable->color = color_red();
    renderable->visible = true;
    renderable->data.quad.width = 100.0f;
    renderable->data.quad.height = 50.0f;
    
    mu_assert("Entity has transform component", 
              ecs_has_component(&ecs, entity, transform_type));
    mu_assert("Entity has renderable component", 
              ecs_has_component(&ecs, entity, renderable_type));
    
    Transform* retrieved_transform = (Transform*)ecs_get_component(&ecs, entity, transform_type);
    mu_assert("Retrieved transform position X", retrieved_transform->position.x == 10.0f);
    
    Renderable* retrieved_renderable = (Renderable*)ecs_get_component(&ecs, entity, renderable_type);
    mu_assert("Retrieved renderable is quad", retrieved_renderable->shape == SHAPE_QUAD);
    mu_assert("Retrieved renderable width", retrieved_renderable->data.quad.width == 100.0f);
    
    ecs_cleanup(&ecs);
    return 0;
}

static char* all_tests() {
    mu_run_test(test_transform_creation);
    mu_run_test(test_transform_operations);
    mu_run_test(test_vec3_operations);
    mu_run_test(test_color_creation);
    mu_run_test(test_renderable_creation);
    mu_run_test(test_ecs_component_integration);
    return 0;
}

int main() {
    mu_test_suite_start();
    char* result = all_tests();
    mu_test_suite_end(result);
}