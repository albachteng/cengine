#include "components.h"
#include "coordinate_system.h"
#include "ecs.h"
#include "input.h"
#include "log.h"
#include "physics.h"
#include "renderer.h"
#include "window.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_CIRCLES 1000 // Reduced for better visibility and performance
#define BOUNDARY_RADIUS WORLD_BOUNDARY_RADIUS // Use shared coordinate system

// Demo configuration constants
#define CIRCLE_RADIUS_MIN 1.0f
#define CIRCLE_RADIUS_MAX 2.0f
#define CIRCLE_MASS_MULTIPLIER 0.1f
#define GRID_SPACING_MULTIPLIER 0.6f // Much tighter grid for better visibility
#define GRID_POSITION_RANDOMNESS 0.3f
#define SPAWN_HEIGHT_OFFSET 30.0f
#define MAX_DELTA_TIME 0.033f
#define MOUSE_INFLUENCE_RADIUS                                                 \
  100.0f // Radius within which mouse affects circles
#define MOUSE_FORCE_STRENGTH                                                   \
  3000.0f // Strength of mouse drag force (increased significantly)
#define IMPULSE_FORCE 500.0f
#define PROGRESS_REPORT_INTERVAL 50 // Less frequent reports for 5K objects

static float random_float(float min, float max) {
  return min + ((float)rand() / RAND_MAX) * (max - min);
}

static Color random_color() {
  return (Color){random_float(0.2f, 1.0f), random_float(0.2f, 1.0f),
                 random_float(0.2f, 1.0f), 1.0f};
}

