#include "ecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ecs_init(ECS *ecs) {
  memset(ecs, 0, sizeof(ECS));
  ecs->next_entity_id = 1;
}

void ecs_cleanup(ECS *ecs) {
  for (size_t i = 0; i < ecs->component_count; i++) {
    if (ecs->components[i].data) {
      free(ecs->components[i].data);
    }
  }
  memset(ecs, 0, sizeof(ECS));
}

Entity ecs_create_entity(ECS *ecs) {
  if (ecs->next_entity_id >= MAX_ENTITIES) {
    fprintf(stderr, "Maximum entities exceeded\n");
    return 0;
  }

  Entity entity = ecs->next_entity_id++;
  ecs->entities[entity].mask = 0;
  ecs->entities[entity].active = true;

  return entity;
}

void ecs_destroy_entity(ECS *ecs, Entity entity) {
  if (entity >= MAX_ENTITIES || !ecs->entities[entity].active) {
    return;
  }

  ecs->entities[entity].active = false;
  ecs->entities[entity].mask = 0;
}

bool ecs_entity_active(ECS *ecs, Entity entity) {
  return entity < MAX_ENTITIES && ecs->entities[entity].active;
}

ComponentType ecs_register_component(ECS *ecs, size_t component_size) {
  if (ecs->component_count >= MAX_COMPONENTS) {
    fprintf(stderr, "Maximum components exceeded\n");
    return MAX_COMPONENTS;
  }

  ComponentType type = ecs->component_count++;
  ComponentArray *array = &ecs->components[type];

  array->component_size = component_size;
  array->capacity = MAX_ENTITIES;
  array->data = calloc(MAX_ENTITIES, component_size);
  array->count = 0;

  if (!array->data) {
    fprintf(stderr, "Failed to allocate component array\n");
    return MAX_COMPONENTS;
  }

  return type;
}

void *ecs_add_component(ECS *ecs, Entity entity, ComponentType type) {
  if (!ecs_entity_active(ecs, entity) || type >= ecs->component_count) {
    return NULL;
  }

  ecs->entities[entity].mask |= (1ULL << type);

  ComponentArray *array = &ecs->components[type];
  return (char *)array->data + (entity * array->component_size);
}

void *ecs_get_component(ECS *ecs, Entity entity, ComponentType type) {
  if (!ecs_has_component(ecs, entity, type)) {
    return NULL;
  }

  ComponentArray *array = &ecs->components[type];
  return (char *)array->data + (entity * array->component_size);
}

void ecs_remove_component(ECS *ecs, Entity entity, ComponentType type) {
  if (!ecs_entity_active(ecs, entity) || type >= ecs->component_count) {
    return;
  }

  ecs->entities[entity].mask &= ~(1ULL << type);
}

bool ecs_has_component(ECS *ecs, Entity entity, ComponentType type) {
  if (!ecs_entity_active(ecs, entity) || type >= ecs->component_count) {
    return false;
  }

  return (ecs->entities[entity].mask & (1ULL << type)) != 0;
}

void ecs_register_system(ECS *ecs, SystemFunc system_func,
                         ComponentMask required_components) {
  if (ecs->system_count >= MAX_SYSTEMS) {
    fprintf(stderr, "Maximum systems exceeded\n");
    return;
  }

  System *system = &ecs->systems[ecs->system_count++];
  system->update = system_func;
  system->required_components = required_components;
  system->active = true;
}

void ecs_update_systems(ECS *ecs, float delta_time) {
  static int debug_frame = 0;
  if (debug_frame % 120 == 0) {
    printf("ecs_update_systems: Running %d systems\n", ecs->system_count);
  }

  for (uint32_t i = 0; i < ecs->system_count; i++) {
    System *system = &ecs->systems[i];
    if (system->active && system->update) {
      // if (debug_frame % 120 == 0) {
      //   printf("  Calling system %d (mask: %llu)\n", i,
      //   system->required_components);
      // }
      system->update(delta_time);
    }
  }
  debug_frame++;
}
