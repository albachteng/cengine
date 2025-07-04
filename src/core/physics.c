#include "physics.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PhysicsWorld *g_physics_world = NULL;

void physics_world_init(PhysicsWorld *world, ECS *ecs,
                        ComponentType transform_type) {
  world->ecs = ecs;
  world->transform_type = transform_type;
  world->verlet_type = ecs_register_component(ecs, sizeof(VerletBody));
  world->collider_type = ecs_register_component(ecs, sizeof(CircleCollider));

  world->gravity = (Vec3){0.0f, -200.0f, 0.0f};
  world->damping = PHYSICS_DEFAULT_DAMPING;
  world->collision_iterations = PHYSICS_DEFAULT_COLLISION_ITERATIONS;

  world->boundary_center = (Vec3){0};
  world->boundary_radius = PHYSICS_DEFAULT_BOUNDARY_RADIUS;

  // Initialize arena for spatial grid allocations
  // 16MB initial: ~2M EntityNodes (8 bytes each) for spatial grid insertions -
  // should handle 5000+ entities across multiple cells
  if (!arena_init(&world->spatial_arena, 16 * 1024 * 1024)) {
    printf("Failed to initialize spatial arena\n");
    return;
  }

  // Initialize spatial grid with appropriate cell size
  float cell_size = PHYSICS_SPATIAL_CELL_SIZE;
  float grid_size = world->boundary_radius * 2.2f;
  Vec3 grid_origin = (Vec3){-grid_size / 2.0f, -grid_size / 2.0f, 0.0f};
  spatial_grid_init(&world->spatial_grid, &world->spatial_arena, grid_origin,
                    grid_size, grid_size, cell_size);

  g_physics_world = world;

  ComponentMask physics_mask = (1ULL << world->transform_type) |
                               (1ULL << world->verlet_type) |
                               (1ULL << world->collider_type);
  printf("Registering physics system with mask %" PRIu64
         " (transform=%d, verlet=%d)\n",
         physics_mask, world->transform_type, world->verlet_type);
  ecs_register_system(ecs, physics_system_update, physics_mask);
}

void physics_world_cleanup(PhysicsWorld *world) {
  spatial_grid_cleanup(&world->spatial_grid);
  arena_cleanup(&world->spatial_arena);
  g_physics_world = NULL;
}

Entity physics_create_circle(PhysicsWorld *world, Vec3 position, float radius,
                             float mass) {
  Entity entity = ecs_create_entity(world->ecs);

  Transform *transform =
      (Transform *)ecs_add_component(world->ecs, entity, world->transform_type);
  *transform = (Transform){0};
  transform->position = position;
  transform->scale = (Vec3){1.0f, 1.0f, 1.0f};

  VerletBody *verlet =
      (VerletBody *)ecs_add_component(world->ecs, entity, world->verlet_type);
  verlet->velocity = (Vec3){0};
  verlet->acceleration = (Vec3){0};
  verlet->old_position = position;
  verlet->is_sleeping = false; // Start awake
  verlet->sleep_timer = 0;

  CircleCollider *collider = (CircleCollider *)ecs_add_component(
      world->ecs, entity, world->collider_type);
  collider->radius = radius;
  collider->mass = mass;
  collider->restitution = PHYSICS_DEFAULT_RESTITUTION;

  return entity;
}

void physics_set_boundary(PhysicsWorld *world, Vec3 center, float radius) {
  world->boundary_center = center;
  world->boundary_radius = radius;
}

void physics_system_update(float delta_time) {
  static int frame_count = 0;
  if (!g_physics_world) {
    printf("NO PHYSICS SYSTEM");
    return;
  }

  if (frame_count % 120 == 0) {
    printf("Physics system running frame %d\n", frame_count);
  }

  physics_verlet_integration(g_physics_world, delta_time);

  for (int i = 0; i < g_physics_world->collision_iterations; i++) {
    physics_solve_collisions(g_physics_world);
    physics_apply_constraints(g_physics_world);
  }

  frame_count++;
}

