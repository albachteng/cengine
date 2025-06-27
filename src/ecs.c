#include "ecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ecs_init(ECS *ecs) {
  // ZII: ecs should already be zero-initialized
  memset(ecs, 0, sizeof(ECS));
  ecs->next_entity_id = 1;
  
  // Initialize arena pool for component allocations
  if (!arena_pool_init(&ecs->component_arena_pool)) {
    fprintf(stderr, "Failed to initialize ECS component arena pool\n");
  }
}

void ecs_cleanup(ECS *ecs) {
  // Component data is allocated in arena pool, so we just clean up the pool
  arena_pool_cleanup(&ecs->component_arena_pool);
  
  // Reset to ZII state
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
  
  // Allocate component array from arena pool
  // Each component type gets MAX_ENTITIES slots (e.g., Transform: 2048 * 36 bytes = 72KB)
  size_t total_size = MAX_ENTITIES * component_size;
  array->data = arena_pool_alloc(&ecs->component_arena_pool, total_size);
  array->count = 0;

  if (!array->data) {
    fprintf(stderr, "Failed to allocate component array from arena pool\n");
    return MAX_COMPONENTS;
  }
  
  // Zero-initialize the component array
  memset(array->data, 0, total_size);

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
    printf("ecs_update_systems: Running %zu systems\n", ecs->system_count);
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

void ecs_iterate_entities(ECS *ecs, ComponentMask component_mask, EntityIteratorFunc func, void *user_data) {
  if (!func) {
    return;
  }
  
  for (Entity entity = 1; entity < ecs->next_entity_id; entity++) {
    if (!ecs_entity_active(ecs, entity)) {
      continue;
    }
    
    // Check if entity has all required components
    if ((ecs->entities[entity].mask & component_mask) == component_mask) {
      func(ecs, entity, user_data);
    }
  }
}

size_t ecs_get_entities_with_components(ECS *ecs, ComponentMask component_mask, Entity *out_entities, size_t max_entities) {
  if (!out_entities || max_entities == 0) {
    return 0;
  }
  
  size_t count = 0;
  for (Entity entity = 1; entity < ecs->next_entity_id && count < max_entities; entity++) {
    if (!ecs_entity_active(ecs, entity)) {
      continue;
    }
    
    // Check if entity has all required components
    if ((ecs->entities[entity].mask & component_mask) == component_mask) {
      out_entities[count++] = entity;
    }
  }
  
  return count;
}
