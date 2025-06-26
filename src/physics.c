#include "physics.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

  world->gravity = vec3_create(0.0f, -200.0f, 0.0f);
  world->damping = 0.99f;
  world->collision_iterations = 20;

  world->boundary_center = vec3_zero();
  world->boundary_radius = 300.0f;
  
  // Initialize spatial grid with appropriate cell size
  float cell_size = 20.0f; // Roughly 4x the average circle radius
  float grid_size = world->boundary_radius * 2.2f; // Cover the boundary with some margin
  Vec3 grid_origin = vec3_create(-grid_size / 2.0f, -grid_size / 2.0f, 0.0f);
  spatial_grid_init(&world->spatial_grid, grid_origin, grid_size, grid_size, cell_size);

  g_physics_world = world;

  ComponentMask physics_mask = (1ULL << world->transform_type) |
                               (1ULL << world->verlet_type) |
                               (1ULL << world->collider_type);
  printf(
      "Registering physics system with mask %" PRIu64 " (transform=%d, verlet=%d)\n",
      physics_mask, world->transform_type, world->verlet_type);
  ecs_register_system(ecs, physics_system_update, physics_mask);
}

void physics_world_cleanup(PhysicsWorld *world) {
  spatial_grid_cleanup(&world->spatial_grid);
  g_physics_world = NULL;
  (void)world;
}

Entity physics_create_circle(PhysicsWorld *world, Vec3 position, float radius,
                             float mass) {
  Entity entity = ecs_create_entity(world->ecs);

  Transform *transform =
      (Transform *)ecs_add_component(world->ecs, entity, world->transform_type);
  *transform = transform_create(position.x, position.y, position.z);

  VerletBody *verlet =
      (VerletBody *)ecs_add_component(world->ecs, entity, world->verlet_type);
  verlet->velocity = vec3_zero();
  verlet->acceleration = vec3_zero();
  verlet->old_position = position;

  CircleCollider *collider = (CircleCollider *)ecs_add_component(
      world->ecs, entity, world->collider_type);
  collider->radius = radius;
  collider->mass = mass;
  collider->restitution = 0.8f;

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

    verlet->acceleration = vec3_zero();
  }
}