void physics_verlet_integration(PhysicsWorld *world, float delta_time) {
  for (Entity entity = 1; entity < world->ecs->next_entity_id; entity++) {
    if (!ecs_entity_active(world->ecs, entity))
      continue;

    if (!ecs_has_component(world->ecs, entity, world->transform_type) ||
        !ecs_has_component(world->ecs, entity, world->verlet_type)) {
      continue;
    }

    Transform *transform = (Transform *)ecs_get_component(
        world->ecs, entity, world->transform_type);
    VerletBody *verlet =
        (VerletBody *)ecs_get_component(world->ecs, entity, world->verlet_type);

    Vec3 current_position = transform->position;

    // Calculate current velocity from position difference
    Vec3 velocity = vec3_multiply(
        vec3_add(current_position, vec3_multiply(verlet->old_position, -1.0f)),
        1.0f / delta_time);
    float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);
    verlet->velocity = velocity;

    // Sleep management
    if (verlet->is_sleeping) {
      // Check if we should wake up (collision or significant external force)
      float accel_magnitude =
          sqrtf(verlet->acceleration.x * verlet->acceleration.x +
                verlet->acceleration.y * verlet->acceleration.y);
      if (speed > PHYSICS_WAKE_VELOCITY_THRESHOLD ||
          accel_magnitude > PHYSICS_WAKE_VELOCITY_THRESHOLD) {
        verlet->is_sleeping = false;
        verlet->sleep_timer = 0;
      } else {
        // Stay asleep - no physics integration
        verlet->acceleration = (Vec3){0};
        continue;
      }
    } else {
      // Check if we should go to sleep
      if (speed < PHYSICS_SLEEP_VELOCITY_THRESHOLD) {
        verlet->sleep_timer++;
        if (verlet->sleep_timer >= PHYSICS_SLEEP_TIME_THRESHOLD) {
          verlet->is_sleeping = true;
          verlet->velocity = (Vec3){0};
          verlet->acceleration = (Vec3){0};
          continue;
        }
      } else {
        verlet->sleep_timer = 0; // Reset timer if moving too fast
      }
    }

    verlet->acceleration = vec3_add(verlet->acceleration, world->gravity);

    Vec3 new_position = vec3_add(
        current_position,
        vec3_add(
            vec3_multiply(vec3_add(current_position,
                                   vec3_multiply(verlet->old_position, -1.0f)),
                          world->damping),
            vec3_multiply(verlet->acceleration, delta_time * delta_time)));

    verlet->old_position = current_position;
    transform->position = new_position;

    verlet->acceleration = (Vec3){0};
  }
}

