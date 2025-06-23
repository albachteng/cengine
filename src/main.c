#include <stdio.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include "window.h"
#include "ecs.h"
#include "components.h"
#include "renderer.h"
#include "input.h"

int main(void) {
    if (!window_init()) {
        return -1;
    }
    
    Window* window = window_create(800, 600, "C Engine");
    if (!window) {
        window_terminate();
        return -1;
    }
    
    ECS ecs;
    ecs_init(&ecs);
    
    Renderer renderer;
    if (!renderer_init(&renderer, &ecs)) {
        printf("Failed to initialize renderer\n");
        window_destroy(window);
        window_terminate();
        return -1;
    }
    
    Entity triangle_entity = ecs_create_entity(&ecs);
    Transform* triangle_transform = (Transform*)ecs_add_component(&ecs, triangle_entity, renderer.transform_type);
    *triangle_transform = transform_create(0.0f, 0.0f, 0.0f);
    
    Renderable* triangle_renderable = (Renderable*)ecs_add_component(&ecs, triangle_entity, renderer.renderable_type);
    *triangle_renderable = renderable_create_triangle(color_red());
    
    Entity quad_entity = ecs_create_entity(&ecs);
    Transform* quad_transform = (Transform*)ecs_add_component(&ecs, quad_entity, renderer.transform_type);
    *quad_transform = transform_create(100.0f, 0.0f, 0.0f);
    
    Renderable* quad_renderable = (Renderable*)ecs_add_component(&ecs, quad_entity, renderer.renderable_type);
    *quad_renderable = renderable_create_quad(100.0f, 80.0f, color_blue());
    
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