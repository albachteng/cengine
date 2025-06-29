#include <stdio.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include "core/window.h"
#include "core/ecs.h"
#include "core/components.h"
#include "core/renderer.h"
#include "core/input.h"

int main(void) {
    printf("Starting C Engine...\n");
    
    if (!window_init()) {
        printf("Failed to initialize window system\n");
        return -1;
    }
    
    printf("Window system initialized\n");
    
    Window* window = window_create(800, 600, "C Engine");
    if (!window) {
        printf("Failed to create window\n");
        window_terminate();
        return -1;
    }
    
    printf("Window created\n");
    
    ECS ecs = {0};  // ZII pattern
    ecs_init(&ecs);
    
    printf("ECS initialized\n");
    
    Renderer renderer = {0};  // ZII pattern
    if (!renderer_init(&renderer, &ecs)) {
        printf("Failed to initialize renderer\n");
        window_destroy(window);
        window_terminate();
        return -1;
    }
    
    printf("Renderer initialized\n");
    
    Entity triangle_entity = ecs_create_entity(&ecs);
    Transform* triangle_transform = (Transform*)ecs_add_component(&ecs, triangle_entity, renderer.transform_type);
    *triangle_transform = (Transform){0};
    triangle_transform->position = (Vec3){0.0f, 0.0f, 0.0f};
    triangle_transform->scale = vec3_one();
    
    Renderable* triangle_renderable = (Renderable*)ecs_add_component(&ecs, triangle_entity, renderer.renderable_type);
    *triangle_renderable = (Renderable){0};
    triangle_renderable->shape = SHAPE_TRIANGLE;
    triangle_renderable->color = color_red();
    triangle_renderable->visible = true;
    
    Entity quad_entity = ecs_create_entity(&ecs);
    Transform* quad_transform = (Transform*)ecs_add_component(&ecs, quad_entity, renderer.transform_type);
    *quad_transform = (Transform){0};
    quad_transform->position = (Vec3){100.0f, 0.0f, 0.0f};
    quad_transform->scale = vec3_one();
    
    Renderable* quad_renderable = (Renderable*)ecs_add_component(&ecs, quad_entity, renderer.renderable_type);
    *quad_renderable = (Renderable){0};
    quad_renderable->shape = SHAPE_QUAD;
    quad_renderable->color = color_blue();
    quad_renderable->visible = true;
    quad_renderable->data.quad.width = 100.0f;
    quad_renderable->data.quad.height = 80.0f;
    
    InputState input;
    input_init(&input, window->handle);
    
    printf("C Engine initialized successfully - Window should be visible with red triangle and blue quad\n");
    printf("Controls: WASD to move triangle, Arrow keys to move quad, ESC to exit\n");
    
    while (!window_should_close(window)) {
        window_poll_events();
        input_update(&input);
        
        if (input_key_down(&input, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window->handle, GLFW_TRUE);
        }
        
        if (input_key_down(&input, GLFW_KEY_W)) {
            triangle_transform->position.y += 2.0f;
        }
        if (input_key_down(&input, GLFW_KEY_S)) {
            triangle_transform->position.y -= 2.0f;
        }
        if (input_key_down(&input, GLFW_KEY_A)) {
            triangle_transform->position.x -= 2.0f;
        }
        if (input_key_down(&input, GLFW_KEY_D)) {
            triangle_transform->position.x += 2.0f;
        }
        
        if (input_key_down(&input, GLFW_KEY_UP)) {
            quad_transform->position.y += 2.0f;
        }
        if (input_key_down(&input, GLFW_KEY_DOWN)) {
            quad_transform->position.y -= 2.0f;
        }
        if (input_key_down(&input, GLFW_KEY_LEFT)) {
            quad_transform->position.x -= 2.0f;
        }
        if (input_key_down(&input, GLFW_KEY_RIGHT)) {
            quad_transform->position.x += 2.0f;
        }
        
        if (input_mouse_pressed(&input, GLFW_MOUSE_BUTTON_LEFT)) {
            double mx, my;
            input_get_mouse_position(&input, &mx, &my);
            printf("Mouse clicked at: (%.1f, %.1f)\n", mx, my);
        }
        
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        
        renderer_begin_frame(&renderer);
        ecs_update_systems(&ecs, 0.016f);
        renderer_end_frame(&renderer);
        
        window_swap_buffers(window);
    }
    
    input_cleanup(&input);
    renderer_cleanup(&renderer);
    ecs_cleanup(&ecs);
    window_destroy(window);
    window_terminate();
    
    return 0;
}