void physics_solve_collisions(PhysicsWorld *world) {

  // Reset arena for this frame's spatial allocations
  arena_reset(&world->spatial_arena);

  // Track arena usage and sleeping objects for debugging
  static int frame_count = 0;
  if (frame_count == 0) {
    ArenaStats stats = {0};
    arena_get_stats(&world->spatial_arena, &stats);
    printf("Spatial arena initialized: Size: %zu KB\n",
           stats.total_size / 1024);
  }

  // Count sleeping objects every 300 frames (5 seconds at 60fps)
  if (frame_count % 300 == 0 && frame_count > 0) {
    int sleeping_count = 0;
    int total_count = 0;
    for (Entity entity = 1; entity < world->ecs->next_entity_id; entity++) {
      if (!ecs_entity_active(world->ecs, entity))
        continue;
      if (!ecs_has_component(world->ecs, entity, world->verlet_type))
        continue;

      VerletBody *verlet = (VerletBody *)ecs_get_component(world->ecs, entity,
                                                           world->verlet_type);
      total_count++;
      if (verlet->is_sleeping)
        sleeping_count++;
    }
    printf("Frame %d: %d/%d objects sleeping (%.1f%%)\n", frame_count,
           sleeping_count, total_count,
           total_count > 0 ? (float)sleeping_count / total_count * 100.0f
                           : 0.0f);
  }

  frame_count++;

  spatial_grid_clear(&world->spatial_grid);

  // Insert all entities into spatial grid (skip sleeping objects for
  // optimization)
  for (Entity entity = 1; entity < world->ecs->next_entity_id; entity++) {
    if (!ecs_entity_active(world->ecs, entity)) {
      continue;
    }

    if (!ecs_has_component(world->ecs, entity, world->transform_type) ||
        !ecs_has_component(world->ecs, entity, world->verlet_type) ||
        !ecs_has_component(world->ecs, entity, world->collider_type)) {
      continue;
    }

    VerletBody *verlet =
        (VerletBody *)ecs_get_component(world->ecs, entity, world->verlet_type);
    if (verlet->is_sleeping) {
      continue; // Skip sleeping objects for collision detection optimization
    }

    Transform *transform = (Transform *)ecs_get_component(
        world->ecs, entity, world->transform_type);
    CircleCollider *collider = (CircleCollider *)ecs_get_component(
        world->ecs, entity, world->collider_type);

    spatial_grid_insert(&world->spatial_grid, &world->spatial_arena, entity,
                        transform->position, collider->radius);
  }

  // Check collisions using spatial partitioning
  for (Entity entity1 = 1; entity1 < world->ecs->next_entity_id; entity1++) {
    if (!ecs_entity_active(world->ecs, entity1)) {
      continue;
    }

    if (!ecs_has_component(world->ecs, entity1, world->transform_type) ||
        !ecs_has_component(world->ecs, entity1, world->verlet_type) ||
        !ecs_has_component(world->ecs, entity1, world->collider_type)) {
      continue;
    }

    Transform *t1 = (Transform *)ecs_get_component(world->ecs, entity1,
                                                   world->transform_type);
    VerletBody *v1 = (VerletBody *)ecs_get_component(world->ecs, entity1,
                                                     world->verlet_type);
    CircleCollider *c1 = (CircleCollider *)ecs_get_component(
        world->ecs, entity1, world->collider_type);

    if (v1->is_sleeping) {
      continue; // Skip sleeping objects as primary collision entity
    }

    // Get potential collision candidates from spatial grid
    Entity *potential_entities;
    int potential_count;
    spatial_grid_get_potential_collisions(
        &world->spatial_grid, entity1, t1->position, c1->radius,
        &potential_entities, &potential_count);

    // Check collisions with potential candidates
    for (int i = 0; i < potential_count; i++) {
      Entity entity2 = potential_entities[i];

      // Skip if entity2 < entity1 to avoid duplicate checks (but allow entity2
      // == entity1 + 1, entity1 + 2, etc.)
      if (entity2 < entity1) {
        continue;
      }

      if (!ecs_entity_active(world->ecs, entity2)) {
        continue;
      }

      if (!ecs_has_component(world->ecs, entity2, world->transform_type) ||
          !ecs_has_component(world->ecs, entity2, world->verlet_type) ||
          !ecs_has_component(world->ecs, entity2, world->collider_type)) {
        continue;
      }

      Transform *t2 = (Transform *)ecs_get_component(world->ecs, entity2,
                                                     world->transform_type);
      VerletBody *v2 = (VerletBody *)ecs_get_component(world->ecs, entity2,
                                                       world->verlet_type);
      CircleCollider *c2 = (CircleCollider *)ecs_get_component(
          world->ecs, entity2, world->collider_type);

      Vec3 normal;
      float penetration;
      if (circle_circle_collision(t1->position, c1->radius, t2->position,
                                  c2->radius, &normal, &penetration)) {
        resolve_circle_collision(t1, v1, c1, t2, v2, c2, normal, penetration);
      }
    }
  }
}

void physics_apply_constraints(PhysicsWorld *world) {
  for (Entity entity = 1; entity < world->ecs->next_entity_id; entity++) {
    if (!ecs_entity_active(world->ecs, entity))
      continue;

    if (!ecs_has_component(world->ecs, entity, world->transform_type) ||
        !ecs_has_component(world->ecs, entity, world->collider_type)) {
      continue;
    }

    Transform *transform = (Transform *)ecs_get_component(
        world->ecs, entity, world->transform_type);
    CircleCollider *collider = (CircleCollider *)ecs_get_component(
        world->ecs, entity, world->collider_type);

    Vec3 to_center = vec3_add(world->boundary_center,
                              vec3_multiply(transform->position, -1.0f));
    float distance =
        sqrtf(to_center.x * to_center.x + to_center.y * to_center.y);
    float max_distance = world->boundary_radius - collider->radius;

    if (distance > max_distance) {
      Vec3 direction = vec3_multiply(to_center, 1.0f / distance);
      transform->position = vec3_add(world->boundary_center,
                                     vec3_multiply(direction, -max_distance));
    }
  }
}

bool circle_circle_collision(Vec3 pos1, float r1, Vec3 pos2, float r2,
                             Vec3 *normal, float *penetration) {
  Vec3 diff = vec3_add(pos2, vec3_multiply(pos1, -1.0f));
  float distance_sq = diff.x * diff.x + diff.y * diff.y;
  float radius_sum = r1 + r2;

  if (distance_sq >= radius_sum * radius_sum) {
    return false;
  }

  float distance = sqrtf(distance_sq);
  *penetration = radius_sum - distance;

  if (distance >
      PHYSICS_OVERLAP_THRESHOLD) { // Use small threshold instead of 0
    *normal = vec3_multiply(diff, 1.0f / distance);
  } else {
    // Generate random direction when circles are nearly overlapping
    // TODO: do we need this anymore?
    float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
    *normal = (Vec3){cosf(angle), sinf(angle), 0.0f};
  }
  return true;
}

