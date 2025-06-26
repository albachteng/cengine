#include "components.h"
#include "ecs.h"
#include "input.h"
#include "physics.h"
#include "renderer.h"
#include "window.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_CIRCLES 20
#define BOUNDARY_RADIUS 100.0f

float random_float(float min, float max) {
  return min + ((float)rand() / RAND_MAX) * (max - min);
}

Color random_color() {
  return color_create(random_float(0.2f, 1.0f), random_float(0.2f, 1.0f),
                      random_float(0.2f, 1.0f), 1.0f);
}

int main(void) {
  srand((unsigned int)time(NULL));

  if (!window_init()) {
    return -1;
  }

  Window *window = window_create(800, 600, "C Engine - Physics Demo");
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

  PhysicsWorld physics;
  physics_world_init(&physics, &ecs, renderer.transform_type);
  physics_set_boundary(&physics,
                       vec3_create(0.0f, 0.0f, 0.0f), // Q: why repeat this?
                       BOUNDARY_RADIUS);

  // Create random circles distributed within the boundary
  printf("Creating %d random circles...\n", NUM_CIRCLES);
  for (int i = 0; i < NUM_CIRCLES; i++) {
    // Smaller radius range to reduce overlaps
    float circle_radius = random_float(2.0f, 5.0f);
    float mass = circle_radius * circle_radius * 0.1f;
    
    // Spread circles in a grid-like pattern with some randomness
    int circles_per_row = (int)sqrtf((float)NUM_CIRCLES) + 1;
    float grid_spacing = (BOUNDARY_RADIUS * 1.5f) / circles_per_row;
    int row = i / circles_per_row;
    int col = i % circles_per_row;
    
    float base_x = (col - circles_per_row / 2.0f) * grid_spacing;
    float base_y = (row - circles_per_row / 2.0f) * grid_spacing;
    
    // Add some randomness to the grid positions
    Vec3 pos = vec3_create(
        base_x + random_float(-grid_spacing * 0.3f, grid_spacing * 0.3f),
        base_y + random_float(-grid_spacing * 0.3f, grid_spacing * 0.3f) + 30.0f, // Start higher up
        0.0f
    );
    
    Entity circle = physics_create_circle(&physics, pos, circle_radius, mass);
    Renderable *renderable = (Renderable *)ecs_add_component(&ecs, circle, renderer.renderable_type);
    *renderable = renderable_create_circle(circle_radius, random_color());
    
    // Progress feedback
    if (i % 5 == 0) {
      printf("Created %d circles...\n", i + 1);
    }
  }

  // Verify components were added correctly (check first few entities)
  printf("Verifying component setup for first 3 entities:\n");
  for (Entity entity = 1; entity <= 3 && entity < ecs.next_entity_id; entity++) {
    printf("Entity %d:\n", entity);
    printf("  Transform: %d, Renderable: %d, Verlet: %d, Collider: %d\n",
           ecs_has_component(&ecs, entity, renderer.transform_type),
           ecs_has_component(&ecs, entity, renderer.renderable_type),
           ecs_has_component(&ecs, entity, physics.verlet_type),
           ecs_has_component(&ecs, entity, physics.collider_type));
  }

  InputState input;
  input_init(&input, window->handle);

  printf("Physics Demo Initialized!\n");
  printf("- %d circles with gravity and collision\n", NUM_CIRCLES);
  printf("- Constrained within spherical boundary\n");
  printf("- ESC to exit, Space to apply upward force\n");

  double last_time = glfwGetTime();

  while (!window_should_close(window)) {
    window_poll_events();
    input_update(&input);

    double current_time = glfwGetTime();
    float delta_time = (float)(current_time - last_time);
    last_time = current_time;

    if (delta_time > 0.033f)
      delta_time = 0.033f;

    if (input_key_down(&input, GLFW_KEY_ESCAPE)) {
      glfwSetWindowShouldClose(window->handle, GLFW_TRUE);
    }

    if (input_key_pressed(&input, GLFW_KEY_SPACE)) {
      for (Entity entity = 1; entity < ecs.next_entity_id; entity++) {
        if (!ecs_entity_active(&ecs, entity))
          continue;

        if (ecs_has_component(&ecs, entity, physics.verlet_type)) {
          printf("has physics.verlet_type");
          VerletBody *verlet = (VerletBody *)ecs_get_component(
              &ecs, entity, physics.verlet_type);
          verlet->acceleration =
              vec3_add(verlet->acceleration, vec3_create(0.0f, 500.0f, 0.0f));
        }
      }
    }

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

    renderer_begin_frame(&renderer);
    ecs_update_systems(&ecs, delta_time);
    renderer_end_frame(&renderer);

    window_swap_buffers(window);
  }

  physics_world_cleanup(&physics);
  input_cleanup(&input);
  renderer_cleanup(&renderer);
  ecs_cleanup(&ecs);
  window_destroy(window);
  window_terminate();

  return 0;
}
