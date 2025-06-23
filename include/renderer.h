#ifndef RENDERER_H
#define RENDERER_H

#include "ecs.h"
#include "components.h"

typedef struct {
    ECS* ecs;
    ComponentType transform_type;
    ComponentType renderable_type;
    
    uint32_t triangle_vao, triangle_vbo;
    uint32_t quad_vao, quad_vbo, quad_ebo;
    uint32_t shader_program;
    
    int u_transform;
    int u_color;
} Renderer;

int renderer_init(Renderer* renderer, ECS* ecs);
void renderer_cleanup(Renderer* renderer);

void renderer_begin_frame(Renderer* renderer);
void renderer_end_frame(Renderer* renderer);
void renderer_render_entities(Renderer* renderer);

int renderer_create_shaders(Renderer* renderer);
void renderer_setup_triangle_mesh(Renderer* renderer);
void renderer_setup_quad_mesh(Renderer* renderer);
void renderer_render_triangle(Renderer* renderer, Transform* transform, Renderable* renderable);
void renderer_render_quad(Renderer* renderer, Transform* transform, Renderable* renderable);

void renderer_system_update(float delta_time);

extern Renderer* g_renderer;

#endif