void resolve_circle_collision(Transform *t1, VerletBody *v1, CircleCollider *c1,
                              Transform *t2, VerletBody *v2, CircleCollider *c2,
                              Vec3 normal, float penetration) {
  // Wake up any sleeping objects involved in collision
  if (v1->is_sleeping) {
    v1->is_sleeping = false;
    v1->sleep_timer = 0;
  }
  if (v2->is_sleeping) {
    v2->is_sleeping = false;
    v2->sleep_timer = 0;
  }

  // Clamp penetration to prevent numerical explosion
  float max_penetration =
      (c1->radius + c2->radius) * PHYSICS_MAX_PENETRATION_RATIO;
  if (penetration > max_penetration) {
    penetration = max_penetration;
  }

  // Remove minimum threshold - all overlaps must be resolved to prevent accumulation

  float total_mass = c1->mass + c2->mass;
  float mass_ratio_1 = c2->mass / total_mass;
  float mass_ratio_2 = c1->mass / total_mass;

  // Use consistent correction factor to prevent energy accumulation
  float correction_factor = PHYSICS_CORRECTION_FACTOR;

  // Ensure normal is properly normalized and Z component is 0 for 2D
  normal.z = 0.0f;
  float normal_length = sqrtf(normal.x * normal.x + normal.y * normal.y);
  if (normal_length > PHYSICS_OVERLAP_THRESHOLD) {
    normal.x /= normal_length;
    normal.y /= normal_length;
  } else {
    // Fallback to random direction if normal is invalid
    float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
    normal.x = cosf(angle);
    normal.y = sinf(angle);
    normal.z = 0.0f;
  }

  // Position correction with consistent factor
  Vec3 correction = vec3_multiply(normal, penetration * correction_factor);
  t1->position =
      vec3_add(t1->position, vec3_multiply(correction, -mass_ratio_1));
  t2->position =
      vec3_add(t2->position, vec3_multiply(correction, mass_ratio_2));
  
  // Add velocity damping to prevent energy buildup
  Vec3 relative_velocity = vec3_add(v1->velocity, vec3_multiply(v2->velocity, -1.0f));
  float relative_speed = vec3_dot(relative_velocity, normal);
  
  if (relative_speed < 0) {
    // Objects are approaching - apply restitution
    Vec3 impulse = vec3_multiply(normal, -(1.0f + PHYSICS_DEFAULT_RESTITUTION) * relative_speed / 2.0f);
    
    v1->velocity = vec3_add(v1->velocity, vec3_multiply(impulse, mass_ratio_1));
    v2->velocity = vec3_add(v2->velocity, vec3_multiply(impulse, -mass_ratio_2));
  }
}

// Spatial partitioning implementation with arena allocator
void spatial_grid_init(SpatialGrid *grid, Arena *arena, Vec3 origin,
                       float width, float height, float cell_size) {
  (void)arena; // TODO: Arena parameter kept for API compatibility but not used
               // since we malloc cells directly
  grid->grid_origin = origin;
  grid->cell_size = cell_size;
  grid->grid_width = (int)(width / cell_size) + 1;
  grid->grid_height = (int)(height / cell_size) + 1;

  int total_cells = grid->grid_width * grid->grid_height;
  // Use malloc for cells array to avoid issues with arena reallocation
  grid->cells = (SpatialCell *)malloc(sizeof(SpatialCell) * total_cells);

  if (!grid->cells) {
    printf(
        "ERROR: Failed to allocate spatial grid cells (%d cells, %zu bytes)\\n",
        total_cells, sizeof(SpatialCell) * total_cells);
    return;
  }

  memset(grid->cells, 0, sizeof(SpatialCell) * total_cells);
  printf("Spatial grid initialized: %dx%d = %d cells\\n", grid->grid_width,
         grid->grid_height, total_cells);
}

void spatial_grid_cleanup(SpatialGrid *grid) {
  if (grid && grid->cells) {
    free(grid->cells);
  }
  memset(grid, 0, sizeof(SpatialGrid));
}