int main(void) {
  // Initialize logging system with ZII config (uses defaults)
  LogConfig log_cfg = {0};
  log_cfg.min_level = LOG_DEBUG; // More verbose for demo
  log_init(&log_cfg);

  LOG_INFO("Starting physics demo with %d circles", NUM_CIRCLES);
  srand((unsigned int)time(NULL));

  if (!window_init()) {
    LOG_ERROR("Window init failed - likely running in headless environment");
    LOG_INFO(
        "This is expected when running without a display. Exiting gracefully.");
    return 0; // Exit gracefully instead of failing
  }

  Window *window =
      window_create(1200, 900, "C Engine - Physics Demo (5K Scale Test)");
  if (!window) {
    LOG_ERROR("Window create failed - likely running in headless environment");
    LOG_INFO(
        "This is expected when running without a display. Exiting gracefully.");
    window_terminate();
    return 0; // Exit gracefully instead of failing
  }

  ECS ecs = {0}; // ZII
  ecs_init(&ecs);

  Renderer renderer = {0}; // ZII
  if (!renderer_init(&renderer, &ecs)) {
    LOG_ERROR("Failed to initialize renderer");
    window_destroy(window);
    window_terminate();
    return -1;
  }

  PhysicsWorld physics = {0}; // ZII
  physics_world_init(&physics, &ecs, renderer.transform_type);
  physics_set_boundary(&physics, (Vec3){0.0f, 0.0f, 0.0f}, BOUNDARY_RADIUS);

  // Create random circles distributed within the boundary
  for (int i = 0; i < NUM_CIRCLES; i++) {
    // Smaller radius range to reduce overlaps
    float circle_radius = random_float(CIRCLE_RADIUS_MIN, CIRCLE_RADIUS_MAX);
    float mass = circle_radius * circle_radius * CIRCLE_MASS_MULTIPLIER;

    // Spread circles in a grid-like pattern with some randomness
    int circles_per_row = (int)sqrtf((float)NUM_CIRCLES) + 1;
    float grid_spacing =
        (BOUNDARY_RADIUS * GRID_SPACING_MULTIPLIER) / circles_per_row;
    int row = i / circles_per_row;
    int col = i % circles_per_row;

    float base_x = (col - circles_per_row / 2.0f) * grid_spacing;
    float base_y = (row - circles_per_row / 2.0f) * grid_spacing;

    // Add some randomness to the grid positions
    Vec3 pos =
        (Vec3){base_x + random_float(-grid_spacing * GRID_POSITION_RANDOMNESS,
                                     grid_spacing * GRID_POSITION_RANDOMNESS),
               base_y +
                   random_float(-grid_spacing * GRID_POSITION_RANDOMNESS,
                                grid_spacing * GRID_POSITION_RANDOMNESS) +
                   SPAWN_HEIGHT_OFFSET, // Start higher up
               0.0f};

    Entity circle = physics_create_circle(&physics, pos, circle_radius, mass);
    Renderable *renderable =
        (Renderable *)ecs_add_component(&ecs, circle, renderer.renderable_type);
    // ZII: Create circle renderable directly
    *renderable = (Renderable){0};
    renderable->shape = SHAPE_CIRCLE;
    renderable->color = random_color();
    renderable->visible = true;
    renderable->data.circle.radius = circle_radius;

    // Progress feedback
    if (i % PROGRESS_REPORT_INTERVAL == 0) {
      printf("Created %d circles...\n", i + 1);
    }
  }

  // Verify components were added correctly (check first few entities)
  printf("Verifying component setup for first 3 entities:\n");
  for (Entity entity = 1; entity <= 3 && entity < ecs.next_entity_id;
       entity++) {
    printf("Entity %d:\n", entity);
    printf("  Transform: %d, Renderable: %d, Verlet: %d, Collider: %d\n",
           ecs_has_component(&ecs, entity, renderer.transform_type),
           ecs_has_component(&ecs, entity, renderer.renderable_type),
           ecs_has_component(&ecs, entity, physics.verlet_type),
           ecs_has_component(&ecs, entity, physics.collider_type));
  }

  InputState input = {0}; // ZII
  input_init(&input, window->handle);

  LOG_INFO("Physics Demo Initialized!");
  LOG_INFO("- %d circles with gravity and collision", NUM_CIRCLES);
  LOG_INFO("- Constrained within spherical boundary");
  LOG_INFO("- ESC to exit, Space to apply upward force");
  LOG_INFO("- Left mouse button to drag circles within %d pixel radius",
           (int)MOUSE_INFLUENCE_RADIUS);

  double last_time = glfwGetTime();

  while (!window_should_close(window)) {
    window_poll_events();
    input_update(&input);

    double current_time = glfwGetTime();
    float delta_time = (float)(current_time - last_time);
    last_time = current_time;

    if (delta_time > MAX_DELTA_TIME)
      delta_time = MAX_DELTA_TIME;

    // Log frame performance
    log_frame_time(delta_time);

    if (input_key_down(&input, GLFW_KEY_ESCAPE)) {
      glfwSetWindowShouldClose(window->handle, GLFW_TRUE);
    }

    if (input_key_pressed(&input, GLFW_KEY_SPACE)) {
      for (Entity entity = 1; entity < ecs.next_entity_id; entity++) {
        if (!ecs_entity_active(&ecs, entity))
          continue;

        if (ecs_has_component(&ecs, entity, physics.verlet_type)) {
          VerletBody *verlet = (VerletBody *)ecs_get_component(
              &ecs, entity, physics.verlet_type);
          verlet->acceleration =
              vec3_add(verlet->acceleration, (Vec3){0.0f, IMPULSE_FORCE, 0.0f});
        }
      }
    }

    // Mouse drag interaction
    if (input_mouse_down(&input, GLFW_MOUSE_BUTTON_LEFT)) {
      double mouse_x, mouse_y;
      input_get_mouse_position(&input, &mouse_x, &mouse_y);

      // Convert screen coordinates to world coordinates
      // Scale to match physics world size (boundary radius = 100 units)
      // Map screen coordinates to world coordinates proportionally
      float screen_radius =
          fminf(window->width, window->height) / 2.0f; // Use smaller dimension
      float world_radius =
          BOUNDARY_RADIUS * 1.2f; // Slightly larger than physics boundary
      float scale_factor = world_radius / screen_radius;

      float world_x = ((float)mouse_x - (window->width / 2.0f)) * scale_factor;
      float world_y = ((window->height / 2.0f) - (float)mouse_y) *
                      scale_factor; // Flip Y axis

      Vec3 mouse_pos = (Vec3){world_x, world_y, 0.0f};

      // Apply force to circles within influence radius
      for (Entity entity = 1; entity < ecs.next_entity_id; entity++) {
        if (!ecs_entity_active(&ecs, entity))
          continue;

        if (!ecs_has_component(&ecs, entity, physics.transform_type) ||
            !ecs_has_component(&ecs, entity, physics.verlet_type) ||
            !ecs_has_component(&ecs, entity, physics.collider_type))
          continue;

        Transform *transform = (Transform *)ecs_get_component(
            &ecs, entity, physics.transform_type);
        VerletBody *verlet =
            (VerletBody *)ecs_get_component(&ecs, entity, physics.verlet_type);
        CircleCollider *collider = (CircleCollider *)ecs_get_component(
            &ecs, entity, physics.collider_type);

        // Calculate distance from mouse to circle center
        Vec3 to_mouse =
            vec3_add(mouse_pos, vec3_multiply(transform->position, -1.0f));
        float distance =
            sqrtf(to_mouse.x * to_mouse.x + to_mouse.y * to_mouse.y);

        // Check if within influence radius (including circle radius for edge
        // detection)
        if (distance <= MOUSE_INFLUENCE_RADIUS + collider->radius) {
          // Wake up the object if it's sleeping
          if (verlet->is_sleeping) {
            verlet->is_sleeping = false;
            verlet->sleep_timer = 0;
          }

          // Calculate force strength based on distance (closer = stronger)
          float force_factor =
              1.0f - (distance / (MOUSE_INFLUENCE_RADIUS + collider->radius));
          force_factor =
              force_factor * force_factor; // Square for non-linear falloff

          // Apply force toward mouse position
          if (distance > 0.1f) { // Avoid division by zero
            Vec3 force_direction =
                vec3_multiply(to_mouse, 1.0f / distance); // Normalize

            // Extra strong force for very close objects (within 30 units)
            float final_force_strength = MOUSE_FORCE_STRENGTH * force_factor;
            if (distance < 30.0f) {
              final_force_strength *= 2.0f; // Double force for close objects
            }

            Vec3 mouse_force =
                vec3_multiply(force_direction, final_force_strength);
            verlet->acceleration = vec3_add(verlet->acceleration, mouse_force);

            // For close objects, also counteract gravity more strongly
            if (distance < 50.0f) {
              verlet->acceleration =
                  vec3_add(verlet->acceleration,
                           (Vec3){0.0f, 200.0f * force_factor, 0.0f});
            }
          }
        }
      }
    }

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

    renderer_begin_frame(&renderer);
    ecs_update_systems(&ecs, delta_time);

    // Render boundary circle for visual reference
    glColor4f(0.5f, 0.5f, 0.5f, 0.3f); // Semi-transparent gray
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 64; i++) {
      float angle = 2.0f * M_PI * i / 64.0f;
      float x = cosf(angle) * BOUNDARY_RADIUS / RENDER_SCALE_FACTOR;
      float y = sinf(angle) * BOUNDARY_RADIUS / RENDER_SCALE_FACTOR;
      glVertex2f(x, y);
    }
    glEnd();

    renderer_end_frame(&renderer);

    window_swap_buffers(window);
  }

  LOG_INFO("Shutting down physics demo");
  physics_world_cleanup(&physics);
  input_cleanup(&input);
  renderer_cleanup(&renderer);
  ecs_cleanup(&ecs);
  window_destroy(window);
  window_terminate();
  log_cleanup();

  return 0;
}
