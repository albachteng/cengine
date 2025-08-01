#include "renderer.h"
#include "coordinate_system.h"
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// RENDER_COORD_SCALE_X, RENDER_COORD_SCALE_Y, RENDER_SCALE_FACTOR are defined
// in coordinate_system.h
#define RENDER_TRIANGLE_SCALE 0.5f
#define CIRCLE_SEGMENTS 16

Renderer *g_renderer = NULL;

int renderer_init(Renderer *renderer, ECS *ecs) {
  // ZII: zero initialize renderer structure
  memset(renderer, 0, sizeof(Renderer));

  renderer->ecs = ecs;
  renderer->transform_type = ecs_register_component(ecs, sizeof(Transform));
  renderer->renderable_type = ecs_register_component(ecs, sizeof(Renderable));

  g_renderer = renderer;

  ComponentMask required =
      (1ULL << renderer->transform_type) | (1ULL << renderer->renderable_type);
  ecs_register_system(ecs, renderer_system_update, required);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Set up OpenGL coordinate system for proper rendering
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0); // Normalized device coordinates
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  return 1;
}

void renderer_cleanup(Renderer *renderer) {
  g_renderer = NULL;
  (void)renderer;
}

int renderer_create_shaders(Renderer *renderer) {
  (void)renderer;
  return 1;
}

void renderer_setup_triangle_mesh(Renderer *renderer) { (void)renderer; }

void renderer_setup_quad_mesh(Renderer *renderer) { (void)renderer; }

void renderer_begin_frame(Renderer *renderer) {
  (void)renderer;
  glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_end_frame(Renderer *renderer) { (void)renderer; }

void renderer_render_triangle(Renderer *renderer, Transform *transform,
                              Renderable *renderable) {
  if (!renderable->visible)
    return;

  glPushMatrix();
  glTranslatef(transform->position.x / RENDER_COORD_SCALE_X,
               transform->position.y / RENDER_COORD_SCALE_Y, 0.0f);
  glScalef(transform->scale.x * RENDER_TRIANGLE_SCALE,
           transform->scale.y * RENDER_TRIANGLE_SCALE, 1.0f);

  glColor4f(renderable->color.r, renderable->color.g, renderable->color.b,
            renderable->color.a);

  glBegin(GL_TRIANGLES);
  glVertex3f(0.0f, 0.5f, 0.0f);
  glVertex3f(-0.5f, -0.5f, 0.0f);
  glVertex3f(0.5f, -0.5f, 0.0f);
  glEnd();

  glPopMatrix();

  (void)renderer;
}

void renderer_render_quad(Renderer *renderer, Transform *transform,
                          Renderable *renderable) {
  if (!renderable->visible)
    return;

  glPushMatrix();
  glTranslatef(transform->position.x / RENDER_COORD_SCALE_X,
               transform->position.y / RENDER_COORD_SCALE_Y, 0.0f);
  glScalef(
      transform->scale.x * renderable->data.quad.width / RENDER_SCALE_FACTOR,
      transform->scale.y * renderable->data.quad.height / RENDER_SCALE_FACTOR,
      1.0f);

  glColor4f(renderable->color.r, renderable->color.g, renderable->color.b,
            renderable->color.a);

  glBegin(GL_QUADS);
  glVertex3f(-0.5f, -0.5f, 0.0f);
  glVertex3f(0.5f, -0.5f, 0.0f);
  glVertex3f(0.5f, 0.5f, 0.0f);
  glVertex3f(-0.5f, 0.5f, 0.0f);
  glEnd();

  glPopMatrix();

  (void)renderer;
}

void renderer_render_entities(Renderer *renderer) {
  for (Entity entity = 1; entity < renderer->ecs->next_entity_id; entity++) {
    if (!ecs_entity_active(renderer->ecs, entity))
      continue;

    if (!ecs_has_component(renderer->ecs, entity, renderer->transform_type) ||
        !ecs_has_component(renderer->ecs, entity, renderer->renderable_type)) {
      continue;
    }

    Transform *transform = (Transform *)ecs_get_component(
        renderer->ecs, entity, renderer->transform_type);
    Renderable *renderable = (Renderable *)ecs_get_component(
        renderer->ecs, entity, renderer->renderable_type);

    switch (renderable->shape) {
    case SHAPE_TRIANGLE:
      renderer_render_triangle(renderer, transform, renderable);
      break;
    case SHAPE_QUAD:
      renderer_render_quad(renderer, transform, renderable);
      break;
    case SHAPE_CIRCLE:
      renderer_render_circle(renderer, transform, renderable);
      break;
    default:
      break;
    }
  }
}

void renderer_render_circle(Renderer *renderer, Transform *transform,
                            Renderable *renderable) {
  if (!renderable->visible)
    return;

  glPushMatrix();
  glTranslatef(transform->position.x / RENDER_SCALE_FACTOR,
               transform->position.y / RENDER_SCALE_FACTOR, 0.0f);
  glScalef(
      transform->scale.x * renderable->data.circle.radius / RENDER_SCALE_FACTOR,
      transform->scale.y * renderable->data.circle.radius / RENDER_SCALE_FACTOR,
      1.0f);

  glColor4f(renderable->color.r, renderable->color.g, renderable->color.b,
            renderable->color.a);

  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(0.0f, 0.0f);
  for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
    float angle = (float)i / (float)CIRCLE_SEGMENTS * 2.0f * (float)M_PI;
    glVertex2f(cosf(angle), sinf(angle));
  }
  glEnd();

  glPopMatrix();

  (void)renderer;
}

void renderer_system_update(float delta_time) {
  (void)delta_time;
  if (g_renderer) {
    renderer_render_entities(g_renderer);
  }
}