void spatial_grid_clear(SpatialGrid *grid) {
  // With arena allocator, we just reset all cell entity lists to NULL
  // The arena will be reset between frames
  if (!grid || !grid->cells) {
    printf("WARNING: spatial_grid_clear called with null grid or cells\n");
    return; // Grid not initialized
  }

  int total_cells = grid->grid_width * grid->grid_height;

  for (int i = 0; i < total_cells; i++) {
    grid->cells[i].entities = NULL;
  }
}

static int spatial_grid_hash(SpatialGrid *grid, int x, int y) {
  if (x < 0 || x >= grid->grid_width || y < 0 || y >= grid->grid_height) {
    return -1;
  }
  return y * grid->grid_width + x;
}

static void spatial_grid_get_cell_coords(SpatialGrid *grid, Vec3 position,
                                         int *x, int *y) {
  *x = (int)((position.x - grid->grid_origin.x) / grid->cell_size);
  *y = (int)((position.y - grid->grid_origin.y) / grid->cell_size);
}

void spatial_grid_insert(SpatialGrid *grid, Arena *arena, Entity entity,
                         Vec3 position, float radius) {
  if (!grid || !grid->cells || !arena) {
    printf(
        "ERROR: spatial_grid_insert called with null grid, cells, or arena\n");
    return;
  }

  // Calculate which cells this entity overlaps (considering its radius)
  int min_x, min_y, max_x, max_y;

  Vec3 min_pos = (Vec3){position.x - radius, position.y - radius, 0.0f};
  Vec3 max_pos = (Vec3){position.x + radius, position.y + radius, 0.0f};

  spatial_grid_get_cell_coords(grid, min_pos, &min_x, &min_y);
  spatial_grid_get_cell_coords(grid, max_pos, &max_x, &max_y);

  // Insert entity into all overlapping cells using arena allocation
  for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
      int cell_index = spatial_grid_hash(grid, x, y);
      if (cell_index >= 0) {
        EntityNode *node = (EntityNode *)arena_alloc(arena, sizeof(EntityNode));
        if (node) {
          node->entity = entity;
          node->next = grid->cells[cell_index].entities;
          grid->cells[cell_index].entities = node;
        } else {
          printf(
              "ERROR: Failed to allocate EntityNode from arena for entity %d\n",
              entity);
          ArenaStats stats = {0};
          arena_get_stats(arena, &stats);
          printf("Arena stats: Size=%zu, Used=%zu, Free=%zu\n",
                 stats.total_size, stats.used_bytes, stats.free_bytes);
          return; // Early return to prevent further crashes
        }
      }
    }
  }
}

void spatial_grid_get_potential_collisions(SpatialGrid *grid, Entity entity,
                                           Vec3 position, float radius,
                                           Entity **out_entities,
                                           int *out_count) {
  static Entity
      potential_entities[PHYSICS_SPATIAL_BUFFER_SIZE]; // Static buffer to avoid
                                                       // allocation
  int count = 0;

  // Calculate which cells this entity overlaps
  int min_x, min_y, max_x, max_y;

  Vec3 min_pos = (Vec3){position.x - radius, position.y - radius, 0.0f};
  Vec3 max_pos = (Vec3){position.x + radius, position.y + radius, 0.0f};

  spatial_grid_get_cell_coords(grid, min_pos, &min_x, &min_y);
  spatial_grid_get_cell_coords(grid, max_pos, &max_x, &max_y);

  // Collect all entities from overlapping cells
  for (int y = min_y; y <= max_y && count < PHYSICS_SPATIAL_BUFFER_SIZE; y++) {
    for (int x = min_x; x <= max_x && count < PHYSICS_SPATIAL_BUFFER_SIZE;
         x++) {
      int cell_index = spatial_grid_hash(grid, x, y);
      if (cell_index >= 0) {
        EntityNode *current = grid->cells[cell_index].entities;
        while (current && count < PHYSICS_SPATIAL_BUFFER_SIZE) {
          // Don't include the entity itself and avoid duplicates
          if (current->entity != entity) {
            bool already_added = false;
            for (int i = 0; i < count; i++) {
              if (potential_entities[i] == current->entity) {
                already_added = true;
                break;
              }
            }
            if (!already_added) {
              potential_entities[count++] = current->entity;
            }
          }
          current = current->next;
        }
      }
    }
  }

  *out_entities = potential_entities;
  *out_count = count;
}
