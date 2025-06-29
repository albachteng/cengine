#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float x, y, z, w;
} Vec4;

typedef struct {
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
} Transform;

typedef enum {
    SHAPE_TRIANGLE,
    SHAPE_QUAD,
    SHAPE_CIRCLE,
    SHAPE_SPRITE,
    SHAPE_CUSTOM_MESH
} ShapeType;

typedef struct {
    float r, g, b, a;
} Color;

typedef struct {
    Vec3* vertices;
    uint32_t vertex_count;
    uint32_t* indices;
    uint32_t index_count;
} Mesh;

typedef struct {
    const char* texture_path;
    int width, height;
    uint32_t texture_id;
} Texture;

typedef struct {
    ShapeType shape;
    Color color;
    bool visible;
    
    union {
        struct {
            float radius;
        } circle;
        
        struct {
            float width, height;
        } quad;
        
        struct {
            Texture texture;
            Vec3 size;
        } sprite;
        
        struct {
            Mesh mesh;
        } custom;
    } data;
    
    uint32_t layer;
} Renderable;

// ZII: Use Transform t = {0}; then set fields directly
// transform_identity() == {0} with .scale = {1,1,1}
void transform_translate(Transform* transform, Vec3 translation);
void transform_rotate(Transform* transform, Vec3 rotation);
void transform_scale_uniform(Transform* transform, float scale);
void transform_scale(Transform* transform, Vec3 scale);

// ZII: Use Color c = {r, g, b, a}; for direct initialization
Color color_from_zero(Color* c, float r, float g, float b, float a); // ZII-compatible
Color color_white(void);
Color color_black(void);
Color color_red(void);
Color color_green(void);
Color color_blue(void);

// ZII: Use Renderable r = {0}; then set .shape, .color, .visible=true, and shape-specific data

// Vec2 operations (optimized for 2D physics)
// ZII: Use Vec2 v = {x, y}; for direct initialization
Vec2 vec2_add(Vec2 a, Vec2 b);
Vec2 vec2_multiply(Vec2 v, float scalar);
float vec2_length_squared(Vec2 v);
float vec2_length(Vec2 v);
Vec2 vec2_normalize(Vec2 v);

// Vec3 operations (for 3D rendering interface)
// ZII: Use Vec3 v = {x, y, z}; for direct initialization
// vec3_zero() == {0}
// vec3_one() == {1.0f, 1.0f, 1.0f}
static inline Vec3 vec3_one(void) { return (Vec3){1.0f, 1.0f, 1.0f}; }
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_multiply(Vec3 v, float scalar);
float vec3_dot(Vec3 a, Vec3 b);

// Conversion utilities
static inline Vec3 vec2_to_vec3(Vec2 v, float z) { return (Vec3){v.x, v.y, z}; }
static inline Vec2 vec3_to_vec2(Vec3 v) { return (Vec2){v.x, v.y}; }

#endif