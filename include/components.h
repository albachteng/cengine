#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <stdint.h>
#include <stdbool.h>

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

Transform transform_create(float x, float y, float z);
Transform transform_identity(void);
void transform_translate(Transform* transform, Vec3 translation);
void transform_rotate(Transform* transform, Vec3 rotation);
void transform_scale_uniform(Transform* transform, float scale);
void transform_scale(Transform* transform, Vec3 scale);

Color color_create(float r, float g, float b, float a);
Color color_from_zero(Color* c, float r, float g, float b, float a); // ZII-compatible
Color color_white(void);
Color color_black(void);
Color color_red(void);
Color color_green(void);
Color color_blue(void);

Renderable renderable_create_triangle(Color color);
Renderable renderable_create_quad(float width, float height, Color color);
Renderable renderable_create_circle(float radius, Color color);
Renderable renderable_create_sprite(const char* texture_path, float width, float height);

Vec3 vec3_create(float x, float y, float z);
Vec3 vec3_zero(void);
Vec3 vec3_one(void);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_multiply(Vec3 v, float scalar);

#endif