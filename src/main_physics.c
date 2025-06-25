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

#define NUM_CIRCLES 2
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

  // Test with 2 circles very close together
  float circle_radius = 5.0f;
  float mass = circle_radius * circle_radius * 0.1f;

  // Circle 1: Higher up
  Vec3 pos1 = vec3_create(0.0f, 20.0f, 0.0f); // Closer together
  Entity circle1 = physics_create_circle(&physics, pos1, circle_radius, mass);
  Renderable *r1 =
      (Renderable *)ecs_add_component(&ecs, circle1, renderer.renderable_type);
  *r1 = renderable_create_circle(circle_radius,
                                 color_create(1.0f, 0.0f, 0.0f, 1.0f));

  // Circle 2: Lower
  Vec3 pos2 = vec3_create(0.0f, 0.0f, 0.0f); // Closer together
  Entity circle2 = physics_create_circle(&physics, pos2, circle_radius, mass);
  Renderable *r2 =
      (Renderable *)ecs_add_component(&ecs, circle2, renderer.renderable_type);
  *r2 = renderable_create_circle(circle_radius,
                                 color_create(0.0f, 1.0f, 0.0f, 1.0f));

  // Verify components were added correctly
  printf("Checking components for circle1 (entity %d):\n", circle1);
  printf("  Has transform: %d (type %d)\n",
         ecs_has_component(&ecs, circle1, renderer.transform_type),
         renderer.transform_type);
  printf("  Has renderable: %d (type %d)\n",
         ecs_has_component(&ecs, circle1, renderer.renderable_type),
         renderer.renderable_type);
  printf("  Has verlet: %d (type %d)\n",
         ecs_has_component(&ecs, circle1, physics.verlet_type),
         physics.verlet_type);
  printf("  Has collider: %d (type %d)\n",
         ecs_has_component(&ecs, circle1, physics.collider_type),
         physics.collider_type);
  printf("  Entity mask: %llu\n", ecs.entities[circle1].mask);

  printf("Checking components for circle2 (entity %d):\n", circle2);
  printf("  Has transform: %d (type %d)\n",
         ecs_has_component(&ecs, circle2, renderer.transform_type),
         renderer.transform_type);
  printf("  Has renderable: %d (type %d)\n",
         ecs_has_component(&ecs, circle2, renderer.renderable_type),
         renderer.renderable_type);
  printf("  Has verlet: %d (type %d)\n",
         ecs_has_component(&ecs, circle2, physics.verlet_type),
         physics.verlet_type);
  printf("  Has collider: %d (type %d)\n",
         ecs_has_component(&ecs, circle2, physics.collider_type),
         physics.collider_type);

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
