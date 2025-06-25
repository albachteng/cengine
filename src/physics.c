#include "physics.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

  g_physics_world = world;

  ComponentMask physics_mask = (1ULL << world->transform_type) |
                               (1ULL << world->verlet_type) |
                               (1ULL << world->collider_type);
  printf(
      "Registering physics system with mask %llu (transform=%d, verlet=%d)\n",
      physics_mask, world->transform_type, world->verlet_type);
  ecs_register_system(ecs, physics_system_update, physics_mask);
}

void physics_world_cleanup(PhysicsWorld *world) {
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
  static int collision_debug_frame = 0;
  bool debug_this_frame = (collision_debug_frame % 240 == 0);

  for (Entity entity1 = 1; entity1 < world->ecs->next_entity_id; entity1++) {
    if (!ecs_entity_active(world->ecs, entity1)) {
      printf("ENTITY 1 IS NOT ACTIVE");
      continue;
    }

    if (!ecs_has_component(world->ecs, entity1, world->transform_type) ||
        !ecs_has_component(world->ecs, entity1, world->verlet_type) ||
        !ecs_has_component(world->ecs, entity1, world->collider_type)) {
      printf("MISSING COMPONENT IN physics_solve_collisions");
      continue;
    }

    Transform *t1 = (Transform *)ecs_get_component(world->ecs, entity1,
                                                   world->transform_type);
    VerletBody *v1 = (VerletBody *)ecs_get_component(world->ecs, entity1,
                                                     world->verlet_type);
    CircleCollider *c1 = (CircleCollider *)ecs_get_component(
        world->ecs, entity1, world->collider_type);

    for (Entity entity2 = entity1 + 1; entity2 < world->ecs->next_entity_id;
         entity2++) {
      if (!ecs_entity_active(world->ecs, entity2)) {
        printf("ENTITY 2 IS NOT ACTIVE");
        continue;
      }

      if (!ecs_has_component(world->ecs, entity2, world->transform_type) ||
          !ecs_has_component(world->ecs, entity2, world->verlet_type) ||
          !ecs_has_component(world->ecs, entity2, world->collider_type)) {
        printf("ENTITY 2 IS MISSING COMPONENT");
        continue;
      }

      Transform *t2 = (Transform *)ecs_get_component(world->ecs, entity2,
                                                     world->transform_type);
      VerletBody *v2 = (VerletBody *)ecs_get_component(world->ecs, entity2,
                                                       world->verlet_type);
      CircleCollider *c2 = (CircleCollider *)ecs_get_component(
          world->ecs, entity2, world->collider_type);

      // if (debug_this_frame) {
      //   printf("Checking collision between entities %d and %d:
      //   pos1(%.2f,%.2f) "
      //          "r1=%.2f pos2(%.2f,%.2f) r2=%.2f\n",
      //          entity1, entity2, t1->position.x, t1->position.y, c1->radius,
      //          t2->position.x, t2->position.y, c2->radius);
      // }

      Vec3 normal;
      float penetration;
      if (circle_circle_collision(t1->position, c1->radius, t2->position,
                                  c2->radius, &normal, &penetration)) {
        if (debug_this_frame) {
          printf("COLLISION DETECTED! Penetration: %.2f\n", penetration);
          printf("COLLISION DETECTED! Normal: (x, y, z) (%.2f, %.2f, %.2f)\n",
                 normal.x, normal.y, normal.z);
        }
        resolve_circle_collision(t1, v1, c1, t2, v2, c2, normal, penetration);
      }
    }
  }
  collision_debug_frame++;
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

  if (distance > 0.0f) {
    *normal = vec3_multiply(diff, 1.0f / distance);
  } else {
    *normal = vec3_create(1.0f, 0.0f, 0.0f);
  }

  return true;
}

void resolve_circle_collision(Transform *t1, VerletBody *v1, CircleCollider *c1,
                              Transform *t2, VerletBody *v2, CircleCollider *c2,
                              Vec3 normal, float penetration) {
  (void)v1;
  (void)v2;

  float total_mass = c1->mass + c2->mass;
  float mass_ratio_1 = c2->mass / total_mass;
  float mass_ratio_2 = c1->mass / total_mass;

  // Apply full penetration correction plus small buffer
  float correction_factor = 1.1f;
  Vec3 correction = vec3_multiply(normal, penetration * correction_factor);
  t1->position =
      vec3_add(t1->position, vec3_multiply(correction, -mass_ratio_1));
  t2->position =
      vec3_add(t2->position, vec3_multiply(correction, mass_ratio_2));
}