void physics_solve_collisions(PhysicsWorld *world) {
  
  // Clear and populate spatial grid
  spatial_grid_clear(&world->spatial_grid);
  
  // Insert all entities into spatial grid
  for (Entity entity = 1; entity < world->ecs->next_entity_id; entity++) {
    if (!ecs_entity_active(world->ecs, entity)) {
      continue;
    }

    if (!ecs_has_component(world->ecs, entity, world->transform_type) ||
        !ecs_has_component(world->ecs, entity, world->verlet_type) ||
        !ecs_has_component(world->ecs, entity, world->collider_type)) {
      continue;
    }

    Transform *transform = (Transform *)ecs_get_component(world->ecs, entity, world->transform_type);
    CircleCollider *collider = (CircleCollider *)ecs_get_component(world->ecs, entity, world->collider_type);
    
    spatial_grid_insert(&world->spatial_grid, entity, transform->position, collider->radius);
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

    Transform *t1 = (Transform *)ecs_get_component(world->ecs, entity1, world->transform_type);
    VerletBody *v1 = (VerletBody *)ecs_get_component(world->ecs, entity1, world->verlet_type);
    CircleCollider *c1 = (CircleCollider *)ecs_get_component(world->ecs, entity1, world->collider_type);

    // Get potential collision candidates from spatial grid
    Entity *potential_entities;
    int potential_count;
    spatial_grid_get_potential_collisions(&world->spatial_grid, entity1, t1->position, c1->radius, &potential_entities, &potential_count);

    // Check collisions with potential candidates
    for (int i = 0; i < potential_count; i++) {
      Entity entity2 = potential_entities[i];
      
      // Skip if entity2 < entity1 to avoid duplicate checks (but allow entity2 == entity1 + 1, entity1 + 2, etc.)
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

      Transform *t2 = (Transform *)ecs_get_component(world->ecs, entity2, world->transform_type);
      VerletBody *v2 = (VerletBody *)ecs_get_component(world->ecs, entity2, world->verlet_type);
      CircleCollider *c2 = (CircleCollider *)ecs_get_component(world->ecs, entity2, world->collider_type);

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

  if (distance > 0.001f) {  // Use small threshold instead of 0
    *normal = vec3_multiply(diff, 1.0f / distance);
  } else {
    // Generate random direction when circles are nearly overlapping
    float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
    *normal = vec3_create(cosf(angle), sinf(angle), 0.0f);
  }

  return true;
}

void resolve_circle_collision(Transform *t1, VerletBody *v1, CircleCollider *c1,
                              Transform *t2, VerletBody *v2, CircleCollider *c2,
                              Vec3 normal, float penetration) {
  (void)v1;
  (void)v2;

  // Clamp penetration to prevent numerical explosion
  float max_penetration = (c1->radius + c2->radius) * 0.8f;
  if (penetration > max_penetration) {
    penetration = max_penetration;
  }

  float total_mass = c1->mass + c2->mass;
  float mass_ratio_1 = c2->mass / total_mass;
  float mass_ratio_2 = c1->mass / total_mass;

  // Reduce correction factor to improve stability
  float correction_factor = 0.8f;
  
  // Ensure normal is properly normalized and Z component is 0 for 2D
  normal.z = 0.0f;
  float normal_length = sqrtf(normal.x * normal.x + normal.y * normal.y);
  if (normal_length > 0.001f) {
    normal.x /= normal_length;
    normal.y /= normal_length;
  } else {
    // Fallback to random direction if normal is invalid
    float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
    normal.x = cosf(angle);
    normal.y = sinf(angle);
    normal.z = 0.0f;
  }
  
  Vec3 correction = vec3_multiply(normal, penetration * correction_factor);
  t1->position =
      vec3_add(t1->position, vec3_multiply(correction, -mass_ratio_1));
  t2->position =
      vec3_add(t2->position, vec3_multiply(correction, mass_ratio_2));
}

// Spatial partitioning implementation
void spatial_grid_init(SpatialGrid* grid, Vec3 origin, float width, float height, float cell_size) {
  grid->grid_origin = origin;
  grid->cell_size = cell_size;
  grid->grid_width = (int)(width / cell_size) + 1;
  grid->grid_height = (int)(height / cell_size) + 1;
  
  int total_cells = grid->grid_width * grid->grid_height;
  grid->cells = (SpatialCell*)malloc(sizeof(SpatialCell) * total_cells);
  
  for (int i = 0; i < total_cells; i++) {
    grid->cells[i].entities = NULL;
  }
}

void spatial_grid_cleanup(SpatialGrid* grid) {
  if (grid->cells) {
    spatial_grid_clear(grid);
    free(grid->cells);
    grid->cells = NULL;
  }
}

void spatial_grid_clear(SpatialGrid* grid) {
  int total_cells = grid->grid_width * grid->grid_height;
  
  for (int i = 0; i < total_cells; i++) {
    EntityNode* current = grid->cells[i].entities;
    while (current) {
      EntityNode* next = current->next;
      free(current);
      current = next;
    }
    grid->cells[i].entities = NULL;
  }
}

static int spatial_grid_hash(SpatialGrid* grid, int x, int y) {
  if (x < 0 || x >= grid->grid_width || y < 0 || y >= grid->grid_height) {
    return -1;
  }
  return y * grid->grid_width + x;
}

static void spatial_grid_get_cell_coords(SpatialGrid* grid, Vec3 position, int* x, int* y) {
  *x = (int)((position.x - grid->grid_origin.x) / grid->cell_size);
  *y = (int)((position.y - grid->grid_origin.y) / grid->cell_size);
}

void spatial_grid_insert(SpatialGrid* grid, Entity entity, Vec3 position, float radius) {
  // Calculate which cells this entity overlaps (considering its radius)
  int min_x, min_y, max_x, max_y;
  
  Vec3 min_pos = vec3_create(position.x - radius, position.y - radius, 0.0f);
  Vec3 max_pos = vec3_create(position.x + radius, position.y + radius, 0.0f);
  
  spatial_grid_get_cell_coords(grid, min_pos, &min_x, &min_y);
  spatial_grid_get_cell_coords(grid, max_pos, &max_x, &max_y);
  
  // Insert entity into all overlapping cells
  for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
      int cell_index = spatial_grid_hash(grid, x, y);
      if (cell_index >= 0) {
        EntityNode* node = (EntityNode*)malloc(sizeof(EntityNode));
        node->entity = entity;
        node->next = grid->cells[cell_index].entities;
        grid->cells[cell_index].entities = node;
      }
    }
  }
}

void spatial_grid_get_potential_collisions(SpatialGrid* grid, Entity entity, Vec3 position, float radius, Entity** out_entities, int* out_count) {
  static Entity potential_entities[1024]; // Static buffer to avoid allocation
  int count = 0;
  
  // Calculate which cells this entity overlaps
  int min_x, min_y, max_x, max_y;
  
  Vec3 min_pos = vec3_create(position.x - radius, position.y - radius, 0.0f);
  Vec3 max_pos = vec3_create(position.x + radius, position.y + radius, 0.0f);
  
  spatial_grid_get_cell_coords(grid, min_pos, &min_x, &min_y);
  spatial_grid_get_cell_coords(grid, max_pos, &max_x, &max_y);
  
  // Collect all entities from overlapping cells
  for (int y = min_y; y <= max_y && count < 1024; y++) {
    for (int x = min_x; x <= max_x && count < 1024; x++) {
      int cell_index = spatial_grid_hash(grid, x, y);
      if (cell_index >= 0) {
        EntityNode* current = grid->cells[cell_index].entities;
        while (current && count < 1024) {
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
