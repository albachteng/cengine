#include "components.h"
#include <string.h>
#include <math.h>

// ZII-only approach: Use Transform t = {0}; then set fields directly
// For identity transform: Transform t = {0}; t.scale = (Vec3){1.0f, 1.0f, 1.0f};

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

// ZII-only approach: Use Color c = {r, g, b, a}; for direct initialization

// ZII-compatible color creation from zero-initialized struct
Color color_from_zero(Color* c, float r, float g, float b, float a) {
    c->r = r; c->g = g; c->b = b; c->a = a;
    return *c;
}

Color color_white(void) { return (Color){1.0f, 1.0f, 1.0f, 1.0f}; }
Color color_black(void) { return (Color){0.0f, 0.0f, 0.0f, 1.0f}; }
Color color_red(void) { return (Color){1.0f, 0.0f, 0.0f, 1.0f}; }
Color color_green(void) { return (Color){0.0f, 1.0f, 0.0f, 1.0f}; }
Color color_blue(void) { return (Color){0.0f, 0.0f, 1.0f, 1.0f}; }

// ZII-only approach: Use Renderable r = {0}; then set fields:
// r.shape = SHAPE_CIRCLE; r.color = (Color){1,0,0,1}; r.visible = true; r.data.circle.radius = 5.0f;

// ZII-only approach: Use Vec3 v = {x, y, z}; for direct initialization
// vec3_zero() == (Vec3){0}
// vec3_one() implemented as inline in header

// Vec2 operations (optimized for 2D physics)
Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}

Vec2 vec2_multiply(Vec2 v, float scalar) {
    return (Vec2){v.x * scalar, v.y * scalar};
}

float vec2_length_squared(Vec2 v) {
    return v.x * v.x + v.y * v.y;
}

float vec2_length(Vec2 v) {
    return sqrtf(vec2_length_squared(v));
}

Vec2 vec2_normalize(Vec2 v) {
    float length = vec2_length(v);
    if (length > 0.0f) {
        return vec2_multiply(v, 1.0f / length);
    }
    return (Vec2){0};
}

// Vec3 operations (for 3D rendering interface)
Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_multiply(Vec3 v, float scalar) {
    return (Vec3){v.x * scalar, v.y * scalar, v.z * scalar};
}