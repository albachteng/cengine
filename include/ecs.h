#ifndef ECS_H
#define ECS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_ENTITIES 1024
#define MAX_COMPONENTS 32
#define MAX_SYSTEMS 32

typedef uint32_t Entity;
typedef uint32_t ComponentType;
typedef uint64_t ComponentMask;

typedef struct {
    ComponentMask mask;
    bool active;
} EntityInfo;

typedef struct {
    void* data;
    size_t component_size;
    size_t count;
    size_t capacity;
} ComponentArray;

typedef void (*SystemFunc)(float delta_time);

typedef struct {
    SystemFunc update;
    ComponentMask required_components;
    bool active;
} System;

typedef struct {
    EntityInfo entities[MAX_ENTITIES];
    ComponentArray components[MAX_COMPONENTS];
    System systems[MAX_SYSTEMS];
    
    Entity next_entity_id;
    uint32_t component_count;
    uint32_t system_count;
} ECS;

Entity ecs_create_entity(ECS* ecs);
void ecs_destroy_entity(ECS* ecs, Entity entity);
bool ecs_entity_active(ECS* ecs, Entity entity);

ComponentType ecs_register_component(ECS* ecs, size_t component_size);
void* ecs_add_component(ECS* ecs, Entity entity, ComponentType type);
void* ecs_get_component(ECS* ecs, Entity entity, ComponentType type);
void ecs_remove_component(ECS* ecs, Entity entity, ComponentType type);
bool ecs_has_component(ECS* ecs, Entity entity, ComponentType type);

void ecs_register_system(ECS* ecs, SystemFunc system_func, ComponentMask required_components);
void ecs_update_systems(ECS* ecs, float delta_time);

void ecs_init(ECS* ecs);
void ecs_cleanup(ECS* ecs);

#endif