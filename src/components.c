#include "components.h"
#include <string.h>

Transform transform_create(float x, float y, float z) {
    Transform t = {0}; // ZII: zero initialize entire structure
    t.position = vec3_create(x, y, z);
    t.scale = vec3_one(); // rotation stays zero from initialization
    return t;
}

Transform transform_identity(void) {
    Transform t = {0}; // ZII: zero initialization with scale fixup
    t.scale = vec3_one();
    return t;
}

void transform_translate(Transform* transform, Vec3 translation) {
    transform->position = vec3_add(transform->position, translation);
}

void transform_rotate(Transform* transform, Vec3 rotation) {
    transform->rotation = vec3_add(transform->rotation, rotation);
}

void transform_scale_uniform(Transform* transform, float scale) {
    transform->scale = vec3_multiply(transform->scale, scale);
}

void transform_scale(Transform* transform, Vec3 scale) {
    transform->scale.x *= scale.x;
    transform->scale.y *= scale.y;
    transform->scale.z *= scale.z;
}

Color color_create(float r, float g, float b, float a) {
    Color c = {r, g, b, a};
    return c;
}

// ZII-compatible color creation from zero-initialized struct
Color color_from_zero(Color* c, float r, float g, float b, float a) {
    c->r = r; c->g = g; c->b = b; c->a = a;
    return *c;
}

Color color_white(void) { return color_create(1.0f, 1.0f, 1.0f, 1.0f); }
Color color_black(void) { return color_create(0.0f, 0.0f, 0.0f, 1.0f); }
Color color_red(void) { return color_create(1.0f, 0.0f, 0.0f, 1.0f); }
Color color_green(void) { return color_create(0.0f, 1.0f, 0.0f, 1.0f); }
Color color_blue(void) { return color_create(0.0f, 0.0f, 1.0f, 1.0f); }

Renderable renderable_create_triangle(Color color) {
    Renderable r = {0}; // ZII: zero initialize
    r.shape = SHAPE_TRIANGLE;
    r.color = color;
    r.visible = true;
    return r;
}

Renderable renderable_create_quad(float width, float height, Color color) {
    Renderable r = {0}; // ZII: zero initialize
    r.shape = SHAPE_QUAD;
    r.color = color;
    r.visible = true;
    r.data.quad.width = width;
    r.data.quad.height = height;
    return r;
}

Renderable renderable_create_circle(float radius, Color color) {
    Renderable r = {0}; // ZII: zero initialize
    r.shape = SHAPE_CIRCLE;
    r.color = color;
    r.visible = true;
    r.data.circle.radius = radius;
    return r;
}

Renderable renderable_create_sprite(const char* texture_path, float width, float height) {
    Renderable r = {0}; // ZII: zero initialize
    r.shape = SHAPE_SPRITE;
    r.color = color_white();
    r.visible = true;
    r.data.sprite.texture.texture_path = texture_path;
    r.data.sprite.size = vec3_create(width, height, 1.0f);
    return r;
}

Vec3 vec3_create(float x, float y, float z) {
    Vec3 v = {x, y, z};
    return v;
}

Vec3 vec3_zero(void) { return vec3_create(0.0f, 0.0f, 0.0f); }
Vec3 vec3_one(void) { return vec3_create(1.0f, 1.0f, 1.0f); }

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 vec3_multiply(Vec3 v, float scalar) {
    return vec3_create(v.x * scalar, v.y * scalar, v.z * scalar);
}